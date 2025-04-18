#include "ogg_play.hpp"
#include "../audioplayer/audio_player_types.hpp"
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
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

using namespace std;

/*
Hàm getch() được sử dụng để đọc một ký tự từ bàn phím mà không cần nhấn Enter
và không hiển thị ký tự đó lên màn hình (non-blocking, non-echoing input).
Thường được dùng để xử lý input trong các ứng dụng điều khiển theo thời gian thực.
*/
char getch()
{
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}
/*
Callback khi trạng thái của session thay đổi
*/
void onStateChange(const StateChangeInfo &info)
{
    cout << "Session state changed from "
         << static_cast<int>(info.old_state) << " to "
         << static_cast<int>(info.new_state) << endl;

    if (info.old_state == PlayState::IDLE && info.new_state == PlayState::READY)
    {
        cout << "File loaded successfully!" << endl;
    }
}
/*
Callback khi phát âm thanh
*/
void onPlayBack(const PlaybackInfo &info)
{
    cout << "Playback info: " << info.current_time << "/" << info.elapsed_time << "/" << info.duration << "\r" << flush;
}
/*
Phát nhiều file OGG đồng thời
*/
bool OggPlay::playMultipleOggFiles(const vector<string> &files)
{
    LOGI("Đang phát nhiều file");
    if (files.empty())
    {
        LOGI("Danh sách file trống");

        return false;
    }

    if (!player)
    {
        if (!init())
        {
            LOGI("Không thể khởi tạo player");
            return false;
        }
    }

    vector<AudioSession *> sessions;
    int i = 0;
    // Phát tất cả các file
    for (const auto &file : files)
    {

        auto oggPlayResult = player->playOggAt(file);
        if (!oggPlayResult.result.isSuccess())
        {
            LOGI("Không thể phát file: %s - %s", std::string(file).c_str(), oggPlayResult.result.message.c_str());

            // Dọn dẹp các session đã tạo trước đó
            for (auto session : sessions)
            {
                player->destroySession(session);
            }
            return false;
        }
        if (i == 1)
            oggPlayResult.session->setVolume(0.3f);
        sessions.push_back(oggPlayResult.session);
        i++;
    }

    // Đợi cho đến khi tất cả các file phát xong
    bool isPlaying;
    do
    {
        isPlaying = false;
        for (const auto &session : sessions)
        {
            if (session->getState() == PlayState::PLAYING)
            {
                isPlaying = true;
                break;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    } while (isPlaying);

    // Dọn dẹp tất cả các session
    for (auto session : sessions)
    {
        player->destroySession(session);
    }

    return true;
}
/*
Phát file âm thanh OGG với các chức năng điều khiển tương tác:
- Input:
  + fileName: đường dẫn file OGG cần phát
  + seekTime: thời điểm bắt đầu phát (ms)
  + endTime: thời điểm kết thúc phát (ms)
  + loop: số lần lặp lại (-1 để lặp vô hạn)
  + volume: âm lượng (0.0 đến 1.0)
- Logic xử lý:
  1. Kiểm tra và khởi tạo player nếu chưa có
  2. Phát file với các callback onPlayBack và onStateChange
  3. Tạo thread riêng để xử lý input điều khiển:
     - P: pause/resume
     - S: stop
     - Q: quit/thoát
  4. Tự động dọn dẹp session sau khi phát xong
- Return: true nếu thành công, false nếu có lỗi
*/
bool OggPlay::playOggWithCallback(const string &fileName, uint32_t seekTime, uint32_t endTime, int loop, float volume)
{
    if (!player)
    {
        if (!init())
        {
            cerr << "Failed to initialize player" << endl;
            return false;
        }
    }
    auto oggPlayResult = player->playOggAt(fileName, seekTime, endTime, loop, onPlayBack, onStateChange);
    if (!oggPlayResult.result.isSuccess())
    {
        cerr << "Failed to play file: " << oggPlayResult.result.getErrorString() << endl;
        return false;
    }
    currentSession = oggPlayResult.session;

    // Thiết lập âm lượng nếu có
    if (volume != 1.0f && currentSession)
    {
        currentSession->setVolume(volume);
    }

    cout << "\nNhấn: p (pause/resume), s (stop), q (quit)" << endl;
    cout << "      > (tăng tốc), < (giảm tốc), r (reset tốc độ)" << endl;

    // Tạo thread riêng để đọc input
    thread inputThread([this]()
                       {
        char key;
        while (currentSession && currentSession->getState() != PlayState::STOPPED) {
            key = tolower(getch());
            switch (key) {
                case 'p':
                    if (currentSession->getState() == PlayState::PLAYING) {
                        currentSession->pause();
                        cout << "\nĐã tạm dừng phát" << endl;
                    } else if (currentSession->getState() == PlayState::PAUSED) {
                        currentSession->resume();
                        cout << "\nTiếp tục phát" << endl;
                    }
                    break;
                    
                case 's':
                    if (currentSession->getState() == PlayState::PLAYING || 
                        currentSession->getState() == PlayState::PAUSED) {
                        currentSession->stop();
                        cout << "\nĐã dừng phát" << endl;
                    }
                    break;
                    
                case 'q':
                    currentSession->stop();
                    cout << "\nKết thúc phiên phát nhạc" << endl;
                    break;

                case 'r':
                    {
                        auto result = currentSession->setPlaybackSpeed(1.0);
                        if (result.isSuccess()) {
                            cout << "\nĐã reset tốc độ về 1.0x" << endl;
                        }
                    }
                    break;

                case '.':  // Tăng tốc
                    {
                        double currentSpeed = currentSession->getPlaybackSpeed();
                        double newSpeed = min(2.5, currentSpeed += 0.1);
                        auto result = currentSession->setPlaybackSpeed(newSpeed);
                        if (result.isSuccess()) {
                            cout << "\nTốc độ phát: " << newSpeed << "x" << endl;
                        }
                    }
                    break;

                case ',':  // Giảm tốc
                    {
                        double currentSpeed = currentSession->getPlaybackSpeed();
                        double newSpeed = max(0.4, currentSpeed -= 0.1);
                        auto result = currentSession->setPlaybackSpeed(newSpeed);
                        if (result.isSuccess()) {
                            cout << "\nTốc độ phát: " << newSpeed << "x" << endl;
                        }
                    }
                    break;
            }
            
            this_thread::sleep_for(chrono::milliseconds(10));
        } });

    inputThread.join();
    player->destroySession(currentSession);
    return true;
}
