import ctypes
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


READ = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32)


class Segment(ctypes.Structure):
    _fields_ = [('offset', ctypes.c_uint32), ('bytes', ctypes.c_uint32)]


class PCM(ctypes.Structure):
    _fields_ = [('rate', ctypes.c_uint32), ('frames', ctypes.c_uint32),
                ('count', ctypes.c_uint32), ('bits', ctypes.c_uint32), ('segments', Segment * 8)]


def reader(data):
    @READ
    def read(context, offset, output, size):
        if offset + size > len(data):
            return False
        ctypes.memmove(output, data[offset:offset + size], size)
        return True
    return read


def block(kind, data):
    return bytes([kind]) + len(data).to_bytes(3, 'little') + data


def voc(data):
    return b'Creative Voice File\x1a' + struct.pack('<3H', 26, 0x114, 0) + data + b'\0'


def midi(tracks, kind=1):
    return b'MThd' + struct.pack('>IHHH', 6, kind, len(tracks), 120) + b''.join(
        b'MTrk' + struct.pack('>I', len(track)) + track for track in tracks)


class OriginalMediaAudioTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        path = Path(cls.temporary.name) / 'media.so'
        source = Path(__file__).resolve().parents[1] / 'media_audio.c'
        subprocess.run(['cc', '-shared', '-fPIC', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        str(source), '-o', str(path)], check=True)
        cls.lib = ctypes.CDLL(str(path))
        cls.lib.duke_voc_probe.argtypes = [READ, ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(PCM)]
        cls.lib.duke_voc_probe.restype = ctypes.c_bool
        cls.lib.duke_pcm_read.argtypes = [READ, ctypes.c_void_p, ctypes.POINTER(PCM), ctypes.c_uint32,
                                         ctypes.c_void_p, ctypes.c_uint32]
        cls.lib.duke_pcm_read.restype = ctypes.c_bool
        cls.lib.duke_midi_merge.argtypes = [READ, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p,
                                           ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
        cls.lib.duke_midi_merge.restype = ctypes.c_bool

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_scatter_pcm_continuation_and_bounded_lookahead(self):
        data = voc(block(1, bytes([165, 0, 10, 20])) + block(2, bytes([30, 40])) + block(5, b'note'))
        pcm = PCM()
        self.assertTrue(self.lib.duke_voc_probe(reader(data), None, len(data), ctypes.byref(pcm)))
        self.assertEqual((pcm.frames, pcm.count, pcm.bits), (4, 2, 8))
        out = ctypes.create_string_buffer(5)
        self.assertTrue(self.lib.duke_pcm_read(reader(data), None, ctypes.byref(pcm), 1, out, 5))
        self.assertEqual(out.raw, bytes([20, 30, 40, 128, 128]))

    def test_signed_16_bit_source_downconverts_in_small_reads(self):
        samples = [-32768, -1, 0, 32767] * 200
        data = voc(block(9, struct.pack('<IBBHI', 11025, 16, 1, 4, 0) +
                         struct.pack('<' + 'h' * len(samples), *samples)))
        pcm = PCM()
        self.assertTrue(self.lib.duke_voc_probe(reader(data), None, len(data), ctypes.byref(pcm)))
        out = ctypes.create_string_buffer(len(samples))
        self.assertTrue(self.lib.duke_pcm_read(reader(data), None, ctypes.byref(pcm), 0, out, len(samples)))
        self.assertEqual(out.raw, bytes([0, 127, 128, 255]) * 200)
        self.assertFalse(self.lib.duke_voc_probe(reader(data[:-3]), None, len(data) - 3, ctypes.byref(PCM())))

    def test_rejects_unsupported_codec_and_too_many_segments(self):
        bad = voc(block(9, struct.pack('<IBBHI', 11025, 16, 1, 0, 0) + b'\0\0'))
        self.assertFalse(self.lib.duke_voc_probe(reader(bad), None, len(bad), ctypes.byref(PCM())))
        bad = voc(block(1, bytes([165, 0, 1])) + block(2, b'x') * 8)
        self.assertFalse(self.lib.duke_voc_probe(reader(bad), None, len(bad), ctypes.byref(PCM())))

    def test_midi_stable_time_order_running_status_and_final_end(self):
        data = midi([b'\0\x90\x3c\x64\x0a\x3d\x6e\x05\xff\x2f\0',
                     b'\0\xff\x51\x03\x07\xa1\x20\x0a\x80\x3c\0\x0a\xff\x2f\0'])
        expected = midi([b'\0\x90\x3c\x64\0\xff\x51\x03\x07\xa1\x20'
                         b'\x0a\x90\x3d\x6e\0\x80\x3c\0\x0a\xff\x2f\0'], kind=0)
        output = ctypes.create_string_buffer(256)
        written = ctypes.c_uint32()
        self.assertTrue(self.lib.duke_midi_merge(reader(data), None, len(data), output, len(output), ctypes.byref(written)))
        self.assertEqual(output.raw[:written.value], expected)
        self.assertFalse(self.lib.duke_midi_merge(reader(data), None, len(data), output, 26, ctypes.byref(written)))
        self.assertEqual(written.value, 0)
        bad = midi([b'\x81\x81\x81\x81\0\x90\x3c\x64'])
        self.assertFalse(self.lib.duke_midi_merge(reader(bad), None, len(bad), output, len(output), ctypes.byref(written)))


if __name__ == '__main__':
    unittest.main()
