"""Create a private Core 2 voxel test copy without changing production apps.

Usage: python stage_probe.py APPS_ROOT NEW_DESTINATION
Use apps commit 6aa21e18560a2413a4cdb5e5cb3ccba5a2263ad7 for this report.
"""
from pathlib import Path
import argparse
import json
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apps_root", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    source, target = args.apps_root.resolve(), args.destination.resolve()
    if target.exists():
        parser.error("destination must not exist; an independent copy is required")
    for directory in ("voxel-craft-cpp", "common"):
        if not (source / directory).is_dir():
            parser.error(f"missing source directory: {directory}")
    if source == target or source in target.parents:
        parser.error("destination must be outside the production apps repository")
    ignore = shutil.ignore_patterns("docs", ".git", ".dev-signing", "build", ".build")
    for directory in ("voxel-craft-cpp", "common"):
        shutil.copytree(source / directory, target / directory, ignore=ignore)
    app = target / "voxel-craft-cpp"
    manifest = json.loads((app / "package.json").read_text())
    for key in ("min_sdk", "target_sdk", "compile_sdk"):
        manifest[key] = [2, 0]
    manifest["id"] = "pxa-next-voxel-probe"
    manifest["name"] = "Core 2 voxel probe"
    (app / "package.json").write_text(json.dumps(manifest, indent=2) + "\n")
    path = app / "main.cpp"
    text = path.read_text()
    anchor = "        options.direct_scanout = true;\n"
    if text.count(anchor) != 1 or "VOXEL_BORROW_COMMANDS" in text:
        raise RuntimeError("source differs from expected benchmark base; review before adapting")
    path.write_text(text.replace(anchor, anchor +
        "#if defined(VOXEL_BORROW_COMMANDS) && VOXEL_BORROW_COMMANDS\n"
        "        options.borrow_commands = true;\n"
        "#endif\n"))
    print(app)


if __name__ == "__main__":
    main()
