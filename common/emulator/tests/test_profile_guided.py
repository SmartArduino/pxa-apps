#!/usr/bin/env python3
"""Exercise profile replacement and opt-out with an actual cached CMake build."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    for tool in ("clang", "cmake", "llvm-profdata"):
        if not shutil.which(tool):
            raise SystemExit(f"Missing tool: {tool}")
    source = root / "source"
    source.mkdir()
    module = Path(__file__).resolve().parents[1] / "ProfileGuided.cmake"
    (source / "core.c").write_text(
        "int work(int x) { return x > 10 ? x * 2 : x + 3; }\n")
    (source / "main.c").write_text(
        "extern int work(int); int main(int argc, char **argv) {\n"
        "volatile int sum = 0; for (int i=0; i<1000; ++i)\n"
        "sum += work(argc > 1 ? i : 0); return sum == 0; }\n")
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\nproject(profile_cache C)\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
        "if(EMU_NATIVE_TEST)\nset(core emu_core)\nelse()\nset(core pxa_main)\nendif()\n"
        "add_library(${core} STATIC core.c)\n"
        "add_executable(probe main.c)\ntarget_link_libraries(probe PRIVATE ${core})\n"
        f'include("{module.as_posix()}")\n')
    log = (root / "commands.log").open("w")
    env = dict(os.environ)
    env.pop("PXA_EMU_PGO_PROFILE", None)

    def run(cmd, expected=0, overrides=None):
        result = subprocess.run(list(map(str, cmd)), env=env | (overrides or {}),
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log.write(json.dumps(list(map(str, cmd))) + "\n" + result.stdout)
        log.flush()
        assert (result.returncode == 0) == (expected == 0), result.stdout
        return result

    train, build = root / "train", root / "build"
    configure = ["cmake", "-S", source, "-DCMAKE_C_COMPILER=clang"]
    run(configure + ["-B", train, "-DEMU_NATIVE_TEST=ON", "-DEMU_PGO_GENERATE=ON"])
    run(["cmake", "--build", train])
    profiles = []
    for index, extra in enumerate(([], ["other-branch"])):
        raw, profile = root / f"{index}.profraw", root / f"{index}.profdata"
        run([train / "probe"] + extra, overrides={"LLVM_PROFILE_FILE": str(raw)})
        run(["llvm-profdata", "merge", raw, "-o", profile])
        profiles.append(profile)
    assert profiles[0].read_bytes() != profiles[1].read_bytes()
    selected = root / "selected.profdata"

    def command():
        commands = json.loads((build / "compile_commands.json").read_text())
        return next(item["command"] for item in commands if item["file"].endswith("/core.c"))

    # Identical pathname, different profile contents must change object flags.
    flags = []
    for profile in profiles:
        shutil.copyfile(profile, selected)
        run(configure + ["-B", build], overrides={"PXA_EMU_PGO_PROFILE": str(selected)})
        run(["cmake", "--build", build], overrides={"PXA_EMU_PGO_PROFILE": str(selected)})
        flags.append(command())
        assert "-fprofile-instr-use=" in flags[-1]
    assert flags[0] != flags[1], "Replaced profile did not invalidate cached objects"
    # Reusing the same build directory without the opt-in must remove all PGO.
    run(configure + ["-B", build])
    run(["cmake", "--build", build])
    assert "-fprofile" not in command(), "Profile leaked into a default build"
    run(configure + ["-B", build], expected=1,
        overrides={"PXA_EMU_PGO_PROFILE": str(root / "missing.profdata")})
    run(configure + ["-B", root / "bad-generate", "-DEMU_PGO_GENERATE=ON"], expected=1)
    run(configure + ["-B", root / "bad-both", "-DEMU_NATIVE_TEST=ON",
                    "-DEMU_PGO_GENERATE=ON", f"-DEMU_PGO_PROFILE={selected}"], expected=1)
    (root / "result.json").write_text(json.dumps({"passed": [
        "native collection", "profile use", "replacement invalidates objects",
        "unset removes PGO", "missing profile rejected", "Guest collection rejected",
        "conflicting modes rejected"]}, indent=2) + "\n")
    print("PASS profile generation, replacement, opt-out and invalid configuration")


if __name__ == "__main__":
    main()
