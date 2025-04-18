#pragma once

#include <cmath>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <ogg/ogg.h>
#include <mutex>
#include <rubberband/RubberBandStretcher.h>

#include "audio_player_types.hpp"
#include "ring_buffer.hpp"
#include "error_code.hpp"
#include "audio_layer.hpp"
#include "opus_types.hpp"

#if defined(__ANDROID__)
    #include <opus.h>
#else
    #include <opus/opus.h>
#endif

using namespace std;

class AudioPlayer;
class RingBuffer;

//Thông tin để quản lý việc play at, durtion, seek, loop, pause
struct PlayBackTiming {
    uint8_t totalLoop{0};       //Tổng số lần sẽ phát lặp lại
    uint8_t currentLoop{0};       //Số lần đã phát lặp lại
    uint32_t seekTime{0};       //Thời gian điểm muốn seek đến để phát âm thanh, tính từ đầu file âm thanh
    ogg_int64_t target_pcm_pos{0}; // Vị trí cần seek đơn vị mẫu âm thanh để phát âm thanh. Tính được từ seekTime


    uint32_t duration{0};       //Thời lượng phát âm thanh
    uint32_t pauseTime{0};      //Thời gian đã tạm dừng phát âm thanh
    uint32_t endTime{0};        //Thời gian khi phát hết âm thanh
    uint32_t elapsedTime{0};    // Thời gian đã phát tính bằng millisecond từ lúc bắt đầu phát
    uint32_t currentTime{0};    // Vị trí hiện tại đang phát tính bằng millisecond

   // uint64_t currentFilePos{0};
    uint64_t prerollFilePos{0};    // Vị trí cần seek đơn vị bytes để chuẩn bị preroll
    uint64_t prerollGranulePos{0}; // Granule position đơn vị mẫu âm thanh tại vị trí cần seek để chuẩn bị preroll
    
    //Thời điểm bắt đầu phát âm thanh
    bool hasStartTime{false};   //Đã đánh dấu startTime để đo elapsedTime chưa
    chrono::time_point<chrono::steady_clock> startTime;

    double speed{1.0};                    // Tốc độ phát hiện tại
    double accumulatedTime{0.0};          // Thời gian đã tích lũy từ các lần pause/resume trước
    chrono::time_point<chrono::steady_clock> speedChangeTime; // Thời điểm thay đổi speed gần nhất
};

/*
    AudioSession là lớp quản lý việc phát âm thanh của một file opus.ogg cụ thể
    Nó quản lý việc phát âm thanh, tạm dừng, tiếp tục, tạm dừng, đặt lại, đóng file âm thanh
    Nó cũng quản lý việc seek, loop, pause, resume, stop
    Nó cũng quản lý việc đọc file âm thanh, phân tích header, decode, mix, output
*/
class AudioSession {
public:
    static constexpr uint32_t SAMPLE_RATE = 48000;
    static constexpr size_t FRAME_SIZE = 960; // Opus frame size
    static constexpr size_t MAX_FRAME_SIZE = 6*960; // Max opus frame size
    static constexpr size_t OGG_BUFFER_SIZE = 2*8192;
    static constexpr size_t RING_BUFFER_SIZE = (FRAME_SIZE * 8);
    
    // Constructor & Destructor
    explicit AudioSession(AudioPlayer* player);
    ~AudioSession();

    // Disable copy
    AudioSession(const AudioSession&) = delete;
    AudioSession& operator=(const AudioSession&) = delete;

    // File Operations
    Result loadFile(const string& fileName);
    void printOggInfo() const;
    Result destroy();
    
    // Playback Control
    Result playAt(uint32_t seekTime = 0, uint32_t duration = 0, int loop = 1);
    Result seekToTime(uint32_t timeMs);
    void stop();
    void pause();
    void resume();
    void release();
    void reset();
    
    // Volume Control
    void setVolume(float volume);
    
    // State & Info Access
    PlayState getState() const { return state; }
    uint32_t getCurrentTime() const { return timing.currentTime; }

    const string& getFileName() const { return fileName; }
    const uint32_t getDuration() const { return oggFile->file_duration; }
    // Audio Processing Callbacks;
    void setPlaybackCallback(PlaybackCallback callback);

    // Thêm setter cho StateChangeCallback
    void setStateChangeCallback(StateChangeCallback callback);

    // Ogg/Opus methods
    Result parseOpusHeader(ogg_packet* op);
    Result initOpusDecoder();

    // Ogg/Opus analyze
    Result printOggPageInfo(const string& fileName);
    Result analyzeOggFile(const string &fileName, bool detailed_packets = false);

    double getPlaybackSpeed();
    Result setPlaybackSpeed(double speed);

private:
    AudioPlayer* player;
    // File Management & Metadata
    string fileName;      // Opus file name
    unique_ptr<OggOpusFile> oggFile;  // Opus file handle
    PlayBackTiming timing;  //Quản lý thời điểm, thời lượng, số lần phát

    // State & Info Access
    atomic<PlayState> state{PlayState::IDLE};

    // Audio Buffer & Mixing
    unique_ptr<RingBuffer> buffer;
    unique_ptr<float[]> pcmBuffer; // Temporary buffer for decoded PCM data
    int mixerBusId = -1;
    float volume = 1.0f;
 
    // Callback cập nhật ứng dụng gọi
    PlaybackCallback playbackCallback;
    void initResample();
    void cleanupResample();

    // Đầu vào ra dạng mono
    Result resampleRubberBand(size_t input_frames, size_t output_frames,
                             const float* in, float* out);

    // AudioCallBack
    size_t audioCallbackOgg(float* pcm_to_speaker, size_t frames);
    // Callback
    StateChangeCallback stateChangeCallback;
    
    // Private Methods
    Result acquireInputBus();
    Result setState(PlayState newState);
    Result seekBeginOfFile();
    Result decodeAndResample(
    const unsigned char* packet,
    int bytes,
    int skipSamples,
    int maxSamples,
    bool applySpeed);
    Result preroll_decode(ogg_int64_t target_pcm_pos, ogg_int64_t preroll_granulepos);
    OggPageStartPos findPageStartPos(OggOpusFile *opusFile, ogg_int64_t position);
    Result preroll_seek(int64_t prerollFilePos, int64_t prerollGranulePos, int64_t target_pcm_pos);
    Result fillBuffer();

    RubberBand::RubberBandStretcher* rubberBand{nullptr};
}; 