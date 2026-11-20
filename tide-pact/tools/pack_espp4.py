"""Sign an ESP32-P4 AOT build under the device installer's pxa- app ID."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package_dir", type=Path, help="unpacked esp32p4 --aot-only build")
    parser.add_argument("output", type=Path)
    parser.add_argument("--sdk", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sdk = args.sdk.resolve()
    tools = sdk / "tools"
    sys.path.insert(0, str(tools))
    from pxa_tools.build import signing_key

    stage_input = args.package_dir.resolve(strict=True)
    if sorted(p.name for p in (stage_input / "artifacts").iterdir()) != ["main.esp32-p4.aot"]:
        parser.error("build the input with --target esp32p4 --aot-only")
    output = args.output.resolve()
    if output.suffix != ".pxa":
        parser.error("output must end in .pxa")
    output.parent.mkdir(parents=True, exist_ok=True)
    metadata = json.loads((root / "package.json").read_text())
    metadata["id"] = "pxa-tide-pact"
    engine = json.loads((root / "pxa.lock").read_text())["toolchain"]["recipe"]["engine_abi"]
    key = signing_key(sdk, None)
    with tempfile.TemporaryDirectory(dir=output.parent) as directory:
        staging = Path(directory)
        package = staging / "package"
        package.mkdir()
        for folder in ("artifacts", "assets"):
            shutil.copytree(stage_input / folder, package / folder)
        manifest = staging / "package.json"
        manifest.write_text(json.dumps(metadata, ensure_ascii=False), encoding="utf-8")
        subprocess.run([sys.executable, tools / "package" / "build_package_manifest.py", manifest, package,
                        key, "esp32-p4", engine, "--artifact-mode", "aot"], check=True)
        container = staging / output.name
        subprocess.run([sys.executable, tools / "package" / "build_pxa_container.py", package, key, container,
                        "--streamable", "--codec", "deflate", "--chunk-size", "4096"], check=True)
        container.replace(output)


if __name__ == "__main__":
    main()
