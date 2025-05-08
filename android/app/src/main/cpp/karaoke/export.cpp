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
    // std::unique_ptr<Karaoke> karaoke = nullptr;

    void karaoke_test(const char *melody, const char *lyric)
    {
        LOGI("===== KARAOKE TECHMASTER =====");
        player = AudioPlayer::getInstance();
        player->init();
        ///////////

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
    bool set_vocal_volume(float volume)
    {
        if (lyric_session)
        {
            lyric_session->setVolume(volume);
            return true;
        }
        return false;
    }
    bool set_mic_volume(float volume)
    {
        LOGI("Setting mic volume to %f", volume);
        KaraokeFactory::setMicVolume(volume);
        return true;
    }
    bool set_melody_volume(float volume)
    {
        if (melody_session)
        {
            melody_session->setVolume(volume);
            return true;
        }
        return false;
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

    bool seek_to_time(uint32_t timeMs)
    {
        if (lyric_session && melody_session)
        {
            lyric_session->seekToTime(timeMs);
            melody_session->seekToTime(timeMs);
            return true;
        }
        return false;
    }

    double getOggOpusDurationMs(const char *path)
    {
        const int BUFFER_SIZE = 4096;
        const int SAMPLE_RATE = 48000;

        __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI", "Đọc thời lượng file: %s", path);

        FILE *fin = fopen(path, "rb");
        if (!fin)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI", "Không thể mở file: %s (errno: %d, %s)",
                                path, errno, strerror(errno));
            return -1;
        }

        // Đọc kích thước file để kiểm tra
        fseek(fin, 0, SEEK_END);
        long fileSize = ftell(fin);
        fseek(fin, 0, SEEK_SET);

        __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI", "Kích thước file: %ld bytes", fileSize);

        if (fileSize <= 0)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI", "File rỗng hoặc kích thước không hợp lệ");
            fclose(fin);
            return -2;
        }

        // Đọc và kiểm tra OGG signature
        char signature[4];
        size_t sigRead = fread(signature, 1, 4, fin);
        fseek(fin, 0, SEEK_SET); // Đặt lại vị trí đọc về đầu file

        if (sigRead != 4)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                "Không thể đọc signature (đọc được %zu bytes)", sigRead);
            fclose(fin);
            return -3;
        }

        bool isOgg = (signature[0] == 'O' && signature[1] == 'g' &&
                      signature[2] == 'g' && signature[3] == 'S');

        if (!isOgg)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                "File không có OGG signature hợp lệ: %02X %02X %02X %02X",
                                (unsigned char)signature[0], (unsigned char)signature[1],
                                (unsigned char)signature[2], (unsigned char)signature[3]);
            fclose(fin);
            return -4;
        }

        __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI", "Signature OGG hợp lệ, bắt đầu đọc Opus");

        ogg_sync_state oy;
        ogg_stream_state os;
        ogg_sync_init(&oy);

        bool headerParsed = false;
        int preskip = 0;
        ogg_int64_t lastGranulePos = -1;

        try
        {
            while (true)
            {
                char *buffer = ogg_sync_buffer(&oy, BUFFER_SIZE);
                size_t bytes = fread(buffer, 1, BUFFER_SIZE, fin);
                if (bytes == 0)
                    break;

                ogg_sync_wrote(&oy, bytes);

                ogg_page og;
                while (ogg_sync_pageout(&oy, &og) == 1)
                {
                    if (!headerParsed)
                    {
                        ogg_stream_init(&os, ogg_page_serialno(&og));
                        ogg_stream_pagein(&os, &og);

                        ogg_packet op;
                        if (ogg_stream_packetout(&os, &op) == 1)
                        {
                            __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI",
                                                "Tìm thấy packet, bytes: %ld", op.bytes);

                            if (op.bytes >= 19)
                            {
                                // Ghi log các byte đầu tiên để kiểm tra
                                char header[9] = {0};
                                memcpy(header, op.packet, 8);
                                header[8] = '\0';

                                __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI",
                                                    "Header kiểm tra: %s", header);

                                if (memcmp(op.packet, "OpusHead", 8) == 0)
                                {
                                    preskip = op.packet[10] | (op.packet[11] << 8);
                                    headerParsed = true;
                                    __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI",
                                                        "Đã đọc được Opus header, preskip: %d", preskip);
                                }
                                else
                                {
                                    __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                                        "Không tìm thấy OpusHead signature trong packet");
                                }
                            }
                            else
                            {
                                __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                                    "Packet quá ngắn (%ld bytes)", op.bytes);
                            }
                        }
                    }

                    ogg_int64_t granPos = ogg_page_granulepos(&og);
                    if (granPos >= 0)
                    {
                        lastGranulePos = granPos;
                        __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI",
                                            "Tìm thấy granule position: %lld", (long long)lastGranulePos);
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                "Exception khi đọc Ogg/Opus: %s", e.what());
            ogg_sync_clear(&oy);
            if (headerParsed)
                ogg_stream_clear(&os);
            fclose(fin);
            return -5;
        }

        ogg_sync_clear(&oy);
        if (headerParsed)
            ogg_stream_clear(&os);
        fclose(fin);

        if (!headerParsed)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                "Không tìm thấy Opus header trong file OGG");
            return -6;
        }

        if (lastGranulePos <= 0)
        {
            __android_log_print(ANDROID_LOG_ERROR, "KaraokeFFI",
                                "Không tìm thấy granule position hợp lệ");
            return -7;
        }

        double duration = ((lastGranulePos - preskip) * 1000.0) / SAMPLE_RATE;
        __android_log_print(ANDROID_LOG_INFO, "KaraokeFFI",
                            "Thời lượng tính được: %.2f ms (granule: %lld, preskip: %d, sample_rate: %d)",
                            duration, (long long)lastGranulePos, preskip, SAMPLE_RATE);

        return duration;
    }
}