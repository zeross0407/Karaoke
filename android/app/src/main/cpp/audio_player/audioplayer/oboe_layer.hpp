#pragma once

#include <vector>
#include <array>
#include <memory>
#include <oboe/Oboe.h>
#include "audio_layer.hpp"
#include "common.hpp"

class AudioSession;

class OboeLayer : public AudioLayer, public oboe::AudioStreamCallback
{
private:
    static constexpr int MAX_BUSES = 4; // Số lượng bus âm thanh tối đa để mix

    struct BusInfo
    {
        bool inUse = false;
        bool isMuted = false;
        float volume = 1.0f;
        uint8_t channels = 1;        // Số kênh của bus
        AudioCallback audioCallback; // Callback để đọc dữ liệu âm thanh
    };

    std::shared_ptr<oboe::AudioStream> audioStream;
    std::array<BusInfo, MAX_BUSES> buses;

    int sampleRate;
    int channels;
    int bufferSize;
    bool playing;

    // Implement Oboe callback
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) override;

public:
    OboeLayer();
    ~OboeLayer() override;

    bool initialize() override;
    void shutdown() override;

    int acquireInputBus() override;
    void releaseInputBus(int busId) override;
    void setInputVolume(int busId, float volume) override;
    void muteInputBus(int busId, bool mute) override;

    void setAudioCallback(int busId, AudioCallback callback) override;

    void start() override;
    void stop() override;

    // Implement Oboe callbacks
    void onErrorBeforeClose(oboe::AudioStream *audioStream, oboe::Result error) override {}
    void onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error) override {}

private:
    bool isValidBus(int busId) const
    {
        return busId >= 0 && busId < MAX_BUSES;
    }

    uint32_t getCurrentTimeMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};
