#include "karaoke_factory.hpp"

#if defined(__APPLE__)
#include "ios_karaoke.hpp"
#include "ios_mic_player.hpp"
#elif defined(__ANDROID__)
#include "android_karaoke.hpp"
#include "android_mic_player.hpp"
#elif defined(_WIN32) || defined(__linux__)
#include "port_audio_layer.hpp"
#else
#error "Platform not supported"
#endif

#include "ogg_play.hpp"
#include <iostream>
#include <thread>
#include <chrono>

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

// Define the static member variable
std::unique_ptr<Karaoke> KaraokeFactory::karaoke = nullptr;

std::unique_ptr<Karaoke> KaraokeFactory::createKaraoke()
{
    return std::make_unique<Karaoke>();
}

void KaraokeFactory::startKaraoke(const char *melody, const char *lyric)
{
    karaoke = createKaraoke();

    if (!karaoke->initialize())
    {
        LOGE("Không thể khởi tạo Karaoke!");
        return;
    }
    
    // Phát âm thanh trực tiếp từ mic
    LOGI("Bắt đầu phát âm thanh trực tiếp từ microphone...");

    if (!karaoke->startRecording())
    {
        LOGE("Không thể bắt đầu ghi âm!");
        return;
    }

    // Thiết lập âm lượng mặc định cao hơn
    karaoke->setMicVolume(10.0f);
    LOGI("Âm lượng mic mặc định: 10.0 (1000%)");

    if (!karaoke->startLivePlayback())
    {
        LOGE("Không thể bắt đầu phát trực tiếp!");
        return;
    }

    // Melody and lyric files
    LOGI("Melody file: %s", melody);
    LOGI("Lyric file: %s", lyric);

    // Log thông báo
    LOGI("Âm lượng nhạc nền: 50%");
    
    // Loop until interrupted
    while (true)
    {
        LOGI("Đang ghi âm và phát...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void KaraokeFactory::stopKaraoke()
{
    if (karaoke)
    {
        karaoke->stopRecording();
        karaoke->stopLivePlayback();
        LOGI("Đã dừng karaoke");
    }
    else
    {
        LOGW("Không có phiên karaoke nào đang chạy");
    }
}