import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest

RECROF = Path(__file__).resolve().parents[1]
APPS = RECROF.parents[1]
sys.path.insert(0, str(RECROF / 'tools'))
SPEC = importlib.util.spec_from_file_location('librequake_resources', RECROF / 'tools/build_librequake_resources.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class LibreQuakeResourcesTest(unittest.TestCase):
    def test_resizes_palette_and_uv_without_changing_geometry_or_frames(self):
        header = bytearray(84)
        struct.pack_into('<4sI', header, 0, b'IDPO', 6)
        struct.pack_into('<6i', header, 48, 1, 8, 8, 2, 1, 1)
        texture = bytes(range(64))
        coordinates = struct.pack('<6i', 32, 6, 4, 0, 2, 6)
        geometry = b'original triangles and animated frame bytes'
        original = bytes(header) + struct.pack('<I', 0) + texture + coordinates + geometry
        result = MODULE.resize_mdl(original, 4)
        self.assertEqual(struct.unpack_from('<ii', result, 52), (4, 4))
        self.assertEqual(result[88:104], bytes(y * 8 + x for y in [1, 3, 5, 7] for x in [1, 3, 5, 7]))
        self.assertEqual(struct.unpack_from('<6i', result, 104), (32, 3, 2, 0, 1, 3))
        self.assertEqual(result[128:], geometry)
        self.assertEqual(result[:52], original[:52])
        self.assertEqual(result[60:84], original[60:84])
        with self.assertRaises(ValueError):
            MODULE.resize_mdl(original[:87], 4)

    def test_preview_exit_changes_only_entities_and_preserves_other_lumps(self):
        header = bytearray(124)
        struct.pack_into('<I', header, 0, 29)
        entities = b'{"classname" "trigger_changelevel" "map" "lq_e0m2"}\0'
        geometry = b'original world geometry'
        for index in range(15):
            struct.pack_into('<II', header, 4 + index * 8, 124, 0)
        struct.pack_into('<II', header, 4, 124, len(entities))
        struct.pack_into('<II', header, 12, 124 + len(entities), len(geometry))
        result = MODULE.preview_level(bytes(header) + entities + geometry)
        offset, length = struct.unpack_from('<II', result, 4)
        self.assertEqual(result[offset:offset + length], entities.replace(b'"lq_e0m2"', b'"start"'))
        offset, length = struct.unpack_from('<II', result, 12)
        self.assertEqual(result[offset:offset + length], geometry)

    def test_pack_rejects_escape_duplicate_and_invalid_data_bounds(self):
        original = MODULE.pack_pak({'maps/start.bsp': b'level', 'gfx/palette.lmp': b'colors'})
        self.assertEqual(MODULE.unpack_pak(original), {'maps/start.bsp': b'level', 'gfx/palette.lmp': b'colors'})
        with self.assertRaises(ValueError):
            MODULE.pack_pak({'../escape': b'bad'})
        bad = bytearray(original)
        _, offset, _ = struct.unpack_from('<4sII', bad)
        struct.pack_into('<I', bad, offset + 60, len(bad) * 2)
        with self.assertRaises(ValueError):
            MODULE.unpack_pak(bad)
        duplicate = MODULE.pack_pak({'map': b'a', 'MAP': b'b'})
        with self.assertRaises(ValueError):
            MODULE.unpack_pak(duplicate)

    def test_native_converter_and_guest_serialize_wide_triangles_identically(self):
        sdk = Path(os.environ.get('WASI_SDK_DIR', APPS.parents[1] / 'deps/pxa-system/.pxa/wasi-sdk-34.0-x86_64-linux'))
        clang = sdk / 'bin/clang'
        if not clang.is_file():
            self.skipTest('WASI SDK required for guest/native format contract')
        initializer = ('.facesfront=1,.triangleSize=4096,.vertindex0=700,.vertindex1=800,'
                       '.vertindex2=900,.triangleOffsetDataPos=0x1abcde,.triangleOffsetNumber=128,'
                       '.offsetS=42,.bufferOffset=1000,.startOffset=65536')
        macros = '#include <stdint.h>\n#define NO_MINIMIZE 0\n#define TRIANGLE_HAS_SENTINEL 0\n#define MTRIANGLE_HAS_OFFSET_DATA 1\n'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            results = []
            for name in ['tools/MCUPackConverter/model.h', 'vendor/quake/model.h']:
                text = (APPS / 'quake' / name).read_text()
                declaration = re.findall(r'typedef struct mtriangle_s\s*\{.*?\}\s*mtriangle_t;', text, re.S)[-1]
                record = macros + declaration + '\nstatic const mtriangle_t record={' + initializer + '};\n'
                source = root / 'contract.c'
                source.write_text(record + '#include <stdio.h>\nint main(void){return fwrite(&record,1,sizeof(record),stdout)==sizeof(record)?0:1;}\n')
                subprocess.run(['cc', str(source), '-o', str(root / 'contract')], check=True, capture_output=True)
                native = subprocess.check_output([str(root / 'contract')])
                self.assertEqual(len(native), 20)
                source.write_text(record + 'const void *probe(void){return &record;}\n')
                wasm = root / 'contract.wasm'
                subprocess.run([str(clang), '-O2', '-nostdlib', '-Wl,--no-entry', '-Wl,--export=probe',
                                str(source), '-o', str(wasm)], check=True, capture_output=True)
                binary = wasm.read_bytes()
                # The fixture contains a single 20-byte constant data segment.
                # Its bytes must match the native converter's actual C layout.
                self.assertIn(native, binary)
                results.append(native)
            self.assertEqual(results[0], results[1])


if __name__ == '__main__':
    unittest.main()
