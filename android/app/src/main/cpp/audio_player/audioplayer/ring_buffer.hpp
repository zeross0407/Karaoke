#pragma once

#include <atomic>
#include <cstddef>
#include "common.hpp"

class RingBuffer {
private:
    float* buffer;        // Buffer cho một kênh duy nhất
    const size_t capacity;
    std::atomic<size_t> readIndex;
    std::atomic<size_t> writeIndex;

public:
    explicit RingBuffer(size_t bufferSize);
    ~RingBuffer();

    // Prevent copying
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    size_t write(const float* data, size_t frames);
    size_t read(float* data, size_t frames);
    void clear();
    size_t availableForRead() const;
    size_t availableForWrite() const;
}; 