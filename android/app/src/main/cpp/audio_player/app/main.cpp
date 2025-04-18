#include "../audioplayer/error_code.hpp"
#include "ogg_play.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <vector>
#include <condition_variable>
#include <unordered_map>
#include <functional>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AudioPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOG_TAG "AudioPlayer"
#define LOGD(...)        \
    printf(__VA_ARGS__); \
    printf("\n")
#define LOGI(...)        \
    printf(__VA_ARGS__); \
    printf("\n")
#define LOGE(...)                 \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n")
#endif

//////////////////////////////////////////////////////////////////////////////////////////

// Định nghĩa kiểu callback từ C++ sang Dart
typedef void (*DartPlaybackCallback)(int currentTime, int elapsedTime, int duration);
typedef void (*DartMultiPlaybackCallback)(int sessionId, int currentTime, int elapsedTime, int duration);
DartPlaybackCallback dart_callback = nullptr;
DartMultiPlaybackCallback dart_multi_callback = nullptr;
std::string current_file_path;
bool player_initialized = false;
AudioPlayer *player = nullptr;
AudioSession *current_session = nullptr;

// Thêm: Quản lý nhiều session audio
constexpr int MAX_MULTI_SESSIONS = 8;
std::unordered_map<int, AudioSession*> multi_sessions;
std::unordered_map<AudioSession*, int> session_to_id;
int next_session_id = 1;

// Hàng đợi thông báo để đảm bảo callback được gọi từ main thread
struct PlaybackNotification
{
    uint32_t current_time;
    uint32_t elapsed_time;
    uint32_t duration;
};

// Thêm: Thông báo cho multi-session với session ID
struct MultiPlaybackNotification
{
    int session_id;
    uint32_t current_time;
    uint32_t elapsed_time;
    uint32_t duration;
};

std::queue<PlaybackNotification> notification_queue;
// Thêm: Hàng đợi thông báo riêng cho multi-session
std::queue<MultiPlaybackNotification> multi_notification_queue;
std::mutex queue_mutex;
std::condition_variable queue_condition;
bool has_pending_notifications = false;
bool has_pending_multi_notifications = false;

// Hàm callback đơn giản để in thông tin phát lại và thêm vào hàng đợi thông báo
void playbackCallback(const PlaybackInfo &info)
{
    // In thông tin ra console
    // LOGI("Playback info: current=%u, elapsed=%u, duration=%u",
    //      info.current_time, info.elapsed_time, info.duration);

    // Kiểm tra nếu đã phát hết file (current_time >= duration)
    if (info.duration > 0 && info.current_time >= info.duration)
    {
        current_session->stop();
        return;
    }

    // Thêm thông báo vào hàng đợi để main thread xử lý
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex);
        notification_queue.push({info.current_time, info.elapsed_time, info.duration});
        has_pending_notifications = true;
    }
    queue_condition.notify_one();
}

// Thêm: Callback cho multi-session
void multiPlaybackCallback(const PlaybackInfo &info, AudioSession* session)
{
    // Tìm session ID từ session
    int session_id = -1;
    {
        auto it = session_to_id.find(session);
        if (it != session_to_id.end()) {
            session_id = it->second;
        } else {
            LOGE("Session không tìm thấy trong session_to_id map");
            return;
        }
    }

    // Kiểm tra nếu đã phát hết file (current_time >= duration)
    if (info.duration > 0 && info.current_time >= info.duration)
    {
        session->stop();
        return;
    }

    // Thêm thông báo vào hàng đợi để main thread xử lý
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex);
        multi_notification_queue.push({session_id, info.current_time, info.elapsed_time, info.duration});
        has_pending_multi_notifications = true;
    }
    queue_condition.notify_one();
}

// Wrapper để cung cấp callback cho từng session
std::function<void(const PlaybackInfo&)> createSessionCallback(AudioSession* session)
{
    return [session](const PlaybackInfo& info) {
        multiPlaybackCallback(info, session);
    };
}

// Hàm xử lý thông báo trên main thread
void process_notifications()
{
    std::vector<PlaybackNotification> notifications_to_process;
    std::vector<MultiPlaybackNotification> multi_notifications_to_process;

    // Lấy tất cả thông báo từ hàng đợi
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (has_pending_notifications)
        {
            while (!notification_queue.empty())
            {
                notifications_to_process.push_back(notification_queue.front());
                notification_queue.pop();
            }
            has_pending_notifications = false;
        }

        // Thêm: Xử lý thông báo multi-session
        if (has_pending_multi_notifications)
        {
            while (!multi_notification_queue.empty())
            {
                multi_notifications_to_process.push_back(multi_notification_queue.front());
                multi_notification_queue.pop();
            }
            has_pending_multi_notifications = false;
        }
    }

    // Xử lý các thông báo (gọi callback về Dart)
    if (dart_callback != nullptr)
    {
        for (const auto &notification : notifications_to_process)
        {
            dart_callback(static_cast<int>(notification.current_time),
                          static_cast<int>(notification.elapsed_time),
                          static_cast<int>(notification.duration));
        }
    }

    // Thêm: Xử lý thông báo multi-session
    if (dart_multi_callback != nullptr)
    {
        for (const auto &notification : multi_notifications_to_process)
        {
            dart_multi_callback(notification.session_id,
                               static_cast<int>(notification.current_time),
                               static_cast<int>(notification.elapsed_time),
                               static_cast<int>(notification.duration));
        }
    }
}

// Thêm hàm mới này
void clear_notification_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!notification_queue.empty()) {
        notification_queue.pop();
    }
    has_pending_notifications = false;
}

// Thêm: Xóa notification cho multi-session cụ thể
void clear_multi_notification_queue(int session_id) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    std::queue<MultiPlaybackNotification> temp_queue;
    while (!multi_notification_queue.empty()) {
        auto notification = multi_notification_queue.front();
        multi_notification_queue.pop();
        // Giữ lại các thông báo của session khác
        if (notification.session_id != session_id) {
            temp_queue.push(notification);
        }
    }
    // Khôi phục hàng đợi
    multi_notification_queue = std::move(temp_queue);
    // Flag vẫn giữ true nếu còn thông báo
    has_pending_multi_notifications = !multi_notification_queue.empty();
}

extern "C"
{
    // Hàm đăng ký callback từ Dart
    void register_dart_callback(DartPlaybackCallback callback)
    {
        dart_callback = callback;
        LOGI("Dart callback registered");
    }

    // Thêm: Đăng ký callback cho multi-session
    void register_multi_dart_callback(DartMultiPlaybackCallback callback)
    {
        dart_multi_callback = callback;
        LOGI("Multi-session dart callback registered");
    }

    // Hàm này được gọi từ main thread của Dart để xử lý các thông báo đang chờ
    void process_pending_notifications()
    {
        process_notifications();
    }

    bool init_player()
    {
        if (!player_initialized)
        {
            player = AudioPlayer::getInstance();
            Result result = player->init();
            if (!result.isSuccess())
            {
                LOGE("Failed to initialize AudioPlayer");
                return false;
            }
            player_initialized = true;
        }
        return true;
    }

    // Hàm phát âm thanh
    bool play_audio(const char *filePath)
    {
        if (!player_initialized)
            return false;
        // Dọn dẹp session cũ nếu có
        if (current_session != nullptr)
        {
            player->destroySession(current_session);
            current_session = nullptr;
        }

        // Chuyển đổi đường dẫn file
        std::string fileName(filePath);
        current_file_path = fileName;
        LOGI("Playing audio file: %s", fileName.c_str());

        // Phát file trực tiếp bằng AudioPlayer và truyền callback
        PlayOggResult result = player->playOggAt(fileName, 0, 0, 1, playbackCallback); // Phát từ đầu đến hết với callback

        if (result.result.isSuccess())
        {
            current_session = result.session;
            LOGI("Starting playback with time 0");
        }
        else
        {
            // Sử dụng thông báo lỗi cố định
            fprintf(stderr, "Failed to play audio file\n");
        }

        return true;
    }

    // Thêm: Hàm phát âm thanh đa luồng, trả về ID của session
    int play_multi_audio(const char *filePath)
    {
        if (!player_initialized) {
            return -1;
        }

        // Kiểm tra số lượng session, đảm bảo không vượt quá giới hạn
        if (multi_sessions.size() >= MAX_MULTI_SESSIONS) {
            LOGE("Maximum number of sessions reached (%d)", MAX_MULTI_SESSIONS);
            return -1;
        }

        std::string fileName(filePath);
        LOGI("Playing multi audio file: %s with session ID %d", fileName.c_str(), next_session_id);

        // Lưu lại ID hiện tại và tăng biến toàn cục
        int current_session_id = next_session_id++;
        
        // Tạo callback riêng cho session này
        auto callback = [current_session_id](const PlaybackInfo& info) {
            // Tìm session từ ID
            auto it = multi_sessions.find(current_session_id);
            if (it != multi_sessions.end()) {
                multiPlaybackCallback(info, it->second);
            }
        };

        // Phát file với callback
        PlayOggResult result = player->playOggAt(fileName, 0, 0, 1, callback);

        if (result.result.isSuccess()) {
            AudioSession* session = result.session;
            multi_sessions[current_session_id] = session;
            session_to_id[session] = current_session_id;
            LOGI("Starting multi playback with session ID %d", current_session_id);
            
            return current_session_id;
        } else {
            LOGE("Failed to play multi audio file: %s", fileName.c_str());
            return -1;
        }
    }

    // Thêm: Dừng phát âm thanh cho session cụ thể
    bool stop_multi(int sessionId)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Stopping audio playback for session %d", sessionId);
            session->stop();
            clear_multi_notification_queue(sessionId);
            return true;
        }
        LOGE("Session %d not found", sessionId);
        return false;
    }

    // Thêm: Tạm dừng phát âm thanh cho session cụ thể
    bool pause_multi_audio(int sessionId)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Pausing audio playback for session %d", sessionId);
            session->pause();
            return true;
        }
        LOGE("Session %d not found", sessionId);
        return false;
    }

    // Thêm: Tiếp tục phát âm thanh cho session cụ thể
    bool resume_multi(int sessionId)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Resuming audio playback for session %d", sessionId);
            session->resume();
            return true;
        }
        LOGE("Session %d not found", sessionId);
        return false;
    }

    // Thêm: Thay đổi tốc độ cho session cụ thể
    double change_multi_speed(int sessionId, double newSpeed)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Changing speed for session %d to %f", sessionId, newSpeed);
            
            // Giới hạn tốc độ trong khoảng hợp lệ
            newSpeed = min(2.5, max(0.5, newSpeed));
            
            // Áp dụng tốc độ mới
            auto result = session->setPlaybackSpeed(newSpeed);
            return newSpeed;
        }
        LOGE("Session %d not found", sessionId);
        return -1.0;
    }

    // Thêm: Seek đến vị trí cụ thể trong file cho session
    bool seek_multi(int sessionId, int timeMs)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Seeking to %d ms for session %d", timeMs, sessionId);
            
            // Xóa notification queue cho session này
            clear_multi_notification_queue(sessionId);
            
            // Seek và resume
            session->seekToTime(timeMs);
            session->resume();
            return true;
        }
        LOGE("Session %d not found", sessionId);
        return false;
    }

    // Thêm: Giải phóng một session cụ thể
    bool release_multi_session(int sessionId)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            LOGI("Releasing session %d", sessionId);
            
            // Xóa khỏi map session_to_id
            session_to_id.erase(session);
            
            // Xóa notification cho session này
            clear_multi_notification_queue(sessionId);
            
            // Giải phóng session
            player->destroySession(session);
            
            // Xóa khỏi map multi_sessions
            multi_sessions.erase(sessionId);
            return true;
        }
        LOGE("Session %d not found", sessionId);
        return false;
    }

    // Thêm: Số lượng session hiện tại
    int get_active_session_count()
    {
        return multi_sessions.size();
    }

    // Thêm: Lấy thời lượng file cho session cụ thể
    int get_multi_duration(int sessionId)
    {
        auto it = multi_sessions.find(sessionId);
        if (it != multi_sessions.end()) {
            AudioSession* session = it->second;
            return session->getDuration();
        }
        return -1;
    }

    // Hàm dừng phát âm thanh
    void stop()
    {
        if (current_session != nullptr)
        {
            LOGI("Stopping audio playback");
            current_session->stop();
            // auto result = current_session->setPlaybackSpeed(2);
        }
    }
    void pause_audio()
    {
        if (current_session != nullptr)
        {
            current_session->pause();
        }
    }
    void resume()
    {
        if (current_session != nullptr)
        {
            current_session->resume();
        }
    }
    double change_speed(double newSpeed)
    {
        if (current_session != nullptr)
        {
            double currentSpeed = current_session->getPlaybackSpeed();
            
            // Giới hạn tốc độ trong khoảng hợp lệ
            newSpeed = min(2.5, max(0.5, newSpeed));
            
            // Luôn đặt tốc độ mới, ngay cả khi đã ở giới hạn
            auto result = current_session->setPlaybackSpeed(newSpeed);
            
            return newSpeed;
        }
        return -1.0;
    }
    void seek(int time)
    {
        if (current_session != nullptr)
        {
            // Xóa notification queue trước khi seek
            clear_notification_queue();
            current_session->seekToTime(time);
            current_session->resume();
            return;
        }
    }
}