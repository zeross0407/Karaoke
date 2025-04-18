#pragma once

#include <oboe/Oboe.h>
#include <vector>
#include <fstream>
#include <string>
#include <mutex>
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "OboeRecorder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGE(...)
#endif
#include "recorder_interface.cpp"

namespace TechMaster
{

    class OboeRecorder : public RecorderInterface, 
                         public oboe::AudioStreamDataCallback,
                         public oboe::AudioStreamErrorCallback
    {
    public:
        OboeRecorder() : mState(RecordingState::STOPPED), mSampleRate(48000) {
            LOGI("OboeRecorder created");
        }

        ~OboeRecorder()
        {
            stopRecording();
        }

        bool initialize(int sampleRate, const std::string &outputPath) override
        {
            LOGI("Initializing OboeRecorder with sample rate %d and output path %s", 
                 sampleRate, outputPath.c_str());
            
            mSampleRate = sampleRate;
            mOutputPath = outputPath;
            mPcmData.clear();
            return true;
        }

        bool startRecording() override
        {
            if (mState == RecordingState::RECORDING)
            {
                LOGI("Already recording");
                return true; // Already recording
            }

            LOGI("Starting recording");
            
            // Clear previous recording data
            mPcmData.clear();
            
            // Open file for writing raw PCM data
            mFile = fopen(mOutputPath.c_str(), "wb");
            if (!mFile) {
                LOGE("Failed to open output file: %s", mOutputPath.c_str());
                return false;
            }

            // Set up the audio stream builder
            oboe::AudioStreamBuilder builder;
            builder.setDirection(oboe::Direction::Input)
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
                ->setSharingMode(oboe::SharingMode::Exclusive)
                // Use 16-bit PCM format for better compatibility with speech recognition
                ->setFormat(oboe::AudioFormat::I16)
                ->setChannelCount(1)
                ->setSampleRate(mSampleRate)
                ->setDataCallback(this)
                ->setErrorCallback(this);
            
            // Open the stream
            oboe::Result result = builder.openStream(mStream);
            if (result != oboe::Result::OK)
            {
                LOGE("Failed to open stream: %s", oboe::convertToText(result));
                if (mFile) {
                    fclose(mFile);
                    mFile = nullptr;
                }
                return false;
            }
            
            LOGI("Stream opened with sample rate: %d, channels: %d, format: %s", 
                 mStream->getSampleRate(), 
                 mStream->getChannelCount(),
                 oboe::convertToText(mStream->getFormat()));

            // Start the stream
            result = mStream->requestStart();
            if (result != oboe::Result::OK)
            {
                LOGE("Failed to start stream: %s", oboe::convertToText(result));
                mStream->close();
                mStream.reset();
                if (mFile) {
                    fclose(mFile);
                    mFile = nullptr;
                }
                return false;
            }

            mState = RecordingState::RECORDING;
            LOGI("Recording started successfully");
            return true;
        }

        bool stopRecording() override
        {
            if (mState != RecordingState::RECORDING || !mStream)
            {
                return true; // Not recording or already stopped
            }

            LOGI("Stopping recording");
            
            // Stop and close the stream
            mStream->requestStop();
            mStream->close();
            mStream.reset();
            
            // Close the file
            if (mFile) {
                fclose(mFile);
                mFile = nullptr;
            }

            mState = RecordingState::STOPPED;
            LOGI("Recording stopped, captured %zu samples", mPcmData.size());
            return true;
        }

        RecordingState getState() const override
        {
            return mState;
        }

        const std::vector<float> &getPcmData() const override
        {
            return mPcmData;
        }

        void clearPcmData() override
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            mPcmData.clear();
        }

        // Implement AudioStreamDataCallback
        oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) override
        {
            // Check if we're still recording
            if (mState != RecordingState::RECORDING) {
                return oboe::DataCallbackResult::Stop;
            }
            
            // Process based on the audio format
            if (audioStream->getFormat() == oboe::AudioFormat::I16) {
                // Process 16-bit PCM data
                int16_t *int16Data = static_cast<int16_t *>(audioData);
                
                // Write raw PCM data to file
                if (mFile) {
                    fwrite(int16Data, sizeof(int16_t), numFrames, mFile);
                }
                
                // Lock to safely modify the PCM data vector
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                // Convert int16 to float and append to our vector
                size_t oldSize = mPcmData.size();
                mPcmData.resize(oldSize + numFrames);
                
                // Convert int16 to float in range [-1.0, 1.0]
                for (int i = 0; i < numFrames; i++) {
                    mPcmData[oldSize + i] = int16Data[i] / 32768.0f;
                }
            } 
            else if (audioStream->getFormat() == oboe::AudioFormat::Float) {
                // Process float data
                float *floatData = static_cast<float *>(audioData);
                
                // Convert to int16 for file writing
                if (mFile) {
                    std::vector<int16_t> int16Buffer(numFrames);
                    for (int i = 0; i < numFrames; i++) {
                        // Clamp to [-1.0, 1.0] and convert to int16
                        float sample = std::max(-1.0f, std::min(1.0f, floatData[i]));
                        int16Buffer[i] = static_cast<int16_t>(sample * 32767.0f);
                    }
                    fwrite(int16Buffer.data(), sizeof(int16_t), numFrames, mFile);
                }
                
                // Lock to safely modify the PCM data vector
                std::lock_guard<std::mutex> lock(mDataMutex);
                
                // Append the new audio data to our vector
                size_t oldSize = mPcmData.size();
                mPcmData.resize(oldSize + numFrames);
                std::memcpy(mPcmData.data() + oldSize, floatData, numFrames * sizeof(float));
            }

            return oboe::DataCallbackResult::Continue;
        }
        
        // Error callback
        bool onError(
            oboe::AudioStream *audioStream,
            oboe::Result error) override {
            
            LOGE("Audio stream error: %s", oboe::convertToText(error));
            return true; // Indicates that the error has been handled
        }

    private:
        RecordingState mState;
        int mSampleRate;
        std::string mOutputPath;
        std::vector<float> mPcmData;
        std::shared_ptr<oboe::AudioStream> mStream;
        std::mutex mDataMutex;
        FILE* mFile = nullptr;
    };

} // namespace TechMaster
