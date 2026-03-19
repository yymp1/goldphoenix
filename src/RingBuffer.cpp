#include "RingBuffer.h"

#include <algorithm>
#include <chrono>

RingBuffer::RingBuffer(int capacity)
    : m_capacity(std::max(1, capacity))
{
}

void RingBuffer::setCapacity(int capacity)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capacity = std::max(1, capacity);
    while (static_cast<int>(m_frames.size()) > m_capacity) {
        m_frames.pop_front();
    }
}

int RingBuffer::capacity() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_capacity;
}

void RingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.clear();
}

FramePacket RingBuffer::clonePacket(const FramePacket& packet) const
{
    FramePacket copy = packet;
    if (!packet.frame.empty()) {
        copy.frame = packet.frame.clone();
    }
    return copy;
}

void RingBuffer::push(const FramePacket& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    FramePacket copy = clonePacket(packet);
    copy.sequence = m_nextSequence++;
    m_frames.push_back(std::move(copy));

    while (static_cast<int>(m_frames.size()) > m_capacity) {
        m_frames.pop_front();
    }

    m_condition.notify_all();
}

bool RingBuffer::latest(FramePacket& out) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frames.empty()) {
        return false;
    }

    out = clonePacket(m_frames.back());
    return true;
}

bool RingBuffer::waitForFrameAfter(qint64 lastSequence, FramePacket& out, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto hasNewFrame = [&]() {
        return !m_frames.empty() && m_frames.back().sequence > lastSequence;
    };

    if (!m_condition.wait_for(lock, std::chrono::milliseconds(timeoutMs), hasNewFrame)) {
        return false;
    }

    out = clonePacket(m_frames.back());
    return true;
}
