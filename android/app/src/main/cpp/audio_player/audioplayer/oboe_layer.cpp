#include "oboe_layer.hpp"
#include <algorithm>
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "OboeLayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

OboeLayer::OboeLayer()
    : sampleRate(0), channels(0), bufferSize(0), playing(false)
{
    LOGI("Creating OboeLayer instance");
}

OboeLayer::~OboeLayer()
{
    if (playing)
    {
        stop();
    }
    shutdown();
}

bool OboeLayer::initialize()
{
    LOGI("Initializing OboeLayer");

    this->sampleRate = 48000;
    this->channels = 1;

    // Cấu hình Oboe stream với các thiết lập tối ưu hóa triệt để
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency) // Sử dụng LowLatency
        ->setSharingMode(oboe::SharingMode::Exclusive) // Thử dùng Exclusive trước
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(channels)
        ->setSampleRate(sampleRate)
        ->setCallback(this)
        // Tăng buffer size để tránh underrun
        ->setBufferCapacityInFrames(16384) // Tăng từ 8192 lên 16384
        // Thêm các thiết lập mới
        ->setDataCallback(this)
        ->setErrorCallback(this)
        // Sử dụng AAudio nếu có thể
        ->setAudioApi(oboe::AudioApi::AAudio)
        // Thêm các thiết lập mới cho MediaTek
        ->setUsage(oboe::Usage::Media)
        ->setContentType(oboe::ContentType::Music)
        ->setSessionId(oboe::SessionId::None)
        ->setFramesPerCallback(512) // Tăng từ 256 lên 512
        // Thêm các thiết lập mới
        ->setInputPreset(oboe::InputPreset::VoicePerformance)
        ->setDeviceId(oboe::kUnspecified);

    // Tạo audio stream
    oboe::Result result = builder.openStream(audioStream);
    if (result != oboe::Result::OK)
    {
        LOGE("Failed to create audio stream: %s", oboe::convertToText(result));

        // Thử lại với OpenSL ES nếu AAudio không hoạt động
        builder.setAudioApi(oboe::AudioApi::OpenSLES);
        result = builder.openStream(audioStream);

        if (result != oboe::Result::OK)
        {
            // Thử lại với Shared mode nếu Exclusive không hoạt động
            builder.setSharingMode(oboe::SharingMode::Shared);
            result = builder.openStream(audioStream);

            if (result != oboe::Result::OK)
            {
                LOGE("Failed to create audio stream with shared mode: %s", oboe::convertToText(result));
                return false;
            }
        }
    }

    // Lấy kích thước buffer thực tế
    bufferSize = audioStream->getBufferSizeInFrames();
    LOGI("Audio stream created successfully - bufferSize=%d, framesPerBurst=%d",
         bufferSize, audioStream->getFramesPerBurst());

    // Điều chỉnh buffer size để tránh underrun
    int desiredBufferSize = audioStream->getFramesPerBurst() * 32; // Tăng từ 16 lên 32
    if (bufferSize < desiredBufferSize)
    {
        result = audioStream->setBufferSizeInFrames(desiredBufferSize);
        if (result == oboe::Result::OK)
        {
            bufferSize = audioStream->getBufferSizeInFrames();
            LOGI("Adjusted buffer size to %d frames", bufferSize);
        }
    }

    // Khởi tạo các bus
    for (int i = 0; i < MAX_BUSES; i++)
    {
        buses[i] = BusInfo();
        buses[i].volume = 1.0f;
        buses[i].isMuted = false;
    }

    return true;
}

void OboeLayer::shutdown()
{
    LOGI("Shutting down OboeLayer");

    if (audioStream)
    {
        audioStream->stop();
        audioStream->close();
        audioStream.reset();
    }

    // Reset tất cả các bus
    for (auto &bus : buses)
    {
        bus = BusInfo();
    }

    LOGI("OboeLayer shutdown completed");
}

int OboeLayer::acquireInputBus()
{
    for (int i = 0; i < MAX_BUSES; i++)
    {
        if (!buses[i].inUse)
        {
            buses[i].inUse = true;
            buses[i].isMuted = false;
            buses[i].volume = 1.0f;
            buses[i].channels = 1;
            LOGI("Acquired bus %d with %d channels", i, 1);
            return i;
        }
    }
    LOGE("Failed to acquire bus - all buses in use");
    return -1;
}

void OboeLayer::releaseInputBus(int busId)
{
    if (isValidBus(busId))
    {
        LOGI("Releasing bus %d", busId);
        buses[busId] = BusInfo(); // Reset to default state
    }
}

void OboeLayer::setInputVolume(int busId, float volume)
{
    if (isValidBus(busId))
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        buses[busId].volume = volume;
        LOGI("Set volume for bus %d to %f", busId, volume);
    }
}

void OboeLayer::muteInputBus(int busId, bool mute)
{
    if (isValidBus(busId))
    {
        buses[busId].isMuted = mute;
        LOGI("Set mute=%d for bus %d", mute, busId);
    }
}

void OboeLayer::setAudioCallback(int busId, AudioCallback callback)
{
    if (isValidBus(busId))
    {
        buses[busId].audioCallback = callback;
        LOGI("Set audio callback for bus %d", busId);
    }
    else
    {
        LOGE("Failed to set audio callback - invalid bus %d", busId);
    }
}

void OboeLayer::start()
{
    if (!playing && audioStream)
    {
        LOGI("Starting audio output");

        // Đảm bảo stream ở trạng thái sẵn sàng
        if (audioStream->getState() != oboe::StreamState::Open)
        {
            LOGI("Stream not in Open state, attempting to reopen");
            shutdown();
            if (!initialize())
            {
                LOGE("Failed to reinitialize stream");
                return;
            }
        }

        oboe::Result result = audioStream->requestStart();
        if (result == oboe::Result::OK)
        {
            playing = true;
            LOGI("Audio output started successfully");
        }
        else
        {
            LOGE("Failed to start audio output - result=%s", oboe::convertToText(result));
        }
    }
}

void OboeLayer::stop()
{
    if (playing && audioStream)
    {
        LOGI("Stopping audio output");
        oboe::Result result = audioStream->requestStop();
        if (result == oboe::Result::OK)
        {
            playing = false;
            LOGI("Audio output stopped successfully");
        }
        else
        {
            LOGE("Failed to stop audio output - result=%s", oboe::convertToText(result));
        }
    }
}

oboe::DataCallbackResult OboeLayer::onAudioReady(
    oboe::AudioStream *audioStream,
    void *audioData,
    int32_t numFrames)
{
    float *outputBuffer = static_cast<float *>(audioData);

    // Xóa buffer đầu ra
    memset(outputBuffer, 0, numFrames * channels * sizeof(float));

    if (!playing)
    {
        return oboe::DataCallbackResult::Continue;
    }

    // Tăng kích thước buffer tạm thời để xử lý nhiều dữ liệu hơn một lần
    float mixBuffer[4096 * 2]; // Buffer lớn hơn cho xử lý hiệu quả
    const int maxFramesPerIteration = 4096;

    // Xử lý từng bus
    bool anyActiveStream = false;
    
    // Xử lý theo từng đoạn lớn hơn để giảm overhead
    for (int frameOffset = 0; frameOffset < numFrames; frameOffset += maxFramesPerIteration)
    {
        int framesToProcess = std::min(maxFramesPerIteration, numFrames - frameOffset);
        
        // Xóa buffer tạm thời
        memset(mixBuffer, 0, framesToProcess * channels * sizeof(float));
        
        // Xử lý từng bus
        for (int busId = 0; busId < MAX_BUSES; busId++)
        {
            auto &bus = buses[busId];

            if (!bus.inUse || bus.isMuted || !bus.audioCallback)
            {
                continue;
            }

            // Xử lý dựa trên số kênh của bus
            if (bus.channels == 1)
            {
                // Mono input
                float tempBuffer[maxFramesPerIteration];

                // Đọc audio từ callback
                size_t framesRead = bus.audioCallback(tempBuffer, framesToProcess);

                if (framesRead > 0)
                {
                    anyActiveStream = true;
                    float busVolume = bus.volume;

                    // Chuyển đổi từ mono sang stereo interleaved nếu cần
                    if (channels == 2)
                    {
                        // Tối ưu hóa vòng lặp
                        for (size_t i = 0; i < framesRead; i += 16)
                        {
                            // Xử lý 16 samples một lần
                            for (size_t j = 0; j < 16 && i + j < framesRead; j++)
                            {
                                float sample = tempBuffer[i + j] * busVolume;
                                mixBuffer[(i + j) * 2] += sample;     // Kênh trái
                                mixBuffer[(i + j) * 2 + 1] += sample; // Kênh phải
                            }
                        }
                    }
                    else
                    {
                        // Mono output - tối ưu hóa vòng lặp
                        for (size_t i = 0; i < framesRead; i += 16)
                        {
                            // Xử lý 16 samples một lần
                            for (size_t j = 0; j < 16 && i + j < framesRead; j++)
                            {
                                mixBuffer[i + j] += tempBuffer[i + j] * busVolume;
                            }
                        }
                    }
                }
            }
            else if (bus.channels == 2)
            {
                // Stereo input
                float tempBuffer[maxFramesPerIteration * 2];

                // Đọc audio từ callback
                size_t framesRead = bus.audioCallback(tempBuffer, framesToProcess);

                if (framesRead > 0)
                {
                    anyActiveStream = true;
                    float busVolume = bus.volume;

                    if (channels == 2)
                    {
                        // Stereo output - tối ưu hóa vòng lặp
                        for (size_t i = 0; i < framesRead; i += 16)
                        {
                            // Xử lý 16 samples một lần
                            for (size_t j = 0; j < 16 && i + j < framesRead; j++)
                            {
                                mixBuffer[(i + j) * 2] += tempBuffer[(i + j) * 2] * busVolume;
                                mixBuffer[(i + j) * 2 + 1] += tempBuffer[(i + j) * 2 + 1] * busVolume;
                            }
                        }
                    }
                    else
                    {
                        // Mono output - tối ưu hóa vòng lặp
                        for (size_t i = 0; i < framesRead; i += 16)
                        {
                            // Xử lý 16 samples một lần
                            for (size_t j = 0; j < 16 && i + j < framesRead; j++)
                            {
                                // Lấy trung bình của 2 kênh
                                float monoSample = (tempBuffer[(i + j) * 2] + tempBuffer[(i + j) * 2 + 1]) * 0.5f * busVolume;
                                mixBuffer[i + j] += monoSample;
                            }
                        }
                    }
                }
            }
        }
        
        // Copy dữ liệu từ mixBuffer sang outputBuffer
        memcpy(outputBuffer + frameOffset * channels, mixBuffer, framesToProcess * channels * sizeof(float));
        
        // Áp dụng soft clipping sau khi đã copy
        if (frameOffset % 4 == 0)
        {
            for (int i = 0; i < framesToProcess * channels; i++)
            {
                float &sample = outputBuffer[frameOffset * channels + i];
                if (sample > 0.98f || sample < -0.98f)
                {
                    float sign = (sample > 0) ? 1.0f : -1.0f;
                    sample = sign * 0.98f;
                }
            }
        }
    }

    if (!anyActiveStream && playing)
    {
        return oboe::DataCallbackResult::Continue;
    }

    return oboe::DataCallbackResult::Continue;
}

// Không định nghĩa lại các phương thức đã có trong header