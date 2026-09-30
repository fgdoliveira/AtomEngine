"""Level F: the pachinko hall (M27).

Behind the night street's pachinko front: a low room packed with rows of
machines under fluorescent panels. One mesh with a dense lightmap - the
ceiling panels as bake-only area lights, the machines' glowing faces and
lamp boxes bleeding colour onto the carpet and the aisles.

Each machine's screen is a quad of material pachinko_screen (or _b on
every other machine); the engine swaps that material's texture for a live
render texture running the attract loop (level file "screens").

Blender Z-up, origin at the entrance, the hall runs north (+Y, game -Z).
"""

import atom_kit as kit
from atom_street import Street

WIDTH, DEPTH, CEILING = 20.0, 18.0, 3.2
ROWS = (-6.0, -2.0, 2.0, 6.0)        # row centres (x); each row is back to back
ROW_Y = (2.6, 15.4)                 # row ends
ROW_HALF = 0.6                       # cabinet depth from the row's centre
PITCH = 0.85                         # machine spacing along a row
PLAYABLE = (-2.0, 7)                 # row centre and index: at the "machine" marker
PANELS = [(x, y) for x in (-8.0, -4.0, 0.0, 4.0, 8.0) for y in (3.0, 7.0, 11.0, 15.0)]


def _flat(m, material, x0, y0, x1, y1, z, down=False):
    tile = kit.tile_of(material)
    corners = [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
    uvs = [(x0 / tile, y0 / tile), (x1 / tile, y0 / tile), (x1 / tile, y1 / tile), (x0 / tile, y1 / tile)]
    if down:
        corners.reverse()
        uvs.reverse()
    m.quad(corners, material, uvs=uvs)


def _machine(m, x_face, y, facing, index, playable=False):
    """One machine on a row side whose face is the plane x = x_face,
    facing +X (facing=1) or -X (-1)."""
    out = x_face + facing * 0.01
    y0, y1 = y - 0.36, y + 0.36

    def panel(x, ya, yb, z0, z1, material):
        # Counter-clockwise seen from the aisle; u runs left to right as
        # seen from there (b is on the left), so the screens aren't mirrored.
        a, b = (yb, ya) if facing > 0 else (ya, yb)
        m.quad([(x, a, z0), (x, b, z0), (x, b, z1), (x, a, z1)], material,
               uvs=[(1, 0), (0, 0), (0, 1), (1, 1)])

    panel(out, y0, y1, 0.95, 1.9, "pachinko_face")
    panel(out + facing * 0.01, y - 0.16, y + 0.16, 1.28, 1.52,
          "pachinko_screen_play" if playable else "pachinko_screen" if index % 2 == 0 else "pachinko_screen_b")
    m.box((x_face + facing * 0.07, y, 0.82), (0.14, 0.72, 0.16), "metal_white",
          faces=[f for f in kit.SIDES + [(0, 0, 1), (0, 0, -1)] if f != (-facing, 0, 0)])
    m.box((x_face + facing * 0.04, y, 2.03), (0.08, 0.76, 0.2), "machine_top",
          faces=[f for f in kit.SIDES + [(0, 0, 1), (0, 0, -1)] if f != (-facing, 0, 0)])
    m.cylinder((x_face + facing * 0.6, y, 0.0), 0.17, 0.58, "black", segments=10)


def build_pachinko_hall(pieces, collision, materials, collection):
    level = Street(pieces, collision, materials, collection)
    m = kit.MeshBuilder(grid=2.0)
    x0, x1 = -WIDTH / 2, WIDTH / 2

    _flat(m, "carpet", x0, 0.0, x1, DEPTH, 0.0)
    _flat(m, "hall_wall", x0, 0.0, x1, DEPTH, CEILING, down=True)
    for px, py in PANELS:
        _flat(m, "ceiling_panel", px - 0.4, py - 0.8, px + 0.4, py + 0.8, CEILING - 0.01, down=True)

    # Walls, facing in; the entrance: glass doors, closed behind you.
    m.box((0, DEPTH + 0.05, CEILING / 2), (WIDTH, 0.1, CEILING), "hall_wall", faces=[(0, -1, 0)])
    m.box((x0 - 0.05, DEPTH / 2, CEILING / 2), (0.1, DEPTH, CEILING), "hall_wall", faces=[(1, 0, 0)])
    m.box((x1 + 0.05, DEPTH / 2, CEILING / 2), (0.1, DEPTH, CEILING), "hall_wall", faces=[(-1, 0, 0)])
    for xa, xb in ((x0, -1.2), (1.2, x1)):
        m.box(((xa + xb) / 2, -0.05, CEILING / 2), (xb - xa, 0.1, CEILING), "hall_wall", faces=[(0, 1, 0)])
    m.box((0, -0.05, 2.8), (2.4, 0.1, 0.8), "hall_wall", faces=[(0, 1, 0)])
    m.quad([(1.2, 0.0, 0.0), (-1.2, 0.0, 0.0), (-1.2, 0.0, 2.4), (1.2, 0.0, 2.4)], "black")
    m.quad([(1.1, 0.0 + kit.DECAL_OFFSET, 1.0), (0.2, 0.0 + kit.DECAL_OFFSET, 1.0),
            (0.2, 0.0 + kit.DECAL_OFFSET, 2.1), (1.1, 0.0 + kit.DECAL_OFFSET, 2.1)], "posters")
    # Posters along the side walls.
    for y in (4.0, 9.0, 14.0):
        wx = x0 + kit.DECAL_OFFSET
        m.quad([(wx, y - 0.7, 1.0), (wx, y + 0.7, 1.0), (wx, y + 0.7, 2.3), (wx, y - 0.7, 2.3)], "posters")

    # Machine rows: a cabinet block, machines on both faces, caps at the ends.
    for cx in ROWS:
        m.box((cx, sum(ROW_Y) / 2, 1.05), (2 * ROW_HALF, ROW_Y[1] - ROW_Y[0], 2.1), "metal_dark",
              faces=kit.SIDES + [(0, 0, 1)])
        count = int((ROW_Y[1] - ROW_Y[0]) / PITCH)
        for i in range(count):
            y = ROW_Y[0] + PITCH * (i + 0.5) + ((ROW_Y[1] - ROW_Y[0]) - count * PITCH) / 2
            _machine(m, cx - ROW_HALF, y, -1, i)
            # The one playable machine (v0.0.5) has its own screen material,
            # so the game can take over that screen alone.
            _machine(m, cx + ROW_HALF, y, 1, i + 1, playable=(cx, i) == PLAYABLE)
        m.box((cx, sum(ROW_Y) / 2, 2.35), (2 * ROW_HALF + 0.1, ROW_Y[1] - ROW_Y[0] + 0.1, 0.3), "machine_top",
              faces=kit.SIDES + [(0, 0, 1), (0, 0, -1)])

    # The prize counter at the back, a shelf of prizes behind it.
    m.box((0, 16.9, 0.55), (8.0, 0.6, 1.1), "wood_light")
    m.box((0, 17.8, 1.4), (8.0, 0.35, 2.4), "vending_front", faces=[(0, -1, 0), (0, 0, 1), (1, 0, 0), (-1, 0, 0)])
    level.add_visual("pachinko", m)

    level.add_marker("spawn", "entrance", 0.0, 1.3, 0)
    level.add_marker("entity", "exit_doors", 0.0, 0.2)
    level.add_marker("entity", "machine", -1.4 + 0.55, 9.0)
    level.add_marker("entity", "prize_counter", 0.0, 16.3)

    boxes = [((0, DEPTH / 2, -0.5), (WIDTH + 2, DEPTH + 2, 1.0)),
             ((0, DEPTH + 0.3, 1.6), (WIDTH + 2, 0.6, 3.2)), ((0, -0.3, 1.6), (WIDTH + 2, 0.6, 3.2)),
             ((x0 - 0.3, DEPTH / 2, 1.6), (0.6, DEPTH + 2, 3.2)), ((x1 + 0.3, DEPTH / 2, 1.6), (0.6, DEPTH + 2, 3.2)),
             ((0, 16.9, 0.55), (8.0, 0.6, 1.1)), ((0, 17.8, 1.4), (8.0, 0.4, 2.8))]
    for cx in ROWS:
        boxes.append(((cx, sum(ROW_Y) / 2, 1.2), (2 * ROW_HALF + 0.3, ROW_Y[1] - ROW_Y[0], 2.4)))
    level.add_collider("pachinko_col", boxes)
    return level


# Bake-only lights: every ceiling panel shines down; the machines' own glow
# (their emission) adds colour on top.
LIGHTS = [((px, py, CEILING - 0.05), (0, 0, 0), (0.8, 1.6), 45.0, (0.95, 0.97, 1.0)) for px, py in PANELS]
