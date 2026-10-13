"""Keeps a committed .blend free of personal data (v0.0.17, M116).

Blender saves its UI with a scene: windows, screens, and the file
browser's last folders - e.g. C:\\Users\\<you>\\Documents. In a public
repository that is a personal path. This rewrites a .blend with only its
scene data (scenes, collections, objects, meshes, materials - no window
manager, no screens), and checks a .blend for absolute paths.

    blender -b --factory-startup <scene.blend> --python Tools/Blender/clean_scene.py            clean it, then check
    blender -b --factory-startup <scene.blend> --python Tools/Blender/clean_scene.py -- --check check only (exit 1 if not clean)

(normally through: pwsh Tools/Blender/export.ps1 -Scene ... , which checks;
and after saving in Blender: pwsh Tools/Blender/clean_scene.ps1 <scene.blend>)
Opened again in Blender, a cleaned file gets the default layout.
"""

import io
import os
import re
import sys

import bpy

# A drive path (C:\ or C:/) or a home folder: what must not be committed.
PERSONAL = re.compile(rb"[A-Za-z]:[\\/][^\x00]{0,80}|/Users/[^\x00/]{1,40}|/home/[^\x00/]{1,40}")


def raw_bytes(path):
    """The .blend's bytes, uncompressed (Blender compresses with zstd)."""
    with open(path, "rb") as file:
        data = file.read()
    if data[:4] == b"\x28\xb5\x2f\xfd":
        import zstandard  # bundled with Blender
        return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)).read()
    return data


def personal_paths(path):
    return sorted({m.group().decode("latin-1").split("\x00")[0] for m in PERSONAL.finditer(raw_bytes(path))})


def clean(path):
    temporary = path + ".clean.blend"
    bpy.data.libraries.write(temporary, set(bpy.data.scenes), path_remap="RELATIVE_ALL", compress=True)
    os.replace(temporary, path)


def main():
    path = bpy.data.filepath
    if not path:
        raise SystemExit("open the .blend: blender -b --factory-startup <scene.blend> --python clean_scene.py")
    if "--check" not in sys.argv:
        clean(path)
        print(f"cleaned: {os.path.basename(path)} (scene data only)")
    found = personal_paths(path)
    for value in found:
        print(f"PERSONAL DATA in {os.path.basename(path)}: {value}")
    if found:
        print("not clean: run Tools/Blender/clean_scene.ps1 on it before committing")
        sys.exit(1)
    print(f"clean: {os.path.basename(path)} holds no absolute path")


try:
    main()
except SystemExit:
    raise
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
