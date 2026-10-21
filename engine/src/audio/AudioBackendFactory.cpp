#include "audio/AudioBackendFactory.hpp"

#include <memory>

#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/common/AudioConfig.hpp"
#include "audio/detail/raylib/RaylibAudioBackend.hpp"

namespace audio {

std::unique_ptr<IAudioBackend> makeAudioBackend(AudioConfig config) {
    auto backend = std::make_unique<detail::RaylibAudioBackend>(config);
    if (backend->isAvailable()) {
        return backend;
    }
    return std::make_unique<NullAudioBackend>();
}

}  // namespace audio
