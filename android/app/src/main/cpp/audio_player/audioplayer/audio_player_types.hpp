#pragma once
#include <functional>


enum class PlayState {
    IDLE,       // Initial state, player is not doing anything
    LOADING,    // Player is loading audio data
    READY,      // Audio is loaded and ready to play
    PLAYING,    // Audio is currently playing
    PAUSED,     // Playback is paused
    STOPPED,    // Playback is stopped (reset to beginning)
    ERROR       // An error occurred during playback/loading
}; 


struct PlaybackInfo {
    uint32_t current_time;    // Vị trí hiện tại trong file (ms)
    uint32_t elapsed_time;    // Thời gian đã phát (ms)
    uint32_t duration;        // Thời lượng file (ms)
};

struct StateChangeInfo {
    PlayState old_state;
    PlayState new_state;
    std::string reason;        // Optional: Lý do thay đổi trạng thái
    uint32_t current_time;     // Thời điểm trạng thái thay đổi (ms từ đầu file)
};

using StateChangeCallback = std::function<void(const StateChangeInfo&)>; // Callback cho sự kiện thay đổi trạng thái
using PlaybackCallback = std::function<void(const PlaybackInfo&)>; // Callback cho audio data và playback info

// Callback cho audio data
// position: kiểu int64_t để đếm số lượng mẫu đã phát
// timestamp: kiểu int64_t để lưu thời gian theo nanosecond
using AudioCallback = std::function<size_t(float* pcm_to_speaker, size_t frames)>; 