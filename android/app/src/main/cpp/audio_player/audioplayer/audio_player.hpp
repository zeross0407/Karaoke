#pragma once
#include "audio_layer.hpp"
#include "audio_session.hpp"
#include "audio_player_types.hpp"
#include "error_code.hpp"
#include "thread_pool.hpp"

#include <memory>
#include <unordered_map>
#include <shared_mutex>

struct PlayOggResult
{
    Result result;
    AudioSession *session;

    PlayOggResult(Result r, AudioSession *s) : result(r), session(s) {}
};
using namespace std;
class AudioPlayer
{
public:
    static constexpr size_t MAX_SESSIONS = 8;

    // Singleton access
    static AudioPlayer *getInstance()
    {
        static AudioPlayer instance;
        return &instance;
    }

    ~AudioPlayer()
    {
        shutdown();
    }
    // Core system functionality
    Result init();
    void shutdown();
    AudioLayer *getAudioLayer() { return audioLayer.get(); }

    // Session management
    AudioSession *createSession();
    void destroySession(AudioSession *session);
    AudioSession *loadFile(const string &fileName);
    PlayOggResult playOggAt(const string &fileName, uint32_t seekTime = 0, uint32_t endTime = 0, int loop = 1,
                            PlaybackCallback playbackCallback = nullptr, StateChangeCallback stateChangeCallback = nullptr);

private:
    // Constructor and assignment operators
    AudioPlayer();
    AudioPlayer(const AudioPlayer &) = delete;
    AudioPlayer &operator=(const AudioPlayer &) = delete;

    // Member variables
    unique_ptr<AudioLayer> audioLayer;
    mutable shared_mutex sessionsMutex;
    unordered_map<AudioSession *, unique_ptr<AudioSession>> sessions;
    ThreadPool threadPool;
};