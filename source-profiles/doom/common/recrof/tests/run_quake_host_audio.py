#!/usr/bin/env python3
"""Compare Host accumulation with the current vendored Quake mixing loop."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
sdk = here.parents[4] / "deps/pxa-system/libpxa"
quake = here.parents[2] / "quake"
source = (quake / "vendor/quake/snd_mix.c").read_text()
start = source.index("void SND_PaintChannelFrom8(portable_samplepair_t *paintbuffer, channel_t *ch, sfx_t *psfx, int count, int pos)\n{")
start = source.index("#if 1\n    int lvol", start)
end = source.index("\n}\n", start)
body = source[start:end]
with tempfile.TemporaryDirectory(prefix="pxa-quake-audio-") as name:
    out = Path(name)
    # Compile the actual upstream gain quantization and scalar loop, without
    # its flash-address adapter or the optional Host hook. No copied reference.
    (out / "quake_original_mix.h").write_text(
        "static void original(portable_samplepair_t *paintbuffer, channel_t *ch, "
        "const signed char *sfx, int count) {\nint i, data;\n" + body + "\n}\n")
    command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-g",
               "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
               "-fno-omit-frame-pointer", "-DPXA_QUAKE_HOST_AUDIO_MIX=0",
               "-I" + str(out), "-I" + str(sdk / "include"),
               str(here / "quake_host_audio_test.c"),
               str(sdk / "src/services/audio/audio_mix.c"),
               str(sdk / "src/core/wire.c"),
               "-o", str(out / "test")]
    subprocess.run(command, check=True)
    subprocess.run([str(out / "test")], check=True,
        env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1",
                 ASAN_OPTIONS="detect_leaks=1"))
