#pragma once

#include "FrameSourceBase.h"

#include <QPair>
#include <QVector>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <opencv2/videoio.hpp>

class CameraFrameSource : public FrameSourceBase
{
    Q_OBJECT

public:
    explicit CameraFrameSource(QObject* parent = nullptr);
    ~CameraFrameSource() override;

    void setCameraIndex(int index);
    int cameraIndex() const;

    static QVector<QPair<int, QString>> enumerateCameras(int maxIndex = 5);

    InputSourceType sourceType() const override;
    bool start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool isRunning() const override;
    bool isPaused() const override;
    QString sourceName() const override;

private:
    void captureLoop();
    static bool openCapture(cv::VideoCapture& capture, int index);

    std::atomic_bool m_running { false };
    std::atomic_bool m_paused { false };
    std::atomic_bool m_stopRequested { false };
    std::thread m_thread;
    std::mutex m_controlMutex;
    std::condition_variable m_controlCv;
    int m_cameraIndex = 0;
    cv::VideoCapture m_capture;
    qint64 m_frameCounter = 0;
};
