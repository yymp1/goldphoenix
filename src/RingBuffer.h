#pragma once

#include "AppTypes.h"

#include <condition_variable>
#include <deque>
#include <mutex>

class RingBuffer
{
public:
    explicit RingBuffer(int capacity = 8);

    void setCapacity(int capacity);
    int capacity() const;
    void clear();
    void push(const FramePacket& packet);
    bool latest(FramePacket& out) const;
    bool waitForFrameAfter(qint64 lastSequence, FramePacket& out, int timeoutMs);

private:
    FramePacket clonePacket(const FramePacket& packet) const;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<FramePacket> m_frames;
    int m_capacity = 8;
    qint64 m_nextSequence = 1;
};
