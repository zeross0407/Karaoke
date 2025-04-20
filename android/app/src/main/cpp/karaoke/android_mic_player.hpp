#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <functional>
#include <deque>
#include <array>
#include <memory>
#include "../audio_player/audioplayer/common.hpp"

// Kích thước mặc định cho ring buffer tính bằng số mẫu
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 8192; // Giảm kích thước buffer để giảm độ trễ nhưng vẫn đủ lớn

// Lock-free SPSC (Single Producer Single Consumer) Ring Buffer
// Tối ưu cho một thread ghi (microphone) và một thread đọc (audio playback)
template <typename T, size_t Capacity>
class LockFreeRingBuffer
{
private:
    // Mảng tĩnh có kích thước cố định từ lúc biên dịch
    std::array<T, Capacity> buffer;

    // Atomic write/read indices để tránh dùng mutex
    std::atomic<size_t> writeIndex;
    std::atomic<size_t> readIndex;

    // Các hàm helper
    inline size_t mask(size_t val) const { return val & (Capacity - 1); }

public:
    LockFreeRingBuffer() : writeIndex(0), readIndex(0)
    {
        // Đảm bảo kích thước là lũy thừa của 2 để tối ưu mask operation
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    }

    // Rõ ràng chỉ ra rằng class này không thể sao chép
    LockFreeRingBuffer(const LockFreeRingBuffer &) = delete;
    LockFreeRingBuffer &operator=(const LockFreeRingBuffer &) = delete;

    // Nhưng có thể di chuyển (move) nếu cần
    LockFreeRingBuffer(LockFreeRingBuffer &&) = default;
    LockFreeRingBuffer &operator=(LockFreeRingBuffer &&) = default;

    // Kiểm tra buffer trống
    bool isEmpty() const
    {
        return readIndex.load(std::memory_order_acquire) ==
               writeIndex.load(std::memory_order_acquire);
    }

    // Kiểm tra buffer đầy
    bool isFull() const
    {
        return (writeIndex.load(std::memory_order_acquire) + 1) % Capacity ==
               readIndex.load(std::memory_order_acquire);
    }

    // Số lượng phần tử có sẵn để đọc
    size_t getAvailableData() const
    {
        size_t write = writeIndex.load(std::memory_order_acquire);
        size_t read = readIndex.load(std::memory_order_acquire);
        if (write >= read)
        {
            return write - read;
        }
        else
        {
            return Capacity + write - read;
        }
    }

    // Khoảng trống có sẵn để ghi
    size_t getAvailableSpace() const
    {
        size_t write = writeIndex.load(std::memory_order_acquire);
        size_t read = readIndex.load(std::memory_order_acquire);
        if (read > write)
        {
            return read - write - 1;
        }
        else
        {
            return Capacity - (write - read) - 1;
        }
    }

    // Ghi dữ liệu vào buffer - được gọi từ producer thread (microphone)
    size_t write(const T *data, size_t count)
    {
        if (count == 0)
            return 0;

        size_t write = writeIndex.load(std::memory_order_relaxed);
        size_t read = readIndex.load(std::memory_order_acquire);

        // Tính toán không gian có sẵn
        size_t available;
        if (read > write)
        {
            available = read - write - 1;
        }
        else
        {
            available = Capacity - (write - read) - 1;
        }

        // Giới hạn số lượng cần ghi
        size_t toWrite = std::min(count, available);
        if (toWrite == 0)
            return 0;

        // Tính vị trí bắt đầu ghi và đoạn đến cuối buffer
        size_t firstPart = std::min(toWrite, Capacity - mask(write));

        // Ghi phần đầu
        std::copy_n(data, firstPart, buffer.data() + mask(write));

        // Ghi phần quay vòng nếu cần
        if (firstPart < toWrite)
        {
            std::copy_n(data + firstPart, toWrite - firstPart, buffer.data());
        }

        // Đảm bảo dữ liệu được ghi hoàn toàn trước khi cập nhật writeIndex
        std::atomic_thread_fence(std::memory_order_release);
        writeIndex.store(mask(write + toWrite), std::memory_order_release);

        return toWrite;
    }

    // Đọc dữ liệu từ buffer - được gọi từ consumer thread (audio playback)
    size_t read(T *data, size_t count)
    {
        if (count == 0)
            return 0;

        size_t read = readIndex.load(std::memory_order_relaxed);
        size_t write = writeIndex.load(std::memory_order_acquire);

        // Tính toán lượng dữ liệu có sẵn
        size_t available;
        if (write >= read)
        {
            available = write - read;
        }
        else
        {
            available = Capacity + write - read;
        }

        // Không có dữ liệu để đọc
        if (available == 0)
        {
            std::memset(data, 0, count * sizeof(T));
            return 0;
        }

        // Giới hạn số lượng cần đọc
        size_t toRead = std::min(count, available);

        // Tính vị trí bắt đầu đọc và đoạn đến cuối buffer
        size_t firstPart = std::min(toRead, Capacity - mask(read));

        // Đọc phần đầu
        std::copy_n(buffer.data() + mask(read), firstPart, data);

        // Đọc phần quay vòng nếu cần
        if (firstPart < toRead)
        {
            std::copy_n(buffer.data(), toRead - firstPart, data + firstPart);
        }

        // Đảm bảo dữ liệu được đọc hoàn toàn trước khi cập nhật readIndex
        std::atomic_thread_fence(std::memory_order_release);
        readIndex.store(mask(read + toRead), std::memory_order_release);

        return toRead;
    }

    // Xóa toàn bộ buffer
    void clear()
    {
        size_t read = readIndex.load(std::memory_order_relaxed);
        writeIndex.store(read, std::memory_order_release);
    }

    // Trả về tổng dung lượng của buffer
    size_t getCapacity() const
    {
        return Capacity;
    }
};

// Player âm thanh đơn giản cho microphone
class MicrophonePlayer : public oboe::AudioStreamCallback
{
public:
    using PlaybackCallback = std::function<void(const float *buffer, size_t frameCount)>;

private:
    std::shared_ptr<oboe::AudioStream> outputStream;
    // Sử dụng lock-free ring buffer với kích thước tĩnh
    LockFreeRingBuffer<float, DEFAULT_RING_BUFFER_SIZE> ringBuffer;
    std::atomic<bool> isPlaying;
    int sampleRate;
    int bufferSize;
    float volume; // Thêm biến volume để điều chỉnh âm lượng mic
    PlaybackCallback playbackCallback;

    // Oboe callback implementation
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *stream,
        void *audioData,
        int32_t numFrames) override;

public:
    MicrophonePlayer();
    ~MicrophonePlayer();

    bool initialize();
    void shutdown();

    bool start();
    void stop();

    // Thêm dữ liệu từ microphone vào buffer
    size_t addAudioData(const float *data, size_t numSamples);

    // Kiểm tra trạng thái
    bool isCurrentlyPlaying() const;

    // Cài đặt
    void setSampleRate(int rate);
    int getSampleRate() const;
    int getChannels() const; // Luôn trả về 1 (mono)

    // Điều chỉnh âm lượng (0.0 - 5.0, mặc định 1.0)
    void setVolume(float newVolume);
    float getVolume() const;

    // Callback hiện trạng phát
    void setPlaybackCallback(PlaybackCallback callback);

    // Xóa dữ liệu trong buffer
    void clearBuffer();
};

// Lớp KaraokePlayer để kết hợp mic với player
class KaraokePlayer
{
private:
    std::unique_ptr<MicrophonePlayer> player;

public:
    KaraokePlayer();
    ~KaraokePlayer();

    bool initialize();
    bool start();
    void stop();

    // Thêm dữ liệu từ mic vào để phát
    size_t addAudioFromMic(const float *data, size_t numSamples);

    // Xóa buffer
    void clearBuffer();

    // Điều khiển âm lượng
    void setVolume(float volume);
    float getVolume() const;
};
