-- RLSS: second sample game built on the engine (2D car football).
-- Like R-Type, it only links the engine modules it needs and never adds game code to `engine/`.
includes("shared/xmake.lua")
includes("server/xmake.lua")
if has_config("graphics") then
    includes("client/xmake.lua")
end
