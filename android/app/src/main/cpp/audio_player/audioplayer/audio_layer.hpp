#pragma once

#include <cstddef>
#include "common.hpp"
#include <functional>

#include "audio_player_types.hpp"

class AudioLayer {
public:
    virtual ~AudioLayer() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
        
    // Thay đổi signature của acquireInputBus
    virtual int acquireInputBus() = 0;
    virtual void releaseInputBus(int busId) = 0;
    virtual void setInputVolume(int busId, float volume) = 0;
    virtual void muteInputBus(int busId, bool mute) = 0;
    
    // Set callbacks cho từng bus
    virtual void setAudioCallback(int busId, AudioCallback callback) = 0;
    
    virtual void start() = 0;
    virtual void stop() = 0;
};