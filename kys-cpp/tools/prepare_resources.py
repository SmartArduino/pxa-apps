#!/usr/bin/env python3
"""Prepare an existing kys-cpp game tree for PXA; never downloads game data."""
import argparse
import io
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tempfile
import zipfile


def normalized(name):
    path = PurePosixPath(name.replace("\\", "/"))
    if path.is_absolute() or not path.parts or any(p in ("..", ".") for p in path.parts):
        raise ValueError(f"Unsafe resource path: {name}")
    return str(path).lower()


def convert_image(data):
    from PIL import Image
    with Image.open(io.BytesIO(data)) as image:
        if getattr(image, "n_frames", 1) != 1:
            raise ValueError("Animated WebP needs explicit frame conversion")
        output = io.BytesIO()
        image.convert("RGBA").save(output, format="PNG")
        return output.getvalue()


def prepare(source, destination):
    source = source.resolve()
    destination = destination.resolve()
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError("Source and destination must be separate trees")
    if destination.exists():
        raise ValueError("Destination must not exist; existing files are never replaced")
    files = {}
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Symlink is not a resource: {path}")
        if path.is_file():
            name = normalized(path.relative_to(source).as_posix())
            output = str(PurePosixPath(name).with_suffix(".png")) if name.endswith(".webp") else name
            if output in files:
                raise ValueError(f"Case/format collision: {output}")
            files[output] = path
    for name in ("config/kysmod.ini", "font/chinese.ttf"):
        if name not in files:
            raise ValueError(f"Missing {name}: pass the actual game directory")
    if not any(p in files for p in ("save/0.db", "save/0.zip")):
        raise ValueError("Current upstream needs save/0.db or save/0.zip; legacy .grp data needs upstream conversion")
    midi = [p for p in files if p.startswith("music/") and p.endswith(".mid")
            and str(PurePosixPath(p).with_suffix(".ogg")) not in files]
    soundfont = files.get("music/mid.sf2")
    if midi and (not soundfont or not shutil.which("fluidsynth") or not shutil.which("ffmpeg")):
        raise ValueError("MIDI music needs game/music/mid.sf2, fluidsynth and ffmpeg for offline rendering")
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Publish only a complete tree. A failed conversion does not leave a usable-looking result.
    with tempfile.TemporaryDirectory(prefix=".kys-prepare-", dir=destination.parent) as staging:
        root = Path(staging) / "game"
        root.mkdir()
        for name, path in files.items():
            out = root / name
            out.parent.mkdir(parents=True, exist_ok=True)
            if path.suffix.lower() == ".webp":
                out.write_bytes(convert_image(path.read_bytes()))
            elif path.suffix.lower() == ".zip":
                names = set()
                with zipfile.ZipFile(path) as src, zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as dst:
                    for info in src.infolist():
                        if info.is_dir():
                            continue
                        entry = normalized(info.filename)
                        if entry.endswith(".webp"):
                            entry = str(PurePosixPath(entry).with_suffix(".png"))
                        if entry in names:
                            raise ValueError(f"Duplicate ZIP resource: {path}: {entry}")
                        names.add(entry)
                        data = src.read(info)
                        if info.filename.lower().endswith(".webp"):
                            data = convert_image(data)
                        dst.writestr(entry, data)
            else:
                shutil.copyfile(path, out)
        # The PXA read-only media bridge opens known paths; it does not expose
        # an ambient directory iterator. Use upstream's ZIP image-group reader.
        resource = root / "resource"
        if resource.is_dir():
            for group in sorted(resource.iterdir()):
                if not group.is_dir() or not any(p.suffix == ".png" for p in group.iterdir()):
                    continue
                archive = Path(str(group) + ".zip")
                if archive.exists():
                    raise ValueError(f"Both loose and ZIP texture groups supplied: {group.name}")
                with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
                    for entry in sorted(group.rglob("*")):
                        if entry.is_file():
                            z.write(entry, entry.relative_to(group).as_posix())
                shutil.rmtree(group)
        for name in midi:
            wav = Path(staging) / "render.wav"
            ogg = (root / name).with_suffix(".ogg")
            if ogg.exists():
                continue
            subprocess.run(["fluidsynth", "-ni", "-r", "16000", "-F", str(wav), str(soundfont), str(files[name])], check=True)
            subprocess.run(["ffmpeg", "-nostdin", "-y", "-i", str(wav), "-ar", "16000", "-ac", "1", "-c:a", "libvorbis", "-q:a", "3", str(ogg)], check=True)
            wav.unlink()
        root.rename(destination)
    return len(files)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    count = prepare(args.source, args.destination)
    print(f"Prepared {count} files in {args.destination}; full gameplay remains to be tested")
