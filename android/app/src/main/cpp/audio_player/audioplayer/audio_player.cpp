#include "audio_player_types.hpp"
#include "audio_player.hpp"
#include "audio_layer_factory.hpp"
#include "audio_layer.hpp"
#include "common.hpp"
#include "audio_session.hpp"
#include <memory>
#include <mutex>
#include <filesystem>
#include <future>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AudioPlayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif
using namespace std;

// Constructor
AudioPlayer::AudioPlayer()
    : audioLayer(AudioLayerFactory::createAudioLayer()) {
}


// Initialization methods
Result AudioPlayer::init() {
    debugPrint("Initializing AudioPlayer...");
    if (!audioLayer) {
        debugPrint("Error: AudioLayer not initialized");
        return Result::error(ErrorCode::Unknown, "AudioLayer not initialized");
    }
    
    if (!audioLayer->initialize()) {
        debugPrint("Error: Failed to initialize audio layer with sample rate={}, channels={}", 48000, 2);
        return Result::error(ErrorCode::Unknown, "Failed to initialize audio layer");
    }
    
    // Start audio output
    audioLayer->start();
    debugPrint("AudioPlayer initialized successfully");
    threadPool.start();
    debugPrint("Thread pool started successfully");
    return Result::success();
}

void AudioPlayer::shutdown() {
    unique_lock lock(sessionsMutex);
    sessions.clear();
    
    if (audioLayer) {
        audioLayer->shutdown();
    }
}

// Session management methods
AudioSession* AudioPlayer::createSession() {
    unique_lock lock(sessionsMutex);
    if (sessions.size() >= MAX_SESSIONS) {
        return nullptr;
    }
    
    auto session = new AudioSession(this);
    sessions[session] = unique_ptr<AudioSession>(session);
    return session;
}

void AudioPlayer::destroySession(AudioSession* session) {
    unique_lock lock(sessionsMutex);
    sessions.erase(session);
}

AudioSession* AudioPlayer::loadFile(const string& fileName) {
    debugPrint("AudioPlayer::loadFile - Trying to load file: {}", fileName);
    
    // Tìm session đã tồn tại
    {
        shared_lock lock(sessionsMutex);
        for (const auto& pair : sessions) {
            AudioSession* session = pair.first;
            if (session->getFileName() == fileName) {
                PlayState state = session->getState();
                if (state == PlayState::STOPPED || state == PlayState::READY) {
                    debugPrint("Found existing session for file: {}", fileName);
                    return session;
                }
            }
        }
    }
    
    // Tạo session mới
    AudioSession* session = createSession();
    if (!session) {
        debugPrint("Failed to create new session");
        return nullptr;
    }
    
    // Load file
    Result result = session->loadFile(fileName);
    if (!result.isSuccess()) {
        debugPrint("Failed to load file: {} - error: {}", fileName, result.getErrorString());
        destroySession(session);
        return nullptr;
    }
    return session;
}

PlayOggResult AudioPlayer::playOggAt(const string& fileName, uint32_t seekTime, uint32_t endTime, int loop,
    PlaybackCallback playbackCallback, StateChangeCallback stateChangeCallback) {
    auto resultPromise = make_shared<promise<PlayOggResult>>();
    auto resultFuture = resultPromise->get_future();
    threadPool.submitTask([this, 
                          fileName, 
                          seekTime, 
                          endTime, 
                          loop,
                          playbackCallback,
                          stateChangeCallback,
                          promise = resultPromise]() {
        try {            
            AudioSession* session = nullptr;
            {
                LOGI("Searching for existing session for file: %s", fileName.c_str());
                shared_lock lock(sessionsMutex);
                for (const auto& pair : sessions) {
                    if (pair.first->getFileName() == fileName && 
                        (pair.first->getState() == PlayState::STOPPED || 
                         pair.first->getState() == PlayState::IDLE)) {
                        session = pair.first;
                        session->reset();
                        LOGI("Found existing session for file: %s", fileName.c_str());
                        break;
                    }
                }
            }
            
            if (!session) {
                if (!filesystem::exists(fileName)) {
                    LOGE("File not found: %s", fileName.c_str());
                    promise->set_value(PlayOggResult(Result::error(ErrorCode::FileNotFound, "File not found"), nullptr));
                    return;
                }
                LOGI("Loading new file: %s", fileName.c_str());
                session = loadFile(fileName);
                if (!session) {
                    LOGE("Failed to load file: %s", fileName.c_str());
                    promise->set_value(PlayOggResult(Result::error(ErrorCode::FileReadError, "Failed to load file"), nullptr));
                    return;
                }
            }
            if (stateChangeCallback) {
                session->setStateChangeCallback(stateChangeCallback);
            }
            if (playbackCallback) {
                session->setPlaybackCallback(playbackCallback);
            }
            Result playResult = session->playAt(seekTime, endTime - seekTime, loop);
            auto result = PlayOggResult(playResult, session);
            promise->set_value(std::move(result));
        } catch (const exception& e) {
            LOGE("Exception in playOggAt task: %s", e.what());
            promise->set_exception(current_exception());
        }
    });

    if (resultFuture.wait_for(chrono::seconds(5)) == future_status::timeout) {
        LOGE("Timeout waiting for playOggAt result");
        return PlayOggResult(Result::error(ErrorCode::Timeout, "Operation timed out"), nullptr);
    }
    
    return resultFuture.get();
}