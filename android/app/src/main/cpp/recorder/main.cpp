#include "opus_encoder.cpp"
#include <string>
#include <memory>
#include <iostream>

// Include platform-specific implementations
#ifdef __APPLE__
#include "ios_record_permission.h"
#include "ios_recorder.cpp"
#endif

#ifdef __ANDROID__
#include "oboe_recorder.cpp"
#endif

// Global variables
namespace
{
    // Recorder instance - sẽ tồn tại suốt vòng đời ứng dụng
    std::unique_ptr<RecorderInterface> g_recorder = nullptr;

    // Encoder instance - sẽ tồn tại suốt vòng đời ứng dụng
    std::unique_ptr<TechMaster::AudioEncoder> g_encoder = nullptr;

    // Các biến toàn cục khác
    int g_sampleRate = 48000; // Mặc định 48kHz
    std::string g_outputPath = "recording.pcm";
    bool g_isInitialized = false;
}

// Main entry point for the library
extern "C"
{

    // Check if microphone permission is granted (iOS only)
    bool recorder_check_permission()
    {
#ifdef __APPLE__
        return ios_init_audio_session();
#else
        // On Android, we'll handle permissions through the Flutter side
        return true;
#endif
    }

    // Khởi tạo recorder và encoder
    bool init_recorder(int sampleRate, const char *outputPath)
    {
        std::cout << "Initializing recorder with sample rate: " << sampleRate << ", path: " << (outputPath ? outputPath : "null") << std::endl;

        // Lưu sample rate toàn cục
        g_sampleRate = (sampleRate > 0) ? sampleRate : 48000;

        // Lưu đường dẫn output
        g_outputPath = outputPath ? outputPath : "recording.pcm";

        // Khởi tạo recorder phù hợp với nền tảng
        if (g_recorder == nullptr)
        {
#ifdef __APPLE__
            g_recorder = std::make_unique<TechMaster::IOSRecorder>();
            // Kiểm tra quyền truy cập microphone trên iOS
            if (!ios_init_audio_session())
            {
                std::cerr << "Microphone permission not granted on iOS" << std::endl;
                return false;
            }
#elif defined(__ANDROID__)
            g_recorder = std::make_unique<TechMaster::OboeRecorder>();
            std::cout << "Created OboeRecorder instance" << std::endl;
            // Android xử lý quyền truy cập ở tầng Flutter
#else
            std::cerr << "Unsupported platform" << std::endl;
            return false;
#endif
        }

        // Khởi tạo encoder
        if (g_encoder == nullptr)
        {
            g_encoder = std::make_unique<TechMaster::AudioEncoder>(g_sampleRate, 1);
            std::cout << "Created AudioEncoder instance" << std::endl;
        }

        // Khởi tạo recorder
        g_isInitialized = g_recorder->initialize(g_sampleRate, g_outputPath);
        std::cout << "Recorder initialization result: " << (g_isInitialized ? "success" : "failed") << std::endl;
        return g_isInitialized;
    }

    // Bắt đầu ghi âm
    bool start_recording()
    {
        std::cout << "Starting recording..." << std::endl;

        if (!g_isInitialized || g_recorder == nullptr)
        {
            std::cerr << "Recorder not initialized. Call init_recorder first." << std::endl;
            return false;
        }

        // Xóa dữ liệu PCM cũ
        g_recorder->clearPcmData();
        std::cout << "Cleared old PCM data" << std::endl;

        // Xóa dữ liệu đã encode cũ
        if (g_encoder != nullptr)
        {
            g_encoder->clearEncodedData();
            std::cout << "Cleared old encoded data" << std::endl;
        }

        bool result = g_recorder->startRecording();
        std::cout << "Start recording result: " << (result ? "success" : "failed") << std::endl;
        return result;
    }

    // Dừng ghi âm và encode
    bool stop_recording_and_encode()
    {
        std::cout << "Stopping recording and encoding..." << std::endl;

        if (g_recorder == nullptr)
        {
            std::cerr << "Recorder is null" << std::endl;
            return false;
        }

        // Dừng ghi âm
        bool stopResult = g_recorder->stopRecording();
        if (!stopResult)
        {
            std::cerr << "Failed to stop recording" << std::endl;
            return false;
        }
        std::cout << "Recording stopped successfully" << std::endl;

        // Lấy dữ liệu PCM và encode
        const std::vector<float> &pcmData = g_recorder->getPcmData();
        if (pcmData.empty())
        {
            std::cerr << "No PCM data to encode" << std::endl;
            return false;
        }
        std::cout << "Got PCM data, size: " << pcmData.size() << " samples" << std::endl;

        // Chuyển đổi từ float sang int16_t trước khi encode
        std::vector<int16_t> int16Data(pcmData.size());
        for (size_t i = 0; i < pcmData.size(); i++)
        {
            // Giới hạn giá trị trong khoảng [-1.0, 1.0] và chuyển sang int16
            float sample = std::max(-1.0f, std::min(1.0f, pcmData[i]));
            int16Data[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        std::cout << "Converted PCM data to int16, size: " << int16Data.size() << " samples" << std::endl;

        // Encode dữ liệu PCM đã chuyển đổi
        bool encodeResult = g_encoder->encode(int16Data);
        std::cout << "Encode result: " << (encodeResult ? "success" : "failed") << std::endl;
        if (encodeResult)
        {
            std::cout << "Encoded data size: " << g_encoder->getEncodedData().size() << " bytes" << std::endl;
        }
        return encodeResult;
    }

    // Lấy kích thước dữ liệu đã encode
    size_t get_encoded_data_size()
    {
        if (g_encoder == nullptr)
        {
            std::cerr << "Encoder is null" << std::endl;
            return 0;
        }

        size_t size = g_encoder->getEncodedData().size();
        std::cout << "Encoded data size: " << size << " bytes" << std::endl;
        return size;
    }

    // Lấy dữ liệu đã encode
    size_t get_encoded_data(uint8_t *buffer, size_t bufferSize)
    {
        std::cout << "Getting encoded data, buffer size: " << bufferSize << " bytes" << std::endl;

        if (g_encoder == nullptr || buffer == nullptr || bufferSize == 0)
        {
            std::cerr << "Invalid parameters for get_encoded_data" << std::endl;
            return 0;
        }

        const std::vector<uint8_t> &encodedData = g_encoder->getEncodedData();
        size_t copySize = std::min(bufferSize, encodedData.size());

        if (copySize > 0)
        {
            std::memcpy(buffer, encodedData.data(), copySize);
        }

        std::cout << "Copied " << copySize << " bytes of encoded data" << std::endl;
        return copySize;
    }

    // Lưu dữ liệu đã encode vào file
    bool save_encoded_data(const char *filePath)
    {
        std::cout << "Saving encoded data to file: " << (filePath ? filePath : "null") << std::endl;

        if (g_encoder == nullptr || filePath == nullptr)
        {
            std::cerr << "Invalid parameters for save_encoded_data" << std::endl;
            return false;
        }

        bool result = g_encoder->saveToFile(filePath);
        std::cout << "Save result: " << (result ? "success" : "failed") << std::endl;
        return result;
    }

    // Giải phóng tài nguyên (chỉ gọi khi ứng dụng kết thúc)
    void cleanup()
    {
        std::cout << "Cleaning up resources" << std::endl;

#ifdef __APPLE__
        ios_deinit_audio_session();
#endif

        g_recorder.reset();
        g_encoder.reset();
        g_isInitialized = false;
        std::cout << "Cleanup completed" << std::endl;
    }
}
