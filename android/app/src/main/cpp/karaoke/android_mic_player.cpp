#include "android_mic_player.hpp"
#include <android/log.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "AndroidMicPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Triển khai MicrophonePlayer

MicrophonePlayer::MicrophonePlayer()
    : outputStream(nullptr), isPlaying(false), sampleRate(48000),
      bufferSize(64), // Buffer size nhỏ để giảm độ trễ
      volume(1.0f)
{
    LOGD("MicrophonePlayer constructor");
}

MicrophonePlayer::~MicrophonePlayer()
{
    LOGD("MicrophonePlayer destructor");
    stop();
    shutdown();
}

bool MicrophonePlayer::initialize()
{
    LOGD("Initializing MicrophonePlayer");

    // Create an output stream with Oboe
    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Shared) // Shared mode for low latency
                          ->setPerformanceMode(oboe::PerformanceMode::LowLatency) // LowLatency for minimal delay
                          ->setFormat(oboe::AudioFormat::Float)
                          ->setChannelCount(1) // Fix cứng mono output
                          ->setSampleRate(sampleRate)
                          ->setFramesPerCallback(64) // Đặt frames per callback thấp
                          ->setDirection(oboe::Direction::Output)
                          ->setBufferCapacityInFrames(64) // Đặt buffer thật nhỏ
                          ->setDataCallback(this)
                          ->openStream(outputStream);

    if (result != oboe::Result::OK)
    {
        LOGE("Failed to create output stream. Error: %s", oboe::convertToText(result));
        return false;
    }

    LOGD("MicrophonePlayer initialized with sample rate: %d, channels: %d (mono)",
         outputStream->getSampleRate(), outputStream->getChannelCount());
    
    // Flush buffer để đảm bảo không có dữ liệu cũ
    clearBuffer();
    
    return true;
}

void MicrophonePlayer::shutdown()
{
    LOGD("Shutting down MicrophonePlayer");

    if (outputStream)
    {
        outputStream->close();
        outputStream.reset();
    }
}

bool MicrophonePlayer::start()
{
    LOGD("Starting MicrophonePlayer");

    if (isPlaying)
    {
        LOGD("Already playing");
        return true;
    }

    if (!outputStream)
    {
        LOGE("Output stream not initialized");
        return false;
    }

    // Xóa buffer
    clearBuffer();

    // Bắt đầu output stream
    oboe::Result result = outputStream->requestStart();
    if (result != oboe::Result::OK)
    {
        LOGE("Failed to start output stream. Error: %s", oboe::convertToText(result));
        return false;
    }

    isPlaying = true;
    LOGD("MicrophonePlayer playback started");
    return true;
}

void MicrophonePlayer::stop()
{
    LOGD("Stopping MicrophonePlayer");

    if (!isPlaying)
    {
        return;
    }

    if (outputStream)
    {
        outputStream->requestStop();
    }

    isPlaying = false;
    LOGD("MicrophonePlayer playback stopped");
}

oboe::DataCallbackResult MicrophonePlayer::onAudioReady(
    oboe::AudioStream *stream,
    void *audioData,
    int32_t numFrames)
{
    // Buffer tạm thời cho dữ liệu âm thanh từ ring buffer
    // Fix cứng mono (1 channel)
    int totalSamples = numFrames; // Mono nên frames = samples
    
    // Sử dụng fixed-size array tránh stack overflow
    // Thông thường numFrames <= 192 cho low latency
    constexpr int MAX_BUFFER_SIZE = 192; // Chỉ hỗ trợ mono (1 channel)
    float tempBuffer[MAX_BUFFER_SIZE];
    
    // Đảm bảo buffer không vượt quá kích thước an toàn
    if (totalSamples > MAX_BUFFER_SIZE) {
        LOGE("Buffer size too large: %d > %d", totalSamples, MAX_BUFFER_SIZE);
        // Giới hạn số lượng mẫu để đảm bảo an toàn
        totalSamples = MAX_BUFFER_SIZE;
    }

    // Đọc dữ liệu âm thanh từ ring buffer - lock-free
    size_t framesRead = ringBuffer.read(tempBuffer, totalSamples);

    // Chỉ áp dụng volume, không áp dụng bất kỳ bộ lọc nào khác
    if (framesRead > 0 && volume != 1.0f) {
        for (size_t i = 0; i < totalSamples; i++) {
            tempBuffer[i] *= volume;
            
            // Giới hạn biên độ tránh clipping
            if (tempBuffer[i] > 1.0f) tempBuffer[i] = 1.0f;
            if (tempBuffer[i] < -1.0f) tempBuffer[i] = -1.0f;
        }
    }

    // Sao chép dữ liệu vào output buffer - direct pass through
    float *outBuffer = static_cast<float *>(audioData);

    if (framesRead == 0) {
        std::memset(outBuffer, 0, totalSamples * sizeof(float));
    } else {
        std::memcpy(outBuffer, tempBuffer, totalSamples * sizeof(float));
    }

    return oboe::DataCallbackResult::Continue;
}

size_t MicrophonePlayer::addAudioData(const float *data, size_t numSamples)
{
    if (!isPlaying) {
        return 0;
    }

    // Ưu tiên độ trễ thấp: xóa buffer nếu đang đầy
    if (ringBuffer.getAvailableSpace() < numSamples) {
        clearBuffer();
        LOGD("Buffer reset to minimize latency");
    }
    
    // Add data to ring buffer - lock-free, direct pass-through
    return ringBuffer.write(data, numSamples);
}

bool MicrophonePlayer::isCurrentlyPlaying() const
{
    return isPlaying;
}

void MicrophonePlayer::setSampleRate(int rate)
{
    if (!isPlaying)
    {
        sampleRate = rate;
    }
}

int MicrophonePlayer::getSampleRate() const
{
    return sampleRate;
}

int MicrophonePlayer::getChannels() const
{
    return 1; // Luôn trả về 1 (mono)
}

void MicrophonePlayer::setPlaybackCallback(PlaybackCallback callback)
{
    playbackCallback = std::move(callback);
}

void MicrophonePlayer::clearBuffer()
{
    ringBuffer.clear();
}

void MicrophonePlayer::setVolume(float newVolume)
{
    // Giới hạn volume trong khoảng 0.0 đến 5.0
    if (newVolume < 0.0f)
        newVolume = 0.0f;
    if (newVolume > 5.0f)
        newVolume = 5.0f;

    volume = newVolume;
    LOGD("Mic volume set to %f", volume);
}

float MicrophonePlayer::getVolume() const
{
    return volume;
}

// Triển khai KaraokePlayer

KaraokePlayer::KaraokePlayer()
    : player(std::make_unique<MicrophonePlayer>())
{
    LOGD("KaraokePlayer constructor");
}

KaraokePlayer::~KaraokePlayer()
{
    LOGD("KaraokePlayer destructor");
    stop();
}

bool KaraokePlayer::initialize()
{
    LOGD("Initializing KaraokePlayer");
    return player->initialize();
}

bool KaraokePlayer::start()
{
    LOGD("Starting KaraokePlayer");
    return player->start();
}

void KaraokePlayer::stop()
{
    LOGD("Stopping KaraokePlayer");
    player->stop();
}

size_t KaraokePlayer::addAudioFromMic(const float *data, size_t numSamples)
{
    return player->addAudioData(data, numSamples);
}

void KaraokePlayer::clearBuffer()
{
    player->clearBuffer();
}

void KaraokePlayer::setVolume(float volume)
{
    player->setVolume(volume);
}

float KaraokePlayer::getVolume() const
{
    return player->getVolume();
}
