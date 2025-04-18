#include "recorder_interface.cpp"
#include <AudioToolbox/AudioToolbox.h>
#include <iostream>
#include <mutex>
#include <vector>

namespace TechMaster
{

    class IOSRecorder : public RecorderInterface
    {
    private:
        AudioQueueRef audioQueue;
        AudioStreamBasicDescription audioFormat;
        AudioQueueBufferRef buffers[3];
        FILE *audioFile;
        std::string filePath;
        int sampleRate;
        RecordingState state;
        std::mutex stateMutex;
        std::vector<float> pcmData; // Store PCM data as float
        std::mutex pcmDataMutex;    // Mutex for pcmData access

        // Callback function for AudioQueue
        static void HandleInputBuffer(
            void *inUserData,
            AudioQueueRef inAQ,
            AudioQueueBufferRef inBuffer,
            const AudioTimeStamp *inStartTime,
            UInt32 inNumPackets,
            const AudioStreamPacketDescription *inPacketDesc)
        {

            IOSRecorder *recorder = static_cast<IOSRecorder *>(inUserData);

            if (recorder->state == RecordingState::RECORDING)
            {
                // Write buffer to file
                if (recorder->audioFile != nullptr)
                {
                    fwrite(inBuffer->mAudioData, 1, inBuffer->mAudioDataByteSize, recorder->audioFile);
                }
                
                // Convert and store PCM data
                {
                    std::lock_guard<std::mutex> lock(recorder->pcmDataMutex);
                    
                    // Convert int16_t samples to float
                    const int16_t* samples = static_cast<const int16_t*>(inBuffer->mAudioData);
                    size_t numSamples = inBuffer->mAudioDataByteSize / sizeof(int16_t);
                    
                    // Reserve space in the vector
                    size_t currentSize = recorder->pcmData.size();
                    recorder->pcmData.resize(currentSize + numSamples);
                    
                    // Convert and copy samples
                    for (size_t i = 0; i < numSamples; i++) {
                        // Convert int16_t to float in range [-1.0, 1.0]
                        recorder->pcmData[currentSize + i] = samples[i] / 32768.0f;
                    }
                }

                // Re-enqueue the buffer for more recording
                AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, nullptr);
            }
        }

    public:
        IOSRecorder() : audioQueue(nullptr),
                        audioFile(nullptr),
                        sampleRate(44100),
                        state(RecordingState::STOPPED)
        {
        }

        ~IOSRecorder() override
        {
            stopRecording();
        }

        bool initialize(int sampleRate, const std::string &outputFilePath) override
        {
            std::lock_guard<std::mutex> lock(stateMutex);

            // Store parameters
            this->sampleRate = sampleRate;
            this->filePath = outputFilePath;

            // Set up the audio format
            memset(&audioFormat, 0, sizeof(audioFormat));
            audioFormat.mSampleRate = sampleRate;
            audioFormat.mFormatID = kAudioFormatLinearPCM;
            audioFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
            audioFormat.mBitsPerChannel = 16;
            audioFormat.mChannelsPerFrame = 1;
            audioFormat.mFramesPerPacket = 1;
            audioFormat.mBytesPerFrame = audioFormat.mBitsPerChannel / 8 * audioFormat.mChannelsPerFrame;
            audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame * audioFormat.mFramesPerPacket;

            return true;
        }

        bool startRecording() override
        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (state == RecordingState::RECORDING)
            {
                return true; // Already recording
            }

            // Clear PCM data before starting a new recording
            clearPcmData();

            // Open the output file
            audioFile = fopen(filePath.c_str(), "wb");
            if (!audioFile)
            {
                std::cerr << "Failed to open output file: " << filePath << std::endl;
                return false;
            }

            // Create a new audio queue for recording
            OSStatus status = AudioQueueNewInput(
                &audioFormat,
                HandleInputBuffer,
                this,
                nullptr,
                kCFRunLoopCommonModes,
                0,
                &audioQueue);

            if (status != noErr)
            {
                std::cerr << "Error creating audio queue: " << status << std::endl;
                fclose(audioFile);
                audioFile = nullptr;
                return false;
            }

            // Allocate and enqueue buffers
            const int bufferSize = 4096;
            for (int i = 0; i < 3; i++)
            {
                status = AudioQueueAllocateBuffer(audioQueue, bufferSize, &buffers[i]);
                if (status != noErr)
                {
                    std::cerr << "Error allocating buffer: " << status << std::endl;
                    AudioQueueDispose(audioQueue, true);
                    fclose(audioFile);
                    audioFile = nullptr;
                    return false;
                }

                AudioQueueEnqueueBuffer(audioQueue, buffers[i], 0, nullptr);
            }

            // Start the queue
            status = AudioQueueStart(audioQueue, nullptr);
            if (status != noErr)
            {
                std::cerr << "Error starting audio queue: " << status << std::endl;
                AudioQueueDispose(audioQueue, true);
                fclose(audioFile);
                audioFile = nullptr;
                return false;
            }

            state = RecordingState::RECORDING;
            return true;
        }

        bool stopRecording() override
        {
            std::lock_guard<std::mutex> lock(stateMutex);

            if (state == RecordingState::STOPPED)
            {
                return true; // Already stopped
            }

            if (audioQueue != nullptr)
            {
                // Stop and dispose of the queue
                AudioQueueStop(audioQueue, true);
                AudioQueueDispose(audioQueue, true);
                audioQueue = nullptr;
            }

            if (audioFile != nullptr)
            {
                fclose(audioFile);
                audioFile = nullptr;
            }

            state = RecordingState::STOPPED;
            return true;
        }

        RecordingState getState() const override
        {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(stateMutex));
            return state;
        }
        
        const std::vector<float>& getPcmData() const override
        {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(pcmDataMutex));
            return pcmData;
        }
        
        void clearPcmData() override
        {
            std::lock_guard<std::mutex> lock(pcmDataMutex);
            pcmData.clear();
        }
    };

} // namespace TechMaster
