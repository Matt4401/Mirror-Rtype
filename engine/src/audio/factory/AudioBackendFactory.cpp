#include "audio/factory/AudioBackendFactory.hpp"

#include <memory>

#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/strategy/raylib/RaylibAudioBackend.hpp"
#include "audio/types/AudioConfig.hpp"
#include "audio/types/AudioTypes.hpp"

namespace audio {

std::unique_ptr<IAudioBackend> makeAudioBackend(AudioBackendKind kind, AudioConfig config) {
    if (kind == AudioBackendKind::Null) {
        return std::make_unique<NullAudioBackend>();
    }
    // Auto and Raylib: raylib is the only audio library for now.
    auto backend = std::make_unique<detail::RaylibAudioBackend>(config);
    if (backend->isAvailable()) {
        return backend;
    }
    return std::make_unique<NullAudioBackend>();
}

}  // namespace audio
