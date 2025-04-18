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
                debugPrint("Searching for existing session for file: {}", fileName);
                shared_lock lock(sessionsMutex);
                for (const auto& pair : sessions) {
                    if (pair.first->getFileName() == fileName && 
                        (pair.first->getState() == PlayState::STOPPED || 
                         pair.first->getState() == PlayState::IDLE)) {
                        session = pair.first;
                        session->reset();
                        debugPrint("Found existing session for file: {}", fileName);
                        break;
                    }
                }
            }
            
            if (!session) {
                if (!filesystem::exists(fileName)) {
                    debugPrint("File not found: {}", fileName);
                    promise->set_value(PlayOggResult(Result::error(ErrorCode::FileNotFound, "File not found"), nullptr));
                    return;
                }
                debugPrint("Loading new file: {}", fileName);
                session = loadFile(fileName);
                if (!session) {
                    debugPrint("Failed to load file: {}", fileName);
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
            debugPrint("Exception in playOggAt task: {}", e.what());
            promise->set_exception(current_exception());
        }
    });

    if (resultFuture.wait_for(chrono::seconds(5)) == future_status::timeout) {
        debugPrint("Timeout waiting for playOggAt result");
        return PlayOggResult(Result::error(ErrorCode::Timeout, "Operation timed out"), nullptr);
    }
    
    return resultFuture.get();
}