#include "ios_mic_player.hpp"
#include <iostream>
#include <cstring>
#include <cmath>

// Triển khai SimpleRingBuffer

SimpleRingBuffer::SimpleRingBuffer(size_t size)
    : capacity(size)
    , writePos(0)
    , readPos(0)
    , dataAvailable(0)
{
    buffer.resize(size);
}

size_t SimpleRingBuffer::write(const float* data, size_t numSamples) {
    if (numSamples == 0) return 0;
    
    std::unique_lock<std::mutex> lock(mutex);
    
    // Kiểm tra không gian còn trống
    size_t availableSpace = capacity - dataAvailable;
    if (availableSpace == 0) return 0;
    
    // Giới hạn số lượng mẫu cần ghi
    size_t samplesToWrite = std::min(numSamples, availableSpace);
    
    // Tính số lượng mẫu có thể ghi từ vị trí hiện tại đến cuối buffer
    size_t samplesToEnd = capacity - writePos;
    
    if (samplesToWrite <= samplesToEnd) {
        // Ghi không quá cuối buffer
        std::memcpy(buffer.data() + writePos, data, samplesToWrite * sizeof(float));
        writePos += samplesToWrite;
        if (writePos == capacity) writePos = 0;
    } else {
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

size_t SimpleRingBuffer::read(float* data, size_t numSamples) {
    if (numSamples == 0) return 0;
    
    std::unique_lock<std::mutex> lock(mutex);
    
    // Nếu không có dữ liệu, trả về và không đợi
    if (dataAvailable == 0) {
        // Điền silence (0) vào buffer
        std::memset(data, 0, numSamples * sizeof(float));
        return 0;
    }
    
    // Giới hạn số lượng mẫu có thể đọc
    size_t samplesToRead = std::min(numSamples, dataAvailable.load());
    
    // Tính số lượng mẫu có thể đọc từ vị trí hiện tại đến cuối buffer
    size_t samplesToEnd = capacity - readPos;
    
    if (samplesToRead <= samplesToEnd) {
        // Đọc không quá cuối buffer
        std::memcpy(data, buffer.data() + readPos, samplesToRead * sizeof(float));
        readPos += samplesToRead;
        if (readPos == capacity) readPos = 0;
    } else {
        // Đọc đến cuối buffer và quay vòng lại
        std::memcpy(data, buffer.data() + readPos, samplesToEnd * sizeof(float));
        std::memcpy(data + samplesToEnd, buffer.data(), (samplesToRead - samplesToEnd) * sizeof(float));
        readPos = samplesToRead - samplesToEnd;
    }
    
    // Cập nhật lượng dữ liệu có sẵn
    dataAvailable -= samplesToRead;
    
    return samplesToRead;
}

void SimpleRingBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    readPos = 0;
    writePos = 0;
    dataAvailable = 0;
}

size_t SimpleRingBuffer::getAvailableData() const {
    return dataAvailable;
}

size_t SimpleRingBuffer::getAvailableSpace() const {
    return capacity - dataAvailable;
}

size_t SimpleRingBuffer::getCapacity() const {
    return capacity;
}

bool SimpleRingBuffer::isEmpty() const {
    return dataAvailable == 0;
}

// Triển khai EchoCanceller
EchoCanceller::EchoCanceller(int sampleRate, int channels)
    : delayMs(100)  // Mặc định 100ms
    , cancellationFactor(0.5f)  // Mặc định 50%
    , lastOutputSample(0.0f)
    , sampleRate(sampleRate)
    , channels(channels)
    , enabled(true)
{
    // Tính kích thước tối đa cho lịch sử (2 giây)
    maxHistorySize = sampleRate * channels * 2;
    playbackHistory.resize(maxHistorySize, 0.0f);
}

void EchoCanceller::addPlaybackData(const float* data, size_t numSamples) {
    if (!enabled) return;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    // Thêm dữ liệu mới vào lịch sử
    for (size_t i = 0; i < numSamples; ++i) {
        playbackHistory.push_back(data[i]);
        
        // Đảm bảo kích thước không vượt quá giới hạn
        if (playbackHistory.size() > maxHistorySize) {
            playbackHistory.pop_front();
        }
    }
}

void EchoCanceller::processMicData(float* data, size_t numSamples) {
    if (!enabled || playbackHistory.empty()) return;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    // Tính toán độ trễ tính bằng số mẫu
    size_t delaySamples = static_cast<size_t>((delayMs * sampleRate) / 1000);
    
    // Đảm bảo độ trễ không quá lớn so với lịch sử đã lưu
    if (delaySamples >= playbackHistory.size()) {
        delaySamples = playbackHistory.size() - 1;
    }
    
    // Xử lý dữ liệu đầu vào
    for (size_t i = 0; i < numSamples; ++i) {
        // Lấy chỉ số trong lịch sử dựa trên độ trễ
        size_t historyIndex = playbackHistory.size() - delaySamples - 1;
        
        if (historyIndex < playbackHistory.size()) {
            // Lấy mẫu từ lịch sử phát
            float playbackSample = playbackHistory[historyIndex];
            
            // Thực hiện echo cancellation bằng cách trừ đi tín hiệu đã phát
            data[i] -= playbackSample * cancellationFactor;
            
            // Áp dụng noise gate đơn giản
            if (std::fabs(data[i]) < 0.01f) {
                data[i] = 0.0f;
            }
            
            // Giới hạn biên độ để tránh clipping
            if (data[i] > 1.0f) data[i] = 1.0f;
            if (data[i] < -1.0f) data[i] = -1.0f;
            
            // Thêm một chút smoothing đơn giản
            data[i] = 0.7f * data[i] + 0.3f * lastOutputSample;
            lastOutputSample = data[i];
        }
    }
}

void EchoCanceller::setDelayMs(int delay) {
    if (delay < 0) delay = 0;
    if (delay > 1000) delay = 1000;  // Giới hạn tối đa 1 giây
    
    delayMs = delay;
}

void EchoCanceller::setCancellationFactor(float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    
    cancellationFactor = factor;
}

void EchoCanceller::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    playbackHistory.clear();
    playbackHistory.resize(maxHistorySize, 0.0f);
    lastOutputSample = 0.0f;
}

void EchoCanceller::setEnabled(bool _enabled) {
    enabled = _enabled;
    if (enabled) {
        clear();  // Reset khi bật lại
    }
}

bool EchoCanceller::isEnabled() const {
    return enabled;
}

// Triển khai MicrophonePlayer

OSStatus MicrophonePlayer::outputCallback(void* inRefCon,
                                        AudioUnitRenderActionFlags* ioActionFlags,
                                        const AudioTimeStamp* inTimeStamp,
                                        UInt32 inBusNumber,
                                        UInt32 inNumberFrames,
                                        AudioBufferList* ioData) {
    return static_cast<MicrophonePlayer*>(inRefCon)->handleOutputCallback(
        ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
}

OSStatus MicrophonePlayer::handleOutputCallback(AudioUnitRenderActionFlags* ioActionFlags,
                                             const AudioTimeStamp* inTimeStamp,
                                             UInt32 inBusNumber,
                                             UInt32 inNumberFrames,
                                             AudioBufferList* ioData) {
    // Buffer tạm thời cho dữ liệu âm thanh từ ring buffer
    std::vector<float> tempBuffer(inNumberFrames * channels);
    
    // Đọc dữ liệu âm thanh từ ring buffer
    size_t framesRead = ringBuffer.read(tempBuffer.data(), inNumberFrames * channels);
    
    // Nếu không có đủ dữ liệu, tempBuffer sẽ chứa silence (0)
    
    // Áp dụng reverb nếu được bật
    if (framesRead > 0 && reverbProcessor) {
        // Áp dụng reverb trực tiếp vào tempBuffer thay vì tạo vector mới
        auto reverbedData = reverbProcessor->process(tempBuffer.data(), tempBuffer.size());
        std::copy(reverbedData.begin(), reverbedData.end(), tempBuffer.begin());
    }
    
    // Áp dụng hệ số volume vào tất cả các mẫu nếu cần
    if (framesRead > 0 && volume != 1.0f) {
        for (size_t i = 0; i < tempBuffer.size(); i++) {
            tempBuffer[i] *= volume;
            
            // Giới hạn biên độ để tránh clipping
            if (tempBuffer[i] > 1.0f) tempBuffer[i] = 1.0f;
            if (tempBuffer[i] < -1.0f) tempBuffer[i] = -1.0f;
        }
    }
    
    // Sao chép dữ liệu vào output buffer
    for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
        // Lấy thông tin buffer
        AudioBuffer& outBuffer = ioData->mBuffers[i];
        float* outData = static_cast<float*>(outBuffer.mData);
        UInt32 outSamples = outBuffer.mDataByteSize / sizeof(float);
        
        // Đảm bảo không copy quá kích thước buffer
        UInt32 samplesToCopy = std::min(outSamples, static_cast<UInt32>(tempBuffer.size()));
        
        // Copy dữ liệu hoặc zero nếu không có đủ dữ liệu
        if (framesRead > 0) {
            std::memcpy(outData, tempBuffer.data(), samplesToCopy * sizeof(float));
            
            // Thêm dữ liệu đã phát vào echo canceller
            if (echoCanceller && echoCanceller->isEnabled()) {
                echoCanceller->addPlaybackData(tempBuffer.data(), samplesToCopy);
            }
        } else {
            std::memset(outData, 0, samplesToCopy * sizeof(float));
        }
    }
    
    // Gọi callback nếu được thiết lập
    if (playbackCallback) {
        playbackCallback(tempBuffer.data(), inNumberFrames);
    }
    
    return noErr;
}

MicrophonePlayer::MicrophonePlayer(size_t bufferSize)
    : audioUnit(nullptr)
    , ringBuffer(bufferSize)
    , isPlaying(false)
    , sampleRate(48000)  // Mặc định là 48kHz
    , channels(1)        // Mặc định là mono
    , bufferSize(1024)   // Buffer size mặc định
    , volume(1.0f)       // Âm lượng mặc định là 1.0 (100%)
{
    echoCanceller = std::make_unique<EchoCanceller>(sampleRate, channels);
    reverbProcessor = std::make_unique<ReverbProcessor>();
    
    // Mặc định ban đầu sẽ tắt echo cancellation
    echoCanceller->setEnabled(false);
    
    // Bật reverb với các tham số mặc định phù hợp cho karaoke
    reverbProcessor->setRoomSize(0.7f);    // Kích thước phòng vừa phải
    reverbProcessor->setDamping(0.4f);     // Tăng damping để âm vang mượt hơn
    reverbProcessor->setWetMix(0.5f);      // Tỷ lệ reverb vừa phải
    reverbProcessor->setDryMix(0.5f);      // Tỷ lệ âm thanh gốc vừa phải
    
    // Bật reverb mặc định
    enableReverb(true);
}

MicrophonePlayer::~MicrophonePlayer() {
    stop();
    shutdown();
}

bool MicrophonePlayer::initialize() {
    debugPrint("Initializing MicrophonePlayer");
    
    // Tạo AudioComponentDescription cho Audio Output Unit
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    // Tìm và tạo Audio Component
    AudioComponent outputComponent = AudioComponentFindNext(NULL, &desc);
    if (outputComponent == nullptr) {
        debugPrint("Failed to find output audio component");
        return false;
    }
    
    // Tạo Audio Unit từ component
    OSStatus status = AudioComponentInstanceNew(outputComponent, &audioUnit);
    if (status != noErr) {
        debugPrint("Failed to create output audio unit instance: {}", status);
        return false;
    }
    
    // Thiết lập định dạng audio cho output
    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    streamFormat.mBitsPerChannel = 32;
    streamFormat.mChannelsPerFrame = channels;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = sizeof(Float32);
    streamFormat.mBytesPerPacket = sizeof(Float32);
    
    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &streamFormat,
                                sizeof(streamFormat));
    if (status != noErr) {
        debugPrint("Failed to set output stream format: {}", status);
        return false;
    }
    
    // Thiết lập callback cho output
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = outputCallback;
    callbackStruct.inputProcRefCon = this;
    
    status = AudioUnitSetProperty(audioUnit,
                                kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input,
                                0,
                                &callbackStruct,
                                sizeof(callbackStruct));
    if (status != noErr) {
        debugPrint("Failed to set render callback: {}", status);
        return false;
    }
    
    // Khởi tạo Audio Unit
    status = AudioUnitInitialize(audioUnit);
    if (status != noErr) {
        debugPrint("Failed to initialize audio unit: {}", status);
        return false;
    }
    
    // Cập nhật echo canceller với sample rate và channels đã thiết lập
    if (echoCanceller) {
        echoCanceller = std::make_unique<EchoCanceller>(sampleRate, channels);
    }
    
    debugPrint("MicrophonePlayer initialized successfully");
    return true;
}

void MicrophonePlayer::shutdown() {
    if (audioUnit) {
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
        audioUnit = nullptr;
    }
}

bool MicrophonePlayer::start() {
    if (isPlaying) {
        debugPrint("Already playing");
        return true;
    }
    
    if (!audioUnit) {
        debugPrint("Audio unit not initialized");
        return false;
    }
    
    // Xóa buffer và echo canceller
    clearBuffer();
    if (echoCanceller) {
        echoCanceller->clear();
    }
    
    // Bắt đầu Audio Unit
    OSStatus status = AudioOutputUnitStart(audioUnit);
    if (status != noErr) {
        debugPrint("Failed to start audio unit: {}", status);
        return false;
    }
    
    isPlaying = true;
    debugPrint("MicrophonePlayer playback started");
    return true;
}

void MicrophonePlayer::stop() {
    if (!isPlaying) {
        return;
    }
    
    if (audioUnit) {
        AudioOutputUnitStop(audioUnit);
    }
    
    isPlaying = false;
    debugPrint("MicrophonePlayer playback stopped");
}

size_t MicrophonePlayer::addAudioData(const float* data, size_t numSamples, bool applyEchoCancellation) {
    if (!isPlaying) {
        std::cout << "Not playing, ignoring audio data" << std::endl;
        return 0;
    }

    // Initialize pitch processor if not already initialized
    // if (!pitchProcessor) {
    //     pitchProcessor = std::make_unique<PitchProcessor>();
    //     std::cout << "Created new PitchProcessor" << std::endl;
    // }

    // Process audio through pitch shifter
    //auto processedData = pitchProcessor->process(data, numSamples);

    // If pitch processing failed, use original data
    // if (processedData.empty()) {
    //     std::cout << "Warning: Pitch processing failed, using original data" << std::endl;
    //     processedData.assign(data, data + numSamples);
    // }

    // Apply echo cancellation if requested
    // if (applyEchoCancellation && echoCanceller && echoCanceller->isEnabled()) {
    //     echoCanceller->processMicData(processedData.data(), processedData.size());
    // }

    // No need to apply reverb here as it will be applied in the output callback
    // This ensures smoother reverb effect

    // Add processed data to ring buffer
    size_t written = ringBuffer.write(data, numSamples);
    return written;
}

bool MicrophonePlayer::isCurrentlyPlaying() const {
    return isPlaying;
}

void MicrophonePlayer::setSampleRate(int rate) {
    if (!isPlaying) {
        sampleRate = rate;
        if (echoCanceller) {
            // Tạo lại echo canceller với sample rate mới
            echoCanceller = std::make_unique<EchoCanceller>(sampleRate, channels);
        }
    }
}

void MicrophonePlayer::setChannels(int numChannels) {
    if (!isPlaying) {
        channels = numChannels;
        if (echoCanceller) {
            // Tạo lại echo canceller với số kênh mới
            echoCanceller = std::make_unique<EchoCanceller>(sampleRate, channels);
        }
    }
}

int MicrophonePlayer::getSampleRate() const {
    return sampleRate;
}

int MicrophonePlayer::getChannels() const {
    return channels;
}

void MicrophonePlayer::setPlaybackCallback(PlaybackCallback callback) {
    playbackCallback = std::move(callback);
}

void MicrophonePlayer::clearBuffer() {
    ringBuffer.clear();
}

void MicrophonePlayer::enableEchoCancellation(bool enable) {
    if (echoCanceller) {
        echoCanceller->setEnabled(enable);
        debugPrint("Echo cancellation {}", enable ? "enabled" : "disabled");
    }
}

bool MicrophonePlayer::isEchoCancellationEnabled() const {
    return echoCanceller ? echoCanceller->isEnabled() : false;
}

void MicrophonePlayer::setEchoCancellationDelay(int delayMs) {
    if (echoCanceller) {
        echoCanceller->setDelayMs(delayMs);
        debugPrint("Echo cancellation delay set to {} ms", delayMs);
    }
}

void MicrophonePlayer::setEchoCancellationFactor(float factor) {
    if (echoCanceller) {
        echoCanceller->setCancellationFactor(factor);
        debugPrint("Echo cancellation factor set to {}", factor);
    }
}

void MicrophonePlayer::setVolume(float newVolume) {
    // Giới hạn volume trong khoảng 0.0 đến 5.0
    if (newVolume < 0.0f) newVolume = 0.0f;
    if (newVolume > 10.0f) newVolume = 10.0f;
    
    volume = newVolume;
    debugPrint("Mic volume set to {}", volume);
}

float MicrophonePlayer::getVolume() const {
    return volume;
}

// Reverb control methods
void MicrophonePlayer::setReverbRoomSize(float size) {
    if (reverbProcessor) {
        reverbProcessor->setRoomSize(size);
    }
}

void MicrophonePlayer::setReverbDamping(float damping) {
    if (reverbProcessor) {
        reverbProcessor->setDamping(damping);
    }
}

void MicrophonePlayer::setReverbWetMix(float wet) {
    if (reverbProcessor) {
        reverbProcessor->setWetMix(wet);
    }
}

void MicrophonePlayer::setReverbDryMix(float dry) {
    if (reverbProcessor) {
        reverbProcessor->setDryMix(dry);
    }
}

void MicrophonePlayer::enableReverb(bool enable) {
    if (enable && !reverbProcessor) {
        reverbProcessor = std::make_unique<ReverbProcessor>();
    } else if (!enable) {
        reverbProcessor.reset();
    }
}

// Triển khai KaraokePlayer

KaraokePlayer::KaraokePlayer() : player(std::make_unique<MicrophonePlayer>()) {
}

KaraokePlayer::~KaraokePlayer() {
    stop();
}

bool KaraokePlayer::initialize() {
    return player->initialize();
}

bool KaraokePlayer::start() {
    return player->start();
}

void KaraokePlayer::stop() {
    player->stop();
}

size_t KaraokePlayer::addAudioFromMic(const float* data, size_t numSamples) {
    return player->addAudioData(data, numSamples, true);  // Mặc định áp dụng echo cancellation
}

void KaraokePlayer::clearBuffer() {
    player->clearBuffer();
}

void KaraokePlayer::enableEchoCancellation(bool enable) {
    player->enableEchoCancellation(enable);
}

bool KaraokePlayer::isEchoCancellationEnabled() const {
    return player->isEchoCancellationEnabled();
}

void KaraokePlayer::setEchoCancellationDelay(int delayMs) {
    player->setEchoCancellationDelay(delayMs);
}

void KaraokePlayer::setEchoCancellationFactor(float factor) {
    player->setEchoCancellationFactor(factor);
}

void KaraokePlayer::setVolume(float volume) {
    if (player) {
        player->setVolume(volume);
    }
}

float KaraokePlayer::getVolume() const {
    return player ? player->getVolume() : 1.0f;
} 