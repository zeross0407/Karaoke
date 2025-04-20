#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <mutex>
#include <functional>
#include <string>
#include <memory>
#include "audio_player/audioplayer/common.hpp"
#include "android_mic_player.hpp"

// Kích thước buffer cho recorder
constexpr size_t RECORDER_BUFFER_SIZE = 1 << 17; // 131072 samples (~2.7s ở 48kHz mono)

// Forward declarations
class AudioPlayer;

class MicrophoneRecorder : public oboe::AudioStreamCallback {
public:
    using RecordingCallback = std::function<void(const float *buffer, size_t frameCount)>;

private:
    std::shared_ptr<oboe::AudioStream> inputStream;
    bool isRecording;
    int sampleRate;
    int bufferSize;
    // Sử dụng lock-free ring buffer thay vì vector và mutex
    LockFreeRingBuffer<float, RECORDER_BUFFER_SIZE> recordedBuffer;
    RecordingCallback recordingCallback;

    // Oboe callback implementation
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) override;

public:
    MicrophoneRecorder();
    ~MicrophoneRecorder();

    bool initialize();
    void shutdown();

    bool startRecording();
    void stopRecording();
    void setRecordingCallback(RecordingCallback callback);

    std::vector<float> getRecordedData() const;
    bool saveToFile(const std::string &filePath);

    bool isCurrentlyRecording() const;

    void setSampleRate(int rate);
    int getSampleRate() const;
    int getChannels() const; // Luôn trả về 1 (mono)
};

class Karaoke {
private:
    std::unique_ptr<MicrophoneRecorder> recorder;
    std::unique_ptr<KaraokePlayer> player;
    bool livePlayback; // Trạng thái phát trực tiếp từ microphone

public:
    Karaoke();
    ~Karaoke();

    bool initialize();

    // Chế độ ghi âm thông thường (không phát lại)
    bool startRecording();
    void stopRecording();
    bool saveRecordingToFile(const std::string &filePath);

    // Chế độ phát trực tiếp từ microphone
    bool startLivePlayback();
    void stopLivePlayback();
    bool isLivePlaybackActive() const;

    // Hiển thị lời bài hát với highlight theo thời gian
    bool loadLyrics(const std::string &jsonFilePath);
    bool startLyricDisplay(const std::string &audioFilePath);
    void stopLyricDisplay();

    // Điều chỉnh âm lượng microphone
    void setMicVolume(float volume);
    float getMicVolume() const;
};
