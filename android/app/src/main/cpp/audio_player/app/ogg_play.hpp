#pragma once

#include <string>
#include <iostream>
#include <memory>

#include "../audioplayer/audio_player.hpp"
#include "../audioplayer/audio_player_types.hpp"

using namespace std;

class OggPlay {
public:
    static OggPlay* getInstance() {
        static OggPlay instance;
        return &instance;
    }
    
    // Đóng gói thành một hàm duy nhất
    bool testOggFile();

    bool seekOggAt(const string& fileName, uint32_t seekTime);
    // Phát file ogg tại thời điểm và thời lượng định sẵn
    bool playOggAt(const string& fileName, uint32_t seekTime=0, uint32_t endTime=0, int loop = 1);
    bool playMultipleOggFiles(const vector<string>& files);
    bool playOggWithCallback(const string& fileName, uint32_t seekTime, uint32_t endTime, int loop, float volume = 1.0f);
    bool analyzeFileOgg();
    bool printFileOgg();
private:
    OggPlay();
    ~OggPlay();
    OggPlay(const OggPlay&) = delete;
    OggPlay& operator=(const OggPlay&) = delete;
    bool init();
    bool loadThenPrintInfo();
    AudioPlayer* player;
    AudioSession* currentSession;
    bool stateChangeVerified;

    // Phân tích file ogg
    bool analyzeOggFile(const string &fileName);
    // In thông tin trang ogg
    bool printOggFileInfo(const string &fileName);
};