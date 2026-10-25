import importlib.util
import hashlib
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('cnc_resources', Path(__file__).resolve().parents[1] / 'tools/build_cnc_resources.py')
cnc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cnc)


def mix(entries, modern=False):
    index, payload = bytearray(), bytearray()
    for name, content in entries:
        index.extend(struct.pack('<III', cnc.name_hash(name), len(payload), len(content)))
        payload.extend(content)
    return (b'\0' * 4 if modern else b'') + struct.pack('<HI', len(entries), len(payload)) + index + payload


class MusicFilterTest(unittest.TestCase):
    def test_nested_music_removed_while_effects_and_maps_preserved(self):
        inner = mix([('INTRO.AUD', b'music'), ('GUN.AUD', b'unchanged effect'),
                     ('SCG01EA.INI', b'unchanged map')])
        original = mix([('LOCAL.MIX', inner), ('SCORES.MIX', b'music archive'),
                        ('CONQUER.MIX', b'unchanged sprites')], modern=True)
        filtered = cnc.filter_mix(original, {'LOCAL.MIX': {'INTRO.AUD': None}, 'SCORES.MIX': None})
        modern, outer = cnc.read_mix(filtered)
        self.assertTrue(modern)
        self.assertNotIn(cnc.name_hash('SCORES.MIX'), outer)
        self.assertEqual(outer[cnc.name_hash('CONQUER.MIX')], b'unchanged sprites')
        _, children = cnc.read_mix(outer[cnc.name_hash('LOCAL.MIX')])
        self.assertEqual(children, {cnc.name_hash('GUN.AUD'): b'unchanged effect',
                                    cnc.name_hash('SCG01EA.INI'): b'unchanged map'})

    def test_missing_reviewed_path_rejected(self):
        with self.assertRaisesRegex(ValueError, 'missing'):
            cnc.filter_mix(mix([('GUN.AUD', b'effect')]), {'INTRO.AUD': None})

    def test_modern_payload_checksum_is_verified(self):
        data = bytearray(mix([('GUN.AUD', b'effect')], modern=True))
        struct.pack_into('<I', data, 0, 0x10000)
        data.extend(hashlib.sha1(b'effect').digest())
        self.assertEqual(cnc.read_mix(data)[1][cnc.name_hash('GUN.AUD')], b'effect')
        data[-1] ^= 1
        with self.assertRaisesRegex(ValueError, 'checksum'):
            cnc.read_mix(data)

    def test_ambiguous_or_out_of_bounds_index_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            cnc.read_mix(mix([('GUN.AUD', b'one'), ('GUN.AUD', b'two')]))
        broken = bytearray(mix([('GUN.AUD', b'effect')]))
        struct.pack_into('<I', broken, 14, 1000)
        with self.assertRaisesRegex(ValueError, 'out-of-bounds'):
            cnc.read_mix(bytes(broken))
        with self.assertRaises(ValueError):
            cnc.read_mix(b'\0' * 6)


if __name__ == '__main__':
    unittest.main()
