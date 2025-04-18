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
class KaraokeFactory
{
public:
    static std::unique_ptr<Karaoke> createKaraoke()
    {
        return std::make_unique<Karaoke>();
    }

    static void startKaraoke(const char *melody, const char *lyric)
    {
        auto karaoke = createKaraoke();

        if (!karaoke->initialize())
        {
            cerr << "Không thể khởi tạo Karaoke!" << endl;
            return;
        }
        // Phát âm thanh trực tiếp từ mic
        cout << "Bắt đầu phát âm thanh trực tiếp từ microphone..." << endl;

        if (!karaoke->startRecording())
        {
            return;
        }

        // Thiết lập âm lượng mặc định cao hơn
        karaoke->setMicVolume(5.0f);
        cout << "Âm lượng mic mặc định: 5.0 (500%)" << endl;

        if (!karaoke->startLivePlayback())
        {
            cerr << "Không thể bắt đầu phát trực tiếp!" << endl;
            return;
        }

        OggPlay::getInstance()->playMultipleOggFiles({melody, lyric});

        LOGI("Melody file: %s", melody);
        LOGI("Lyric file: %s", lyric);

        // Phát nhạc nền với âm lượng 50%
        cout << "Âm lượng nhạc nền: 50%" << endl;
        while (true)
        {
            cout << "Đang ghi âm" << endl;
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
};
