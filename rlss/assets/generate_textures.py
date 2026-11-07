#!/usr/bin/env python3
"""Draws the RLSS textures: stadium, arena, car, wheel, boost flame, ball, glows and HUD pieces.

Original artwork, drawn procedurally (no third-party image), deterministic: run it again after a change and only the
edited textures differ. Needs Pillow and NumPy:

    python3 rlss/assets/generate_textures.py            # writes rlss/assets/textures/*.png

The arena geometry is read from `rlss/shared/Arena.hpp` and the wall thickness from
`rlss/client/ArenaViewPlugin.hpp`, so the picture always matches the physics. The client places the arena tiles
(`arena_<row>_<column>.png`) with the same ARENA_SCALE, ARENA_MARGIN and TILE (see `ArenaViewPlugin.cpp`).

Pixel densities (the Wren views and the client rely on them):
- arena: ARENA_SCALE px per uu, cut into tiles of at most TILE px;
- stadium: STADIUM_SCALE px per uu (blurry on purpose, it is far behind);
- car body, details, wheel, flame: CAR_SCALE px per uu;
- ball: the texture spans the ball diameter exactly.
"""

from __future__ import annotations

import math
import random
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "rlss" / "assets" / "textures"
FONT = ROOT / "rlss" / "assets" / "fonts" / "RussoOne-Regular.ttf"

ARENA_SCALE = 1.25
STADIUM_SCALE = 0.6
CAR_SCALE = 4.0
TILE = 1024
#: The arena picture extends this far past the outer walls (the camera never shows past them).
ARENA_MARGIN = 120.0

BLUE = np.array([40, 130, 255], dtype=np.float32)
ORANGE = np.array([255, 128, 24], dtype=np.float32)
WHITE = np.array([235, 242, 255], dtype=np.float32)


# --------------------------------------------------------------------------------------------------------------------
# Geometry read from the C++ headers
# --------------------------------------------------------------------------------------------------------------------


def read_constants(path: Path, names: dict[str, float]) -> dict[str, float]:
    """Evaluates the `inline constexpr float NAME = expression;` lines of a header, in order."""
    pattern = re.compile(r"inline\s+constexpr\s+float\s+(\w+)\s*=\s*([^;]+);")
    for name, expression in pattern.findall(path.read_text()):
        expression = re.sub(r"(\d+\.\d*|\d+)F\b", r"\1", expression)
        names[name] = float(eval(expression, {"__builtins__": {}}, dict(names)))  # noqa: S307 (our own header)
    return names


ARENA = read_constants(ROOT / "rlss" / "shared" / "Arena.hpp", {})
ARENA = read_constants(ROOT / "rlss" / "client" / "ArenaViewPlugin.hpp", ARENA)
LEFT, TOP, RIGHT, BOTTOM = ARENA["LEFT"], ARENA["TOP"], ARENA["RIGHT"], ARENA["BOTTOM"]
CORNER = ARENA["CORNER_RADIUS"]
GOAL_TOP, GOAL_BOTTOM, GOAL_DEPTH = ARENA["GOAL_TOP"], ARENA["GOAL_BOTTOM"], ARENA["GOAL_DEPTH"]
GOAL_CORNER = ARENA["GOAL_CORNER_RADIUS"]
WALL = ARENA["ARENA_WALL"]
BALL_RADIUS = ARENA["BALL_RADIUS"]


# --------------------------------------------------------------------------------------------------------------------
# Small image helpers (float arrays, straight alpha)
# --------------------------------------------------------------------------------------------------------------------


def smoothstep(edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def over(dst: np.ndarray, rgb: np.ndarray, alpha: np.ndarray) -> None:
    """Composites a layer (rgb: HxWx3 or 3, alpha: HxW in [0, 1]) over `dst` (HxWx4, straight alpha), in place."""
    alpha = np.clip(alpha, 0.0, 1.0)[..., None]
    dst_alpha = dst[..., 3:4]
    out_alpha = alpha + dst_alpha * (1.0 - alpha)
    safe = np.where(out_alpha > 1e-6, out_alpha, 1.0)
    dst[..., :3] = (rgb * alpha + dst[..., :3] * dst_alpha * (1.0 - alpha)) / safe
    dst[..., 3:4] = out_alpha


def to_image(rgba: np.ndarray) -> Image.Image:
    out = rgba.copy()
    out[..., 3] *= 255.0
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA")


def mix(a: np.ndarray, b: np.ndarray, t: np.ndarray) -> np.ndarray:
    return a + (b - a) * t[..., None]


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT), size)


def save(image: Image.Image, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    image.save(OUT / name, optimize=True)
    print(f"  {name:24} {image.width} x {image.height}")


# --------------------------------------------------------------------------------------------------------------------
# Signed distances (negative inside), on pixel grids in world units
# --------------------------------------------------------------------------------------------------------------------


def sd_round_box(x, y, left, top, right, bottom, radii):
    """Box with one radius per corner: (top-left, top-right, bottom-left, bottom-right). y points down."""
    cx, cy = (left + right) / 2.0, (top + bottom) / 2.0
    hx, hy = (right - left) / 2.0, (bottom - top) / 2.0
    tl, tr, bl, br = radii
    r = np.where(x > cx, np.where(y > cy, br, tr), np.where(y > cy, bl, tl))
    qx = np.abs(x - cx) - hx + r
    qy = np.abs(y - cy) - hy + r
    outside = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0))
    return outside + np.minimum(np.maximum(qx, qy), 0.0) - r


class ArenaField:
    """Distances to the arena (rounded rectangle) and to the two goal boxes on a pixel grid."""

    #: The goal boxes reach this far into the arena, so that the union stays exact around the goal mouths.
    GOAL_OVERLAP = 320.0

    def __init__(self, x: np.ndarray, y: np.ndarray):
        self.x, self.y = x, y
        self.arena = sd_round_box(x, y, LEFT, TOP, RIGHT, BOTTOM, (CORNER,) * 4)
        g = GOAL_CORNER
        self.goal_left = sd_round_box(x, y, LEFT - GOAL_DEPTH, GOAL_TOP, LEFT + self.GOAL_OVERLAP, GOAL_BOTTOM,
                                      (g, 0.0, g, 0.0))
        self.goal_right = sd_round_box(x, y, RIGHT - self.GOAL_OVERLAP, GOAL_TOP, RIGHT + GOAL_DEPTH, GOAL_BOTTOM,
                                       (0.0, g, 0.0, g))
        self.union = np.minimum(self.arena, np.minimum(self.goal_left, self.goal_right))
        # Outward normal of the union.
        gy, gx = np.gradient(self.union)
        length = np.hypot(gx, gy) + 1e-6
        self.nx, self.ny = gx / length, gy / length


# --------------------------------------------------------------------------------------------------------------------
# Stadium (parallax backdrop)
# --------------------------------------------------------------------------------------------------------------------


def team_side(u: np.ndarray) -> np.ndarray:
    """Color along the arena, u in [0, 1] from the left wall to the right one: blue, white in the middle, orange."""
    blue_part = mix(BLUE[None, :], WHITE[None, :], smoothstep(0.0, 0.5, u))
    return mix(blue_part, ORANGE[None, :], smoothstep(0.5, 1.0, u))


def draw_stadium() -> dict[str, float]:
    rng = random.Random(7)
    world_w, world_h = 4000.0, 2300.0
    w, h = int(world_w * STADIUM_SCALE), int(world_h * STADIUM_SCALE)
    img = Image.new("RGB", (w, h))
    px = np.zeros((h, w, 4), dtype=np.float32)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = yy / h
    u = xx / w
    sky = mix(np.array([4, 6, 18], np.float32), np.array([16, 22, 52], np.float32), smoothstep(0.0, 0.55, v))
    px[..., :3] = sky
    px[..., 3] = 1.0
    img = to_image(px).convert("RGB")
    draw = ImageDraw.Draw(img, "RGBA")

    # Roof truss with a ring of lamps.
    roof = int(h * 0.11)
    draw.rectangle([0, 0, w, roof], fill=(10, 12, 24, 255))
    for i in range(0, w, 46):
        draw.line([(i, 0), (i + 46, roof)], fill=(26, 30, 52, 255), width=3)
        draw.line([(i + 46, 0), (i, roof)], fill=(26, 30, 52, 255), width=3)
    draw.rectangle([0, roof - 6, w, roof], fill=(34, 40, 70, 255))

    # Stands: three tiers of seats full of fans, with LED ribbons between them.
    tiers = [(0.15, 0.35), (0.40, 0.60), (0.65, 0.86)]
    ribbon_font = font(int(h * 0.028))
    for index, (start, end) in enumerate(tiers):
        top, bottom = int(h * start), int(h * end)
        draw.rectangle([0, top, w, bottom], fill=(20, 22, 38, 255))
        rows = (bottom - top) // 7
        for row in range(rows):
            y = top + 3 + row * 7
            depth_shade = 0.55 + 0.45 * (row / max(rows - 1, 1))
            x = rng.randrange(0, 4)
            while x < w:
                side = x / w
                if rng.random() < 0.88:
                    roll = rng.random()
                    if roll < 0.45:
                        base = BLUE if rng.random() > side else ORANGE
                    elif roll < 0.7:
                        base = WHITE
                    else:
                        base = np.array([rng.randrange(60, 200) for _ in range(3)], np.float32)
                    shade = depth_shade * rng.uniform(0.45, 1.0)
                    color = tuple(int(c * shade) for c in base) + (255,)
                    draw.rectangle([x, y, x + 3, y + 4], fill=color)
                x += rng.choice((5, 5, 6, 7))
        # LED ribbon under the tier: team gradient with the game name.
        ribbon_top, ribbon_bottom = bottom, bottom + int(h * 0.04)
        ribbon = np.zeros((ribbon_bottom - ribbon_top, w, 4), np.float32)
        ribbon[..., :3] = team_side(np.linspace(0.0, 1.0, w, dtype=np.float32))[None, :, :] * 0.55
        ribbon[..., 3] = 1.0
        img.paste(to_image(ribbon).convert("RGB"), (0, ribbon_top))
        label = "RLSS  ·  CAR SOCCER LEAGUE  ·  " if index % 2 == 0 else "GOAL!  ·  RLSS  ·  "
        text_w = draw.textlength(label, font=ribbon_font)
        offset = -rng.randrange(0, int(text_w))
        while offset < w:
            draw.text((offset, ribbon_top + (ribbon_bottom - ribbon_top) / 2), label, font=ribbon_font,
                      fill=(255, 255, 255, 200), anchor="lm")
            offset += text_w

    # Pitch-side boards.
    boards_top = int(h * 0.9)
    draw.rectangle([0, boards_top, w, h], fill=(12, 14, 26, 255))
    for i in range(0, w, 220):
        color = tuple(int(c) for c in (BLUE if i < w / 2 else ORANGE) * 0.45) + (255,)
        draw.rectangle([i + 6, boards_top + 8, i + 214, boards_top + 40], fill=color)

    img = img.filter(ImageFilter.GaussianBlur(1.6))

    # Floodlights: soft glows and beams, added on top of the blurred stands.
    light = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ldraw = ImageDraw.Draw(light)
    for fx in (0.08, 0.3, 0.7, 0.92):
        cx, cy = int(w * fx), int(h * 0.06)
        ldraw.polygon([(cx - 18, cy), (cx + 18, cy), (cx + 260, h), (cx - 260, h)], fill=(190, 210, 255, 16))
        for radius, alpha in ((90, 30), (50, 60), (22, 140), (9, 255)):
            ldraw.ellipse([cx - radius * 2, cy - radius, cx + radius * 2, cy + radius], fill=(225, 235, 255, alpha))
    light = light.filter(ImageFilter.GaussianBlur(14))
    img = Image.alpha_composite(img.convert("RGBA"), light)

    # Darken and vignette so the game reads clearly in front of it.
    arr = np.asarray(img).astype(np.float32)
    vignette = 1.0 - 0.45 * (((u - 0.5) * 2.0) ** 2) * 0.6 - 0.35 * np.clip((0.35 - v) / 0.35, 0, 1)
    arr[..., :3] *= (0.6 * vignette)[..., None]
    save(Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA").convert("RGB"), "stadium.png")
    return {"STADIUM_WIDTH": world_w, "STADIUM_HEIGHT": world_h}


# --------------------------------------------------------------------------------------------------------------------
# Arena
# --------------------------------------------------------------------------------------------------------------------


def hex_lines(w: int, h: int, origin_x: float, origin_y: float, size: float, color, width: int) -> np.ndarray:
    """Alpha mask (0..1) of a honeycomb, `size` px from center to corner, aligned on the world origin."""
    layer = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(layer)
    dx = size * 1.5
    dy = size * math.sqrt(3.0)
    col0 = int(math.floor(-origin_x / dx)) - 1
    row0 = int(math.floor(-origin_y / dy)) - 1
    for col in range(col0, col0 + int(w / dx) + 3):
        for row in range(row0, row0 + int(h / dy) + 3):
            cx = origin_x + col * dx
            cy = origin_y + row * dy + (dy / 2.0 if col % 2 else 0.0)
            points = [(cx + size * math.cos(math.pi / 3 * k), cy + size * math.sin(math.pi / 3 * k)) for k in range(7)]
            draw.line(points, fill=color, width=width)
    return np.asarray(layer).astype(np.float32) / 255.0


def draw_arena() -> None:
    """Draws the arena tile by tile (the whole picture does not fit in memory as float arrays)."""
    s = ARENA_SCALE
    world_left = LEFT - GOAL_DEPTH - WALL - ARENA_MARGIN
    world_top = TOP - WALL - ARENA_MARGIN
    world_right = RIGHT + GOAL_DEPTH + WALL + ARENA_MARGIN
    world_bottom = BOTTOM + WALL + ARENA_MARGIN
    width = int(math.ceil((world_right - world_left) * s))
    height = int(math.ceil((world_bottom - world_top) * s))
    for row in range(math.ceil(height / TILE)):
        for col in range(math.ceil(width / TILE)):
            x0, y0 = col * TILE, row * TILE
            tile = arena_tile(world_left + x0 / s, world_top + y0 / s, min(TILE, width - x0), min(TILE, height - y0))
            save(tile, f"arena_{row}_{col}.png")


def arena_tile(world_left: float, world_top: float, w: int, h: int) -> Image.Image:
    """One piece of the arena picture, `w` x `h` px, its top-left pixel corner at (`world_left`, `world_top`) uu."""
    s = ARENA_SCALE
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    x = world_left + (xx + 0.5) / s
    y = world_top + (yy + 0.5) / s
    f = ArenaField(x, y)
    d = f.union
    aa = 0.75 / s  # half a pixel, in uu
    u = np.clip((x - LEFT) / (RIGHT - LEFT), 0.0, 1.0)
    side = team_side(u)
    out = np.zeros((h, w, 4), np.float32)

    # 1. Structure around the arena: dark panels.
    panel = np.array([16, 20, 36], np.float32) + 6.0 * (np.floor(x / 160.0) % 2)[..., None]
    seams = (np.abs((x % 160.0) - 80.0) > 78.0) | (np.abs((y % 220.0) - 110.0) > 108.0)
    panel = np.where(seams[..., None], panel * 0.6, panel)
    over(out, panel, smoothstep(WALL - aa, WALL + aa, d))

    # 2. The field: tinted glass, the stadium shows through. Team light on each half, honeycomb, markings.
    inside_arena = smoothstep(aa, -aa, f.arena)
    glass = mix(np.array([6, 10, 28], np.float32), np.array([10, 18, 44], np.float32), smoothstep(TOP, BOTTOM, y))
    over(out, glass, inside_arena * 0.5)
    halves = np.where((x < (LEFT + RIGHT) / 2)[..., None], BLUE, ORANGE)
    center_falloff = np.abs(x - (LEFT + RIGHT) / 2) / ((RIGHT - LEFT) / 2)
    over(out, halves, inside_arena * 0.10 * smoothstep(0.1, 1.0, center_falloff))
    honeycomb = hex_lines(w, h, (LEFT - world_left) * s, (TOP - world_top) * s, 54.0 * s, 255, 2)
    over(out, mix(side, WHITE[None, None, :] * np.ones_like(side), np.full(u.shape, 0.4, np.float32)),
         inside_arena * honeycomb * 0.10)
    mid_x = (LEFT + RIGHT) / 2
    mid_y = (TOP + BOTTOM) / 2
    line = smoothstep(4.0 + aa, 4.0 - aa, np.abs(x - mid_x))
    ring = np.hypot(x - mid_x, y - mid_y)
    circle = smoothstep(4.0 + aa, 4.0 - aa, np.abs(ring - 170.0)) + smoothstep(3.0 + aa, 3.0 - aa,
                                                                               np.abs(ring - 205.0)) * 0.5
    over(out, WHITE, inside_arena * np.clip(line + circle, 0, 1) * 0.28)
    over(out, WHITE, inside_arena * smoothstep(26.0, 0.0, ring) * 0.35)

    # 3. Goals: dark team-colored boxes with a net.
    for goal, team, wall_x, outward in ((f.goal_left, BLUE, LEFT, -1.0), (f.goal_right, ORANGE, RIGHT, 1.0)):
        inside_goal = smoothstep(aa, -aa, goal) * smoothstep(-aa, aa, f.arena)
        depth = np.clip((x - wall_x) * outward / GOAL_DEPTH, 0.0, 1.0)
        base = team * 0.16 + 6.0
        over(out, mix(base[None, None, :] * np.ones((h, w, 3), np.float32), base * 0.45, depth), inside_goal * 0.96)
        net = hex_lines(w, h, (wall_x - world_left) * s, (GOAL_TOP - world_top) * s, 16.0 * s, 255, 2)
        over(out, team * 0.9 + 25.0, inside_goal * net * (0.55 - 0.35 * depth))
        over(out, team, inside_goal * smoothstep(0.6, 0.0, depth) * 0.22)

    # 4. Depth of the arena: a band along the inner walls seen in perspective. Grass on the floor, panels elsewhere.
    band_depth = 64.0
    band = smoothstep(-band_depth - aa, -band_depth + aa, d) * smoothstep(aa, -aa, d)
    # Only along the arena walls, fading out at the goal posts (there is no wall across the goal mouths).
    band = band * smoothstep(aa, -aa, f.arena) * smoothstep(10.0, 90.0, np.minimum(f.goal_left, f.goal_right))
    t = np.clip(-d / band_depth, 0.0, 1.0)  # 0 at the wall, 1 at the back
    floor_part = smoothstep(0.35, 0.85, f.ny)
    # Mowing stripes converging toward the center of the arena.
    perspective_x = mid_x + (x - mid_x) / (1.0 - 0.18 * t)
    stripes = (np.floor(perspective_x / 120.0) % 2)[..., None]
    grass = np.array([36, 122, 52], np.float32) + stripes * np.array([10, 22, 10], np.float32)
    grass = grass * (1.0 - 0.5 * t)[..., None]
    wall_panel = mix(np.array([44, 54, 88], np.float32) * np.ones((h, w, 3), np.float32),
                     np.array([18, 22, 40], np.float32), t)
    wall_panel = wall_panel + side * (0.20 * smoothstep(0.25, 0.0, np.abs(t - 0.5)))[..., None]
    over(out, mix(wall_panel, grass, floor_part), band)
    touchline = smoothstep(-5.0 - aa, -5.0 + aa, d) * band
    over(out, WHITE, touchline * floor_part * 0.85)

    # 5. Neon spill from the walls into the arena.
    goal_near = np.minimum(f.goal_left, f.goal_right) < f.arena + 1.0
    team_near_goal = np.where((x < mid_x)[..., None], BLUE, ORANGE)
    neon = np.where(goal_near[..., None], team_near_goal, side)
    over(out, neon, smoothstep(-90.0, 0.0, d) * smoothstep(aa, -aa, d) * 0.35)

    # 6. The walls: a metal tube with a neon edge on the inside and a thin light on the outside.
    tube = smoothstep(-aa, aa, d) * smoothstep(WALL + aa, WALL - aa, d)
    shine = 1.0 - np.abs((d / WALL) - 0.35) * 1.2
    metal = np.array([38, 46, 72], np.float32) + 34.0 * np.clip(shine, 0, 1)[..., None]
    over(out, metal, tube)
    over(out, neon * 0.5 + 120.0, tube * smoothstep(7.0 + aa, 7.0 - aa, d))
    over(out, np.minimum(neon * 0.25 + 200.0, 255.0), tube * smoothstep(3.0 + aa, 3.0 - aa, d))
    over(out, neon, tube * smoothstep(WALL - 4.0 - aa, WALL - 4.0 + aa, d) * 0.6)

    # 7. Goal lines and posts.
    for team, wall_x in ((BLUE, LEFT), (ORANGE, RIGHT)):
        in_mouth = smoothstep(GOAL_TOP - aa, GOAL_TOP + aa, y) * smoothstep(GOAL_BOTTOM + aa, GOAL_BOTTOM - aa, y)
        glow = np.exp(-np.abs(x - wall_x) / 14.0)
        over(out, team, in_mouth * glow * 0.5)
        over(out, np.minimum(team + 120.0, 255.0), in_mouth * smoothstep(3.0 + aa, 3.0 - aa, np.abs(x - wall_x)))
        for post_y in (GOAL_TOP, GOAL_BOTTOM):
            distance = np.hypot(x - wall_x, y - post_y)
            over(out, team, smoothstep(34.0, 0.0, distance) * 0.6)
            over(out, np.minimum(team + 140.0, 255.0), smoothstep(10.0 + aa, 10.0 - aa, distance))

    return to_image(out)


# --------------------------------------------------------------------------------------------------------------------
# Car (side view, nose to the right, CAR_SCALE px per uu; the hitbox, 118 x 36 uu, is centered in the canvas)
# --------------------------------------------------------------------------------------------------------------------

CAR_CANVAS = (140.0, 64.0)  # uu
SUPER = 4  # supersampling of the small sprites


def car_canvas() -> tuple[Image.Image, ImageDraw.ImageDraw, callable]:
    w = int(CAR_CANVAS[0] * CAR_SCALE * SUPER)
    h = int(CAR_CANVAS[1] * CAR_SCALE * SUPER)
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))

    def p(fx: float, up: float) -> tuple[float, float]:
        """Car frame (uu, forward and up from the hitbox center) to canvas pixels."""
        return ((CAR_CANVAS[0] / 2 + fx) * CAR_SCALE * SUPER, (CAR_CANVAS[1] / 2 - up) * CAR_SCALE * SUPER)

    return image, ImageDraw.Draw(image), p


def finish(image: Image.Image) -> Image.Image:
    return image.resize((image.width // SUPER, image.height // SUPER), Image.LANCZOS)


# Body silhouette, in the car frame: low wedge, raked windshield, short roof, fastback and a spoiler.
BODY = [(-62, -4), (-63, 8), (-60, 13), (-40, 15), (-26, 26), (-6, 28), (10, 26), (26, 14), (52, 9), (62, 4),
        (64, -4), (60, -10), (48, -13), (-56, -13), (-62, -9)]
SPOILER = [(-60, 13), (-67, 21), (-56, 22), (-48, 15)]
WINDOWS = [(-24, 24), (-6, 25.8), (9, 24), (21, 15), (-30, 16)]
WHEEL_CENTERS = (-38.0, 38.0)
WHEEL_UP = -7.0
WHEEL_RADIUS = 13.5


def arch_cut(draw: ImageDraw.ImageDraw, p, fill) -> None:
    for wx in WHEEL_CENTERS:
        r = WHEEL_RADIUS + 3.5
        a = p(wx - r, WHEEL_UP + r)
        b = p(wx + r, WHEEL_UP - r)
        draw.ellipse([a, b], fill=fill)


def draw_car() -> None:
    # Tintable body: greyscale, multiplied by the player color at run time.
    image, draw, p = car_canvas()
    draw.polygon([p(*pt) for pt in BODY], fill=(232, 232, 232, 255))
    draw.polygon([p(*pt) for pt in SPOILER], fill=(200, 200, 200, 255))
    # Lighter top, darker sill, two-tone stripe along the flank.
    draw.polygon([p(-40, 15), p(-26, 26), p(-6, 28), p(10, 26), p(26, 14), p(52, 9), p(62, 4), p(-63, 8), p(-60, 13)],
                 fill=(255, 255, 255, 255))
    draw.polygon([p(-58, 2), p(60, 0), p(58, -3), p(-60, -1)], fill=(150, 150, 150, 255))
    draw.polygon([p(-56, -13), p(48, -13), p(56, -8), p(-60, -7)], fill=(120, 120, 120, 255))
    arch_cut(draw, p, (0, 0, 0, 0))
    save(finish(image), "car_body.png")

    # Untinted details on top: glass, trims, lights, wheel wells.
    image, draw, p = car_canvas()
    draw.polygon([p(*pt) for pt in WINDOWS], fill=(14, 20, 34, 255))
    draw.polygon([p(-20, 23.5), p(-8, 25), p(-14, 17), p(-26, 17)], fill=(110, 170, 230, 120))
    draw.line([p(-6, 25.8), p(-4, 16)], fill=(30, 36, 50, 255), width=int(2.4 * CAR_SCALE * SUPER / 4))
    draw.polygon([p(-62, -9), p(-56, -13), p(48, -13), p(60, -10), p(64, -4), p(58, -7), p(-58, -7)],
                 fill=(22, 24, 30, 255))
    draw.polygon([p(56, 7), p(63, 3), p(63.5, -1), p(55, 2)], fill=(255, 246, 210, 255))  # headlight
    draw.polygon([p(-63, 8), p(-60, 12), p(-58, 6), p(-62.5, 4)], fill=(255, 40, 50, 255))  # tail light
    draw.rectangle([p(-66, -6), p(-61, -9)], fill=(70, 74, 84, 255))  # exhaust
    for wx in WHEEL_CENTERS:
        r = WHEEL_RADIUS + 3.5
        draw.ellipse([p(wx - r, WHEEL_UP + r), p(wx + r, WHEEL_UP - r)], outline=(18, 20, 26, 255),
                     width=int(1.6 * CAR_SCALE * SUPER))
        draw.pieslice([p(wx - r, WHEEL_UP + r), p(wx + r, WHEEL_UP - r)], 180, 360, fill=(10, 11, 16, 255))
    save(finish(image), "car_details.png")

    # Wheel: tire with tread, five-spoke rim. Drawn centered, 2 * WHEEL_RADIUS uu wide.
    size = int(2 * WHEEL_RADIUS * CAR_SCALE * SUPER)
    wheel = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    wd = ImageDraw.Draw(wheel)
    c = size / 2
    wd.ellipse([0, 0, size - 1, size - 1], fill=(24, 25, 30, 255))
    for k in range(18):
        a = 2 * math.pi * k / 18
        r0, r1 = c * 0.86, c * 0.99
        wd.line([(c + r0 * math.cos(a), c + r0 * math.sin(a)), (c + r1 * math.cos(a), c + r1 * math.sin(a))],
                fill=(44, 46, 54, 255), width=size // 40)
    rim = c * 0.66
    wd.ellipse([c - rim, c - rim, c + rim, c + rim], fill=(170, 176, 190, 255))
    wd.ellipse([c - rim * 0.86, c - rim * 0.86, c + rim * 0.86, c + rim * 0.86], fill=(40, 44, 54, 255))
    for k in range(5):
        a = 2 * math.pi * k / 5
        spoke = [(c + rim * 0.95 * math.cos(a + da), c + rim * 0.95 * math.sin(a + da)) for da in (-0.16, 0.16)]
        wd.polygon([(c + 0.18 * rim * math.cos(a - 0.9), c + 0.18 * rim * math.sin(a - 0.9)), *spoke,
                    (c + 0.18 * rim * math.cos(a + 0.9), c + 0.18 * rim * math.sin(a + 0.9))],
                   fill=(200, 206, 220, 255))
    wd.ellipse([c - rim * 0.22, c - rim * 0.22, c + rim * 0.22, c + rim * 0.22], fill=(225, 230, 240, 255))
    save(finish(wheel), "wheel.png")


def draw_flame() -> None:
    """Boost flame, pointing left (out of the exhaust), its hot end on the right edge. 80 x 24 uu."""
    w, h = int(80 * CAR_SCALE), int(24 * CAR_SCALE)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    along = 1.0 - xx / (w - 1)  # 0 at the exhaust (right), 1 at the tip
    across = np.abs(yy - (h - 1) / 2) / ((h - 1) / 2)
    radius = np.clip(1.0 - along ** 1.4, 0.0, 1.0) * 0.95 + 0.05
    shape = smoothstep(1.0, 0.55, across / radius) * smoothstep(1.0, 0.6, along)
    core = smoothstep(0.55, 0.0, across / radius) * smoothstep(0.7, 0.0, along)
    color = mix(np.array([255, 120, 20], np.float32) * np.ones((h, w, 3), np.float32),
                np.array([255, 236, 170], np.float32), core)
    color = mix(color, np.array([255, 60, 20], np.float32), smoothstep(0.4, 1.0, along) * (1 - core))
    out = np.zeros((h, w, 4), np.float32)
    over(out, color, shape)
    save(to_image(out), "boost_flame.png")


def draw_glow() -> None:
    """Soft round glow, white: tinted at run time (ball halo, lights)."""
    size = 256
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.hypot(xx - (size - 1) / 2, yy - (size - 1) / 2) / (size / 2)
    alpha = np.clip(1.0 - r, 0.0, 1.0) ** 2.2
    out = np.zeros((size, size, 4), np.float32)
    over(out, np.array([255, 255, 255], np.float32), alpha)
    save(to_image(out), "glow.png")


def draw_puff() -> None:
    """Boost trail puff: a round, soft particle, white (tinted)."""
    size = 96
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.hypot(xx - (size - 1) / 2, yy - (size - 1) / 2) / (size / 2)
    alpha = smoothstep(1.0, 0.25, r)
    out = np.zeros((size, size, 4), np.float32)
    over(out, np.array([255, 255, 255], np.float32), alpha)
    save(to_image(out), "puff.png")


# --------------------------------------------------------------------------------------------------------------------
# Ball: panels of a truncated icosahedron, glowing seams; lighting in a separate, non-rotating overlay
# --------------------------------------------------------------------------------------------------------------------


def icosahedron_centers() -> np.ndarray:
    phi = (1 + math.sqrt(5)) / 2
    verts = []
    for a in (-1, 1):
        for b in (-phi, phi):
            verts += [(0, a, b), (a, b, 0), (b, 0, a)]
    verts = np.array(verts, np.float64)
    verts /= np.linalg.norm(verts, axis=1, keepdims=True)
    faces = []
    for i in range(12):
        for j in range(i + 1, 12):
            for k in range(j + 1, 12):
                if (np.dot(verts[i], verts[j]) > 0.4 and np.dot(verts[j], verts[k]) > 0.4
                        and np.dot(verts[i], verts[k]) > 0.4):
                    center = verts[i] + verts[j] + verts[k]
                    faces.append(center / np.linalg.norm(center))
    return verts, np.array(faces)


def draw_ball() -> None:
    size = 512
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    nx = (xx + 0.5 - size / 2) / (size / 2)
    ny = (yy + 0.5 - size / 2) / (size / 2)
    r2 = nx * nx + ny * ny
    disc = np.clip((1.0 - np.sqrt(r2)) * size / 2 + 0.5, 0.0, 1.0)
    nz = np.sqrt(np.clip(1.0 - r2, 0.0, 1.0))
    points = np.stack([nx, ny, nz], axis=-1)
    pentagons, hexagons = icosahedron_centers()
    rot = np.array([[0.93, -0.25, 0.27], [0.33, 0.9, -0.28], [-0.17, 0.36, 0.92]])
    rot, _ = np.linalg.qr(rot)
    centers = np.vstack([pentagons, hexagons]) @ rot.T
    dots = points @ centers.T
    order = np.argsort(-dots, axis=-1)
    first = np.take_along_axis(dots, order[..., :1], -1)[..., 0]
    second = np.take_along_axis(dots, order[..., 1:2], -1)[..., 0]
    is_pentagon = order[..., 0] < 12
    seam = smoothstep(0.022, 0.006, first - second)
    base = np.where(is_pentagon[..., None], np.array([46, 52, 66]), np.array([214, 220, 230])).astype(np.float32)
    panel_shade = 0.9 + 0.1 * first[..., None]
    color = base * panel_shade
    color = mix(color, np.array([90, 230, 255], np.float32), seam.astype(np.float32))
    out = np.zeros((size, size, 4), np.float32)
    over(out, color.astype(np.float32), disc.astype(np.float32))
    save(to_image(out), "ball.png")

    # Lighting overlay (does not turn with the ball): light from the top-left, dark rim, small highlight.
    light = np.array([-0.45, -0.6, 0.66])
    lambert = np.clip(points @ light, 0.0, 1.0)
    shade = np.zeros((size, size, 4), np.float32)
    darkness = (1.0 - lambert) ** 1.6 * 0.75 + smoothstep(0.75, 1.0, np.sqrt(r2)) * 0.25
    over(shade, np.array([4, 6, 16], np.float32), (darkness * disc).astype(np.float32))
    spec = np.exp(-(((nx + 0.38) ** 2 + (ny + 0.45) ** 2) / 0.02))
    over(shade, np.array([255, 255, 255], np.float32), (spec * 0.75 * disc).astype(np.float32))
    rim = smoothstep(0.86, 0.99, np.sqrt(r2)) * smoothstep(0.0, 0.6, nx + ny + 0.6)
    over(shade, np.array([120, 200, 255], np.float32), (rim * 0.35 * disc).astype(np.float32))
    save(to_image(shade), "ball_shade.png")


# --------------------------------------------------------------------------------------------------------------------
# HUD pieces (screen space, white: tinted at run time)
# --------------------------------------------------------------------------------------------------------------------


def draw_hud() -> None:
    # Boost meter: a disc, and one tick of the ring (drawn rotated, many times).
    size = 512
    image = Image.new("RGBA", (size * SUPER // 2, size * SUPER // 2), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    s = image.width
    draw.ellipse([0, 0, s - 1, s - 1], fill=(8, 12, 26, 210))
    draw.ellipse([s * 0.2, s * 0.2, s * 0.8, s * 0.8], fill=(14, 20, 40, 235))
    draw.ellipse([s * 0.2, s * 0.2, s * 0.8, s * 0.8], outline=(80, 100, 150, 120), width=s // 160)
    save(image.resize((size // 2, size // 2), Image.LANCZOS), "hud_disc.png")

    tick = Image.new("RGBA", (24 * SUPER, 64 * SUPER), (0, 0, 0, 0))
    ImageDraw.Draw(tick).rounded_rectangle([0, 0, tick.width - 1, tick.height - 1], radius=10 * SUPER,
                                           fill=(255, 255, 255, 255))
    save(tick.resize((24, 64), Image.LANCZOS), "hud_tick.png")

    # Score panels: slanted on their outer side, white.
    w, h = 220 * SUPER, 64 * SUPER
    slant = 26 * SUPER
    for name, poly in (("hud_slant_left.png", [(slant, 0), (w, 0), (w, h), (0, h)]),
                       ("hud_slant_right.png", [(0, 0), (w - slant, 0), (w, h), (0, h)])):
        panel = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(panel).polygon(poly, fill=(255, 255, 255, 255))
        save(panel.resize((w // SUPER, h // SUPER), Image.LANCZOS), name)

    # Horizontal soft band (banners): opaque in the middle, fading on both sides.
    w, h = 512, 64
    xs = np.abs(np.linspace(-1.0, 1.0, w, dtype=np.float32))
    ys = np.abs(np.linspace(-1.0, 1.0, h, dtype=np.float32))
    alpha = smoothstep(1.0, 0.55, xs)[None, :] * smoothstep(1.0, 0.7, ys)[:, None]
    out = np.zeros((h, w, 4), np.float32)
    over(out, np.array([255, 255, 255], np.float32), alpha)
    save(to_image(out), "hud_band.png")


# --------------------------------------------------------------------------------------------------------------------
# Goal explosion: an animated strip per team, a shockwave ring and a spark
# --------------------------------------------------------------------------------------------------------------------

EXPLOSION_FRAMES = 20
EXPLOSION_FRAME = 192  # px, square frames side by side


def value_noise(rng: np.random.Generator, size: int, cells: int) -> np.ndarray:
    """Smooth noise in [0, 1], `cells` random values across, bicubic between them."""
    grid = (rng.random((cells, cells)) * 255).astype(np.uint8)
    return np.asarray(Image.fromarray(grid, "L").resize((size, size), Image.BICUBIC)).astype(np.float32) / 255.0


def draw_explosion(name: str, flame: np.ndarray, core: np.ndarray, seed: int) -> None:
    """Fireball in `EXPLOSION_FRAMES` frames: a white-hot burst, flames in the team color, then dark smoke fading."""
    rng = np.random.default_rng(seed)
    size = EXPLOSION_FRAME
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    px = (xx + 0.5) / size * 2.0 - 1.0  # -1..1
    py = (yy + 0.5) / size * 2.0 - 1.0
    # Puffs thrown out of the center, each with its direction, speed and size.
    count = 46
    angles = rng.uniform(0.0, 2.0 * math.pi, count)
    speeds = rng.uniform(0.18, 0.55, count) ** 0.8
    radii = rng.uniform(0.07, 0.17, count)
    # Noise twice as big as a frame: each frame reads a window that drifts (no wrap-around seam).
    turbulence = [value_noise(rng, size * 2, cells * 2) for cells in (6, 12, 24)]
    smoke = flame * 0.12 + 22.0
    strip = np.zeros((size, size * EXPLOSION_FRAMES, 4), np.float32)
    for frame in range(EXPLOSION_FRAMES):
        t = frame / (EXPLOSION_FRAMES - 1)
        grow = 1.0 - (1.0 - t) ** 3  # fast at first, then slows down
        density = np.zeros((size, size), np.float32)
        for angle, speed, radius in zip(angles, speeds, radii):
            cx = math.cos(angle) * speed * 0.8 * grow
            cy = math.sin(angle) * speed * 0.8 * grow - 0.1 * t  # hot smoke rises
            r = radius * (0.6 + 1.0 * grow)
            density += np.exp(-((px - cx) ** 2 + (py - cy) ** 2) / (r * r))
        density = np.clip(density / 2.2, 0.0, 1.5)
        # Boiling edges: the noise drifts over time and eats into the puffs.
        def window(layer: np.ndarray, dx: int, dy: int) -> np.ndarray:
            return layer[dy:dy + size, dx:dx + size]

        noise = (0.45 * window(turbulence[0], frame * 2, 0) + 0.35 * window(turbulence[1], 0, frame * 3)
                 + 0.2 * window(turbulence[2], frame * 4, frame * 2))
        density = density * (0.35 + 1.3 * noise)
        # Heat: a white-hot flash at the center that dies fast, flames that cool down as the cloud spreads.
        distance = np.hypot(px, py)
        flash = np.exp(-(distance / (0.12 + 0.35 * grow)) ** 2) * (1.0 - t) ** 2.5 * 1.8
        heat = flash + density * 1.15 * (1.0 - t) ** 1.4
        color = mix(smoke * np.ones((size, size, 3), np.float32), flame * 0.6, smoothstep(0.12, 0.35, heat))
        color = mix(color, flame, smoothstep(0.3, 0.6, heat))
        color = mix(color, core, smoothstep(0.6, 0.95, heat))
        color = mix(color, np.array([255, 255, 255], np.float32), smoothstep(0.95, 1.3, heat))
        alpha = np.clip(smoothstep(0.18, 0.55, density) + smoothstep(0.3, 0.8, flash), 0.0, 1.0)
        alpha = alpha * (1.0 - smoothstep(0.6, 1.0, t)) * smoothstep(0.98, 0.8, distance)  # never cut by the frame
        frame_px = np.zeros((size, size, 4), np.float32)
        over(frame_px, color, alpha)
        strip[:, frame * size:(frame + 1) * size] = frame_px
    save(to_image(strip), name)


def draw_goal_effects() -> None:
    draw_explosion("goal_explosion_blue.png", np.array([40, 140, 255], np.float32),
                   np.array([170, 230, 255], np.float32), 11)
    draw_explosion("goal_explosion_orange.png", np.array([255, 120, 20], np.float32),
                   np.array([255, 225, 140], np.float32), 12)

    # Shockwave: a bright thin ring with a soft trail inside, white (tinted with the team color).
    size = 256
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.hypot(xx - (size - 1) / 2, yy - (size - 1) / 2) / (size / 2)
    ring = np.exp(-((r - 0.9) / 0.035) ** 2) + 0.35 * smoothstep(0.45, 0.9, r) * smoothstep(0.95, 0.9, r)
    out = np.zeros((size, size, 4), np.float32)
    over(out, np.array([255, 255, 255], np.float32), np.clip(ring, 0.0, 1.0))
    save(to_image(out), "shockwave.png")

    # Spark: a streak, hot at its head (right), fading along its tail.
    w, h = 128, 24
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    along = xx / (w - 1)
    across = np.abs(yy - (h - 1) / 2) / ((h - 1) / 2)
    width = 0.25 + 0.75 * along
    alpha = smoothstep(1.0, 0.3, across / width) * smoothstep(0.0, 0.85, along) * smoothstep(1.0, 0.94, along)
    out = np.zeros((h, w, 4), np.float32)
    over(out, np.array([255, 255, 255], np.float32), alpha)
    save(to_image(out), "spark.png")


def main() -> int:
    print(f"writing {OUT.relative_to(ROOT)}/")
    for old in OUT.glob("arena_*.png"):
        old.unlink()
    draw_stadium()
    draw_arena()
    draw_car()
    draw_flame()
    draw_glow()
    draw_puff()
    draw_ball()
    draw_hud()
    draw_goal_effects()
    return 0


if __name__ == "__main__":
    sys.exit(main())
