#pragma once

#include "BurstCaptureManager.h"
#include "MotionDetector.h"
#include "RingBuffer.h"

#include <QObject>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

class ProcessingWorker : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingWorker(std::shared_ptr<RingBuffer> buffer, QObject* parent = nullptr);
    ~ProcessingWorker() override;

    void start();
    void stop();
    void setRoi(const QRect& roi);
    void setMotionParameters(const MotionParameters& parameters);
    void setBurstParameters(const BurstParameters& parameters);
    void setAutoTriggerEnabled(bool enabled);
    void requestManualTrigger();
    void resetAnalysis();

signals:
    void motionStateChanged(bool motionDetected, double largestArea);
    void statusChanged(const QString& status);
    void burstStarted(const QString& reason);
    void burstFrameCaptured(int index, const QImage& image);
    void burstCompleted(const BurstResult& result);
    void errorOccurred(const QString& message);

private:
    void processingLoop();
    static QString statusToText(PipelineStatus status);

    std::shared_ptr<RingBuffer> m_buffer;
    std::thread m_thread;
    std::atomic_bool m_running { false };
    std::mutex m_mutex;
    MotionDetector m_detector;
    BurstCaptureManager m_burstManager;
    QRect m_roi;
    bool m_autoTriggerEnabled = true;
    PipelineStatus m_status = PipelineStatus::Idle;
};
