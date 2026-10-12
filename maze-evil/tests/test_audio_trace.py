import unittest
from analyse_audio_trace import analyse


class AudioTraceTest(unittest.TestCase):
    def test_device_clock_and_silent_onset(self):
        result = analyse([
            '42\tAudioTrace: input us=110000 sampled=100000 phase=2 x=220 y=120',
            'AudioTrace: tone us=130000 session=1 packet=0 duration=220 delay=0 queued=3520',
            'AudioTrace: render us=135000 voice=0 packet=0 duration=220 delay=0',
            'AudioTrace: uart us=140000 samples=160 nonzero=0',
            'AudioTrace: uart us=150000 samples=160 nonzero=1',
        ])
        tone = result['tones'][0]
        self.assertEqual(tone['fifo_ahead_ms'], 0)
        self.assertEqual(tone['render_wait_ms'], 5)
        self.assertEqual(tone['release_sample_to_submit_ms'], 30)
        self.assertEqual(tone['quiet_release_sample_to_uart_ms'], 50)

    def test_busy_uart_cannot_be_attributed_to_new_sound(self):
        result = analyse([
            'AudioTrace: uart us=110000 samples=160 nonzero=1',
            'AudioTrace: tone us=130000 session=1 packet=0 duration=220 delay=0 queued=19520',
            'AudioTrace: uart us=140000 samples=160 nonzero=1',
            'AudioTrace: tone_rejected us=150000 session=1 duration=120 delay=0',
        ])
        self.assertEqual(result['rejected'], 1)
        self.assertEqual(result['tones'][0]['fifo_ahead_ms'], 1000)
        self.assertNotIn('quiet_submit_to_uart_ms', result['tones'][0])
        self.assertNotIn('render_wait_ms', result['tones'][0])

    def test_reused_packet_is_paired_with_next_render(self):
        result = analyse([
            'AudioTrace: tone us=100000 session=1 packet=0 duration=220 delay=0 queued=3520',
            'AudioTrace: render us=110000 voice=0 packet=0 duration=220 delay=0',
            'AudioTrace: tone us=500000 session=1 packet=0 duration=220 delay=0 queued=3520',
            'AudioTrace: render us=520000 voice=0 packet=0 duration=220 delay=0',
        ])
        self.assertEqual([t['render_wait_ms'] for t in result['tones']], [10, 20])

    def test_parallel_group_has_no_fifo_wait_estimate(self):
        result = analyse([
            'AudioTrace: effect us=100000 session=1 packet=0 layers=3 track=0 queued=3840',
            'AudioTrace: effect_render us=101000 voice=0 packet=0 layers=3 track=0',
            'AudioTrace: effect_rejected us=102000 session=1 layers=5 track=255',
        ])
        self.assertEqual(result['accepted'], 1)
        self.assertEqual(result['rejected'], 1)
        self.assertEqual(result['tones'][0]['render_wait_ms'], 1)
        self.assertNotIn('fifo_ahead_ms', result['tones'][0])

    def test_phase_boundary_excludes_history_from_previous_case(self):
        result = analyse([
            'AudioTrace: effect us=100000 session=1 packet=0 layers=3 track=0 queued=3840',
            'AudioTrace: effect us=200000 session=1 packet=0 layers=3 track=0 queued=3840',
            'AudioTrace: effect_render us=201000 voice=0 packet=0 layers=3 track=0',
        ], start_us=150000)
        self.assertEqual(result['accepted'], 1)
        self.assertEqual(result['tones'][0]['render_wait_ms'], 1)

    def test_replayed_history_is_not_counted_twice(self):
        line = 'AudioTrace: tone us=100000 session=1 packet=0 duration=220 delay=0 queued=3520'
        self.assertEqual(analyse([line, line])['accepted'], 1)


if __name__ == '__main__':
    unittest.main()
