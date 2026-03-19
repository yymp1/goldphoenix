#include "FrameSourceBase.h"

FrameSourceBase::FrameSourceBase(QObject* parent)
    : QObject(parent)
{
}

void FrameSourceBase::setBuffer(const std::shared_ptr<RingBuffer>& buffer)
{
    m_buffer = buffer;
}

bool FrameSourceBase::getLatestFrame(FramePacket& out) const
{
    return m_buffer ? m_buffer->latest(out) : false;
}

void FrameSourceBase::clearBuffer() const
{
    if (m_buffer) {
        m_buffer->clear();
    }
}

void FrameSourceBase::pushFrame(const cv::Mat& frame, qint64 frameNumber, qint64 timestampMs)
{
    if (!m_buffer || frame.empty()) {
        return;
    }

    FramePacket packet;
    packet.frame = frame;
    packet.frameNumber = frameNumber;
    packet.timestampMs = timestampMs;
    m_buffer->push(packet);
}
