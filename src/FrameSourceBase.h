#pragma once

#include "AppTypes.h"
#include "RingBuffer.h"

#include <QObject>

#include <memory>

class FrameSourceBase : public QObject
{
    Q_OBJECT

public:
    explicit FrameSourceBase(QObject* parent = nullptr);
    ~FrameSourceBase() override = default;

    void setBuffer(const std::shared_ptr<RingBuffer>& buffer);
    bool getLatestFrame(FramePacket& out) const;

    virtual InputSourceType sourceType() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isPaused() const = 0;
    virtual bool canSeek() const { return false; }
    virtual bool seekFrame(int frameIndex) { Q_UNUSED(frameIndex); return false; }
    virtual bool nextFrame() { return false; }
    virtual void setLoop(bool enabled) { Q_UNUSED(enabled); }
    virtual QString sourceName() const = 0;

signals:
    void errorOccurred(const QString& message);
    void videoStateChanged(const VideoState& state);

protected:
    void clearBuffer() const;
    void pushFrame(const cv::Mat& frame, qint64 frameNumber, qint64 timestampMs);

    std::shared_ptr<RingBuffer> m_buffer;
};
