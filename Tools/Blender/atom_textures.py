"""Procedural, tileable, low-resolution textures for the AtomEngine kit.

Every generator returns an (H, W, 4) float32 array of sRGB-encoded values in
[0, 1], rows ordered bottom-to-top (Blender's image.pixels convention).
Seeds are fixed so the output is reproducible run to run.
"""

import numpy as np


# --------------------------------------------------------------------------
# Noise helpers
# --------------------------------------------------------------------------

def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(width, height, cells_x, cells_y, rng):
    """Tileable value noise in [0, 1]."""
    grid = rng.random((cells_y, cells_x))

    xs = np.arange(width) * cells_x / width
    ys = np.arange(height) * cells_y / height
    x0 = np.floor(xs).astype(int)
    y0 = np.floor(ys).astype(int)
    fx = _smooth(xs - x0)[None, :]
    fy = _smooth(ys - y0)[:, None]
    x1 = (x0 + 1) % cells_x
    y1 = (y0 + 1) % cells_y

    a = grid[y0[:, None], x0[None, :]]
    b = grid[y0[:, None], x1[None, :]]
    c = grid[y1[:, None], x0[None, :]]
    d = grid[y1[:, None], x1[None, :]]
    top = a + (b - a) * fx
    bottom = c + (d - c) * fx
    return top + (bottom - top) * fy


def fbm(width, height, cells, octaves, rng, stretch=(1, 1), gain=0.5):
    """Fractal sum of value noise; stretch=(sx, sy) elongates features."""
    total = np.zeros((height, width))
    amplitude = 1.0
    norm = 0.0
    for octave in range(octaves):
        c = cells * (2 ** octave)
        cx = max(1, int(c / stretch[0]))
        cy = max(1, int(c / stretch[1]))
        total += value_noise(width, height, cx, cy, rng) * amplitude
        norm += amplitude
        amplitude *= gain
    return total / norm


def _rgba(rgb):
    alpha = np.ones(rgb.shape[:2] + (1,))
    return np.concatenate([np.clip(rgb, 0.0, 1.0), alpha], axis=2).astype(np.float32)


def _tint(value, color):
    """value (H, W) scales an sRGB color (3,) -> (H, W, 3)."""
    return value[:, :, None] * np.asarray(color)[None, None, :]


def _mix(a, b, t):
    return a + (b - a) * t[:, :, None]


def _grime(size, rng, strength=0.25):
    """Large soft stains, darker toward the bottom of the tile."""
    stains = fbm(size, size, 3, 4, rng)
    ys = np.linspace(0.0, 1.0, size)[:, None]
    return 1.0 - strength * np.clip(stains * 1.4 - 0.4, 0, 1) - 0.08 * (1.0 - ys)


# --------------------------------------------------------------------------
# Materials
# --------------------------------------------------------------------------

def wood_boards(size=256, seed=1, color=(0.24, 0.17, 0.12), boards=8):
    """Vertical weathered boards (yakisugi-like dark cedar)."""
    rng = np.random.default_rng(seed)
    xs = np.arange(size)
    board = (xs * boards // size)
    tones = 0.75 + 0.35 * rng.random(boards)
    tone = tones[board][None, :].repeat(size, 0)

    grain = fbm(size, size, 4, 4, rng, stretch=(1, 12))
    streaks = fbm(size, size, 8, 2, rng, stretch=(1, 30))
    value = tone * (0.7 + 0.45 * grain) * (0.85 + 0.25 * streaks)

    edge = (xs % (size // boards))
    gap = (edge < 2)[None, :].repeat(size, 0)
    value = np.where(gap, value * 0.35, value)
    value *= _grime(size, rng, 0.3)
    return _rgba(_tint(value, color))


def plaster(size=256, seed=2, color=(0.74, 0.71, 0.64)):
    """Off-white plaster with rain streaks and stains."""
    rng = np.random.default_rng(seed)
    base = 0.88 + 0.12 * fbm(size, size, 6, 4, rng)
    streaks = fbm(size, size, 10, 3, rng, stretch=(1, 16))
    streaks = np.clip(streaks * 1.8 - 0.9, 0, 1)
    value = base * (1.0 - 0.3 * streaks) * _grime(size, rng, 0.35)
    rgb = _tint(value, color)
    # Slight green-brown discoloration in the stains.
    stain = np.clip(fbm(size, size, 3, 3, rng) * 1.6 - 0.8, 0, 1)
    rgb = _mix(rgb, rgb * np.array([0.8, 0.82, 0.7]), stain)
    return _rgba(rgb)


def roof_tiles(size=256, seed=3, color=(0.27, 0.29, 0.31), rows=4, cols=8):
    """Kawara clay tiles: rounded columns, overlapping rows."""
    rng = np.random.default_rng(seed)
    xs = np.linspace(0.0, 1.0, size, endpoint=False)
    ys = np.linspace(0.0, 1.0, size, endpoint=False)
    column = 0.65 + 0.35 * np.abs(np.sin(np.pi * xs * cols))[None, :]
    row_phase = (ys * rows) % 1.0
    # Each row is dark where the next row overlaps it (its top edge).
    overlap = (0.55 + 0.45 * (1.0 - row_phase ** 3))[:, None]
    value = column * overlap
    value *= 0.85 + 0.25 * fbm(size, size, 6, 3, rng)
    value *= _grime(size, rng, 0.2)
    rgb = _tint(value, color)
    lichen = np.clip(fbm(size, size, 5, 4, rng) * 2.0 - 1.25, 0, 1)
    rgb = _mix(rgb, np.array([0.42, 0.44, 0.33]) * value[:, :, None], lichen)
    return _rgba(rgb)


def concrete(size=256, seed=4, color=(0.55, 0.55, 0.53)):
    rng = np.random.default_rng(seed)
    value = 0.8 + 0.2 * fbm(size, size, 8, 5, rng)
    pits = rng.random((size, size)) < 0.004
    value = np.where(pits, value * 0.6, value)
    value *= _grime(size, rng, 0.35)
    return _rgba(_tint(value, color))


def asphalt(size=256, seed=5, color=(0.21, 0.21, 0.22)):
    rng = np.random.default_rng(seed)
    grit = rng.random((size, size))
    value = 0.8 + 0.2 * fbm(size, size, 16, 3, rng) + 0.12 * (grit - 0.5)
    patches = np.clip(fbm(size, size, 3, 3, rng) * 2.0 - 1.0, 0, 1)
    value *= 1.0 - 0.18 * patches
    crack_noise = fbm(size, size, 4, 4, rng)
    cracks = np.abs(crack_noise - 0.5) < 0.008
    value = np.where(cracks, value * 0.45, value)
    return _rgba(_tint(value, color))


def stone_blocks(size=256, seed=6, color=(0.47, 0.46, 0.42), rows=4, cols=3):
    """Running-bond cut stone with dark mortar and moss."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    row = np.floor(ys * rows).astype(int)
    shifted = (xs + 0.5 * (row % 2) / cols) % 1.0
    col = np.floor(shifted * cols).astype(int)
    fx = (shifted * cols) % 1.0
    fy = (ys * rows) % 1.0

    tones = 0.7 + 0.4 * rng.random((rows, cols))
    value = tones[row % rows, col % cols]
    value *= 0.8 + 0.3 * fbm(size, size, 12, 3, rng)
    # Bevelled edges read as mortar joints.
    edge = np.minimum(np.minimum(fx, 1 - fx) * 3.0, np.minimum(fy, 1 - fy) * 2.2)
    value *= np.clip(edge * 10.0, 0.25, 1.0)
    rgb = _tint(value, color)

    moss = np.clip(fbm(size, size, 4, 4, rng) * 2.2 - 1.1 + (1.0 - fy) * 0.15, 0, 1)
    rgb = _mix(rgb, np.array([0.24, 0.29, 0.14]) * (0.6 + 0.4 * value[:, :, None]), moss)
    return _rgba(rgb)


def faded_paint(size=256, seed=7, color=(0.58, 0.19, 0.11), bare=(0.35, 0.32, 0.28)):
    """Faded vermilion with patches of peeled paint."""
    rng = np.random.default_rng(seed)
    value = 0.85 + 0.2 * fbm(size, size, 6, 4, rng)
    rgb = _tint(value, color)
    peel = np.clip(fbm(size, size, 6, 5, rng) * 3.0 - 1.9, 0, 1)
    rgb = _mix(rgb, _tint(value, bare), peel)
    rgb *= _grime(size, rng, 0.3)[:, :, None]
    return _rgba(rgb)


def painted_metal(size=256, seed=8, color=(0.80, 0.80, 0.78)):
    """Off-white painted sheet metal with rust bleed."""
    rng = np.random.default_rng(seed)
    value = 0.92 + 0.08 * fbm(size, size, 6, 3, rng)
    rgb = _tint(value * _grime(size, rng, 0.25), color)
    rust = np.clip(fbm(size, size, 8, 4, rng, stretch=(1, 4)) * 2.6 - 1.75, 0, 1)
    rgb = _mix(rgb, np.array([0.40, 0.22, 0.12]), rust)
    return _rgba(rgb)


def dark_metal(size=128, seed=9, color=(0.26, 0.26, 0.27)):
    rng = np.random.default_rng(seed)
    value = 0.8 + 0.25 * fbm(size, size, 6, 3, rng)
    rgb = _tint(value, color)
    rust = np.clip(fbm(size, size, 5, 4, rng) * 2.2 - 1.2, 0, 1)
    rgb = _mix(rgb, np.array([0.33, 0.19, 0.11]), rust)
    return _rgba(rgb)


def flat(color, size=16, seed=10, variation=0.08):
    rng = np.random.default_rng(seed)
    value = 1.0 - variation + variation * fbm(size, size, 2, 2, rng)
    return _rgba(_tint(value, color))


def lattice(size=128, seed=11, bars=10, wood=(0.22, 0.15, 0.10), back=(0.05, 0.05, 0.05)):
    """Koshi: dense vertical slats over a dark interior."""
    rng = np.random.default_rng(seed)
    xs = np.arange(size)
    period = size // bars
    bar = ((xs % period) < period * 0.55)[None, :].repeat(size, 0)
    grain = 0.75 + 0.35 * fbm(size, size, 4, 3, rng, stretch=(1, 10))
    shade = 0.8 + 0.2 * np.sin(np.pi * (xs % period) / (period * 0.55))[None, :]
    wood_rgb = _tint(grain * shade, wood)
    back_rgb = _tint(0.7 + 0.3 * grain, back)
    return _rgba(np.where(bar[:, :, None], wood_rgb, back_rgb))


def shoji(size=128, seed=12, cols=3, rows=4):
    """Paper screen with a thin wooden grid, yellowed and stained."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    frame = ((xs * cols) % 1.0 < 0.06) | ((ys * rows) % 1.0 < 0.05)
    paper = 0.85 + 0.15 * fbm(size, size, 6, 3, rng)
    paper_rgb = _tint(paper * _grime(size, rng, 0.35), (0.80, 0.77, 0.66))
    frame_rgb = _tint(0.8 + 0.2 * paper, (0.30, 0.22, 0.15))
    return _rgba(np.where(frame[:, :, None], frame_rgb, paper_rgb))


def hazard_stripes(size=128, seed=13, stripes=4):
    """Yellow/black diagonal pole guard."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    band = np.floor((xs + ys) * stripes).astype(int) % 2 == 0
    wear = 0.8 + 0.25 * fbm(size, size, 6, 3, rng)
    yellow = _tint(wear, (0.78, 0.62, 0.12))
    black = _tint(wear, (0.08, 0.08, 0.08))
    rgb = np.where(band[:, :, None], yellow, black)
    rgb *= _grime(size, rng, 0.35)[:, :, None]
    return _rgba(rgb)


def road_paint(size=64, seed=14):
    """Worn white line paint; asphalt shows through the gaps."""
    rng = np.random.default_rng(seed)
    wear = fbm(size, size, 6, 4, rng)
    rgb = _mix(
        _tint(np.full((size, size), 0.22), (1, 1, 1)),
        _tint(0.85 + 0.1 * wear, (0.82, 0.82, 0.78)),
        np.clip(wear * 2.5 - 0.6, 0, 1),
    )
    return _rgba(rgb)


def vending_front(width=128, height=256, seed=15):
    """Non-tiling vending machine face: drink rows, buttons, dispenser."""
    rng = np.random.default_rng(seed)
    rgb = np.zeros((height, width, 3)) + np.array([0.10, 0.11, 0.12])

    def rect(x0, y0, x1, y1, color):
        rgb[int(y0 * height):int(y1 * height), int(x0 * width):int(x1 * width)] = color

    # Rows are listed top to bottom; image rows go bottom to top.
    can_colors = [
        (0.75, 0.12, 0.10), (0.10, 0.30, 0.65), (0.85, 0.75, 0.30),
        (0.15, 0.50, 0.25), (0.90, 0.90, 0.88), (0.45, 0.25, 0.15),
    ]
    rect(0.05, 0.40, 0.95, 0.95, (0.78, 0.80, 0.82))  # lit display window
    for r in range(3):
        top = 0.93 - r * 0.18
        for c in range(6):
            color = can_colors[rng.integers(len(can_colors))]
            x = 0.09 + c * 0.145
            rect(x, top - 0.12, x + 0.10, top, color)
            rect(x, top - 0.155, x + 0.10, top - 0.135, (0.20, 0.85, 0.35))  # button
    rect(0.62, 0.22, 0.90, 0.34, (0.06, 0.06, 0.07))  # coin panel
    rect(0.70, 0.27, 0.74, 0.31, (0.85, 0.30, 0.10))
    rect(0.12, 0.04, 0.88, 0.16, (0.02, 0.02, 0.02))  # dispenser slot

    noise = 0.9 + 0.1 * fbm(width, height, 4, 3, rng)
    return _rgba(rgb * noise[:, :, None])


def dirt(size=256, seed=17, color=(0.33, 0.29, 0.23)):
    """Packed earth with gravel and patchy dead grass."""
    rng = np.random.default_rng(seed)
    value = 0.75 + 0.3 * fbm(size, size, 6, 5, rng)
    gravel = rng.random((size, size))
    value = np.where(gravel < 0.03, value * 1.35, value)
    value = np.where(gravel > 0.985, value * 0.6, value)
    rgb = _tint(value, color)
    grass = np.clip(fbm(size, size, 4, 4, rng) * 2.4 - 1.3, 0, 1)
    rgb = _mix(rgb, _tint(value, (0.30, 0.31, 0.18)), grass)
    return _rgba(rgb)


def paddy(size=256, seed=18):
    """Flooded rice paddy: murky water with rows of young shoots."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    water = 0.8 + 0.2 * fbm(size, size, 4, 4, rng)
    rgb = _tint(water, (0.24, 0.26, 0.22))
    rows = (np.abs(((ys * 8) % 1.0) - 0.5) < 0.12)
    clumps = ((xs * 16) % 1.0 < 0.5) & rows
    shoots = clumps & (fbm(size, size, 16, 2, rng) > 0.4)
    rgb = np.where(shoots[:, :, None], _tint(water, (0.30, 0.38, 0.18)), rgb)
    return _rgba(rgb)
