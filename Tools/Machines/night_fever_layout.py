"""Writes Games/Demo/Assets/Machines/night_fever.json: the playable pachinko machine's
layout (v0.0.5). The machine file is the data the game reads; this script
is only how it was laid out - nail rows as chords of the board's circle,
with gaps left for the reels and the pockets. Edit and rerun:

    python Tools/Machines/night_fever_layout.py
"""

import json
import math
import os

CX, CY = 160, 122          # the board's centre on the 320x240 screen
OUTER, INNER = 110, 101    # the outer rail and the launch lane's inner guide
NAIL_RADIUS = 90           # nails stay inside this circle


def polar(r, degrees):
    """A point on a circle round the centre; 0 = right, 90 = up."""
    a = math.radians(degrees)
    return [round(CX + r * math.cos(a), 2), round(CY - r * math.sin(a), 2)]


def cup(x0, x1, y0, y1):
    """A pocket's walls: two sides and a floor, open at the top."""
    return [[x0, y0, x0, y1], [x1, y0, x1, y1], [x0, y1, x1, y1]]


def nail_rows():
    rows = []
    for k, y in enumerate(range(26, 196, 12)):
        half = math.sqrt(max(0.0, NAIL_RADIUS ** 2 - (y - CY) ** 2)) - 4
        if half < 8:
            continue  # the circle is too narrow here
        offset = 6 if k % 2 else 0
        x0 = math.ceil((CX - half - offset) / 12) * 12 + offset
        skip = []
        if 44 <= y <= 110:
            skip.append([122, 198])                 # the reels, and room over the roof
        if 98 <= y <= 136:
            skip.append([102, 218])                 # the roads and the start pocket
        if 138 <= y <= 160:
            skip += [[90, 110], [210, 230]]         # the side pockets
        if 184 <= y <= 210:
            skip.append([138, 182])                 # the attacker
        row = {"y": y, "x0": x0, "x1": round(CX + half, 1), "spacing": 12}
        if skip:
            row["skip"] = skip
        rows.append(row)
    return rows


def road_nails():
    """Two slanted rows of nails closer together than a ball ("road
    nails"), a little wider apart than a ball: some balls bounce along
    them toward the start pocket, others slip through the gaps."""
    points = []
    for (x0, y0), (x1, y1) in (((110, 103), (146, 117)), ((210, 103), (174, 117))):
        count = 5
        for i in range(count):
            t = i / (count - 1)
            points.append([round(x0 + (x1 - x0) * t, 1), round(y0 + (y1 - y0) * t, 1)])
    return points


def machine():
    walls = []
    walls += cup(152, 168, 122, 131)      # start pocket
    walls += cup(95, 105, 144, 153)       # side pockets: narrow, they pay well
    walls += cup(215, 225, 144, 153)
    walls += cup(142, 178, 194, 204)      # attacker (its lid is the gate)
    # The reel frame, with a pitched roof: a flat top would hold balls.
    walls += [[128, 64, 160, 54], [160, 54, 192, 64], [192, 64, 192, 98], [192, 98, 128, 98], [128, 98, 128, 64]]
    walls.append(polar(OUTER, 217) + polar(INNER, 217))    # the lane's floor
    walls.append(polar(INNER, 217) + [150, 232])           # board floor, left
    walls.append(polar(OUTER, -25) + [170, 232])           # board floor, right
    walls.append(polar(OUTER, 40) + polar(95, 46))         # stopper past the top
    return {
        "$schema": "../Schemas/machine.schema.json",
        "name": "Night Fever",
        "field": {"min": [50, 4], "max": [270, 238]},
        "walls": walls,
        "arcs": [
            {"center": [CX, CY], "radius": OUTER, "from": 217, "to": -25, "segments": 60},
            {"center": [CX, CY], "radius": INNER, "from": 217, "to": 118, "segments": 24},
        ],
        "nails": {"radius": 1, "rows": nail_rows(), "points": [[136, 190], [184, 190]] + road_nails()},
        "launch": {"position": polar(105.5, 210), "direction": [-0.5, -0.866],
                   "minSpeed": 400, "maxSpeed": 470, "perSecond": 1.7, "jitter": 0.015},
        "pockets": [
            {"name": "start", "kind": "start", "min": [153, 123], "max": [167, 130], "payout": 3},
            {"name": "left", "kind": "side", "min": [96, 145], "max": [104, 152], "payout": 7},
            {"name": "right", "kind": "side", "min": [216, 145], "max": [224, 152], "payout": 7},
            {"name": "attacker", "kind": "attacker", "min": [143, 195], "max": [177, 203], "payout": 12},
            {"name": "out", "kind": "out", "min": [148, 226], "max": [172, 238]},
            {"name": "foul", "kind": "foul", "min": [66, 180], "max": [84, 192]},
        ],
        "gate": [142, 194, 178, 194],
        "reels": {"min": [130, 66], "max": [190, 96]},
        # About 1 start in 12 balls and a hit 1 spin in 99: a fever every
        # ~1100 balls. Outside a fever the machine pays back about what it
        # takes (side pockets 7, start 3); a fever is 8 rounds of
        # up to 9 balls into the attacker, 12 each.
        "rules": {"odds": 99, "maxHeld": 4, "reachChance": 0.12, "spinSeconds": 2.4, "reachSeconds": 2.0,
                  "resultSeconds": 1.2, "feverRounds": 8, "ballsPerRound": 9, "roundSeconds": 25,
                  "intervalSeconds": 1.5},
    }


if __name__ == "__main__":
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    path = os.path.join(root, "Games", "Demo", "Assets", "Machines", "night_fever.json")
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        json.dump(machine(), file, indent=2)
        file.write("\n")
    print("Wrote", os.path.relpath(path, root))
