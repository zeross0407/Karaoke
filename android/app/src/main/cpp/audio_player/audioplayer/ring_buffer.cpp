#include "ring_buffer.hpp"
#include <cstring>
#include <algorithm>

RingBuffer::RingBuffer(size_t bufferSize)
    : capacity(bufferSize)
    , readIndex(0)
    , writeIndex(0) 
{
    buffer = new float[capacity];
}

RingBuffer::~RingBuffer() {
    delete[] buffer;
}

// Ghi dữ liệu vào buffer
size_t RingBuffer::write(const float* data, size_t frames) {
    const size_t available = availableForWrite();
    frames = std::min(frames, available);
    
    if (frames == 0) return 0;

    const size_t write = writeIndex.load();
    const size_t firstPart = std::min(frames, capacity - write);
    
    // Ghi phần đầu tiên
    memcpy(buffer + write, data, firstPart * sizeof(float));
    
    // Xử lý trường hợp wrap-around
    if (firstPart < frames) {
        const size_t secondPart = frames - firstPart;
        memcpy(buffer, data + firstPart, secondPart * sizeof(float));
    }
    
    writeIndex.store((write + frames) % capacity);
    return frames;
}

// Đọc dữ liệu từ buffer
size_t RingBuffer::read(float* data, size_t frames) {
    const size_t available = availableForRead();
    frames = std::min(frames, available);
    
    if (frames == 0) return 0;

    const size_t read = readIndex.load();
    const size_t firstPart = std::min(frames, capacity - read);
    
    // Đọc phần đầu tiên
    memcpy(data, buffer + read, firstPart * sizeof(float));
    
    // Xử lý trường hợp wrap-around
    if (firstPart < frames) {
        const size_t secondPart = frames - firstPart;
        memcpy(data + firstPart, buffer, secondPart * sizeof(float));
    }
    
    readIndex.store((read + frames) % capacity);
    return frames;
}

void RingBuffer::clear() {
    readIndex.store(0);
    writeIndex.store(0);
}

size_t RingBuffer::availableForRead() const {
    const size_t write = writeIndex.load();
    const size_t read = readIndex.load();
    
    if (write >= read) {
        return write - read;
    }
    return capacity - read + write;
}

size_t RingBuffer::availableForWrite() const {
    return capacity - availableForRead() - 1;
}