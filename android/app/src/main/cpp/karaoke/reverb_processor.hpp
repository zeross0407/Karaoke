#pragma once

#include <vector>
#include <memory>

class ReverbProcessor {
public:
    ReverbProcessor();
    ~ReverbProcessor();

    // Process audio data and return processed samples
    std::vector<float> process(const float* input, size_t numSamples);

    // Set reverb parameters
    void setRoomSize(float size);    // 0.0 to 1.0
    void setDamping(float damping);  // 0.0 to 1.0
    void setWetMix(float wet);       // 0.0 to 1.0
    void setDryMix(float dry);       // 0.0 to 1.0
    void setEnabled(bool enable);    // Enable/disable reverb

private:
    static constexpr int NUM_DELAY_LINES = 6;
    
    struct DelayLine {
        std::vector<float> buffer;
        size_t writeIndex;
        float feedback;
        float gain;
        float lastSample;  // For smoothing between samples
    };
    
    std::vector<DelayLine> delayLines;
    std::vector<float> filterBuffer;  // Buffer for low-pass filter
    
    float roomSize;
    float damping;
    float wetMix;
    float dryMix;
    bool enabled;
    
    // Low-pass filter implementation
    float lowPassFilter(float input, float coeff, int channel);
    
    // Process a single delay line
    void processDelayLine(DelayLine& line, const float* input, float* output, size_t numSamples);
    
    // Initialize delay lines
    void initializeDelayLines();
}; 