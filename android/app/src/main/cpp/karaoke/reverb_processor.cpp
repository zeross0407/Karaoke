#include "reverb_processor.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

ReverbProcessor::ReverbProcessor() {
    // Default parameters
    roomSize = 0.5f;
    damping = 0.5f;
    wetMix = 0.33f;
    dryMix = 0.7f;
    enabled = true;
    
    // Initialize delay lines
    initializeDelayLines();
}

ReverbProcessor::~ReverbProcessor() {
    // Clean up
}

void ReverbProcessor::initializeDelayLines() {
    // Sử dụng nhiều delay line hơn với các khoảng thời gian khác nhau
    // để tạo ra âm thanh phản xạ tự nhiên hơn
    delayLines.resize(NUM_DELAY_LINES);
    
    // Các giá trị delay dựa trên các phản xạ trong một căn phòng thực tế
    // Độ trễ dài hơn cho phòng lớn hơn và nghe tự nhiên hơn
    const float delays[NUM_DELAY_LINES] = {
        0.0437f,  // ~21ms
        0.0581f,  // ~28ms
        0.0671f,  // ~32ms
        0.0829f,  // ~40ms
        0.0991f,  // ~48ms
        0.1139f   // ~55ms
    };
    
    // Giảm feedback và gain để tránh tiếng rè
    const float feedbacks[NUM_DELAY_LINES] = {
        0.30f, 0.28f, 0.26f, 0.24f, 0.22f, 0.20f
    };
    
    const float gains[NUM_DELAY_LINES] = {
        0.60f, 0.55f, 0.50f, 0.45f, 0.40f, 0.35f
    };
    
    // Tạo các delay line với thời gian delay khác nhau
    for (int i = 0; i < NUM_DELAY_LINES; ++i) {
        auto& line = delayLines[i];
        size_t delaySamples = static_cast<size_t>(delays[i] * 48000); // 48kHz sample rate
        
        // Đảm bảo buffer đủ lớn
        line.buffer.resize(delaySamples, 0.0f);
        line.writeIndex = 0;
        line.feedback = feedbacks[i];
        line.gain = gains[i];
        line.lastSample = 0.0f; // Khởi tạo mẫu cuối cùng
    }
    
    // Khởi tạo buffer cho low-pass filter
    filterBuffer.resize(2, 0.0f);
}

void ReverbProcessor::setRoomSize(float size) {
    roomSize = std::clamp(size, 0.0f, 1.0f);
    
    // Điều chỉnh feedback dựa trên kích thước phòng 
    // Phòng lớn hơn = echo lâu tàn hơn
    for (int i = 0; i < NUM_DELAY_LINES; ++i) {
        // Tăng feedback cho phòng lớn
        delayLines[i].feedback = 0.30f + (0.5f * roomSize);
    }
}

void ReverbProcessor::setDamping(float value) {
    damping = std::clamp(value, 0.0f, 1.0f);
    // Damping cao hơn = lọc low-pass nhiều hơn, âm thanh mềm hơn
}

void ReverbProcessor::setWetMix(float wet) {
    wetMix = std::clamp(wet, 0.0f, 1.0f);
}

void ReverbProcessor::setDryMix(float dry) {
    dryMix = std::clamp(dry, 0.0f, 1.0f);
}

void ReverbProcessor::setEnabled(bool enable) {
    enabled = enable;
    
    // Reset các delay line khi bật lại
    if (enable) {
        for (auto& line : delayLines) {
            std::fill(line.buffer.begin(), line.buffer.end(), 0.0f);
            line.writeIndex = 0;
            line.lastSample = 0.0f;
        }
        
        std::fill(filterBuffer.begin(), filterBuffer.end(), 0.0f);
    }
}

float ReverbProcessor::lowPassFilter(float input, float coeff, int channel) {
    // Áp dụng low-pass filter với hệ số cao hơn để lọc mạnh hơn
    float output = input * (1.0f - coeff) + filterBuffer[channel] * coeff;
    filterBuffer[channel] = output;
    return output;
}

void ReverbProcessor::processDelayLine(DelayLine& line, const float* input, float* output, size_t numSamples) {
    const size_t bufferSize = line.buffer.size();
    const float dampCoeff = damping * 0.5f; // Hệ số damping
    
    for (size_t i = 0; i < numSamples; ++i) {
        // Tính toán vị trí đọc với wrap-around
        size_t readIndex = (line.writeIndex + bufferSize - 1) % bufferSize;
        
        // Lấy mẫu delay
        float delayedSample = line.buffer[readIndex];
        
        // Áp dụng low-pass filter (damping)
        delayedSample = lowPassFilter(delayedSample, dampCoeff, 0);
        
        // Áp dụng feedback và gain
        float feedbackSample = delayedSample * line.feedback;
        
        // Kết hợp đầu vào với feedback
        float processedSample = input[i] + feedbackSample;
        
        // Hạn chế để tránh quá tải
        processedSample = std::clamp(processedSample, -1.0f, 1.0f);
        
        // Áp dụng gain
        processedSample *= line.gain;
        
        // Lưu vào delay buffer
        line.buffer[line.writeIndex] = processedSample;
        
        // Kết hợp đầu vào với đầu ra
        output[i] = input[i] * dryMix + delayedSample * wetMix;
        
        // Cập nhật vị trí ghi với wrap-around
        line.writeIndex = (line.writeIndex + 1) % bufferSize;
    }
}

std::vector<float> ReverbProcessor::process(const float* input, size_t numSamples) {
    if (!enabled || numSamples == 0) {
        // Nếu reverb bị tắt, trả về bản sao của đầu vào
        return std::vector<float>(input, input + numSamples);
    }
    
    std::vector<float> output(numSamples, 0.0f);
    std::vector<float> tempBuffer(numSamples);
    
    // Sao chép đầu vào vào đầu ra với dry mix
    for (size_t i = 0; i < numSamples; ++i) {
        output[i] = input[i] * dryMix;
    }
    
    // Xử lý qua tất cả các delay line
    for (auto& line : delayLines) {
        // Xử lý delay line
        std::fill(tempBuffer.begin(), tempBuffer.end(), 0.0f);
        
        // Tính delay line
        for (size_t i = 0; i < numSamples; ++i) {
            const size_t bufferSize = line.buffer.size();
            size_t readIndex = line.writeIndex;
            
            // Lấy mẫu delay
            float delayedSample = line.buffer[readIndex];
            
            // Áp dụng low-pass filter mạnh hơn để giảm rè
            float dampCoeff = damping * 0.85f;
            delayedSample = delayedSample * (1.0f - dampCoeff) + line.lastSample * dampCoeff;
            line.lastSample = delayedSample;
            
            // Lưu kết quả vào buffer tạm
            tempBuffer[i] = delayedSample * wetMix;
            
            // Áp dụng feedback với hệ số giảm để tránh tích lũy tiếng rè
            float newSample = input[i] + (delayedSample * line.feedback * 0.9f);
            
            // Clipping để tránh quá tải - giảm threshold xuống
            newSample = std::clamp(newSample, -0.95f, 0.95f);
            
            // Áp dụng gain
            newSample *= line.gain;
            
            // Cập nhật buffer
            line.buffer[line.writeIndex] = newSample;
            
            // Cập nhật write index
            line.writeIndex = (line.writeIndex + 1) % bufferSize;
        }
        
        // Kết hợp buffer tạm với đầu ra
        for (size_t i = 0; i < numSamples; ++i) {
            output[i] += tempBuffer[i];
        }
    }
    
    // Giới hạn đầu ra để tránh clipping và thêm một lớp lọc nhẹ cuối cùng
    for (size_t i = 0; i < numSamples; ++i) {
        // Soft clipping để tránh biến dạng
        if (output[i] > 0.8f) {
            output[i] = 0.8f + (output[i] - 0.8f) * 0.5f;
        } else if (output[i] < -0.8f) {
            output[i] = -0.8f + (output[i] + 0.8f) * 0.5f;
        }
        
        // Giới hạn cuối cùng
        output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }
    
    return output;
} 