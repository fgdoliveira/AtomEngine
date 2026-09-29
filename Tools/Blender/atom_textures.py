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


def tatami(size=256, seed=19):
    """Woven rush mat: fine horizontal weave, a dark cloth border."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    weave = 0.8 + 0.2 * np.sin(ys * size * np.pi * 0.5) ** 2
    tone = weave * (0.85 + 0.15 * fbm(size, size, 6, 3, rng, stretch=(8, 1)))
    rgb = _tint(tone * _grime(size, rng, 0.25), (0.58, 0.54, 0.36))
    border = (xs < 0.05) | (xs > 0.95)
    rgb = np.where(border[:, :, None], _tint(0.8 + 0.2 * weave, (0.10, 0.10, 0.09)), rgb)
    return _rgba(rgb)


def gravel(size=256, seed=20):
    """Raked pale shrine gravel."""
    rng = np.random.default_rng(seed)
    ys = np.mgrid[0:size, 0:size][0] / size
    stones = 0.7 + 0.3 * rng.random((size, size))
    stones = 0.5 * stones + 0.5 * (0.75 + 0.25 * fbm(size, size, 32, 2, rng))
    raked = 0.9 + 0.1 * np.sin(ys * np.pi * 16) ** 2
    return _rgba(_tint(stones * raked * _grime(size, rng, 0.2), (0.62, 0.61, 0.57)))


def bark(size=128, seed=22):
    rng = np.random.default_rng(seed)
    value = 0.6 + 0.4 * fbm(size, size, 6, 4, rng, stretch=(1, 8))
    return _rgba(_tint(value, (0.26, 0.17, 0.12)))


def foliage(size=128, seed=23):
    rng = np.random.default_rng(seed)
    value = 0.55 + 0.45 * fbm(size, size, 12, 3, rng)
    return _rgba(_tint(value, (0.10, 0.16, 0.09)))


def wood_floor(size=256, seed=24, color=(0.36, 0.25, 0.16)):
    """Polished floorboards running along one axis."""
    rng = np.random.default_rng(seed)
    ys = np.arange(size)
    board = ys * 6 // size
    tones = 0.8 + 0.3 * rng.random(6)
    tone = tones[board][:, None].repeat(size, 1)
    grain = fbm(size, size, 4, 4, rng, stretch=(12, 1))
    value = tone * (0.75 + 0.35 * grain)
    gap = ((ys % (size // 6)) < 2)[:, None].repeat(size, 1)
    value = np.where(gap, value * 0.4, value)
    return _rgba(_tint(value, color))


def fusuma(size=128, seed=25):
    """Sliding paper door: faded washi with a faint pattern and a dark frame."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / size
    paper = 0.85 + 0.15 * fbm(size, size, 5, 3, rng)
    pattern = 0.94 + 0.06 * np.sin(xs * 40 + np.sin(ys * 12) * 2)
    rgb = _tint(paper * pattern * _grime(size, rng, 0.35), (0.66, 0.62, 0.52))
    frame = (xs < 0.04) | (xs > 0.96) | (ys < 0.03) | (ys > 0.97)
    rgb = np.where(frame[:, :, None], _tint(paper, (0.12, 0.09, 0.07)), rgb)
    return _rgba(rgb)


def scroll(width=64, height=192, seed=26):
    """Hanging scroll: silk mount and a column of dark brushed marks."""
    rng = np.random.default_rng(seed)
    rgb = np.zeros((height, width, 3)) + np.array([0.30, 0.26, 0.20])
    rgb[int(height * 0.12):int(height * 0.9), int(width * 0.12):int(width * 0.88)] = (0.72, 0.68, 0.58)
    ink = fbm(width, height, 3, 4, rng, stretch=(1, 3))
    column = (np.abs(np.arange(width) - width / 2) < width * 0.12)[None, :]
    rows = np.arange(height)[:, None]
    strokes = column & (ink > 0.52) & (rows > height * 0.2) & (rows < height * 0.82)
    rgb = np.where(strokes[:, :, None], np.array([0.06, 0.05, 0.05]), rgb)
    return _rgba(rgb * (0.9 + 0.1 * fbm(width, height, 4, 3, rng))[:, :, None])


def lantern_paper(size=64, seed=27):
    rng = np.random.default_rng(seed)
    value = 0.8 + 0.2 * fbm(size, size, 4, 3, rng)
    return _rgba(_tint(value, (0.95, 0.72, 0.42)))


# --------------------------------------------------------------------------
# Alpha-tested (M17): the alpha channel is the shape; the engine discards
# pixels below the material's cutoff.
# --------------------------------------------------------------------------

def _with_alpha(rgb, alpha):
    """RGBA with transparent texels filled by the mean opaque colour.
    Bilinear filtering and mipmaps blend transparent texels into the edges,
    so black ones would draw a dark fringe; and a zero albedo would read as
    'no light' in the bake."""
    alpha = np.clip(alpha, 0.0, 1.0)
    opaque = alpha >= 0.5
    if opaque.any():
        mean = rgb[opaque].mean(axis=0)
        rgb = np.where(opaque[:, :, None], rgb, mean[None, None, :])
    return np.concatenate([np.clip(rgb, 0.0, 1.0), alpha[:, :, None]], axis=2).astype(np.float32)


def leaves(size=128, seed=40, color=(0.25, 0.34, 0.16)):
    """A cluster of small leaves, dense in the middle and ragged at the rim,
    for crossed foliage cards (bushes, tree canopies)."""
    rng = np.random.default_rng(seed)
    alpha = np.zeros((size, size))
    shade = np.zeros((size, size))
    ys, xs = np.mgrid[0:size, 0:size]
    for _ in range(420):
        cx, cy = np.clip(rng.normal(size / 2, size * 0.2, 2), 4, size - 5)
        r = rng.uniform(3.0, 6.5)
        angle = rng.uniform(0, np.pi)
        dx, dy = xs - cx, ys - cy
        u = (dx * np.cos(angle) + dy * np.sin(angle)) / r
        v = (-dx * np.sin(angle) + dy * np.cos(angle)) / (r * 0.45)
        leaf = (u * u + v * v) < 1.0
        alpha[leaf] = 1.0
        # Leaves lower in the cluster sit in their neighbours' shade.
        shade[leaf] = rng.uniform(0.7, 1.0) * (0.8 + 0.2 * cy / size)
    rgb = _tint(shade, color) * (0.85 + 0.3 * fbm(size, size, 4, 3, rng))[:, :, None]
    return _with_alpha(rgb, alpha)


def grass(size=128, seed=41, color=(0.36, 0.38, 0.19)):
    """Tapering blades rising from the bottom row, some dry and bent."""
    rng = np.random.default_rng(seed)
    alpha = np.zeros((size, size))
    shade = np.zeros((size, size))
    for _ in range(40):
        x0 = rng.uniform(4, size - 4)
        height = rng.uniform(0.35, 0.98) * size
        lean = rng.uniform(-0.35, 0.35)
        width = rng.uniform(0.5, 1.1)
        dry = rng.uniform(0.7, 1.15)
        for y in range(int(height)):
            t = y / height
            x = x0 + lean * y * t
            half = width * (1.0 - t) + 0.3
            lo, hi = int(max(0, x - half)), int(min(size - 1, x + half))
            alpha[y, lo:hi + 1] = 1.0
            shade[y, lo:hi + 1] = dry * (0.55 + 0.45 * t)  # darker at the root
    return _with_alpha(_tint(shade, color), alpha)


def noren(size=128, seed=42, color=(0.11, 0.14, 0.27)):
    """Indigo shop curtain in three panels, a faded white crest, a torn and
    holed hem. Top rows hang from the rod."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size]
    cloth = 0.75 + 0.25 * fbm(size, size, 6, 4, rng, stretch=(1, 3))
    rgb = _tint(cloth, color)
    crest = (np.hypot(xs - size / 2, ys - size * 0.62) - size * 0.14)
    ring = np.abs(crest) < size * 0.025
    rgb = np.where(ring[:, :, None], np.array([0.62, 0.62, 0.58]) * cloth[:, :, None], rgb)
    rgb *= (1.0 - 0.3 * (1.0 - ys / size))[:, :, None]  # grime toward the hem

    alpha = np.ones((size, size))
    # Slits between the panels, up to near the rod.
    for x in (size / 3, 2 * size / 3):
        alpha[(np.abs(xs - x) < 1.5) & (ys < size * 0.82)] = 0.0
    # Ragged hem and moth holes.
    hem = size * (0.08 + 0.12 * value_noise(size, 1, 12, 1, rng)[0])
    alpha[ys < hem[None, :]] = 0.0
    holes = fbm(size, size, 10, 2, rng) > 0.78
    alpha[holes & (ys < size * 0.6)] = 0.0
    return _with_alpha(rgb, alpha)


def chain_link(size=64, seed=43):
    """Diamond wire mesh, galvanised with rust; tiles in both directions."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size]
    period = size // 4
    a = (xs + ys) % period
    b = (xs - ys) % period
    wire = (a < 2) | (b < 2)
    rust = fbm(size, size, 4, 3, rng)
    rgb = _mix(np.zeros((size, size, 3)) + np.array([0.55, 0.56, 0.55]),
               np.zeros((size, size, 3)) + np.array([0.42, 0.26, 0.15]),
               np.clip(rust * 1.6 - 0.6, 0, 1))
    return _with_alpha(rgb, wire.astype(float))


# --------------------------------------------------------------------------
# Decals (M18): alpha-blended over the surface they lie on.
# --------------------------------------------------------------------------

def water_stain(size=128, seed=50, color=(0.20, 0.17, 0.12)):
    """Streaks running down from a leak: strongest at the top, fading and
    breaking up toward the bottom (rows run bottom to top)."""
    rng = np.random.default_rng(seed)
    streaks = fbm(size, size, 8, 4, rng, stretch=(1, 10))
    ys = np.linspace(0.0, 1.0, size)[:, None]
    xs = np.linspace(-1.0, 1.0, size)[None, :]
    alpha = np.clip(streaks * 2.6 - 0.7, 0, 1) * np.clip(ys * 1.5, 0, 1)
    alpha *= np.clip(1.2 - xs * xs, 0, 1)  # soft sides
    alpha = np.clip(alpha, 0, 0.85)
    rgb = _tint(0.8 + 0.2 * fbm(size, size, 6, 3, rng), color)
    return _with_alpha(rgb, alpha * (alpha > 0.03))


def grime(size=128, seed=51, color=(0.12, 0.11, 0.09)):
    """A soft dark blotch, denser at the bottom where dirt splashes up."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size] / (size - 1.0)
    radial = np.clip(1.0 - np.hypot((xs - 0.5) * 2.0, (ys - 0.35) * 2.2), 0, 1)
    alpha = np.clip(radial * (0.7 + 1.1 * fbm(size, size, 6, 4, rng)) - 0.1, 0, 0.85)
    alpha *= 1.0 - 0.5 * ys
    rgb = _tint(0.8 + 0.2 * fbm(size, size, 8, 2, rng), color)
    return _with_alpha(rgb, alpha * (alpha > 0.03))


def shop_sign(width=256, height=64, seed=52):
    """A painted shop sign, long faded: cream board, five dark red blocks
    standing in for lettering, paint peeling off in patches."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    rgb = np.zeros((height, width, 3)) + np.array([0.70, 0.64, 0.50])
    for k in range(5):
        x0 = int(width * (0.08 + 0.18 * k))
        glyph = (xs >= x0) & (xs < x0 + width * 0.12) & (ys > height * 0.2) & (ys < height * 0.8)
        strokes = fbm(width, height, 12, 2, rng) > 0.45
        rgb = np.where((glyph & strokes)[:, :, None], np.array([0.42, 0.10, 0.08]), rgb)
    border = (xs < 3) | (xs > width - 4) | (ys < 3) | (ys > height - 4)
    rgb = np.where(border[:, :, None], np.array([0.25, 0.18, 0.12]), rgb)
    rgb *= (0.75 + 0.25 * fbm(width, height, 6, 3, rng))[:, :, None]
    peel = fbm(width, height, 10, 3, rng)
    alpha = np.where(peel > 0.62, 0.0, 0.92)
    return _with_alpha(rgb, alpha)


def ofuda(width=32, height=128, seed=53):
    """A paper talisman strip: aged paper, a red seal and inked strokes,
    edges torn."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    paper = 0.8 + 0.2 * fbm(width, height, 3, 3, rng)
    rgb = _tint(paper, (0.82, 0.77, 0.62))
    ink = (np.abs(xs - width / 2) < width * 0.18) & (fbm(width, height, 2, 3, rng, stretch=(1, 4)) > 0.5) \
        & (ys > height * 0.1) & (ys < height * 0.72)
    rgb = np.where(ink[:, :, None], np.array([0.08, 0.07, 0.07]), rgb)
    seal = np.hypot(xs - width / 2, ys - height * 0.83) < width * 0.22
    rgb = np.where(seal[:, :, None], np.array([0.55, 0.12, 0.08]), rgb)
    edge = 1.0 + 1.5 * value_noise(width, height, 4, 16, rng)
    torn = (xs < edge) | (xs > width - 1 - edge) | (ys < 2.0 * edge)
    alpha = np.where(torn, 0.0, 0.95)
    return _with_alpha(rgb, alpha)


def road_diamond(width=64, height=192, seed=54):
    """The worn white diamond painted before a crossing (a Japanese road
    marking), drawn as an outline, long axis along the road."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    u = np.abs(xs - width / 2) / (width / 2)
    v = np.abs(ys - height / 2) / (height / 2)
    d = u + v
    outline = (d < 0.95) & (d > 0.70)
    wear = fbm(width, height, 6, 4, rng)
    alpha = np.where(outline, np.clip(wear * 1.6 - 0.25, 0, 0.9), 0.0)
    rgb = np.zeros((height, width, 3)) + np.array([0.85, 0.85, 0.82])
    return _with_alpha(rgb, alpha * (alpha > 0.03))


# --------------------------------------------------------------------------
# Night (M23)
# --------------------------------------------------------------------------

def night_sky(width=1024, height=512, seed=60):
    """Equirectangular night sky, rows TOP FIRST (it's written as a PNG, not
    a Blender image): row 0 is the zenith, the middle row the horizon.
    Deep blue overhead; a low orange band where a city's lights stain the
    haze; a few stars above it; faint clouds lit from below. sRGB."""
    rng = np.random.default_rng(seed)
    rows = np.linspace(0.0, 1.0, height)[:, None]          # 0 zenith .. 1 nadir
    height_above = np.clip((0.5 - rows) * 2.0, 0.0, 1.0)    # 1 zenith .. 0 horizon
    zenith = np.array([0.045, 0.055, 0.110])
    horizon = np.array([0.300, 0.200, 0.175])
    glow = np.exp(-height_above * 5.0)                       # city light, low
    rgb = zenith[None, None, :] * (1.0 - glow[:, :, None]) + horizon[None, None, :] * glow[:, :, None]
    rgb = np.broadcast_to(rgb, (height, width, 3)).copy()

    # Clouds: tileable around the horizon (value noise wraps in x).
    clouds = fbm(width, height, 8, 4, rng, stretch=(1, 3))
    cover = np.clip(clouds * 1.8 - 0.8, 0.0, 1.0) * (height_above > 0.02)
    lit_below = np.array([0.20, 0.13, 0.11]) * (0.3 + 0.7 * glow)[:, :, None]
    rgb = rgb * (1.0 - 0.45 * cover[:, :, None]) + lit_below * cover[:, :, None] * 0.35

    # Stars, fewer toward the horizon's haze, hidden by clouds.
    stars = rng.random((height, width)) > 0.9985
    twinkle = rng.uniform(0.35, 0.9, (height, width))
    star = stars * twinkle * np.clip(height_above * 2.0 - 0.15, 0.0, 1.0) * (1.0 - cover)
    rgb += star[:, :, None] * np.array([0.85, 0.87, 0.95])

    # Below the horizon: the dark ground the fog covers anyway.
    below = rows > 0.5
    rgb = np.where(below[:, :, None], horizon[None, None, :] * 0.6, rgb)
    return np.clip(rgb, 0.0, 1.0).astype(np.float32)


def neon_sign(width=64, height=256, seed=61, tube_color=(1.0, 0.25, 0.55), border_color=(0.25, 0.85, 1.0)):
    """A vertical shop sign: dark lacquered board, tube lettering. Returns
    (base, emissive mask), rows bottom first like the other materials.
    Unlit, the tubes read as pale glass; the mask lights exactly them."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    board = 0.8 + 0.2 * fbm(width, height, 4, 3, rng, stretch=(1, 4))
    base = _tint(board, (0.08, 0.06, 0.07))
    frame = (xs < 3) | (xs > width - 4) | (ys < 3) | (ys > height - 4)

    # Four "characters": rectangles of tube strokes, pink and a cyan border.
    tube = np.zeros((height, width), bool)
    for k in range(4):
        y0 = int(height * (0.08 + 0.225 * k))
        y1 = y0 + int(height * 0.17)
        cell = (ys >= y0) & (ys < y1) & (xs > width * 0.2) & (xs < width * 0.8)
        pattern = fbm(width, height, 5, 1, rng) > 0.3
        strokes = cell & (((ys - y0) % 12 < 3) | ((xs - int(width * 0.2)) % 14 < 3)) & pattern
        tube |= strokes
    border = (((xs >= 5) & (xs <= 7)) | ((xs >= width - 8) & (xs <= width - 6)) |
              ((ys >= 5) & (ys <= 7)) | ((ys >= height - 8) & (ys <= height - 6))) & ~frame
    base = np.where(frame[:, :, None], np.array([0.20, 0.18, 0.16]), base)
    base = np.where((tube | border)[:, :, None], np.array([0.55, 0.50, 0.55]), base)

    mask = np.zeros((height, width, 3))
    mask = np.where(tube[:, :, None], np.array(tube_color), mask)      # pink by default
    mask = np.where(border[:, :, None], np.array(border_color), mask)  # cyan by default
    return _rgba(base), _rgba(mask)


def lamp_glass(size=16):
    """Warm lamp glass: base and mask both flat (the whole face glows)."""
    flat = np.ones((size, size, 3)) * np.array([1.0, 0.86, 0.62])
    return _rgba(flat * 0.9), _rgba(flat)


# --------------------------------------------------------------------------
# City layers (M24)
# --------------------------------------------------------------------------

# Facade atlas styles, one per quadrant (column, row) of the atlas:
# (wall colour, window columns, window rows, window fill, lit share).
FACADE_STYLES = [
    ((0.46, 0.45, 0.43), 4, 8, 0.55, 0.35),   # concrete office block
    ((0.52, 0.47, 0.40), 3, 8, 0.45, 0.45),   # tiled apartments
    ((0.33, 0.24, 0.20), 5, 8, 0.35, 0.25),   # dark brick, narrow windows
    ((0.30, 0.31, 0.33), 4, 7, 0.60, 0.30),   # grey tower, shops below
]


def facade_atlas(size=256, seed=70):
    """Four facades in the quadrants of one texture (rows bottom first).
    Returns (base, emissive mask): by day a grid of dark glass, by night a
    scatter of lit windows - warm, a few cold fluorescent."""
    rng = np.random.default_rng(seed)
    half = size // 2
    base = np.zeros((size, size, 3))
    mask = np.zeros((size, size, 3))
    for index, (wall, columns, rows, fill, lit) in enumerate(FACADE_STYLES):
        x0, y0 = (index % 2) * half, (index // 2) * half
        grime = 0.8 + 0.2 * fbm(half, half, 6, 3, rng)
        region = _tint(grime, wall)
        glow = np.zeros((half, half, 3))
        cell_w, cell_h = half / columns, half / rows
        for row in range(rows):
            for column in range(columns):
                wx0 = int(column * cell_w + cell_w * (1 - fill) / 2)
                wx1 = int((column + 1) * cell_w - cell_w * (1 - fill) / 2)
                wy0 = int(row * cell_h + cell_h * 0.25)
                wy1 = int((row + 1) * cell_h - cell_h * 0.2)
                shop = index == 3 and row == 0
                if shop:  # a lit shopfront across the ground floor
                    wx0, wx1, wy0, wy1 = int(column * cell_w) + 1, int((column + 1) * cell_w) - 1, 2, int(cell_h) - 2
                region[wy0:wy1, wx0:wx1] = (0.07, 0.08, 0.09)
                if shop or rng.random() < lit:
                    tint = (1.0, 0.82, 0.55) if rng.random() > 0.2 else (0.75, 0.9, 1.0)
                    dim = rng.uniform(0.5, 1.0)
                    glow[wy0:wy1, wx0:wx1] = np.array(tint) * dim
                    region[wy0:wy1, wx0:wx1] = np.array(tint) * 0.35
        base[y0:y0 + half, x0:x0 + half] = region
        mask[y0:y0 + half, x0:x0 + half] = glow
    return _rgba(base), _rgba(mask)


def skyline(width=1024, height=256, seed=71):
    """A horizontally tileable strip of building silhouettes (rows bottom
    first): dark shapes with scattered lit windows, antennas on some roofs.
    Returns (base with alpha, emissive mask)."""
    rng = np.random.default_rng(seed)
    alpha = np.zeros((height, width))
    windows = np.zeros((height, width), bool)
    x = 0
    while x < width:
        w = int(rng.integers(18, 70))
        w = min(w, width - x)
        top = int(height * rng.uniform(0.25, 0.92))
        alpha[:top, x:x + w] = 1.0
        # Lit windows: a sparse grid inside the building.
        for wy in range(6, top - 4, 7):
            for wx in range(x + 3, x + w - 3, 6):
                if rng.random() < 0.18:
                    windows[wy:wy + 3, wx:wx + 2] = True
        if rng.random() < 0.3 and w > 10:  # an antenna
            ax = x + w // 2
            alpha[top:min(height, top + int(height * 0.08)), ax:ax + 1] = 1.0
        x += w
    base = np.zeros((height, width, 3)) + np.array([0.035, 0.038, 0.05])
    base = np.where(windows[:, :, None], np.array([0.6, 0.5, 0.35]), base)
    mask = np.where(windows[:, :, None], np.array([1.0, 0.8, 0.55]), np.zeros((height, width, 3)))
    return _with_alpha(base, alpha), _rgba(mask)


# --------------------------------------------------------------------------
# Night street (M25)
# --------------------------------------------------------------------------

def wet_asphalt(size=256, seed=81):
    """Asphalt after rain: darker, the patches darker still."""
    return asphalt(size, seed, color=(0.11, 0.11, 0.12))


# Shopfront atlas styles, one per quadrant (index % 2 across, // 2 up):
# (awning colour, interior light, sign lettering light); None = shuttered.
SHOP_STYLES = [
    ((0.45, 0.10, 0.08), (1.00, 0.78, 0.50), (1.00, 0.90, 0.70)),   # ramen: warm
    ((0.12, 0.33, 0.55), (0.90, 0.97, 1.00), (0.50, 0.85, 1.00)),   # convenience store: cold
    ((0.22, 0.07, 0.17), (1.00, 0.40, 0.32), (1.00, 0.30, 0.60)),   # bar: red
    None,                                                          # closed: shutter
]


def shopfront_atlas(size=256, seed=80):
    """Ground-floor shopfronts, one per quadrant; a quadrant covers a 4 m
    wide, 4 m tall front. Rows bottom first. Returns (base, emissive mask):
    lit glass (shelves and counters as darker bands), a coloured awning and
    a sign board whose lettering glows. The shuttered one only has a dim
    sign and a graffiti tag."""
    rng = np.random.default_rng(seed)
    half = size // 2
    base = np.zeros((size, size, 3))
    mask = np.zeros((size, size, 3))
    ys, xs = np.mgrid[0:half, 0:half]
    v, u = ys / half, xs / half
    for index, style in enumerate(SHOP_STYLES):
        x0, y0 = (index % 2) * half, (index // 2) * half
        grime = 0.8 + 0.2 * fbm(half, half, 6, 3, rng)
        region = _tint(grime, (0.30, 0.29, 0.28))            # concrete frame
        glow = np.zeros((half, half, 3))
        kick = v < 0.05
        glass = (v >= 0.05) & (v < 0.62) & (np.abs(u - 0.5) < 0.46) & (np.abs(u - 0.62) > 0.012)
        awning = (v >= 0.62) & (v < 0.70)
        board = (v >= 0.72) & (v < 0.95) & (np.abs(u - 0.5) < 0.47)
        glyphs = np.zeros((half, half), bool)
        for k in range(4):
            gx = 0.12 + 0.2 * k
            cell = board & (u > gx) & (u < gx + 0.14) & (v > 0.76) & (v < 0.91)
            glyphs |= cell & (fbm(half, half, 9, 1, rng) > 0.42)
        region = np.where(kick[:, :, None], np.array([0.10, 0.10, 0.10]), region)
        if style is None:
            # A rolled-down shutter over the whole front, ribbed.
            shutter = (v >= 0.05) & (v < 0.70) & (np.abs(u - 0.5) < 0.47)
            ribs = 0.75 + 0.25 * ((ys % 4) < 2) + 0.1 * fbm(half, half, 8, 2, rng, stretch=(4, 1))
            region = np.where(shutter[:, :, None], _tint(ribs, (0.42, 0.43, 0.44)), region)
            tag = shutter & (fbm(half, half, 5, 3, rng) > 0.66) & (v < 0.45)
            region = np.where(tag[:, :, None], np.array([0.55, 0.15, 0.40]), region)
            region = np.where(board[:, :, None], np.array([0.20, 0.19, 0.17]), region)
            region = np.where(glyphs[:, :, None], np.array([0.35, 0.33, 0.30]), region)
            glow = np.where(glyphs[:, :, None], np.array([0.12, 0.10, 0.08]), glow)
        else:
            awning_color, interior, lettering = style
            shelves = 0.55 + 0.45 * (((ys // 6) % 3) != 0) * (0.7 + 0.3 * fbm(half, half, 12, 2, rng))
            light = _tint(shelves, interior)
            region = np.where(glass[:, :, None], light * 0.45, region)
            glow = np.where(glass[:, :, None], light, glow)
            folds = 0.8 + 0.2 * np.cos(u * 40.0)
            region = np.where(awning[:, :, None], _tint(folds, awning_color), region)
            region = np.where(board[:, :, None], np.array([0.12, 0.11, 0.10]), region)
            region = np.where(glyphs[:, :, None], np.array(lettering) * 0.6, region)
            glow = np.where(glyphs[:, :, None], np.array(lettering), glow)
        base[y0:y0 + half, x0:x0 + half] = region
        mask[y0:y0 + half, x0:x0 + half] = glow
    return _rgba(base), _rgba(mask)


# Neon reflection colours, one per column of neon_reflection().
REFLECTION_COLORS = [
    (1.0, 0.25, 0.55),   # pink
    (0.25, 0.85, 1.0),   # cyan
    (1.0, 0.62, 0.15),   # amber
    (1.0, 0.85, 0.60),   # warm white (shop windows, lamps)
]


def neon_reflection(width=128, height=128, seed=82):
    """Sign light smeared on wet asphalt: a soft vertical streak per column
    (see REFLECTION_COLORS), strongest at v = 0 (under the light) and
    fading away from it, broken into ripples. A decal: returns (base with
    alpha, emissive mask)."""
    rng = np.random.default_rng(seed)
    columns = len(REFLECTION_COLORS)
    ys, xs = np.mgrid[0:height, 0:width]
    column = xs * columns // width
    u = (xs % (width // columns)) / (width // columns) - 0.5
    v = ys / height
    across = np.exp(-(u / 0.2) ** 2)
    along = np.clip(1.0 - v, 0.0, 1.0) ** 1.5
    ripples = 0.55 + 0.45 * fbm(width, height, 6, 3, rng, stretch=(1, 6))
    alpha = np.clip(across * along * ripples * 1.1, 0.0, 0.85)
    colors = np.array(REFLECTION_COLORS)[column]
    rgba = _with_alpha(colors * 0.3, alpha * (alpha > 0.02))
    return rgba, _rgba(colors * (alpha > 0.02)[:, :, None] * np.clip(alpha * 1.4, 0, 1)[:, :, None])


def puddle(size=128, seed=83):
    """A puddle: dark water with a ragged edge (decal)."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size]
    r = np.hypot(xs / size - 0.5, ys / size - 0.5) * 2.0
    shape = 1.0 - r + 0.45 * (fbm(size, size, 4, 3, rng) - 0.5)
    alpha = np.clip((shape - 0.25) * 6.0, 0.0, 0.85)
    rgb = np.zeros((size, size, 3)) + np.array([0.03, 0.035, 0.045])
    return _with_alpha(rgb, alpha)


def pachinko_front(width=512, height=256, seed=84):
    """The pachinko hall's front, 16 m wide and 8 m tall (rows bottom
    first): glass doors onto rows of bright machines, a red awning edged
    with bulbs, and a huge sign board with lettering outlined in bulbs.
    Returns (base, emissive mask)."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    v, u = ys / height, xs / width
    base = _tint(0.8 + 0.2 * fbm(width, height, 8, 3, rng), (0.16, 0.14, 0.14))
    glow = np.zeros((height, width, 3))

    doors = (v > 0.02) & (v < 0.40) & ((xs % 64) > 4)
    machines = 0.4 + 0.6 * ((xs % 10) < 6) * (0.6 + 0.4 * fbm(width, height, 24, 2, rng))
    hue = np.where(((xs // 10) % 3)[:, :, None] == 0, np.array([1.0, 0.55, 0.25]),
                   np.where(((xs // 10) % 3)[:, :, None] == 1, np.array([1.0, 0.3, 0.6]), np.array([0.5, 0.8, 1.0])))
    interior = hue * machines[:, :, None]
    base = np.where(doors[:, :, None], interior * 0.4, base)
    glow = np.where(doors[:, :, None], interior, glow)

    awning = (v >= 0.42) & (v < 0.52)
    base = np.where(awning[:, :, None], np.array([0.55, 0.06, 0.05]), base)
    bulbs = awning & ((xs % 8) < 3) & (np.abs(ys - height * 0.47) < 2)
    base = np.where(bulbs[:, :, None], np.array([0.9, 0.85, 0.6]), base)
    glow = np.where(bulbs[:, :, None], np.array([1.0, 0.9, 0.6]), glow)

    board = (v >= 0.55) & (v < 0.97)
    base = np.where(board[:, :, None], np.array([0.08, 0.03, 0.05]), base)
    edge = board & ((v < 0.57) | (v > 0.95) | (u < 0.01) | (u > 0.99))
    marquee = edge & (((xs + ys) % 6) < 2)
    base = np.where(marquee[:, :, None], np.array([0.9, 0.8, 0.5]), base)
    glow = np.where(marquee[:, :, None], np.array([1.0, 0.85, 0.5]), glow)
    for k in range(5):
        gx = 0.08 + 0.18 * k
        cell = board & (u > gx) & (u < gx + 0.13) & (v > 0.61) & (v < 0.91)
        strokes = cell & ((((ys // 5) % 3) == 0) | (((xs // 6) % 3) == 0)) & (fbm(width, height, 10, 1, rng) > 0.35)
        color = np.array([1.0, 0.25, 0.7]) if k % 2 == 0 else np.array([1.0, 0.75, 0.2])
        base = np.where(strokes[:, :, None], color * 0.5, base)
        glow = np.where(strokes[:, :, None], color, glow)
    return _rgba(base), _rgba(glow)


def train_side(width=256, height=64, seed=85):
    """One side of a commuter car, 18 m long and 3.2 m tall (rows bottom
    first): dull steel, a coloured line, a band of lit windows broken by
    doors. Returns (base, emissive mask)."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    v = ys / height
    base = _tint(0.8 + 0.2 * fbm(width, height, 8, 2, rng, stretch=(4, 1)), (0.55, 0.56, 0.57))
    base = np.where(((v > 0.28) & (v < 0.34))[:, :, None], np.array([0.1, 0.45, 0.25]), base)
    doors = ((xs % 64) > 26) & ((xs % 64) < 38)
    windows = (v > 0.42) & (v < 0.78) & ((xs % 16) > 1) & ~doors
    door_glass = doors & (v > 0.45) & (v < 0.75)
    lit = windows | door_glass
    base = np.where(doors[:, :, None] & ~door_glass[:, :, None], np.array([0.45, 0.46, 0.47]), base)
    base = np.where(lit[:, :, None], np.array([0.55, 0.58, 0.55]), base)
    glow = np.where(lit[:, :, None], np.array([0.85, 0.95, 0.9]), np.zeros((height, width, 3)))
    return _rgba(base), _rgba(glow)


def posters(width=128, height=128, seed=86):
    """Layers of torn posters on a wall (decal): coloured sheets, blocks of
    print, ragged edges."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    rgb = np.zeros((height, width, 3)) + np.array([0.75, 0.72, 0.62])
    alpha = np.zeros((height, width))
    palette = [(0.80, 0.25, 0.20), (0.85, 0.80, 0.55), (0.25, 0.40, 0.65), (0.90, 0.88, 0.82)]
    for k in range(6):
        x0, y0 = rng.integers(0, width - 50), rng.integers(0, height - 60)
        w, h = rng.integers(36, 56), rng.integers(44, 64)
        sheet = (xs >= x0) & (xs < x0 + w) & (ys >= y0) & (ys < y0 + h)
        rgb = np.where(sheet[:, :, None], np.array(palette[k % 4]), rgb)
        text = sheet & ((ys - y0) % 7 < 2) & ((xs - x0) > 4) & ((xs - x0) < w - 4)
        rgb = np.where(text[:, :, None], np.array([0.15, 0.13, 0.12]), rgb)
        alpha = np.where(sheet, 0.9, alpha)
    torn = fbm(width, height, 10, 3, rng) > 0.64
    alpha = np.where(torn, 0.0, alpha)
    rgb *= (0.7 + 0.3 * fbm(width, height, 5, 3, rng))[:, :, None]
    return _with_alpha(rgb, alpha)


def timetable(width=64, height=96, seed=87):
    """A back-lit bus timetable: columns of times on white. Returns (base,
    emissive mask) - the panel glows faintly."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    rgb = np.zeros((height, width, 3)) + np.array([0.88, 0.88, 0.84])
    header = ys > height * 0.82
    rgb = np.where(header[:, :, None], np.array([0.15, 0.30, 0.60]), rgb)
    times = (~header) & (ys % 6 < 2) & ((xs % 20) > 3) & ((xs % 20) < 15) & (rng.random((height, width)) > 0.15)
    rgb = np.where(times[:, :, None], np.array([0.10, 0.10, 0.12]), rgb)
    return _rgba(rgb), _rgba(rgb * 0.8)


# --------------------------------------------------------------------------
# Pachinko hall (M27)
# --------------------------------------------------------------------------

def carpet(size=256, seed=90):
    """Hall carpet: dark red with a gold diamond lattice, worn in patches."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:size, 0:size]
    u, v = (xs % 64) / 64.0 - 0.5, (ys % 64) / 64.0 - 0.5
    diamond = np.abs(np.abs(u) + np.abs(v) - 0.35) < 0.04
    dots = (np.abs(u) + np.abs(v)) < 0.06
    rgb = _tint(0.85 + 0.15 * fbm(size, size, 32, 2, rng), (0.32, 0.06, 0.08))
    rgb = np.where((diamond | dots)[:, :, None], np.array([0.55, 0.42, 0.15]), rgb)
    wear = np.clip(fbm(size, size, 4, 3, rng) * 1.6 - 0.7, 0, 1) * 0.35
    return _rgba(rgb * (1.0 - wear[:, :, None]))


def pachinko_face(width=64, height=128, seed=91):
    """A machine's playfield behind glass (rows bottom first): a painted
    field, a ring of lamps and a gold frame. Returns (base, emissive
    mask): the lamps and the brighter art glow."""
    rng = np.random.default_rng(seed)
    ys, xs = np.mgrid[0:height, 0:width]
    u, v = xs / width, ys / height
    art = fbm(width, height, 3, 3, rng)
    rgb = np.stack([0.25 + 0.6 * art, 0.12 + 0.3 * (1 - art), 0.45 + 0.4 * np.sin(v * 6.0) ** 2], axis=2)
    frame = (u < 0.06) | (u > 0.94) | (v < 0.04) | (v > 0.96)
    rgb = np.where(frame[:, :, None], np.array([0.75, 0.6, 0.25]), rgb)
    ring = np.abs(np.hypot((u - 0.5) * 1.0, (v - 0.55) * 0.5) - 0.36) < 0.03
    lamps = ring & (((xs + ys) % 6) < 3)
    rgb = np.where(lamps[:, :, None], np.array([1.0, 0.9, 0.5]), rgb)
    mask = np.where(lamps[:, :, None], np.array([1.0, 0.85, 0.45]), rgb * 0.25)
    mask = np.where(frame[:, :, None], np.array([0.0, 0.0, 0.0]), mask)
    return _rgba(rgb), _rgba(mask)


def machine_top(width=64, height=16, seed=92):
    """The lamp box on top of a machine: a coloured band of lights."""
    ys, xs = np.mgrid[0:height, 0:width]
    hue = np.where((xs // 8)[:, :, None] % 3 == 0, np.array([1.0, 0.25, 0.5]),
                   np.where((xs // 8)[:, :, None] % 3 == 1, np.array([1.0, 0.8, 0.2]), np.array([0.3, 0.8, 1.0])))
    lit = (ys > 3) & (ys < height - 3)
    rgb = np.where(lit[:, :, None], hue, np.array([0.1, 0.1, 0.12]))
    return _rgba(rgb * 0.8), _rgba(np.where(lit[:, :, None], hue, 0.0))


def ceiling_panel(size=32):
    """A fluorescent ceiling panel: a milky diffuser (the whole face glows)."""
    ys, xs = np.mgrid[0:size, 0:size]
    frame = (xs < 2) | (xs > size - 3) | (ys < 2) | (ys > size - 3)
    rgb = np.where(frame[:, :, None], np.array([0.6, 0.6, 0.6]), np.array([0.95, 0.97, 1.0]))
    return _rgba(rgb), _rgba(np.where(frame[:, :, None], 0.0, rgb))


def screen_placeholder(size=16):
    """What a machine screen shows in Blender; the engine replaces it with
    a live render texture (the level's "screens")."""
    flat = np.ones((size, size, 3)) * np.array([0.1, 0.12, 0.3])
    return _rgba(flat), _rgba(flat)
