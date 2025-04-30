#include "karaoke_factory.hpp"
#include "ogg_play.hpp"
#include <thread>
#include <mutex>
using namespace std;

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AudioPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOG_TAG "AudioPlayer"
#define LOGD(...)        \
    printf(__VA_ARGS__); \
    printf("\n")
#define LOGI(...)        \
    printf(__VA_ARGS__); \
    printf("\n")
#define LOGW(...)        \
    printf(__VA_ARGS__); \
    printf("\n")
#define LOGE(...)                 \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n")
#endif

// Mutex để bảo vệ tài nguyên chung
std::mutex audio_mutex;

extern "C"
{
    bool karaoke_playing = false;
    AudioPlayer *player = nullptr;
    AudioSession *melody_session = nullptr;
    AudioSession *lyric_session = nullptr;

    int get_duration()
    {
        if (lyric_session != nullptr)
            return lyric_session->getDuration();
        return 0;
    }

    void karaoke_test(const char *melody, const char *lyric)
    {
        LOGI("===== KARAOKE TECHMASTER =====");

        ///////////

        player = AudioPlayer::getInstance();
        if (!player)
        {
            LOGE("Không thể khởi tạo AudioPlayer");
        }
        player->init();

        PlayOggResult rs1 = player->playOggAt(melody);
        if (rs1.result.isSuccess())
        {
            melody_session = rs1.session;
        }

        PlayOggResult rs2 = player->playOggAt(lyric);
        lyric_session = rs2.session;
        LOGI("Durationz: %d", lyric_session->getDuration());
        if (!rs1.result.isSuccess())
        {
            LOGI("Không thể phát file");
        }
        // Thread 1: Chạy KaraokeFactory::startKaraoke
        std::thread karaoke_thread([melody, lyric]()
                                   {
            try {
                {
                    std::lock_guard<std::mutex> lock(audio_mutex);
                    LOGI("Starting karaoke");
                }
                KaraokeFactory::startKaraoke();
                LOGI("Karaoke processing completed.");
            } catch (const std::exception& e) {
                LOGE("Error in karaoke thread: %s", e.what());
            } });

        karaoke_thread.detach();
        karaoke_playing = true;
    }
    bool karaoke_pause()
    {

        if (karaoke_playing)
        {
            LOGI("Pausing karaoke");
            KaraokeFactory::stopKaraoke();
            melody_session->pause();
            lyric_session->pause();
            karaoke_playing = false;
            return true;
        }
        else
        {
            LOGI("Karaoke is not playing");
            KaraokeFactory::startKaraoke();
            melody_session->resume();
            lyric_session->resume();
            karaoke_playing = true;
            return true;
        }
        return false;
    }
}