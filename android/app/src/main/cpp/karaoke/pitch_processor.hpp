#pragma once

#include <vector>
#include <memory>
#include <rubberband/RubberBandStretcher.h>

class PitchProcessor {
public:
    PitchProcessor();
    ~PitchProcessor();

    // Process audio data and return processed samples
    std::vector<float> process(const float* input, size_t numSamples);

    // Set pitch shift in semitones
    void setPitchShift(float semitones);

private:
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    double timeRatio;
    double pitchScale;
}; 