#include "VideoFileFrameSource.h"

#include <QDebug>
#include <QFileInfo>

#include <algorithm>
#include <chrono>
#include <cmath>

VideoFileFrameSource::VideoFileFrameSource(QObject* parent)
    : FrameSourceBase(parent)
{
}

VideoFileFrameSource::~VideoFileFrameSource()
{
    stop();
}

void VideoFileFrameSource::setFilePath(const QString& path)
{
    m_filePath = path;
}

QString VideoFileFrameSource::filePath() const
{
    return m_filePath;
}

bool VideoFileFrameSource::loop() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_loop;
}

InputSourceType VideoFileFrameSource::sourceType() const
{
    return InputSourceType::VideoFile;
}

bool VideoFileFrameSource::openVideo()
{
    if (m_filePath.isEmpty() || !QFileInfo::exists(m_filePath)) {
        emit errorOccurred(QStringLiteral("视频文件不存在，请先选择有效视频。"));
        return false;
    }

    if (!m_capture.open(m_filePath.toStdString(), cv::CAP_ANY)) {
        emit errorOccurred(QStringLiteral("视频文件打开失败：%1").arg(m_filePath));
        return false;
    }

    m_fps = m_capture.get(cv::CAP_PROP_FPS);
    if (!std::isfinite(m_fps) || m_fps <= 1.0) {
        m_fps = 30.0;
    }

    m_totalFrames = static_cast<qint64>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
    m_totalMs = (m_totalFrames > 0)
        ? static_cast<qint64>((static_cast<double>(m_totalFrames) / m_fps) * 1000.0)
        : 0;
    m_currentFrame = -1;
    return true;
}

void VideoFileFrameSource::closeVideo()
{
    if (m_capture.isOpened()) {
        m_capture.release();
    }
}

bool VideoFileFrameSource::start()
{
    if (m_running.load()) {
        return true;
    }

    clearBuffer();

    if (!openVideo()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_pendingSeekFrame = -1;
        m_stepOnce = false;
    }

    m_stopRequested = false;
    m_playing = true;
    m_running = true;
    m_thread = std::thread(&VideoFileFrameSource::playbackLoop, this);
    qInfo() << "Video source started:" << m_filePath;
    return true;
}

void VideoFileFrameSource::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_stopRequested = true;
    m_controlCv.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    closeVideo();
    m_running = false;
    m_playing = false;
    qInfo() << "Video source stopped";

    emitState(false);
}

void VideoFileFrameSource::pause()
{
    m_playing = false;
    emitState(false);
}

void VideoFileFrameSource::resume()
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_totalFrames > 0 && m_currentFrame >= (m_totalFrames - 1)) {
            m_pendingSeekFrame = 0;
        }
    }

    m_playing = true;
    m_controlCv.notify_all();
    emitState(true);
}

bool VideoFileFrameSource::isRunning() const
{
    return m_running.load();
}

bool VideoFileFrameSource::isPaused() const
{
    return !m_playing.load();
}

bool VideoFileFrameSource::canSeek() const
{
    return true;
}

bool VideoFileFrameSource::seekFrame(int frameIndex)
{
    if (!m_running.load()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_pendingSeekFrame = std::max(0, frameIndex);
    }
    m_controlCv.notify_all();
    return true;
}

bool VideoFileFrameSource::nextFrame()
{
    if (!m_running.load()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_playing = false;
        m_stepOnce = true;
    }
    m_controlCv.notify_all();
    return true;
}

void VideoFileFrameSource::setLoop(bool enabled)
{
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_loop = enabled;
    }
    emitState(m_playing.load());
}

QString VideoFileFrameSource::sourceName() const
{
    QFileInfo info(m_filePath);
    return info.fileName().isEmpty() ? QStringLiteral("视频文件") : info.fileName();
}

void VideoFileFrameSource::emitState(bool playing)
{
    VideoState state;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        state.currentFrame = std::max<qint64>(m_currentFrame, 0);
        state.totalFrames = std::max<qint64>(m_totalFrames, 0);
        state.currentMs = (m_currentFrame >= 0 && m_fps > 0.0)
            ? static_cast<qint64>((static_cast<double>(m_currentFrame) / m_fps) * 1000.0)
            : 0;
        state.totalMs = std::max<qint64>(m_totalMs, 0);
        state.fps = m_fps;
        state.playing = playing;
        state.loop = m_loop;
    }

    emit videoStateChanged(state);
}

bool VideoFileFrameSource::readAndPushFrame(bool* reachedEnd)
{
    if (reachedEnd) {
        *reachedEnd = false;
    }

    cv::Mat frame;
    if (!m_capture.read(frame) || frame.empty()) {
        if (reachedEnd) {
            *reachedEnd = true;
        }
        return false;
    }

    const qint64 frameNumber = static_cast<qint64>(m_capture.get(cv::CAP_PROP_POS_FRAMES)) - 1;
    const double posMs = m_capture.get(cv::CAP_PROP_POS_MSEC);
    const qint64 currentMs = (std::isfinite(posMs) && posMs >= 0.0)
        ? static_cast<qint64>(posMs)
        : ((frameNumber >= 0 && m_fps > 0.0)
            ? static_cast<qint64>((static_cast<double>(frameNumber) / m_fps) * 1000.0)
            : 0);

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_currentFrame = std::max<qint64>(0, frameNumber);
    }

    pushFrame(frame, frameNumber, currentMs);
    emitState(m_playing.load());
    return true;
}

void VideoFileFrameSource::playbackLoop()
{
    using namespace std::chrono_literals;

    while (!m_stopRequested.load()) {
        int pendingSeek = -1;
        bool playContinuously = false;
        bool stepOnce = false;
        bool loopPlayback = false;

        {
            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_controlCv.wait(lock, [&]() {
                return m_stopRequested.load() || m_playing.load() || m_pendingSeekFrame >= 0 || m_stepOnce;
            });

            if (m_stopRequested.load()) {
                break;
            }

            pendingSeek = m_pendingSeekFrame;
            if (m_pendingSeekFrame >= 0) {
                m_pendingSeekFrame = -1;
            }

            stepOnce = m_stepOnce;
            if (m_stepOnce) {
                m_stepOnce = false;
            }

            playContinuously = m_playing.load();
            loopPlayback = m_loop;
        }

        if (pendingSeek >= 0) {
            if (!m_capture.set(cv::CAP_PROP_POS_FRAMES, pendingSeek)) {
                emit errorOccurred(QStringLiteral("视频定位失败：帧 %1").arg(pendingSeek));
                continue;
            }

            bool reachedEnd = false;
            if (!readAndPushFrame(&reachedEnd)) {
                emit errorOccurred(QStringLiteral("视频定位后读取帧失败。"));
                continue;
            }

            if (!playContinuously) {
                continue;
            }
        } else {
            if (!playContinuously && !stepOnce) {
                continue;
            }

            bool reachedEnd = false;
            if (!readAndPushFrame(&reachedEnd)) {
                if (reachedEnd) {
                    if (loopPlayback) {
                        if (!m_capture.set(cv::CAP_PROP_POS_FRAMES, 0)) {
                            emit errorOccurred(QStringLiteral("视频循环回放失败。"));
                            continue;
                        }
                        {
                            std::lock_guard<std::mutex> lock(m_stateMutex);
                            m_currentFrame = -1;
                        }
                        continue;
                    }

                    m_playing = false;
                    emitState(false);
                    continue;
                }

                emit errorOccurred(QStringLiteral("视频解码失败。"));
                std::this_thread::sleep_for(30ms);
                continue;
            }
        }

        if (!m_playing.load()) {
            continue;
        }

        const int frameDelayMs = std::clamp(static_cast<int>(1000.0 / m_fps), 5, 200);
        std::this_thread::sleep_for(std::chrono::milliseconds(frameDelayMs));
    }
}
