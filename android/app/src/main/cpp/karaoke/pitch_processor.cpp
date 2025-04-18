#include "pitch_processor.hpp"
#include <iostream>
#include <algorithm>

PitchProcessor::PitchProcessor() {
    // Khởi tạo Rubber Band với các thông số tối ưu cho xử lý thời gian thực
    RubberBand::RubberBandStretcher::Options options = 
        RubberBand::RubberBandStretcher::OptionProcessRealTime |
        RubberBand::RubberBandStretcher::OptionPitchHighQuality |
        RubberBand::RubberBandStretcher::OptionChannelsTogether;
    
    stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        44100,  // Sample rate
        1,      // Channels (mono)
        options
    );
    
    timeRatio = 1.0;  // Không thay đổi tốc độ
    pitchScale = std::pow(2.0, 0.0 / 12.0); // Mặc định tăng 5 cung
    
    // Cập nhật thông số cho Rubber Band
    stretcher->setPitchScale(pitchScale);
    stretcher->setTimeRatio(timeRatio);
    
    // Cấu hình các thông số xử lý
    stretcher->setMaxProcessSize(1024);
    stretcher->setExpectedInputDuration(0);
    
    std::cout << "PitchProcessor initialized with pitch shift: 5.0 semitones" << std::endl;
}

PitchProcessor::~PitchProcessor() = default;

void PitchProcessor::setPitchShift(float semitones) {
    // Chuyển đổi semitones thành tỷ lệ pitch
    pitchScale = std::pow(2.0, semitones / 12.0);
    
    // Giới hạn phạm vi thay đổi pitch
    pitchScale = std::max(0.5, std::min(2.0, pitchScale));
    
    // Cập nhật thông số cho Rubber Band
    stretcher->setPitchScale(pitchScale);
    stretcher->setTimeRatio(timeRatio);
    
    std::cout << "Setting pitch scale to: " << pitchScale << " (semitones: " << semitones << ")" << std::endl;
}

std::vector<float> PitchProcessor::process(const float* input, size_t numSamples) {
    if (numSamples == 0) {
        return std::vector<float>();
    }

    // Chuẩn bị buffer đầu vào
    inputBuffer.assign(input, input + numSamples);
    
    // Tạo mảng con trỏ cho Rubber Band
    const float* inputPtrs[1] = { inputBuffer.data() };
    
    // Xử lý âm thanh
    stretcher->process(inputPtrs, numSamples, false);
    
    // Lấy kết quả đã xử lý
    outputBuffer.clear();
    outputBuffer.reserve(numSamples);
    
    // Lấy các mẫu đã xử lý
    size_t available = stretcher->available();
    if (available > 0) {
        outputBuffer.resize(available);
        float* outputPtrs[1] = { outputBuffer.data() };
        stretcher->retrieve(outputPtrs, available);
    }
    
    // Nếu không có mẫu nào được xử lý, trả về dữ liệu gốc
    if (outputBuffer.empty()) {
        outputBuffer.assign(input, input + numSamples);
    }
    
    return outputBuffer;
} 