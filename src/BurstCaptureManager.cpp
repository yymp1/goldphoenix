#include "BurstCaptureManager.h"

#include "ImageUtils.h"

#include <algorithm>

namespace
{
constexpr int kActionTailFrames = 2;
constexpr int kMinCandidateGapFrames = 2;
constexpr int kPreferredActionFrame = 1;
constexpr int kMaxActionFrames = 18;
}

void BurstCaptureManager::setParameters(const BurstParameters& parameters)
{
    m_parameters = parameters;
}

void BurstCaptureManager::setRoi(const QRect& roi)
{
    m_roi = roi;
}

void BurstCaptureManager::resetCurrentAction()
{
    m_actionActive = false;
    m_actionFrameCounter = 0;
    m_silentFrameCounter = 0;
    m_actionStartPacket = {};
    m_candidates.clear();
}

void BurstCaptureManager::reset()
{
    resetCurrentAction();
    m_images.clear();
    m_frameNumbers.clear();
    m_captureId.clear();
    m_timestamp = {};
    m_reason.clear();
}

bool BurstCaptureManager::isActionActive() const
{
    return m_actionActive;
}

int BurstCaptureManager::capturedCount() const
{
    return m_images.size();
}

const QVector<cv::Mat>& BurstCaptureManager::images() const
{
    return m_images;
}

QString BurstCaptureManager::captureId() const
{
    return m_captureId;
}

QDateTime BurstCaptureManager::timestamp() const
{
    return m_timestamp;
}

QString BurstCaptureManager::reason() const
{
    return m_reason;
}

FramePacket BurstCaptureManager::clonePacket(const FramePacket& packet) const
{
    FramePacket copy = packet;
    if (!packet.frame.empty()) {
        copy.frame = packet.frame.clone();
    }
    return copy;
}

void BurstCaptureManager::ensureSession(const QString& reason)
{
    if (m_captureId.isEmpty()) {
        m_timestamp = QDateTime::currentDateTime();
        m_captureId = QStringLiteral("capture_%1")
            .arg(m_timestamp.toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    }

    m_reason = reason;
}

BurstResult BurstCaptureManager::buildResult() const
{
    BurstResult result;
    result.captureId = m_captureId;
    result.timestamp = m_timestamp;
    result.reason = m_reason;

    for (int index = 0; index < m_images.size(); ++index) {
        const qint64 frameNumber = (index < m_frameNumbers.size()) ? m_frameNumbers.at(index) : -1;
        result.images.push_back(ImageUtils::annotateFrameNumber(ImageUtils::matToQImage(m_images.at(index)), frameNumber));
        result.frameNumbers.push_back(frameNumber);
    }

    return result;
}

void BurstCaptureManager::appendImage(const cv::Mat& frame, qint64 frameNumber)
{
    if (frame.empty()) {
        return;
    }

    m_images.push_back(frame.clone());
    m_frameNumbers.push_back(frameNumber);
    while (m_images.size() > m_parameters.burstCount) {
        m_images.removeFirst();
        if (!m_frameNumbers.isEmpty()) {
            m_frameNumbers.removeFirst();
        }
    }
}

double BurstCaptureManager::scoreCandidate(const ActionCandidate& candidate) const
{
    const QRect effectiveRoi = m_roi.isEmpty() ? QRect(0, 0, candidate.packet.frame.cols, candidate.packet.frame.rows) : m_roi;
    const int desiredTopOffset = std::max(8, effectiveRoi.height() / 14);
    const int topOffset = std::max(0, candidate.motionBox.top() - effectiveRoi.top());
    const double topScore = std::abs(topOffset - desiredTopOffset) * 6.0;

    const double ageScore = std::abs(candidate.actionFrameIndex - kPreferredActionFrame) * 24.0
        + std::max(0, candidate.actionFrameIndex - 4) * 45.0;

    const double roiArea = std::max(1, effectiveRoi.width() * effectiveRoi.height());
    const double areaRatio = candidate.motionArea / roiArea;
    const double targetAreaRatio = 0.020;
    const double areaScore = std::abs(areaRatio - targetAreaRatio) * 4500.0;

    const double upperBandPenalty = (candidate.motionBox.top() > (effectiveRoi.top() + effectiveRoi.height() / 3))
        ? 400.0
        : 0.0;

    return topScore + ageScore + areaScore + upperBandPenalty;
}

void BurstCaptureManager::considerCandidate(const FramePacket& packet, const MotionResult& motion)
{
    if (!motion.motionDetected || motion.motionBox.isEmpty() || packet.frame.empty()) {
        return;
    }

    const QRect effectiveRoi = m_roi.isEmpty() ? QRect(0, 0, packet.frame.cols, packet.frame.rows) : m_roi;
    if (!effectiveRoi.intersects(motion.motionBox)) {
        return;
    }

    const int topOffset = motion.motionBox.top() - effectiveRoi.top();
    if (topOffset < 0 || topOffset > (effectiveRoi.height() / 2)) {
        return;
    }

    ActionCandidate candidate;
    candidate.packet = clonePacket(packet);
    candidate.motionBox = motion.motionBox;
    candidate.motionArea = motion.largestArea;
    candidate.actionFrameIndex = m_actionFrameCounter;
    candidate.score = scoreCandidate(candidate);
    m_candidates.push_back(std::move(candidate));
}

BurstUpdate BurstCaptureManager::captureManualFrame(const FramePacket& packet, const QString& reason)
{
    BurstUpdate update;
    if (packet.frame.empty()) {
        return update;
    }

    ensureSession(reason);
    appendImage(packet.frame, packet.frameNumber);
    update.sessionChanged = true;
    update.actionCompleted = true;
    update.capturedFrameNumbers.push_back(packet.frameNumber);
    update.result = buildResult();
    return update;
}

BurstUpdate BurstCaptureManager::startAction(const FramePacket& packet, const MotionResult& motion, const QString& reason)
{
    resetCurrentAction();
    ensureSession(reason);

    m_actionActive = true;
    m_actionStartPacket = clonePacket(packet);
    m_actionFrameCounter = 0;
    m_silentFrameCounter = 0;
    considerCandidate(packet, motion);

    BurstUpdate update;
    update.started = true;
    return update;
}

BurstUpdate BurstCaptureManager::processAction(const FramePacket& packet, const MotionResult& motion)
{
    BurstUpdate update;
    if (!m_actionActive || packet.frame.empty()) {
        return update;
    }

    ++m_actionFrameCounter;

    if (motion.motionDetected) {
        m_silentFrameCounter = 0;
        considerCandidate(packet, motion);
    } else {
        ++m_silentFrameCounter;
    }

    if (m_actionFrameCounter >= kMaxActionFrames || m_silentFrameCounter >= kActionTailFrames) {
        update = finalizeAction();
    }

    return update;
}

BurstUpdate BurstCaptureManager::finalizeAction()
{
    BurstUpdate update;
    update.actionCompleted = true;

    QVector<ActionCandidate> ranked = m_candidates;
    if (ranked.isEmpty() && m_actionStartPacket.isValid()) {
        ActionCandidate fallback;
        fallback.packet = clonePacket(m_actionStartPacket);
        fallback.motionBox = m_roi;
        fallback.motionArea = 0.0;
        fallback.actionFrameIndex = 0;
        fallback.score = 9999.0;
        ranked.push_back(std::move(fallback));
    }

    std::sort(ranked.begin(), ranked.end(), [](const ActionCandidate& left, const ActionCandidate& right) {
        if (left.score == right.score) {
            return left.packet.frameNumber < right.packet.frameNumber;
        }
        return left.score < right.score;
    });

    QVector<ActionCandidate> selected;
    for (const ActionCandidate& candidate : ranked) {
        bool tooClose = false;
        for (const ActionCandidate& accepted : selected) {
            if (std::abs(candidate.packet.frameNumber - accepted.packet.frameNumber) < kMinCandidateGapFrames) {
                tooClose = true;
                break;
            }
        }

        if (tooClose) {
            continue;
        }

        selected.push_back(candidate);
        if (selected.size() >= m_parameters.capturesPerAction) {
            break;
        }
    }

    std::sort(selected.begin(), selected.end(), [](const ActionCandidate& left, const ActionCandidate& right) {
        return left.packet.frameNumber < right.packet.frameNumber;
    });

    for (const ActionCandidate& candidate : selected) {
        appendImage(candidate.packet.frame, candidate.packet.frameNumber);
        update.capturedFrameNumbers.push_back(candidate.packet.frameNumber);
    }

    if (!selected.isEmpty()) {
        update.sessionChanged = true;
        update.result = buildResult();
    }

    resetCurrentAction();
    return update;
}
