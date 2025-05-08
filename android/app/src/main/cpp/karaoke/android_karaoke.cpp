#include "android_karaoke.hpp"
#include <iostream>
#include <android/log.h>

#define LOG_TAG "AndroidKaraoke"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Triển khai MicrophoneRecorder

oboe::DataCallbackResult MicrophoneRecorder::onAudioReady(
    oboe::AudioStream *audioStream,
    void *audioData,
    int32_t numFrames)
{

    float *inputBuffer = static_cast<float *>(audioData);
    int numSamples = numFrames; // Mono nên frames = samples

    // Fast-path: gọi callback ngay lập tức nếu có
    if (recordingCallback)
    {
        recordingCallback(inputBuffer, numFrames);
    }

    // Lưu dữ liệu vào ring buffer - chỉ khi cần thiết
    if (isRecording)
    {
        // Sử dụng lock-free writing thay vì mutex
        recordedBuffer.write(inputBuffer, numSamples);
    }

    return oboe::DataCallbackResult::Continue;
}

MicrophoneRecorder::MicrophoneRecorder()
    : inputStream(nullptr), isRecording(false), sampleRate(48000),
      bufferSize(64) // Giảm mạnh kích thước buffer để giảm độ trễ
{
    LOGD("MicrophoneRecorder constructor");
}

MicrophoneRecorder::~MicrophoneRecorder()
{
    LOGD("MicrophoneRecorder destructor");
    stopRecording();
    shutdown();
}

bool MicrophoneRecorder::initialize()
{
    LOGD("Initializing MicrophoneRecorder");

    // Create an audio stream
    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setSharingMode(oboe::SharingMode::Shared)
                              ->setPerformanceMode(oboe::PerformanceMode::None)
                              ->setFormat(oboe::AudioFormat::Float)
                              ->setChannelCount(1) // Fix cứng mono
                              ->setSampleRate(sampleRate)
                              ->setDirection(oboe::Direction::Input)
                              ->setInputPreset(oboe::InputPreset::VoiceCommunication) // Best for voice
                              ->setFramesPerCallback(64)                              // Giảm frames per callback
                              ->setBufferCapacityInFrames(64)                         // Buffer cực nhỏ
                              ->setCallback(this)
                              ->openStream(inputStream);

    if (result != oboe::Result::OK)
    {
        LOGE("Failed to create input stream. Error: %s", oboe::convertToText(result));
        return false;
    }

    // Đặt cấu hình AAudio stream
    result = inputStream->setBufferSizeInFrames(64);
    if (result != oboe::Result::OK)
    {
        LOGD("Could not set buffer size: %s", oboe::convertToText(result));
        // Không fatal, vẫn tiếp tục
    }

    LOGD("Input stream created with sample rate: %d, channels: %d (mono), buffer size: %d",
         inputStream->getSampleRate(), inputStream->getChannelCount(),
         inputStream->getBufferSizeInFrames());

    return true;
}

void MicrophoneRecorder::shutdown()
{
    LOGD("Shutting down MicrophoneRecorder");

    if (inputStream)
    {
        inputStream->close();
        inputStream.reset();
    }
}

bool MicrophoneRecorder::startRecording()
{
    if (isRecording)
    {
        LOGD("Already recording");
        return true;
    }

    if (!inputStream)
    {
        LOGE("Input stream not initialized");
        return false;
    }

    // Xóa dữ liệu đã ghi trước đó
    recordedBuffer.clear();

    // Bắt đầu input stream
    oboe::Result result = inputStream->requestStart();
    if (result != oboe::Result::OK)
    {
        LOGE("Failed to start input stream. Error: %s", oboe::convertToText(result));
        return false;
    }

    isRecording = true;
    LOGD("Recording started");
    return true;
}

void MicrophoneRecorder::stopRecording()
{
    if (!isRecording)
    {
        return;
    }

    if (inputStream)
    {
        inputStream->requestStop();
    }

    isRecording = false;
    LOGD("Recording stopped");
}

void MicrophoneRecorder::setRecordingCallback(RecordingCallback callback)
{
    recordingCallback = std::move(callback);
}

std::vector<float> MicrophoneRecorder::getRecordedData() const
{
    // Trích xuất toàn bộ dữ liệu từ ring buffer
    std::vector<float> result;

    // Lấy kích thước dữ liệu có sẵn
    size_t availableData = recordedBuffer.getAvailableData();
    if (availableData == 0)
    {
        return result; // Trả về vector rỗng nếu không có dữ liệu
    }

    // Cấp phát bộ nhớ cho vector kết quả
    result.resize(availableData);

    // Tạo một buffer tạm thời cho dữ liệu
    std::vector<float> tempData(availableData);

    // Đọc dữ liệu vào buffer tạm thời
    // Sử dụng một phương pháp an toàn hơn thay vì sao chép toàn bộ ring buffer
    // LockFreeRingBuffer không thể được sao chép do có các atomic members
    size_t read = const_cast<LockFreeRingBuffer<float, RECORDER_BUFFER_SIZE> &>(recordedBuffer).read(tempData.data(), availableData);

    // Sao chép dữ liệu đã đọc vào vector kết quả
    std::copy(tempData.begin(), tempData.begin() + read, result.begin());

    // Điều chỉnh kích thước của result nếu không đọc được đủ dữ liệu
    if (read < availableData)
    {
        result.resize(read);
    }

    return result;
}

bool MicrophoneRecorder::saveToFile(const std::string &filePath)
{
    // Trích xuất dữ liệu trước
    std::vector<float> recordedData = getRecordedData();

    if (recordedData.empty())
    {
        LOGE("No recorded data to save");
        return false;
    }

    // Mở file để ghi
    FILE *file = fopen(filePath.c_str(), "wb");
    if (!file)
    {
        LOGE("Failed to open file for writing: %s", filePath.c_str());
        return false;
    }

    // Số lượng mẫu (sample) đã ghi âm
    size_t totalSamples = recordedData.size();

    // Kích thước dữ liệu âm thanh tính bằng byte
    uint32_t dataSize = totalSamples * sizeof(int16_t);

    // Cấu trúc header WAV file
    // RIFF header
    fwrite("RIFF", 1, 4, file);
    uint32_t fileSize = 36 + dataSize; // Kích thước toàn bộ file - 8 byte
    fwrite(&fileSize, 1, 4, file);
    fwrite("WAVE", 1, 4, file);

    // Format chunk
    fwrite("fmt ", 1, 4, file);
    uint32_t fmtSize = 16; // PCM format chunk size
    fwrite(&fmtSize, 1, 4, file);
    uint16_t audioFormat = 1; // PCM = 1
    fwrite(&audioFormat, 1, 2, file);
    uint16_t numChannels = 1; // Mono
    fwrite(&numChannels, 1, 2, file);
    uint32_t sampleRateWav = sampleRate;
    fwrite(&sampleRateWav, 1, 4, file);
    uint32_t byteRate = sampleRate * 1 * sizeof(int16_t); // Mono, nên channels = 1
    fwrite(&byteRate, 1, 4, file);
    uint16_t blockAlign = 1 * sizeof(int16_t); // Mono, nên channels = 1
    fwrite(&blockAlign, 1, 2, file);
    uint16_t bitsPerSample = 16; // 16-bit
    fwrite(&bitsPerSample, 1, 2, file);

    // Data chunk
    fwrite("data", 1, 4, file);
    fwrite(&dataSize, 1, 4, file);

    // Chuyển đổi từ float (-1.0 đến 1.0) sang int16_t (-32768 đến 32767)
    std::vector<int16_t> pcmData(totalSamples);
    for (size_t i = 0; i < totalSamples; i++)
    {
        float sample = recordedData[i];
        // Giới hạn phạm vi
        if (sample > 1.0f)
            sample = 1.0f;
        if (sample < -1.0f)
            sample = -1.0f;
        // Chuyển đổi sang int16_t
        pcmData[i] = static_cast<int16_t>(sample * 32767.0f);
    }

    // Ghi dữ liệu âm thanh
    fwrite(pcmData.data(), sizeof(int16_t), totalSamples, file);

    // Đóng file
    fclose(file);

    LOGD("Saved recorded audio to file: %s", filePath.c_str());
    LOGD("  - Sample rate: %d Hz", sampleRate);
    LOGD("  - Channels: 1 (mono)");
    LOGD("  - Duration: %.2f seconds", static_cast<float>(totalSamples) / sampleRate);

    return true;
}

bool MicrophoneRecorder::isCurrentlyRecording() const
{
    return isRecording;
}

void MicrophoneRecorder::setSampleRate(int rate)
{
    if (!isRecording)
    {
        sampleRate = rate;
    }
}

int MicrophoneRecorder::getChannels() const
{
    return 1; // Luôn trả về 1 (mono)
}

int MicrophoneRecorder::getSampleRate() const
{
    return sampleRate;
}

// Triển khai Karaoke

Karaoke::Karaoke()
    : recorder(std::make_unique<MicrophoneRecorder>()),
      player(std::make_unique<KaraokePlayer>()),
      livePlayback(false)
{
    LOGD("Karaoke constructor");
}

Karaoke::~Karaoke()
{
    LOGD("Karaoke destructor");
    if (recorder && recorder->isCurrentlyRecording())
    {
        recorder->stopRecording();
    }

    if (player)
    {
        player->stop();
    }
}

bool Karaoke::initialize()
{
    LOGD("Initializing Karaoke");

    // Khởi tạo player trước để sẵn sàng nhận dữ liệu ngay lập tức
    if (!player->initialize())
    {
        LOGE("Failed to initialize player");
        return false;
    }

    // Khởi tạo recorder sau
    if (!recorder->initialize())
    {
        LOGE("Failed to initialize microphone recorder");
        return false;
    }

    // Thiết lập callback từ microphone để stream trực tiếp đến player
    // Direct pass-through để giảm độ trễ tối đa
    recorder->setRecordingCallback([this](const float *buffer, size_t frameCount)
                                   {
        if (livePlayback && player) {
            // Truyền trực tiếp dữ liệu âm thanh đến player, không xử lý
            player->addAudioFromMic(buffer, frameCount);
        } });

    return true;
}

bool Karaoke::startRecording()
{
    // Ghi âm bình thường, không phát trực tiếp
    livePlayback = false;
    return recorder->startRecording();
}

void Karaoke::stopRecording()
{
    recorder->stopRecording();
    // Đảm bảo chế độ live playback cũng bị tắt
    if (livePlayback)
    {
        stopLivePlayback();
    }
}

bool Karaoke::saveRecordingToFile(const std::string &filePath)
{
    return recorder->saveToFile(filePath);
}

bool Karaoke::startLivePlayback()
{
    // Đảm bảo player đã được khởi tạo
    if (!player)
    {
        LOGE("Player not initialized");
        return false;
    }

    // Đánh dấu chế độ phát trực tiếp trước khi bắt đầu
    livePlayback = true;

    // Bắt đầu player trước để sẵn sàng xử lý dữ liệu audio
    if (!player->start())
    {
        LOGE("Failed to start player");
        livePlayback = false;
        return false;
    }

    // Thiết lập âm lượng phù hợp cho mono
    //player->setVolume(3.0f);
    //LOGD("Mic volume set to 3.0 for clarity");

    // Bắt đầu recorder sau
    if (!recorder->isCurrentlyRecording())
    {
        if (!recorder->startRecording())
        {
            LOGE("Failed to start recording for live playback");
            player->stop();
            livePlayback = false;
            return false;
        }
    }

    LOGD("Live playback started with minimum latency configuration");
    return true;
}

void Karaoke::stopLivePlayback()
{
    livePlayback = false;

    // Dừng player
    if (player)
    {
        player->stop();
    }

    // Không dừng recorder ở đây để có thể tiếp tục ghi âm nếu cần

    LOGD("Live playback stopped");
}

bool Karaoke::isLivePlaybackActive() const
{
    return livePlayback;
}

// Các phương thức điều chỉnh âm lượng
void Karaoke::setMicVolume(float volume)
{
    if (player)
    {
        player->setVolume(volume);
        LOGD("Karaoke mic volume set to %f", volume);
    }
}

float Karaoke::getMicVolume() const
{
    return player ? player->getVolume() : 1.0f;
}
