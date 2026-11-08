/**
 * Assets of the RLSS client, run at start-up by the AssetScriptingPlugin (paths from the repository root).
 *
 * Every texture and sound is aliased by its file name without extension ("car_body", "engine_loop"); the arena
 * pictures are the "arena_<row>_<column>" tiles. Textures are smooth: they stay clean whatever the camera zoom.
 */
import "core/Assets" for Assets

Assets.preloadSmoothTextures("rlss/assets/textures", "global")
Assets.preloadSounds("rlss/assets/sounds", "global")
// SIL Open Font License, see rlss/assets/fonts/OFL.txt.
Assets.font("rlss_font", "rlss/assets/fonts/RussoOne-Regular.ttf")
