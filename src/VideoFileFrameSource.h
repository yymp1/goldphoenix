#pragma once

#include "FrameSourceBase.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <opencv2/videoio.hpp>

class VideoFileFrameSource : public FrameSourceBase
{
    Q_OBJECT

public:
    explicit VideoFileFrameSource(QObject* parent = nullptr);
    ~VideoFileFrameSource() override;

    void setFilePath(const QString& path);
    QString filePath() const;
    bool loop() const;

    InputSourceType sourceType() const override;
    bool start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool isRunning() const override;
    bool isPaused() const override;
    bool canSeek() const override;
    bool seekFrame(int frameIndex) override;
    bool nextFrame() override;
    void setLoop(bool enabled) override;
    QString sourceName() const override;

private:
    bool openVideo();
    void closeVideo();
    void emitState(bool playing);
    bool readAndPushFrame(bool* reachedEnd);
    void playbackLoop();

    std::atomic_bool m_running { false };
    std::atomic_bool m_playing { false };
    std::atomic_bool m_stopRequested { false };
    std::thread m_thread;
    mutable std::mutex m_stateMutex;
    std::condition_variable m_controlCv;
    cv::VideoCapture m_capture;
    QString m_filePath;
    bool m_loop = false;
    int m_pendingSeekFrame = -1;
    bool m_stepOnce = false;
    double m_fps = 30.0;
    qint64 m_totalFrames = 0;
    qint64 m_totalMs = 0;
    qint64 m_currentFrame = -1;
};
