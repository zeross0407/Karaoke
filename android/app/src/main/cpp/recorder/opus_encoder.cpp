#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <fstream>
#include <cstring>
#include <opus.h>
#include <ogg/ogg.h>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace TechMaster
{

    /**
     * AudioEncoder - A class for encoding PCM audio data to Opus OGG format
     */
    class AudioEncoder
    {
    private:
        // Opus encoder state
        OpusEncoder *opusEncoder;

        // Ogg stream state
        ogg_stream_state os;

        // Ogg page
        ogg_page og;

        // Ogg packet
        ogg_packet op;

        // Encoder parameters
        int sampleRate;
        int channels;
        int frameSize;
        int maxFrameSize;
        int maxPacketSize;

        // Encoded data
        std::vector<uint8_t> encodedData;

        // Serial number for Ogg stream
        int serialNo;

        // Packet counter
        ogg_int64_t packetNo;

        // Initialize Ogg stream
        bool initOggStream()
        {
            serialNo = rand();
            if (ogg_stream_init(&os, serialNo) != 0)
            {
                std::cerr << "Failed to initialize Ogg stream" << std::endl;
                return false;
            }
            packetNo = 0;
            return true;
        }

        // Write Ogg header packets
        bool writeOggHeader()
        {
            // Create OpusHead packet
            unsigned char header[19];
            header[0] = 'O';
            header[1] = 'p';
            header[2] = 'u';
            header[3] = 's';
            header[4] = 'H';
            header[5] = 'e';
            header[6] = 'a';
            header[7] = 'd';
            header[8] = 1; // Version
            header[9] = channels;
            // Pre-skip (16 bit, little endian)
            header[10] = 0;
            header[11] = 0;
            // Sample rate (32 bit, little endian)
            header[12] = sampleRate & 0xFF;
            header[13] = (sampleRate >> 8) & 0xFF;
            header[14] = (sampleRate >> 16) & 0xFF;
            header[15] = (sampleRate >> 24) & 0xFF;
            // Output gain (16 bit, little endian)
            header[16] = 0;
            header[17] = 0;
            // Channel mapping
            header[18] = 0; // Mono or stereo

            // Create Ogg packet for header
            op.packet = header;
            op.bytes = 19;
            op.b_o_s = 1; // Beginning of stream
            op.e_o_s = 0;
            op.granulepos = 0;
            op.packetno = packetNo++;

            // Add packet to Ogg stream
            if (ogg_stream_packetin(&os, &op) != 0)
            {
                std::cerr << "Failed to add header packet to Ogg stream" << std::endl;
                return false;
            }

            // Create OpusTags packet
            const char *vendor = "TechMaster AudioEncoder";
            int vendor_length = strlen(vendor);

            unsigned char *tags = new unsigned char[8 + vendor_length + 4];
            tags[0] = 'O';
            tags[1] = 'p';
            tags[2] = 'u';
            tags[3] = 's';
            tags[4] = 'T';
            tags[5] = 'a';
            tags[6] = 'g';
            tags[7] = 's';
            // Vendor string length (32 bit, little endian)
            tags[8] = vendor_length & 0xFF;
            tags[9] = (vendor_length >> 8) & 0xFF;
            tags[10] = (vendor_length >> 16) & 0xFF;
            tags[11] = (vendor_length >> 24) & 0xFF;
            // Vendor string
            memcpy(tags + 12, vendor, vendor_length);
            // User comment list length (32 bit, little endian)
            tags[12 + vendor_length] = 0;
            tags[13 + vendor_length] = 0;
            tags[14 + vendor_length] = 0;
            tags[15 + vendor_length] = 0;

            // Create Ogg packet for tags
            op.packet = tags;
            op.bytes = 8 + vendor_length + 4;
            op.b_o_s = 0;
            op.e_o_s = 0;
            op.granulepos = 0;
            op.packetno = packetNo++;

            // Add packet to Ogg stream
            if (ogg_stream_packetin(&os, &op) != 0)
            {
                std::cerr << "Failed to add tags packet to Ogg stream" << std::endl;
                delete[] tags;
                return false;
            }

            delete[] tags;
            return true;
        }

        // Flush Ogg pages to encoded data
        bool flushOggPages(bool end = false)
        {
            // Flush all pages
            while (end ? ogg_stream_flush(&os, &og) : ogg_stream_pageout(&os, &og))
            {
                // Append page data to encoded data
                encodedData.insert(encodedData.end(), og.header, og.header + og.header_len);
                encodedData.insert(encodedData.end(), og.body, og.body + og.body_len);
            }
            return true;
        }

        // Ước tính mức nhiễu nền từ dữ liệu PCM
        int16_t estimateNoiseFloor(const std::vector<int16_t>& pcmData) 
        {
            if (pcmData.size() < 1000) {
                return 300; // Giá trị mặc định nếu không đủ dữ liệu
            }
            
            // Kích thước cửa sổ để phân tích (10ms)
            const size_t WINDOW_SIZE = sampleRate / 100;
            
            // Số cửa sổ
            const size_t numWindows = pcmData.size() / WINDOW_SIZE;
            
            if (numWindows < 10) {
                return 300; // Cần ít nhất 10 cửa sổ để phân tích
            }
            
            // Tính RMS cho mỗi cửa sổ
            std::vector<int16_t> windowRMS(numWindows);
            for (size_t i = 0; i < numWindows; i++) {
                size_t startIdx = i * WINDOW_SIZE;
                size_t endIdx = startIdx + WINDOW_SIZE;
                
                // Tính RMS cho cửa sổ hiện tại
                double sumSquares = 0.0;
                for (size_t j = startIdx; j < endIdx; j++) {
                    sumSquares += static_cast<double>(pcmData[j]) * pcmData[j];
                }
                
                windowRMS[i] = static_cast<int16_t>(std::sqrt(sumSquares / WINDOW_SIZE));
            }
            
            // Sắp xếp các giá trị RMS
            std::sort(windowRMS.begin(), windowRMS.end());
            
            // Lấy giá trị trung vị của 20% cửa sổ có RMS thấp nhất
            size_t noiseWindowCount = numWindows / 5;
            int16_t noiseFloor = windowRMS[noiseWindowCount / 2];
            
            std::cout << "Estimated noise floor: " << noiseFloor << std::endl;
            
            return noiseFloor;
        }

        // Phương pháp đơn giản để loại bỏ nhiễu
        std::vector<int16_t> simpleNoiseReduction(const std::vector<int16_t>& pcmData) 
        {
            if (pcmData.empty()) {
                return pcmData;
            }
            
            std::cout << "Applying noise reduction..." << std::endl;
            
            // Ước tính mức nhiễu nền
            int16_t noiseFloor = estimateNoiseFloor(pcmData);
            
            // Ngưỡng noise gate (cao hơn mức nhiễu nền một chút)
            int16_t noiseGateThreshold = noiseFloor * 1.5;
            
            // Kích thước cửa sổ để phân tích và xử lý (5ms)
            const size_t WINDOW_SIZE = sampleRate / 200;
            
            // Tạo vector mới để lưu dữ liệu đã xử lý
            std::vector<int16_t> processedData = pcmData;
            
            // Xử lý từng cửa sổ
            for (size_t i = 0; i < pcmData.size(); i += WINDOW_SIZE) {
                size_t endIdx = std::min(i + WINDOW_SIZE, pcmData.size());
                
                // Tính RMS cho cửa sổ hiện tại
                double sumSquares = 0.0;
                for (size_t j = i; j < endIdx; j++) {
                    sumSquares += static_cast<double>(pcmData[j]) * pcmData[j];
                }
                
                double rms = std::sqrt(sumSquares / (endIdx - i));
                
                // Hệ số giảm âm dựa trên RMS
                float attenuationFactor = 1.0f;
                
                if (rms < noiseGateThreshold) {
                    // Áp dụng soft knee để tránh artifacts
                    if (rms < noiseFloor) {
                        // Giảm âm mạnh nếu dưới mức nhiễu nền
                        attenuationFactor = 0.1f;
                    } else {
                        // Giảm âm dần dần trong vùng giữa noiseFloor và noiseGateThreshold
                        float ratio = (rms - noiseFloor) / (noiseGateThreshold - noiseFloor);
                        attenuationFactor = 0.1f + 0.9f * ratio;
                    }
                    
                    // Áp dụng hệ số giảm âm cho cửa sổ
                    for (size_t j = i; j < endIdx; j++) {
                        processedData[j] = static_cast<int16_t>(pcmData[j] * attenuationFactor);
                    }
                }
            }
            
            // Áp dụng smoothing để tránh artifacts
            const size_t SMOOTHING_WINDOW = 5; // Số mẫu để làm mịn
            if (processedData.size() > SMOOTHING_WINDOW * 2) {
                std::vector<int16_t> smoothedData = processedData;
                
                for (size_t i = SMOOTHING_WINDOW; i < processedData.size() - SMOOTHING_WINDOW; i++) {
                    // Tính giá trị trung bình của cửa sổ trượt
                    int32_t sum = 0;
                    for (size_t j = i - SMOOTHING_WINDOW; j <= i + SMOOTHING_WINDOW; j++) {
                        sum += processedData[j];
                    }
                    smoothedData[i] = static_cast<int16_t>(sum / (2 * SMOOTHING_WINDOW + 1));
                }
                
                processedData = smoothedData;
            }
            
            std::cout << "Noise reduction completed" << std::endl;
            
            return processedData;
        }

        // Đơn giản hóa phương pháp phát hiện và loại bỏ khoảng lặng
        std::vector<int16_t> simpleTrimSilence(const std::vector<int16_t>& pcmData) 
        {
            if (pcmData.empty()) {
                std::cout << "PCM data is empty" << std::endl;
                return pcmData;
            }
            
            std::cout << "Original PCM data size: " << pcmData.size() << " samples" << std::endl;
            
            // Ngưỡng phát hiện tiếng nói (giá trị tuyệt đối)
            // Giá trị này có thể điều chỉnh, thấp hơn sẽ nhạy hơn với âm thanh nhỏ
            const int16_t SILENCE_THRESHOLD = 500; // Khoảng 1.5% của giá trị tối đa (32767)
            
            // Kích thước cửa sổ để phân tích (20ms)
            const size_t WINDOW_SIZE = sampleRate / 50;
            
            // Số cửa sổ liên tiếp cần vượt ngưỡng để xác định là tiếng nói
            const size_t MIN_VOICE_WINDOWS = 3;
            
            // Đệm trước và sau khoảng tiếng nói (200ms)
            const size_t BUFFER_WINDOWS = sampleRate / (5 * WINDOW_SIZE);
            
            // Tính số cửa sổ
            const size_t numWindows = pcmData.size() / WINDOW_SIZE + (pcmData.size() % WINDOW_SIZE > 0 ? 1 : 0);
            
            // Mảng đánh dấu cửa sổ có tiếng nói hay không
            std::vector<bool> isVoiceWindow(numWindows, false);
            
            // Phân tích từng cửa sổ
            for (size_t i = 0; i < numWindows; i++) {
                size_t startIdx = i * WINDOW_SIZE;
                size_t endIdx = std::min(startIdx + WINDOW_SIZE, pcmData.size());
                
                // Tìm giá trị tuyệt đối lớn nhất trong cửa sổ
                int16_t maxAbs = 0;
                for (size_t j = startIdx; j < endIdx; j++) {
                    maxAbs = std::max(maxAbs, static_cast<int16_t>(std::abs(pcmData[j])));
                }
                
                // Đánh dấu cửa sổ có tiếng nói nếu vượt ngưỡng
                isVoiceWindow[i] = (maxAbs > SILENCE_THRESHOLD);
            }
            
            // Tìm chuỗi cửa sổ liên tiếp có tiếng nói
            size_t startWindow = 0;
            size_t endWindow = 0;
            bool foundVoice = false;
            
            // Tìm chuỗi cửa sổ liên tiếp có tiếng nói đủ dài
            for (size_t i = 0; i < numWindows; i++) {
                if (isVoiceWindow[i]) {
                    // Đếm số cửa sổ liên tiếp có tiếng nói
                    size_t consecutiveVoiceWindows = 1;
                    for (size_t j = i + 1; j < numWindows && isVoiceWindow[j]; j++) {
                        consecutiveVoiceWindows++;
                    }
                    
                    // Nếu đủ dài, đánh dấu là đoạn tiếng nói
                    if (consecutiveVoiceWindows >= MIN_VOICE_WINDOWS) {
                        if (!foundVoice) {
                            startWindow = i;
                            foundVoice = true;
                        }
                        endWindow = i + consecutiveVoiceWindows - 1;
                        i += consecutiveVoiceWindows - 1; // Bỏ qua các cửa sổ đã xét
                    }
                }
            }
            
            // Nếu không tìm thấy tiếng nói, trả về dữ liệu gốc
            if (!foundVoice) {
                std::cout << "No voice detected in the audio" << std::endl;
                return pcmData;
            }
            
            // Thêm đệm trước và sau
            if (startWindow > BUFFER_WINDOWS) {
                startWindow -= BUFFER_WINDOWS;
            } else {
                startWindow = 0;
            }
            
            if (endWindow + BUFFER_WINDOWS < numWindows) {
                endWindow += BUFFER_WINDOWS;
            } else {
                endWindow = numWindows - 1;
            }
            
            // Tính vị trí mẫu bắt đầu và kết thúc
            size_t startSample = startWindow * WINDOW_SIZE;
            size_t endSample = std::min((endWindow + 1) * WINDOW_SIZE, pcmData.size());
            
            // Tạo vector mới chỉ chứa phần tiếng nói
            std::vector<int16_t> trimmedData(pcmData.begin() + startSample, pcmData.begin() + endSample);
            
            std::cout << "Trimmed PCM data size: " << trimmedData.size() 
                      << " samples (removed " << pcmData.size() - trimmedData.size() 
                      << " samples, " << (100.0 * (pcmData.size() - trimmedData.size()) / pcmData.size()) 
                      << "% reduction)" << std::endl;
            
            return trimmedData;
        }

        // Pad the audio data to ensure it has complete frames
        std::vector<int16_t> padAudio(const std::vector<int16_t> &pcmData)
        {
            if (pcmData.empty())
            {
                return pcmData;
            }

            // Calculate how many complete frames we have
            int totalFrames = pcmData.size() / (channels * frameSize);

            // Calculate how many samples are left that don't make a complete frame
            int remainingSamples = pcmData.size() % (channels * frameSize);

            // If we have a complete number of frames, no padding needed
            if (remainingSamples == 0)
            {
                return pcmData;
            }

            // Calculate how many samples to add to complete the last frame
            int paddingSamples = (channels * frameSize) - remainingSamples;

            // Create a new vector with the padded data
            std::vector<int16_t> paddedData = pcmData;

            // Add padding (zeros) to complete the last frame
            paddedData.resize(pcmData.size() + paddingSamples, 0);

            std::cout << "Padded audio: added " << paddingSamples
                      << " samples to complete the last frame. New size = "
                      << paddedData.size() << " samples" << std::endl;

            return paddedData;
        }

    public:
        // Constructor
        AudioEncoder(int sampleRate = 48000, int channels = 1)
            : opusEncoder(nullptr), sampleRate(sampleRate), channels(channels),
              frameSize(sampleRate / 50),              // 20ms frame
              maxFrameSize(sampleRate * channels * 2), // 1 second buffer
              maxPacketSize(maxFrameSize)
        {

            // Initialize Opus encoder
            int error;
            opusEncoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &error);
            if (error != OPUS_OK || opusEncoder == nullptr)
            {
                std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                return;
            }

            // Set bitrate to 24 kbps (good for speech)
            opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(24000));

            // Enable DTX (Discontinuous Transmission) for better speech encoding
            opus_encoder_ctl(opusEncoder, OPUS_SET_DTX(1));

            // Set complexity to 10 (highest) for better quality
            opus_encoder_ctl(opusEncoder, OPUS_SET_COMPLEXITY(10));

            // Initialize Ogg stream
            initOggStream();

            // Write Ogg header
            writeOggHeader();

            // Flush header pages
            flushOggPages();
        }

        // Destructor
        ~AudioEncoder()
        {
            if (opusEncoder)
            {
                opus_encoder_destroy(opusEncoder);
                opusEncoder = nullptr;
            }
            ogg_stream_clear(&os);
        }

        // Encode PCM data to Opus OGG
        bool encode(const std::vector<int16_t> &pcmData)
        {
            if (!opusEncoder)
            {
                std::cerr << "Encoder not initialized" << std::endl;
                return false;
            }

            if (pcmData.empty())
            {
                std::cerr << "No PCM data to encode" << std::endl;
                return false;
            }

            std::cout << "Starting audio processing pipeline..." << std::endl;
            std::cout << "Input PCM data size: " << pcmData.size() << " samples" << std::endl;

            // Bước 1: Loại bỏ nhiễu
            //std::vector<int16_t> denoisedData = simpleNoiseReduction(pcmData);
            
            // Bước 2: Loại bỏ khoảng lặng
            //std::vector<int16_t> trimmedData = simpleTrimSilence(denoisedData);
            
            // Bước 3: Đảm bảo dữ liệu có đủ frame hoàn chỉnh
            std::vector<int16_t> paddedData = padAudio(pcmData);

            // Calculate number of frames
            int numFrames = paddedData.size() / (channels * frameSize);

            std::cout << "Encoding " << numFrames << " frames of audio" << std::endl;

            // Process each frame
            for (int i = 0; i < numFrames; i++)
            {
                // Encode frame
                unsigned char packet[maxPacketSize];
                int packetSize = opus_encode(
                    opusEncoder,
                    &paddedData[i * channels * frameSize],
                    frameSize,
                    packet,
                    maxPacketSize);

                if (packetSize < 0)
                {
                    std::cerr << "Failed to encode frame: " << opus_strerror(packetSize) << std::endl;
                    return false;
                }

                // Create Ogg packet
                op.packet = packet;
                op.bytes = packetSize;
                op.b_o_s = 0;
                op.e_o_s = (i == numFrames - 1) ? 1 : 0; // End of stream for last packet
                op.granulepos = (i + 1) * frameSize;
                op.packetno = packetNo++;

                // Add packet to Ogg stream
                if (ogg_stream_packetin(&os, &op) != 0)
                {
                    std::cerr << "Failed to add audio packet to Ogg stream" << std::endl;
                    return false;
                }

                // Flush pages
                flushOggPages();
            }

            // Flush end pages if this is the end of the stream
            flushOggPages(true);

            std::cout << "Encoding completed. Total encoded data size: " << encodedData.size() << " bytes" << std::endl;

            return true;
        }

        // Encode PCM data from raw buffer
        bool encodeFromBuffer(const void *buffer, size_t bufferSize)
        {
            // Convert buffer to int16_t vector
            const int16_t *pcmBuffer = static_cast<const int16_t *>(buffer);
            size_t numSamples = bufferSize / sizeof(int16_t);
            std::vector<int16_t> pcmData(pcmBuffer, pcmBuffer + numSamples);

            return encode(pcmData);
        }

        // Save encoded data to file
        bool saveToFile(const std::string &filePath)
        {
            std::ofstream file(filePath, std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open file for writing: " << filePath << std::endl;
                return false;
            }

            file.write(reinterpret_cast<const char *>(encodedData.data()), encodedData.size());
            file.close();

            return true;
        }

        // Get encoded data
        const std::vector<uint8_t> &getEncodedData() const
        {
            return encodedData;
        }

        // Clear encoded data
        void clearEncodedData()
        {
            encodedData.clear();

            // Reset Ogg stream
            ogg_stream_clear(&os);
            initOggStream();

            // Write new Ogg header
            writeOggHeader();

            // Flush header pages
            flushOggPages();
        }
    };

} // namespace TechMaster