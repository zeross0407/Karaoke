#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <mutex>
#include <functional>
#include <string>
#include <memory>
#include "../audioplayer/common.hpp"
#include "ios_mic_player.hpp"

// Forward declarations
class AudioPlayer;

class MicrophoneRecorder
{
public:
    using RecordingCallback = std::function<void(const float *buffer, size_t frameCount)>;

private:
    AudioUnit audioUnit;
    bool isRecording;
    int sampleRate;
    int channels;
    int bufferSize;
    mutable std::mutex dataMutex;
    std::vector<float> recordedData;
    RecordingCallback recordingCallback;

    static OSStatus inputCallback(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData);

    OSStatus handleInputCallback(AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData);

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

    void setChannels(int numChannels);
    void setSampleRate(int rate);

    int getChannels() const;
    int getSampleRate() const;
};

class Karaoke
{
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

    // Điều khiển echo cancellation
    void enableEchoCancellation(bool enable);
    bool isEchoCancellationEnabled() const;
    void setEchoCancellationDelay(int delayMs);
    void setEchoCancellationFactor(float factor);
};