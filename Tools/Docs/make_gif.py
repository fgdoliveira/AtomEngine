"""Turns numbered PNG frame sequences into looping GIFs.

    <blender python> Tools/Docs/make_gif.py <folder with stem_NNN.png> <out folder> [fps]

Every `<stem>_NNN.png` group in the folder becomes `<out>/<stem>.gif`:
frames halved in size, one 256-colour palette per GIF (median cut), looping.

Runs with Blender's bundled Python, which has numpy; the PNG reader and
the GIF encoder (LZW) are written here, so nothing else is needed.
"""

import os
import re
import struct
import sys
import zlib
from collections import defaultdict

import numpy as np


# --------------------------------------------------------------------------
# PNG reading (8-bit RGB/RGBA, non-interlaced: what stb_image_write writes)
# --------------------------------------------------------------------------

def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"{path}: not a PNG"
    pos, idat = 8, bytearray()
    width = height = channels = 0
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and colour in (2, 6) and interlace == 0, f"{path}: unsupported PNG"
            channels = 4 if colour == 6 else 3
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    raw = np.frombuffer(zlib.decompress(bytes(idat)), dtype=np.uint8)
    stride = width * channels
    rows = raw.reshape(height, stride + 1)
    out = np.zeros((height, stride), dtype=np.uint8)
    previous = np.zeros(stride, dtype=np.int32)
    for y in range(height):
        kind, line = rows[y, 0], rows[y, 1:].astype(np.int32)
        if kind == 0:
            current = line
        elif kind == 2:
            current = (line + previous) & 0xFF
        else:
            # Sub, Average and Paeth depend on the pixel to the left: per byte.
            current = np.zeros(stride, dtype=np.int32)
            for x in range(stride):
                left = current[x - channels] if x >= channels else 0
                up = previous[x]
                if kind == 1:
                    predictor = left
                elif kind == 3:
                    predictor = (left + up) // 2
                else:
                    up_left = previous[x - channels] if x >= channels else 0
                    p = left + up - up_left
                    pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
                    predictor = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                current[x] = (line[x] + predictor) & 0xFF
        out[y] = current
        previous = current
    return out.reshape(height, width, channels)[:, :, :3]


# --------------------------------------------------------------------------
# Palette (median cut) and GIF writing
# --------------------------------------------------------------------------

def median_cut(pixels, colours=256):
    boxes = [pixels]
    while len(boxes) < colours:
        # Split the box with the widest channel range.
        index = max(range(len(boxes)), key=lambda i: np.ptp(boxes[i], axis=0).max() if len(boxes[i]) > 1 else -1)
        box = boxes[index]
        if len(box) < 2:
            break
        channel = int(np.argmax(np.ptp(box, axis=0)))
        box = box[box[:, channel].argsort()]
        middle = len(box) // 2
        boxes[index:index + 1] = [box[:middle], box[middle:]]
    palette = np.array([b.mean(axis=0) for b in boxes], dtype=np.float32)
    return np.vstack([palette, np.zeros((colours - len(palette), 3), np.float32)])


def palette_lookup(palette):
    """Nearest palette entry for every 5-bit-per-channel colour."""
    levels = (np.arange(32) * 8 + 4).astype(np.float32)
    cube = np.stack(np.meshgrid(levels, levels, levels, indexing="ij"), axis=-1).reshape(-1, 3)
    nearest = np.empty(len(cube), dtype=np.uint8)
    for start in range(0, len(cube), 4096):
        chunk = cube[start:start + 4096]
        distance = ((chunk[:, None, :] - palette[None, :, :]) ** 2).sum(axis=2)
        nearest[start:start + 4096] = distance.argmin(axis=1)
    return nearest.reshape(32, 32, 32)


def lzw(indices, min_code_size=8):
    """GIF LZW: variable-width codes (up to 12 bits), packed LSB first."""
    clear, end = 1 << min_code_size, (1 << min_code_size) + 1
    table = {bytes([i]): i for i in range(clear)}
    next_code, size = end + 1, min_code_size + 1
    out, bits, count = bytearray(), 0, 0

    def emit(code):
        nonlocal bits, count
        bits |= code << count
        count += size
        while count >= 8:
            out.append(bits & 0xFF)
            bits >>= 8
            count -= 8

    emit(clear)
    word = b""
    for value in indices.tobytes():
        candidate = word + bytes([value])
        if candidate in table:
            word = candidate
            continue
        emit(table[word])
        if next_code < 4096:
            table[candidate] = next_code
            next_code += 1
            if next_code > (1 << size) and size < 12:
                size += 1
        else:
            emit(clear)
            table = {bytes([i]): i for i in range(clear)}
            next_code, size = end + 1, min_code_size + 1
        word = bytes([value])
    if word:
        emit(table[word])
    emit(end)
    if count:
        out.append(bits & 0xFF)
    return bytes(out)


def write_gif(path, frames, palette, fps):
    height, width = frames[0].shape[:2]
    delay = max(2, round(100 / fps))  # hundredths of a second
    with open(path, "wb") as f:
        f.write(b"GIF89a" + struct.pack("<HHBBB", width, height, 0xF7, 0, 0))
        f.write(np.clip(palette, 0, 255).astype(np.uint8).tobytes())
        f.write(b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00")  # loop forever
        for frame in frames:
            f.write(b"\x21\xF9\x04\x00" + struct.pack("<H", delay) + b"\x00\x00")
            f.write(b"\x2C" + struct.pack("<HHHHB", 0, 0, width, height, 0))
            data = lzw(frame)
            f.write(b"\x08")
            for start in range(0, len(data), 255):
                block = data[start:start + 255]
                f.write(bytes([len(block)]) + block)
            f.write(b"\x00")
        f.write(b"\x3B")


def make_gif(paths, out_path, fps):
    frames = []
    for path in paths:
        rgb = read_png(path).astype(np.float32)
        h, w = rgb.shape[0] // 2 * 2, rgb.shape[1] // 2 * 2
        frames.append(rgb[:h, :w].reshape(h // 2, 2, w // 2, 2, 3).mean(axis=(1, 3)))
    sample = np.concatenate([f[::3, ::3].reshape(-1, 3) for f in frames[::2]])
    palette = median_cut(sample)
    lookup = palette_lookup(palette)
    indexed = []
    for f in frames:
        q = np.clip(f, 0, 255).astype(np.uint8) >> 3
        indexed.append(lookup[q[:, :, 0], q[:, :, 1], q[:, :, 2]])
    write_gif(out_path, indexed, palette, fps)


def main():
    folder, out_folder = sys.argv[1], sys.argv[2]
    fps = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
    sequences = defaultdict(list)
    for name in sorted(os.listdir(folder)):
        match = re.fullmatch(r"(.+)_(\d{3})\.png", name)
        if match:
            sequences[match.group(1)].append(os.path.join(folder, name))
    os.makedirs(out_folder, exist_ok=True)
    for stem, paths in sorted(sequences.items()):
        out_path = os.path.join(out_folder, stem + ".gif")
        make_gif(paths, out_path, fps)
        print(f"{out_path}: {len(paths)} frames, {os.path.getsize(out_path) // 1024} KB")


if __name__ == "__main__":
    main()
