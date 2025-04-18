#include "audio_layer_factory.hpp"
#include "audio_layer.hpp"
#if defined(__APPLE__)
    #include "audio_toolbox_layer.hpp"
#elif defined(__ANDROID__)
    #include "oboe_layer.hpp"
#elif defined(_WIN32) || defined(__linux__)
    #include "port_audio_layer.hpp"
#else
    #error "Platform not supported"
#endif


std::unique_ptr<AudioLayer> AudioLayerFactory::createAudioLayer() {
#if defined(__APPLE__)
    return std::make_unique<AudioToolBoxLayer>();
#elif defined(__ANDROID__)
    return std::make_unique<OboeLayer>();
#elif defined(_WIN32) || defined(__linux__)
    return std::make_unique<PortAudioLayer>();
#else
    #error "Platform not supported"
#endif
} 