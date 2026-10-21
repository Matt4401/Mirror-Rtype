#include "audio/AudioBackendFactory.hpp"

#include <memory>

#include "audio/AudioConfig.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/detail/RaylibAudioBackend.hpp"

namespace audio {

std::unique_ptr<IAudioBackend> makeAudioBackend(AudioConfig config) {
    auto backend = std::make_unique<detail::RaylibAudioBackend>(config);
    if (backend->isAvailable()) {
        return backend;
    }
    return std::make_unique<NullAudioBackend>();
}

}  // namespace audio
