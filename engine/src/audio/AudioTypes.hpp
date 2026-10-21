#pragma once

#include <cstdint>

namespace audio {

/** @brief Handle of a loaded sound effect; `NO_SOUND` marks an invalid one. */
using SoundId = std::uint32_t;

/** @brief Handle of a loaded music stream; `NO_MUSIC` marks an invalid one. */
using MusicId = std::uint32_t;

}  // namespace audio
