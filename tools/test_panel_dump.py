"""Decoder and multi-reader coherence regressions; no hardware access."""
import unittest
from m65_panel_dump import capture


class PanelDumpTests(unittest.TestCase):
    def words(self):
        words = {0: 0x03602065, 0x7FC: 0x10004, 0x400: 0x504E4C31}
        words.update({0x400 + 4*n: 0 for n in range(1,24)})
        words[0x404] = 123
        words[0x408] = 1 | 2 | 8 | 16
        words[0x40C] = 512
        for bank in range(8): words[0x410 + 4*bank] = 0xFFFFFF
        words[0x410] &= ~(1 << 11)
        words[0x430] = 1 << 22
        words[0x434] = 1 << 7
        return words

    def test_panel_bit_labels_and_high_led_bits(self):
        result = capture(self.words().__getitem__)
        self.assertEqual(result['generation'], 123)
        self.assertEqual(result['switch_pressed_bits'][0], [11])
        self.assertEqual(result['led_asserted_bits'][0], [22,39])
        self.assertTrue(result['buttons_pressed']['power_on'])
        self.assertFalse(result['buttons_pressed']['load'])

    def test_old_image_is_rejected_before_alias_reads(self):
        words = self.words(); words[0x7FC] = 0x10003
        with self.assertRaisesRegex(ValueError, 'loaded image has 1.3'): capture(words.__getitem__)

    def test_concurrent_capture_retries(self):
        words = self.words(); calls = 0
        def read(offset):
            nonlocal calls
            if offset == 0x404:
                calls += 1
                if calls == 2: words[offset] = 124
            return words[offset]
        self.assertEqual(capture(read)['generation'], 124)
        self.assertEqual(calls, 4)

    def test_continuous_interference_is_reported(self):
        words = self.words()
        def read(offset):
            if offset == 0x404: words[offset] += 1
            return words[offset]
        with self.assertRaisesRegex(ValueError, 'repeatedly changed'): capture(read)


if __name__ == '__main__': unittest.main()
