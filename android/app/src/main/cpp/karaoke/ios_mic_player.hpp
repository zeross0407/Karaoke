#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <functional>
#include <deque>
#include "../audioplayer/common.hpp"
#include "pitch_processor.hpp"
#include "reverb_processor.hpp"

// Kích thước mặc định cho ring buffer tính bằng số mẫu
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 48000 * 2; // 2 giây ở mức 48kHz

// RingBuffer đơn giản hóa cho việc truyền dữ liệu từ microphone đến audio output
class SimpleRingBuffer {
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
    size_t write(const float* data, size_t numSamples);

    // Đọc dữ liệu từ buffer
    size_t read(float* data, size_t numSamples);

    // Xóa toàn bộ dữ liệu trong buffer
    void clear();

    // Truy vấn thông tin
    size_t getAvailableData() const;
    size_t getAvailableSpace() const;
    size_t getCapacity() const;
    bool isEmpty() const;
};

// Echo Canceller để giảm hiện tượng tiếng vọng lại
class EchoCanceller {
private:
    // Độ trễ giữa thời điểm phát âm thanh và thời điểm nó quay trở lại microphone (ms)
    int delayMs;
    
    // Hệ số giảm trừ echo (0.0 - 1.0)
    float cancellationFactor;
    
    // Buffer lưu lịch sử âm thanh đã phát (để trừ khỏi tín hiệu microphone)
    std::deque<float> playbackHistory;
    
    // Kích thước tối đa của lịch sử (samples)
    size_t maxHistorySize;
    
    // Mẫu đầu ra gần nhất
    float lastOutputSample;
    
    // Mutex bảo vệ dữ liệu
    mutable std::mutex mutex;
    
    // Sample rate
    int sampleRate;
    
    // Số kênh
    int channels;

public:
    EchoCanceller(int sampleRate = 48000, int channels = 1);
    ~EchoCanceller() = default;
    
    // Thêm dữ liệu âm thanh đã phát vào lịch sử
    void addPlaybackData(const float* data, size_t numSamples);
    
    // Xử lý dữ liệu từ mic để loại bỏ echo
    void processMicData(float* data, size_t numSamples);
    
    // Cài đặt các tham số
    void setDelayMs(int delay);
    void setCancellationFactor(float factor);
    
    // Xóa lịch sử
    void clear();
    
    // Kích hoạt/vô hiệu hóa echo cancellation
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
private:
    bool enabled;
};

// Player âm thanh đơn giản cho microphone
class MicrophonePlayer {
public:
    using PlaybackCallback = std::function<void(const float* buffer, size_t frameCount)>;

private:
    AudioUnit audioUnit;
    SimpleRingBuffer ringBuffer;
    std::atomic<bool> isPlaying;
    int sampleRate;
    int channels;
    int bufferSize;
    float volume;  // Thêm biến volume để điều chỉnh âm lượng mic
    PlaybackCallback playbackCallback;
    std::unique_ptr<EchoCanceller> echoCanceller;
    std::unique_ptr<PitchProcessor> pitchProcessor;
    std::unique_ptr<ReverbProcessor> reverbProcessor;

    static OSStatus outputCallback(void* inRefCon,
                                AudioUnitRenderActionFlags* ioActionFlags,
                                const AudioTimeStamp* inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList* ioData);

    OSStatus handleOutputCallback(AudioUnitRenderActionFlags* ioActionFlags,
                              const AudioTimeStamp* inTimeStamp,
                              UInt32 inBusNumber,
                              UInt32 inNumberFrames,
                              AudioBufferList* ioData);

public:
    MicrophonePlayer(size_t bufferSize = DEFAULT_RING_BUFFER_SIZE);
    ~MicrophonePlayer();

    bool initialize();
    void shutdown();

    bool start();
    void stop();

    // Thêm dữ liệu từ microphone vào buffer, với tùy chọn xử lý echo cancellation
    size_t addAudioData(const float* data, size_t numSamples, bool applyEchoCancellation = true);

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
    
    // Điều khiển echo canceller
    void enableEchoCancellation(bool enable);
    bool isEchoCancellationEnabled() const;
    void setEchoCancellationDelay(int delayMs);
    void setEchoCancellationFactor(float factor);

    // Reverb control methods
    void setReverbRoomSize(float size);
    void setReverbDamping(float damping);
    void setReverbWetMix(float wet);
    void setReverbDryMix(float dry);
    void enableReverb(bool enable);
};

// Lớp KaraokePlayer để kết hợp mic với player
class KaraokePlayer {
private:
    std::unique_ptr<MicrophonePlayer> player;
    
public:
    KaraokePlayer();
    ~KaraokePlayer();
    
    bool initialize();
    bool start();
    void stop();
    
    // Thêm dữ liệu từ mic vào để phát
    size_t addAudioFromMic(const float* data, size_t numSamples);
    
    // Xóa buffer
    void clearBuffer();
    
    // Điều khiển âm lượng
    void setVolume(float volume);
    float getVolume() const;
    
    // Điều khiển echo canceller
    void enableEchoCancellation(bool enable);
    bool isEchoCancellationEnabled() const;
    void setEchoCancellationDelay(int delayMs);
    void setEchoCancellationFactor(float factor);
}; 