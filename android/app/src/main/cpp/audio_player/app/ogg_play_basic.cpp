#include "ogg_play.hpp"
#include "../audioplayer/audio_player_types.hpp"
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
using namespace std;

// OggPlay implementation
OggPlay::OggPlay() 
    : player(nullptr)
    , currentSession(nullptr)
    , stateChangeVerified(false) {
}

OggPlay::~OggPlay() {
}

bool OggPlay::init() {
    player = AudioPlayer::getInstance();
    if (!player) {
        cerr << "Failed to get AudioPlayer instance" << endl;
        return false;
    }
    
    return true;
}

bool OggPlay::loadThenPrintInfo() {
    if (!player) {
        cerr << "Player not initialized" << endl;
        return false;
    }

    vector<string> testFiles = {"italy1.ogg", "voice.ogg"};
    
    for (const auto& fileName : testFiles) {
        filesystem::path dataPath = filesystem::current_path() / "data" / fileName;
        
        cout << "\n=== Testing file: " << fileName << " ===" << endl;
        
        if (!filesystem::exists(dataPath)) {
            cerr << "File not found: " << dataPath << endl;
            continue;
        }
        
        currentSession = player->loadFile(dataPath.string());
        if (!currentSession) {
            cerr << "Failed to load file: " << dataPath << endl;
            continue;
        }
        
        // In thông tin file
        currentSession->printOggInfo();
        
        // Kiểm tra trạng thái session
        if (currentSession->getState() != PlayState::READY) {
            cerr << "Lỗi: Session không ở trạng thái READY" << endl;
        }
        
        // Dọn dẹp session trước khi load file tiếp theo
        player->destroySession(currentSession);
    }
    
    return true;
}

bool OggPlay::testOggFile() {
    cout << "\n=== Testing OGG Files ===" << endl;
    
    if (!init()) {
        return false;
    }
    
    return loadThenPrintInfo();
}
bool OggPlay::analyzeOggFile(const string &fileName) {
    filesystem::path dataPath = filesystem::current_path() / "data" / fileName;
    if (!filesystem::exists(dataPath)) {
        cerr << "File không tồn tại: " << dataPath << endl;
        return false;
    }

    cout << "\n=== Analyze OGG Files ===" << endl;
    
    if (!player) {
        if (!init()) {
            cerr << "Failed to initialize player" << endl;
            return false;
        }
    }
    
    player->createSession();
    currentSession->analyzeOggFile(dataPath.string(), true);
    player->destroySession(currentSession);
    return true;
}

bool OggPlay::printOggFileInfo(const string &fileName) {
    filesystem::path dataPath = filesystem::current_path() / "data" / fileName;
    if (!filesystem::exists(dataPath)) {
        cerr << "File không tồn tại: " << dataPath << endl;
        return false;
    }
    cout << "\n===Pring OGG page ===" << endl;
    if (!player) {
        if (!init()) {
            cerr << "Failed to initialize player" << endl;
            return false;
        }
    }
    player->createSession();
    currentSession->printOggPageInfo(dataPath.string());
    player->destroySession(currentSession);
    return true;
}

bool OggPlay::playOggAt(const string& fileName, uint32_t seekTime, uint32_t endTime, int loop) {
    if (!player) {
        if (!init()) {
            cerr << "Failed to initialize player" << endl;
            return false;
        }
    }

    auto oggPlayResult = player->playOggAt(fileName, seekTime, endTime, loop);

    if (!oggPlayResult.result.isSuccess()) {
        cerr << "Failed to play file: " << oggPlayResult.result.getErrorString() << endl;
        return false;
    }
    currentSession = oggPlayResult.session;
    
     // Đợi cho đến khi phát xong
    while (currentSession->getState() == PlayState::PLAYING) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    
    // Dọn dẹp session
    player->destroySession(currentSession);

    return true;
}

bool OggPlay::analyzeFileOgg() {
    string filename;
    while (true) {
        cout << "Nhập tên file ogg cần đọc (ấn 'x' để thoát): ";
        cin >> filename;
        if (filename == "x") {
            break;
        }

        debugPrint("Analyzing file: {}", filename);
        analyzeOggFile(filename);
    }
    return true;
}

bool OggPlay::printFileOgg() {
    string filename;
    while (true) {
        cout << "Nhập tên file ogg cần đọc (ấn 'x' để thoát): ";
        cin >> filename;
        if (filename == "x") {
            break;
        }
        debugPrint("Pring ogg file info: {}", filename);
        printOggFileInfo(filename);
    }
    return true;
}