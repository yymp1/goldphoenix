#include "ProcessingWorker.h"

ProcessingWorker::ProcessingWorker(std::shared_ptr<RingBuffer> buffer, QObject* parent)
    : QObject(parent)
    , m_buffer(std::move(buffer))
{
}

ProcessingWorker::~ProcessingWorker()
{
    stop();
}

void ProcessingWorker::start()
{
    if (m_running.load()) {
        return;
    }

    m_running = true;
    m_thread = std::thread(&ProcessingWorker::processingLoop, this);
}

void ProcessingWorker::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ProcessingWorker::setRoi(const QRect& roi)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roi = roi;
    m_detector.reset();
    m_burstManager.setRoi(roi);
}

void ProcessingWorker::setMotionParameters(const MotionParameters& parameters)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_detector.setParameters(parameters);
}

void ProcessingWorker::setBurstParameters(const BurstParameters& parameters)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_burstManager.setParameters(parameters);
}

void ProcessingWorker::setAutoTriggerEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoTriggerEnabled = enabled;
}

QString ProcessingWorker::statusToText(PipelineStatus status)
{
    switch (status) {
    case PipelineStatus::Idle:
        return QStringLiteral("未触发");
    case PipelineStatus::MotionDetected:
        return QStringLiteral("检测到运动");
    case PipelineStatus::Capturing:
        return QStringLiteral("动作跟踪中");
    case PipelineStatus::Completed:
        return QStringLiteral("已抓到关键帧");
    }

    return QStringLiteral("未触发");
}

void ProcessingWorker::requestManualTrigger()
{
    if (!m_buffer) {
        emit errorOccurred(QStringLiteral("抓拍链路未初始化。"));
        return;
    }

    FramePacket latest;
    if (!m_buffer->latest(latest)) {
        emit errorOccurred(QStringLiteral("当前没有可用于抓拍的画面。"));
        return;
    }

    std::optional<BurstResult> sessionResult;
    QString statusText;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const BurstUpdate update = m_burstManager.captureManualFrame(latest, QStringLiteral("manual"));
        if (update.sessionChanged) {
            sessionResult = update.result;
        }
        m_status = PipelineStatus::Completed;
        statusText = statusToText(m_status);
    }

    emit burstStarted(QStringLiteral("manual"));
    emit statusChanged(statusText);
    if (sessionResult.has_value()) {
        emit burstCompleted(*sessionResult);
    }
}

void ProcessingWorker::resetAnalysis()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_detector.reset();
        m_burstManager.reset();
        m_burstManager.setRoi(m_roi);
        m_status = PipelineStatus::Idle;
    }

    emit motionStateChanged(false, 0.0);
    emit statusChanged(statusToText(PipelineStatus::Idle));
}

void ProcessingWorker::processingLoop()
{
    qint64 lastSequence = 0;

    while (m_running.load()) {
        FramePacket packet;
        if (!m_buffer || !m_buffer->waitForFrameAfter(lastSequence, packet, 100)) {
            continue;
        }

        lastSequence = packet.sequence;

        MotionResult motion;
        QString startReason;
        QString statusText;
        std::optional<BurstResult> sessionResult;
        bool actionCompleted = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            motion = m_detector.detect(packet.frame, m_roi, packet.timestampMs);

            if (!m_burstManager.isActionActive() && m_autoTriggerEnabled && motion.shouldTrigger) {
                BurstUpdate update = m_burstManager.startAction(packet, motion, QStringLiteral("motion"));
                if (update.started) {
                    startReason = QStringLiteral("motion");
                }
            } else if (m_burstManager.isActionActive()) {
                BurstUpdate update = m_burstManager.processAction(packet, motion);
                actionCompleted = update.actionCompleted;
                if (update.sessionChanged) {
                    sessionResult = update.result;
                }
            }

            PipelineStatus nextStatus = PipelineStatus::Idle;
            if (m_burstManager.isActionActive()) {
                nextStatus = PipelineStatus::Capturing;
            } else if (sessionResult.has_value() || actionCompleted) {
                nextStatus = PipelineStatus::Completed;
            } else if (motion.motionDetected) {
                nextStatus = PipelineStatus::MotionDetected;
            }

            if (nextStatus != m_status) {
                m_status = nextStatus;
                statusText = statusToText(nextStatus);
            }
        }

        emit motionStateChanged(motion.motionDetected, motion.largestArea);

        if (!startReason.isEmpty()) {
            emit burstStarted(startReason);
        }

        if (!statusText.isEmpty()) {
            emit statusChanged(statusText);
        }

        if (sessionResult.has_value()) {
            emit burstCompleted(*sessionResult);
        }
    }
}
