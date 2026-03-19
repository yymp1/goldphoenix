#pragma once

#include "AppTypes.h"
#include "MotionDetector.h"

#include <QDateTime>

struct BurstUpdate
{
    bool started = false;
    bool actionCompleted = false;
    bool sessionChanged = false;
    QVector<qint64> capturedFrameNumbers;
    BurstResult result;
};

class BurstCaptureManager
{
public:
    void setParameters(const BurstParameters& parameters);
    void setRoi(const QRect& roi);
    void reset();

    bool isActionActive() const;
    int capturedCount() const;
    const QVector<cv::Mat>& images() const;
    QString captureId() const;
    QDateTime timestamp() const;
    QString reason() const;

    BurstUpdate captureManualFrame(const FramePacket& packet, const QString& reason);
    BurstUpdate startAction(const FramePacket& packet, const MotionResult& motion, const QString& reason);
    BurstUpdate processAction(const FramePacket& packet, const MotionResult& motion);

private:
    struct ActionCandidate
    {
        FramePacket packet;
        QRect motionBox;
        double motionArea = 0.0;
        int actionFrameIndex = 0;
        double score = 0.0;
    };

    void resetCurrentAction();
    void ensureSession(const QString& reason);
    FramePacket clonePacket(const FramePacket& packet) const;
    void considerCandidate(const FramePacket& packet, const MotionResult& motion);
    double scoreCandidate(const ActionCandidate& candidate) const;
    BurstResult buildResult() const;
    void appendImage(const cv::Mat& frame, qint64 frameNumber);
    BurstUpdate finalizeAction();

    BurstParameters m_parameters;
    QRect m_roi;
    bool m_actionActive = false;
    int m_actionFrameCounter = 0;
    int m_silentFrameCounter = 0;
    FramePacket m_actionStartPacket;
    QVector<ActionCandidate> m_candidates;
    QVector<cv::Mat> m_images;
    QVector<qint64> m_frameNumbers;
    QString m_captureId;
    QDateTime m_timestamp;
    QString m_reason;
};
