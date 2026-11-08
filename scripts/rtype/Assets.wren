/**
 * Assets of the R-Type client, run at start-up by the AssetScriptingPlugin (paths from the repository root).
 * The client loads the "global" group; a level would declare its own group and load it on entering.
 */
import "core/Assets" for Assets

Assets.texture("player_ship", "sprites/r-typesheet42.gif")
Assets.texture("bydos", "sprites/spritesheet5.png")
Assets.texture("player_bullet", "sprites/r-typesheet-amo.png")

Assets.texture("star_small", "sprites/star_small.png")
Assets.texture("star_medium", "sprites/star_medium.png")
Assets.texture("star_large", "sprites/star_large.png")
