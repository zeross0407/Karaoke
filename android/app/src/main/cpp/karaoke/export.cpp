// #include "karaoke_factory.cpp"
// using namespace std;
// #ifdef __ANDROID__
// #include <android/log.h>
// #define LOG_TAG "AudioPlayer"
// #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
// #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
// #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
// #else
// #define LOG_TAG "AudioPlayer"
// #define LOGD(...)        \
//     printf(__VA_ARGS__); \
//     printf("\n")
// #define LOGI(...)        \
//     printf(__VA_ARGS__); \
//     printf("\n")
// #define LOGE(...)                 \
//     fprintf(stderr, __VA_ARGS__); \
//     fprintf(stderr, "\n")
// #endif
// extern "C"
// {

//     void karaoke_test(const char *melody, const char *lyric)
//     {
//         // Test chức năng karaoke - ghi âm
//         LOGI("===== KARAOKE TECHMASTER =====");
//         KaraokeFactory::startKaraoke(melody, lyric);
//         return;
//     }
// }
#include "karaoke_factory.cpp"
#include "ogg_play.hpp"
#include <thread> // Thêm thư viện std::thread
#include <mutex>  // Thêm thư viện std::mutex cho thread safety (nếu cần)

using namespace std;

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

// Mutex để bảo vệ tài nguyên chung (nếu cần)
std::mutex audio_mutex;

extern "C"
{
    void karaoke_test(const char *melody, const char *lyric)
    {
        LOGI("===== KARAOKE TECHMASTER =====");

        // Thread 1: Chạy KaraokeFactory::startKaraoke
        std::thread karaoke_thread([melody, lyric]()
        {
            try {
                {
                    std::lock_guard<std::mutex> lock(audio_mutex);
                    LOGI("Starting karaoke in thread ID: %lu", std::this_thread::get_id());
                }
                KaraokeFactory::startKaraoke(melody, lyric);
                LOGI("Karaoke processing completed.");
            } catch (const std::exception& e) {
                LOGE("Error in karaoke thread: %s", e.what());
            }
        });

        // Thread 2: Chạy OggPlay::playMultipleOggFiles
        std::thread ogg_thread([melody, lyric]()
        {
            try {
                {
                    std::lock_guard<std::mutex> lock(audio_mutex);
                    LOGI("Starting OggPlay in thread ID: %lu", std::this_thread::get_id());
                }
                OggPlay::getInstance()->playMultipleOggFiles({melody, lyric});
                LOGI("OggPlay processing completed.");
            } catch (const std::exception& e) {
                LOGE("Error in OggPlay thread: %s", e.what());
            }
        });

        // Detach cả hai thread để chúng chạy độc lập
        karaoke_thread.detach();
        ogg_thread.detach();
    }
}