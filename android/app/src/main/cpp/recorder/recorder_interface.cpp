#include <cstdint>
#include <string>
#include <vector>

class RecorderInterface
{
public:
    // Recording state
    enum class RecordingState
    {
        STOPPED,
        RECORDING
    };

    virtual ~RecorderInterface() = default;

    // Initialize the recorder with sample rate
    virtual bool initialize(int sampleRate, const std::string &outputFilePath) = 0;

    // Start recording
    virtual bool startRecording() = 0;

    // Stop recording
    virtual bool stopRecording() = 0;

    // Check if currently recording
    virtual RecordingState getState() const = 0;
    
    // Get PCM data as vector of float
    virtual const std::vector<float>& getPcmData() const = 0;
    
    // Clear PCM data
    virtual void clearPcmData() = 0;
};
