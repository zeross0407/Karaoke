#include "ios_karaoke.hpp"
#include <iostream>

// Triển khai MicrophoneRecorder

OSStatus MicrophoneRecorder::inputCallback(void *inRefCon,
                                           AudioUnitRenderActionFlags *ioActionFlags,
                                           const AudioTimeStamp *inTimeStamp,
                                           UInt32 inBusNumber,
                                           UInt32 inNumberFrames,
                                           AudioBufferList *ioData)
{
    return static_cast<MicrophoneRecorder *>(inRefCon)->handleInputCallback(
        ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

OSStatus MicrophoneRecorder::handleInputCallback(AudioUnitRenderActionFlags *ioActionFlags,
                                                 const AudioTimeStamp *inTimeStamp,
                                                 UInt32 inBusNumber,
                                                 UInt32 inNumberFrames,
                                                 AudioBufferList *ioData)
{

    // Cấp phát một AudioBufferList tạm thời để nhận dữ liệu âm thanh
    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = channels;
    bufferList.mBuffers[0].mDataByteSize = inNumberFrames * sizeof(float);

    // Tạo buffer tạm thời
    std::vector<float> tempBuffer(inNumberFrames * channels);
    bufferList.mBuffers[0].mData = tempBuffer.data();

    // Render dữ liệu từ microphone vào buffer
    OSStatus status = AudioUnitRender(audioUnit, ioActionFlags, inTimeStamp,
                                      inBusNumber, inNumberFrames, &bufferList);

    if (status != noErr)
    {
        debugPrint("AudioUnitRender failed with status: {}", status);
        return status;
    }

    // Lưu dữ liệu vào vector
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        size_t startIndex = recordedData.size();
        recordedData.resize(startIndex + inNumberFrames * channels);
        memcpy(recordedData.data() + startIndex, tempBuffer.data(),
               inNumberFrames * channels * sizeof(float));
    }

    // Gọi callback nếu được thiết lập
    if (recordingCallback)
    {
        recordingCallback(tempBuffer.data(), inNumberFrames);
    }

    return noErr;
}

MicrophoneRecorder::MicrophoneRecorder()
    : audioUnit(nullptr), isRecording(false), sampleRate(48000) // Sử dụng 48kHz là chuẩn cho audio chuyên nghiệp
      ,
      channels(1) // Mặc định là mono
      ,
      bufferSize(1024) // Kích thước buffer mặc định
{
}

MicrophoneRecorder::~MicrophoneRecorder()
{
    stopRecording();
    shutdown();
}

bool MicrophoneRecorder::initialize()
{
    debugPrint("Initializing MicrophoneRecorder");

    // Tạo AudioComponentDescription cho Audio Unit Input
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput; // Sử dụng HAL để truy cập microphone
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    // Tìm và tạo Audio Component
    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    if (inputComponent == nullptr)
    {
        debugPrint("Failed to find input audio component");
        return false;
    }

    // Tạo Audio Unit từ component
    OSStatus status = AudioComponentInstanceNew(inputComponent, &audioUnit);
    if (status != noErr)
    {
        debugPrint("Failed to create audio unit instance: {}", status);
        return false;
    }

    // Cấu hình để sử dụng microphone input
    UInt32 enableInput = 1;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  1, // Input element
                                  &enableInput,
                                  sizeof(enableInput));
    if (status != noErr)
    {
        debugPrint("Failed to enable input: {}", status);
        return false;
    }

    // Tắt output (chúng ta chỉ cần ghi âm, không phát âm thanh)
    UInt32 disableOutput = 0;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0, // Output element
                                  &disableOutput,
                                  sizeof(disableOutput));
    if (status != noErr)
    {
        debugPrint("Failed to disable output: {}", status);
        return false;
    }

    // Đặt input device mặc định (built-in microphone)
    AudioDeviceID defaultInputDevice;
    UInt32 propertySize = sizeof(defaultInputDevice);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &propertyAddress,
                                        0,
                                        nullptr,
                                        &propertySize,
                                        &defaultInputDevice);
    if (status != noErr)
    {
        debugPrint("Failed to get default input device: {}", status);
        return false;
    }

    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global,
                                  0,
                                  &defaultInputDevice,
                                  sizeof(defaultInputDevice));
    if (status != noErr)
    {
        debugPrint("Failed to set input device: {}", status);
        return false;
    }

    // Cấu hình định dạng audio
    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsFloat | kLinearPCMFormatFlagIsPacked;
    streamFormat.mBitsPerChannel = 32;
    streamFormat.mChannelsPerFrame = channels;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = streamFormat.mChannelsPerFrame * sizeof(float);
    streamFormat.mBytesPerPacket = streamFormat.mBytesPerFrame;

    // Đặt định dạng cho input
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, // Output scope của input bus
                                  1,                      // Input element
                                  &streamFormat,
                                  sizeof(streamFormat));
    if (status != noErr)
    {
        debugPrint("Failed to set input stream format: {}", status);
        return false;
    }

    // Thiết lập callback để nhận dữ liệu từ microphone
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = inputCallback;
    callbackStruct.inputProcRefCon = this;

    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    if (status != noErr)
    {
        debugPrint("Failed to set input callback: {}", status);
        return false;
    }

    // Khởi tạo Audio Unit
    status = AudioUnitInitialize(audioUnit);
    if (status != noErr)
    {
        debugPrint("Failed to initialize audio unit: {}", status);
        return false;
    }

    debugPrint("MicrophoneRecorder initialized successfully");
    return true;
}

void MicrophoneRecorder::shutdown()
{
    if (audioUnit)
    {
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
    }
}

bool MicrophoneRecorder::startRecording()
{
    if (isRecording)
    {
        debugPrint("Already recording");
        return true;
    }

    if (!audioUnit)
    {
        debugPrint("Audio unit not initialized");
        return false;
    }

    // Xóa dữ liệu đã ghi trước đó
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        recordedData.clear();
    }

    // Bắt đầu Audio Unit
    OSStatus status = AudioOutputUnitStart(audioUnit);
    if (status != noErr)
    {
        debugPrint("Failed to start audio unit: {}", status);
        return false;
    }

    isRecording = true;
    debugPrint("Recording started");
    return true;
}

void MicrophoneRecorder::stopRecording()
{
    if (!isRecording)
    {
        return;
    }

    if (audioUnit)
    {
        AudioOutputUnitStop(audioUnit);
    }

    isRecording = false;
    debugPrint("Recording stopped, total frames: {}", recordedData.size() / channels);
}

void MicrophoneRecorder::setRecordingCallback(RecordingCallback callback)
{
    recordingCallback = std::move(callback);
}

std::vector<float> MicrophoneRecorder::getRecordedData() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return recordedData;
}

bool MicrophoneRecorder::saveToFile(const std::string &filePath)
{
    std::lock_guard<std::mutex> lock(dataMutex);

    if (recordedData.empty())
    {
        debugPrint("No recorded data to save");
        return false;
    }

    // Mở file để ghi
    FILE *file = fopen(filePath.c_str(), "wb");
    if (!file)
    {
        debugPrint("Failed to open file for writing: {}", filePath);
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
    uint16_t numChannels = channels;
    fwrite(&numChannels, 1, 2, file);
    uint32_t sampleRateWav = sampleRate;
    fwrite(&sampleRateWav, 1, 4, file);
    uint32_t byteRate = sampleRate * channels * sizeof(int16_t);
    fwrite(&byteRate, 1, 4, file);
    uint16_t blockAlign = channels * sizeof(int16_t);
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

    debugPrint("Saved recorded audio to file: {}", filePath);
    debugPrint("  - Sample rate: {} Hz", sampleRate);
    debugPrint("  - Channels: {}", channels);
    debugPrint("  - Duration: {:.2f} seconds", static_cast<float>(totalSamples) / (sampleRate * channels));

    return true;
}

bool MicrophoneRecorder::isCurrentlyRecording() const
{
    return isRecording;
}

void MicrophoneRecorder::setChannels(int numChannels)
{
    if (!isRecording)
    {
        channels = numChannels;
    }
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
    return channels;
}

int MicrophoneRecorder::getSampleRate() const
{
    return sampleRate;
}

// Triển khai Karaoke với chức năng live playback

Karaoke::Karaoke()
    : recorder(std::make_unique<MicrophoneRecorder>()), player(std::make_unique<KaraokePlayer>()), livePlayback(false)
{
}

Karaoke::~Karaoke()
{
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
    // Khởi tạo recorder
    if (!recorder->initialize())
    {
        debugPrint("Failed to initialize microphone recorder");
        return false;
    }

    // Khởi tạo player
    if (!player->initialize())
    {
        debugPrint("Failed to initialize player");
        return false;
    }

    // Thiết lập callback từ microphone để stream trực tiếp đến player
    recorder->setRecordingCallback([this](const float *buffer, size_t frameCount)
                                   {
        if (livePlayback && player) {
            // Thêm dữ liệu vào player để phát
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
        debugPrint("Player not initialized");
        return false;
    }

    // Bắt đầu player
    if (!player->start())
    {
        debugPrint("Failed to start player");
        return false;
    }

    // Đánh dấu chế độ phát trực tiếp
    livePlayback = true;

    // Bắt đầu recorder nếu chưa hoạt động
    if (!recorder->isCurrentlyRecording())
    {
        if (!recorder->startRecording())
        {
            debugPrint("Failed to start recording for live playback");
            player->stop();
            livePlayback = false;
            return false;
        }
    }

    debugPrint("Live playback started");
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

    debugPrint("Live playback stopped");
}

bool Karaoke::isLivePlaybackActive() const
{
    return livePlayback;
}

// Các phương thức điều khiển echo cancellation
void Karaoke::enableEchoCancellation(bool enable)
{
    if (player)
    {
        player->enableEchoCancellation(enable);
        debugPrint("Karaoke echo cancellation {}", enable ? "enabled" : "disabled");
    }
}

bool Karaoke::isEchoCancellationEnabled() const
{
    return player ? player->isEchoCancellationEnabled() : false;
}

void Karaoke::setEchoCancellationDelay(int delayMs)
{
    if (player)
    {
        player->setEchoCancellationDelay(delayMs);
    }
}

void Karaoke::setEchoCancellationFactor(float factor)
{
    if (player)
    {
        player->setEchoCancellationFactor(factor);
    }
}

// Các phương thức điều chỉnh âm lượng
void Karaoke::setMicVolume(float volume)
{
    if (player)
    {
        player->setVolume(volume);
        debugPrint("Karaoke mic volume set to {}", volume);
    }
}

float Karaoke::getMicVolume() const
{
    return player ? player->getVolume() : 1.0f;
}
