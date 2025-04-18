#include "android_mic_player.hpp"
#include <android/log.h>
#include <cstring>
#include <cmath>

#define LOG_TAG "AndroidMicPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Triển khai SimpleRingBuffer

SimpleRingBuffer::SimpleRingBuffer(size_t size)
    : capacity(size), writePos(0), readPos(0), dataAvailable(0)
{
    buffer.resize(size);
}

size_t SimpleRingBuffer::write(const float *data, size_t numSamples)
{
    if (numSamples == 0)
        return 0;

    std::unique_lock<std::mutex> lock(mutex);

    // Kiểm tra không gian còn trống
    size_t availableSpace = capacity - dataAvailable;
    if (availableSpace == 0)
        return 0;

    // Giới hạn số lượng mẫu cần ghi
    size_t samplesToWrite = std::min(numSamples, availableSpace);

    // Tính số lượng mẫu có thể ghi từ vị trí hiện tại đến cuối buffer
    size_t samplesToEnd = capacity - writePos;

    if (samplesToWrite <= samplesToEnd)
    {
        // Ghi không quá cuối buffer
        std::memcpy(buffer.data() + writePos, data, samplesToWrite * sizeof(float));
        writePos += samplesToWrite;
        if (writePos == capacity)
            writePos = 0;
    }
    else
    {
        // Ghi đến cuối buffer và quay vòng lại
        std::memcpy(buffer.data() + writePos, data, samplesToEnd * sizeof(float));
        std::memcpy(buffer.data(), data + samplesToEnd, (samplesToWrite - samplesToEnd) * sizeof(float));
        writePos = samplesToWrite - samplesToEnd;
    }

    // Cập nhật lượng dữ liệu có sẵn
    dataAvailable += samplesToWrite;

    // Thông báo cho thread đang đợi dữ liệu
    lock.unlock();
    dataCondition.notify_one();

    return samplesToWrite;
}

size_t SimpleRingBuffer::read(float *data, size_t numSamples)
{
    if (numSamples == 0)
        return 0;

    std::unique_lock<std::mutex> lock(mutex);

    // Nếu không có dữ liệu, trả về và không đợi
    if (dataAvailable == 0)
    {
        // Điền silence (0) vào buffer
        std::memset(data, 0, numSamples * sizeof(float));
        return 0;
    }

    // Giới hạn số lượng mẫu có thể đọc
    size_t samplesToRead = std::min(numSamples, dataAvailable.load());

    // Tính số lượng mẫu có thể đọc từ vị trí hiện tại đến cuối buffer
    size_t samplesToEnd = capacity - readPos;

    if (samplesToRead <= samplesToEnd)
    {
        // Đọc không quá cuối buffer
        std::memcpy(data, buffer.data() + readPos, samplesToRead * sizeof(float));
        readPos += samplesToRead;
        if (readPos == capacity)
            readPos = 0;
    }
    else
    {
        // Đọc đến cuối buffer và quay vòng lại
        std::memcpy(data, buffer.data() + readPos, samplesToEnd * sizeof(float));
        std::memcpy(data + samplesToEnd, buffer.data(), (samplesToRead - samplesToEnd) * sizeof(float));
        readPos = samplesToRead - samplesToEnd;
    }

    // Cập nhật lượng dữ liệu có sẵn
    dataAvailable -= samplesToRead;

    return samplesToRead;
}

void SimpleRingBuffer::clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    readPos = 0;
    writePos = 0;
    dataAvailable = 0;
}

size_t SimpleRingBuffer::getAvailableData() const
{
    return dataAvailable;
}

size_t SimpleRingBuffer::getAvailableSpace() const
{
    return capacity - dataAvailable;
}

size_t SimpleRingBuffer::getCapacity() const
{
    return capacity;
}

bool SimpleRingBuffer::isEmpty() const
{
    return dataAvailable == 0;
}

// Triển khai MicrophonePlayer

MicrophonePlayer::MicrophonePlayer(size_t bufferSize)
    : outputStream(nullptr), ringBuffer(bufferSize), isPlaying(false), sampleRate(48000)
      ,
      channels(1) // Thay đổi thành mono để giảm tiếng rè (từ 2 thành 1)
      ,
      bufferSize(64) // Giảm mạnh buffer size để giảm độ trễ
      ,
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
                          ->setChannelCount(channels) // Mono output
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

    LOGD("MicrophonePlayer initialized with sample rate: %d, channels: %d",
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
    int channelCount = stream->getChannelCount();
    std::vector<float> tempBuffer(numFrames * channelCount);

    // Đọc dữ liệu âm thanh từ ring buffer
    size_t framesRead = ringBuffer.read(tempBuffer.data(), numFrames * channelCount);

    // Chỉ áp dụng volume, không áp dụng bất kỳ bộ lọc nào khác
    if (framesRead > 0 && volume != 1.0f) {
        for (size_t i = 0; i < tempBuffer.size(); i++) {
            tempBuffer[i] *= volume;
            
            // Giới hạn biên độ tránh clipping
            if (tempBuffer[i] > 1.0f) tempBuffer[i] = 1.0f;
            if (tempBuffer[i] < -1.0f) tempBuffer[i] = -1.0f;
        }
    }

    // Sao chép dữ liệu vào output buffer - direct pass through
    float *outBuffer = static_cast<float *>(audioData);

    if (framesRead == 0) {
        std::memset(outBuffer, 0, numFrames * channelCount * sizeof(float));
    } else {
        std::memcpy(outBuffer, tempBuffer.data(), numFrames * channelCount * sizeof(float));
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
    
    // Add data to ring buffer - direct pass-through
    size_t written = ringBuffer.write(data, numSamples);
    return written;
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

void MicrophonePlayer::setChannels(int numChannels)
{
    if (!isPlaying)
    {
        channels = numChannels;
    }
}

int MicrophonePlayer::getSampleRate() const
{
    return sampleRate;
}

int MicrophonePlayer::getChannels() const
{
    return channels;
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
