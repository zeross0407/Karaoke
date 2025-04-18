#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <functional>
#include <deque>
#include "../audio_player/audioplayer/common.hpp"

// Kích thước mặc định cho ring buffer tính bằng số mẫu
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 48000 * 0.5; // 0.5 giây ở mức 48kHz

// RingBuffer đơn giản hóa cho việc truyền dữ liệu từ microphone đến audio output
class SimpleRingBuffer
{
private:
    std::vector<float> buffer;
    size_t writePos;
    size_t readPos;
    size_t capacity;
    std::atomic<size_t> dataAvailable;
    mutable std::mutex mutex;
    std::condition_variable dataCondition;

public:
    SimpleRingBuffer(size_t size = DEFAULT_RING_BUFFER_SIZE);
    ~SimpleRingBuffer() = default;

    // Ghi dữ liệu vào buffer
    size_t write(const float *data, size_t numSamples);

    // Đọc dữ liệu từ buffer
    size_t read(float *data, size_t numSamples);

    // Xóa toàn bộ dữ liệu trong buffer
    void clear();

    // Truy vấn thông tin
    size_t getAvailableData() const;
    size_t getAvailableSpace() const;
    size_t getCapacity() const;
    bool isEmpty() const;
};

// Player âm thanh đơn giản cho microphone
class MicrophonePlayer : public oboe::AudioStreamCallback
{
public:
    using PlaybackCallback = std::function<void(const float *buffer, size_t frameCount)>;

private:
    std::shared_ptr<oboe::AudioStream> outputStream;
    SimpleRingBuffer ringBuffer;
    std::atomic<bool> isPlaying;
    int sampleRate;
    int channels;
    int bufferSize;
    float volume; // Thêm biến volume để điều chỉnh âm lượng mic
    PlaybackCallback playbackCallback;

    // Oboe callback implementation
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *stream,
        void *audioData,
        int32_t numFrames) override;

public:
    MicrophonePlayer(size_t bufferSize = DEFAULT_RING_BUFFER_SIZE);
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
    void setChannels(int numChannels);
    int getSampleRate() const;
    int getChannels() const;

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
