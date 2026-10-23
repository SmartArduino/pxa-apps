import ctypes
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


class OriginalTitlePCXTest(unittest.TestCase):
    def test_rle_palette_sampling_and_malformed_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = Path(__file__).resolve().parents[1]
            wrapper = root / 'wrapper.cpp'
            wrapper.write_text('#include "title-pcx.h"\nextern "C" bool decode(const unsigned char *s,unsigned n,unsigned char *o,unsigned char *p){return ol_decode_title_pcx(s,n,o,2,1,p);}\n')
            binary = root / 'pcx.so'
            subprocess.run(['c++', '-shared', '-fPIC', '-I', str(source), str(wrapper),
                            str(source / 'title-pcx.cpp'), '-o', str(binary)], check=True)
            decode = ctypes.CDLL(str(binary)).decode
            decode.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p]
            decode.restype = ctypes.c_bool
            header = bytearray(128)
            header[0:4] = bytes([10, 5, 1, 8])
            struct.pack_into('<4H', header, 4, 0, 0, 3, 1)
            header[65] = 1
            struct.pack_into('<H', header, 66, 4)
            palette = bytearray(768)
            palette[3:6] = bytes([128, 64, 32])
            # First scanline 0,0,0,0; second scanline 1,1,1,1.
            data = bytes(header) + b'\xc4\0\xc4\x01\x0c' + palette
            output, colors = ctypes.create_string_buffer(2), ctypes.create_string_buffer(512)
            self.assertTrue(decode(data, len(data), output, colors))
            self.assertEqual(output.raw, b'\x01\x01')
            self.assertEqual(struct.unpack_from('<H', colors.raw, 2)[0], 0x8204)
            bad = bytes(header) + b'\xc0\x01\x0c' + palette
            self.assertFalse(decode(bad, len(bad), output, colors))
            self.assertFalse(decode(data[:-1], len(data) - 1, output, colors))
            header[65] = 3
            bad = bytes(header) + data[128:]
            self.assertFalse(decode(bad, len(bad), output, colors))


if __name__ == '__main__':
    unittest.main()
