#pragma once
#include "audio_layer.hpp"
#include "common.hpp"
#include <memory>  // for std::unique_ptr

class AudioLayerFactory {
public:
    static std::unique_ptr<AudioLayer> createAudioLayer();
}; 