#include "audio_session.hpp"
#include "audio_player.hpp"
#include "ring_buffer.hpp"
#include "audio_player_types.hpp"
#include <chrono>
#include <functional>

using namespace std;
AudioSession::AudioSession(AudioPlayer *player)
    : player(player) {}

AudioSession::~AudioSession()
{
    release();
}

// Thêm method mới để setup audio bus
Result AudioSession::acquireInputBus()
{
    if (mixerBusId >= 0)
    {
        // Nếu đã có bus, release nó trước
        auto *audioLayer = player->getAudioLayer();
        audioLayer->releaseInputBus(mixerBusId);
        mixerBusId = -1;
    }

    auto *audioLayer = player->getAudioLayer();
    mixerBusId = audioLayer->acquireInputBus();

    if (mixerBusId < 0)
    {
        return Result::error(ErrorCode::AudioSetupError, "Failed to acquire mixer bus");
    }

    audioLayer->setAudioCallback(mixerBusId,
                                 bind(&AudioSession::audioCallbackOgg, this, placeholders::_1, placeholders::_2));

    this->pcmBuffer = make_unique<float[]>(MAX_FRAME_SIZE);

    return Result::success();
}

void AudioSession::setVolume(float newVolume)
{
    volume = clamp(newVolume, 0.0f, 1.0f);
    if (mixerBusId >= 0)
    {
        auto *audioLayer = player->getAudioLayer();
        audioLayer->setInputVolume(mixerBusId, volume);
    }
}

void AudioSession::pause()
{
    auto currentState = state.load();
    if (currentState != PlayState::PLAYING)
    {
        return;
    }

    // Tính toán và lưu thời gian đã phát trước khi pause
    if (timing.hasStartTime)
    {
        auto now = chrono::steady_clock::now();
        auto timeSinceSpeedChange = chrono::duration_cast<chrono::milliseconds>(
                                        now - timing.speedChangeTime)
                                        .count();
        timing.accumulatedTime += timeSinceSpeedChange * timing.speed;
        timing.hasStartTime = false;
    }

    auto *audioLayer = player->getAudioLayer();
    audioLayer->muteInputBus(mixerBusId, true);
    setState(PlayState::PAUSED);
}

void AudioSession::resume()
{
    auto currentState = state.load();
    if (currentState != PlayState::PAUSED)
    {
        return;
    }

    auto *audioLayer = player->getAudioLayer();
    audioLayer->muteInputBus(mixerBusId, false);

    // Reset thời điểm bắt đầu đo tốc độ mới
    timing.speedChangeTime = chrono::steady_clock::now();
    timing.hasStartTime = true;

    setState(PlayState::PLAYING);
}

void AudioSession::stop()
{
    auto currentState = state.load();
    if (currentState != PlayState::PLAYING &&
        currentState != PlayState::PAUSED)
    {
        return;
    }

    buffer->clear();
    setState(PlayState::STOPPED);
}

void AudioSession::release()
{
    stop();

    if (mixerBusId >= 0)
    {
        auto *audioLayer = player->getAudioLayer();
        audioLayer->releaseInputBus(mixerBusId);
        mixerBusId = -1;
    }
    cleanupResample();
    setState(PlayState::IDLE);
}
/*
    Reset session để chuẩn bị cho việc phát lại từ đầu
*/
void AudioSession::reset()
{
    if (state.load() == PlayState::ERROR)
    {
        setState(PlayState::IDLE);
    }
    timing = PlayBackTiming();
    buffer->clear();
    pcmBuffer.reset();
}

void AudioSession::setPlaybackCallback(PlaybackCallback callback)
{
    playbackCallback = callback;
}

void AudioSession::setStateChangeCallback(StateChangeCallback callback)
{
    stateChangeCallback = callback;
}

Result AudioSession::setState(PlayState newState)
{
    PlayState oldState = state.load();

    // Validate state transition
    bool validTransition = false;
    string reason;

    switch (oldState)
    {
    case PlayState::IDLE:
        validTransition = (newState == PlayState::LOADING || newState == PlayState::ERROR);
        break;

    case PlayState::LOADING:
        validTransition = (newState == PlayState::READY ||
                           newState == PlayState::ERROR ||
                           newState == PlayState::IDLE);
        break;

    case PlayState::READY:
        validTransition = (newState == PlayState::PLAYING ||
                           newState == PlayState::IDLE ||
                           newState == PlayState::ERROR);
        break;

    case PlayState::PLAYING:
        validTransition = (newState == PlayState::PAUSED ||
                           newState == PlayState::STOPPED ||
                           newState == PlayState::ERROR);
        break;

    case PlayState::PAUSED:
        validTransition = (newState == PlayState::PLAYING ||
                           newState == PlayState::STOPPED ||
                           newState == PlayState::ERROR);
        break;

    case PlayState::STOPPED:
        validTransition = (newState == PlayState::PLAYING ||
                           newState == PlayState::IDLE ||
                           newState == PlayState::ERROR);
        break;

    case PlayState::ERROR:
        validTransition = (newState == PlayState::IDLE ||
                           newState == PlayState::LOADING);
        break;
    }

    if (!validTransition)
    {
        debugPrint("Invalid state transition: {} -> {}",
                   static_cast<int>(oldState),
                   static_cast<int>(newState));
        return Result::error(ErrorCode::InvalidState, "Invalid state transition");
    }

    state.store(newState);

    if (stateChangeCallback)
    {
        StateChangeInfo info{
            .old_state = oldState,
            .new_state = newState,
            .reason = reason,
            .current_time = getCurrentTime()};
        stateChangeCallback(info);
    }
    return Result::success();
}