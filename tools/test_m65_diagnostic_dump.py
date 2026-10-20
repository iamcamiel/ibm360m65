"""Protocol/decoder regressions without hardware access."""
import unittest
from m65_diagnostic_dump import Diagnostics, BASE


class Fake:
    def __init__(self):
        self.writes = []
        self.seq = 0
        self.words = {0: 0x03602065, 0x7F8: 0x10026, 0x7FC: 0x10007, BASE: 0x44424731}
        self.reply = 0
        self.finished = True

    def read(self, offset):
        if offset in (BASE + 4, BASE + 8):
            return self.seq if self.finished or offset == BASE + 4 else self.seq - 1
        if offset == BASE + 12:
            return self.reply
        return self.words.get(offset, 0)

    def write(self, offset, value):
        assert offset == BASE
        self.writes.append(value)
        self.seq = (self.seq + 1) & 0xFFFFFFFF
        op, arg = ((value >> 24) - 0xA0), value & 0xFFFFFF
        if op == 0:
            self.reply = 2 | (2 << 12)
        elif op == 7:
            self.reply = {0: 1, 1: 0xFFFFFFFE, 2: 2, 3: 2, 6: 2, 7: 3, 8: 0x3A48, 9: 3, 10: 0xA47}[arg]
        elif op == 11:
            self.reply = [0x3A48, 3, 3][arg]
        elif op in (1, 2, 3, 4, 8, 9, 10, 12):
            self.reply = 0
        elif op == 5:
            self.reply = 0xE0000001 if arg == 33 else arg
        elif op == 6:
            self.reply = [0x1000010, 50, 1, 0, 0x2036, 75, 1, 0][arg]


class Tests(unittest.TestCase):
    def test_complete_decode_and_no_implicit_control(self):
        f = Fake()
        result = Diagnostics(f.read, f.write).dump()
        self.assertTrue(result['event_reason']['breakpoint'])
        self.assertEqual(result['event_breakpoint']['hits'], 3)
        self.assertEqual(result['breakpoint']['rosar'], 'A48')
        self.assertEqual(result['event_core_tick'], (2 << 32) | 0xFFFFFFFE)
        self.assertEqual(result['at_event']['ic'], 'E0000001')
        self.assertEqual([x['rosar'] for x in result['history']], ['010', '036'])
        self.assertTrue(result['history'][0]['event'])
        self.assertTrue(result['history'][1]['stor_check'])
        self.assertEqual(result['history'][1]['core_tick'], (1 << 32) | 75)
        self.assertTrue(all((x >> 24) - 0xA0 in (0, 5, 6, 7, 11) for x in f.writes))

    def test_configuration_is_explicit_and_never_arms_or_releases(self):
        f = Fake(); d = Diagnostics(f.read, f.write)
        d.configure_breakpoint(0xA48, 128)
        self.assertEqual(f.writes, [0xAA000080, 0xA9003A48])
        f.writes.clear(); d.configure_breakpoint(0xA49, 2, 'entry')
        self.assertEqual(f.writes, [0xAA000002, 0xA9001A49])

    def test_invalid_configuration_never_writes(self):
        for address, hit, mode in [(4096, 1, 'p0'), (-1, 1, 'p0'), (0, 0, 'p0'),
                                   (0, 1 << 24, 'p0'), (0, 1, 'bad')]:
            f = Fake()
            with self.assertRaises(ValueError):
                Diagnostics(f.read, f.write).configure_breakpoint(address, hit, mode)
            self.assertEqual(f.writes, [])

    def test_configuration_rejection_does_not_continue(self):
        f = Fake()
        def reject(offset, value):
            f.write(offset, value); f.reply = 0xE0000002
        with self.assertRaises(RuntimeError):
            Diagnostics(f.read, reject).configure_breakpoint(0xA48, 128)
        self.assertEqual(f.writes, [0xAA000080])

    def test_old_image_rejected_before_any_write(self):
        f = Fake(); f.words[0x7F8] = 0x10024
        with self.assertRaises(ValueError):
            Diagnostics(f.read, f.write)
        self.assertEqual(f.writes, [])

    def test_sequence_wrap(self):
        f = Fake(); f.seq = 0xFFFFFFFF
        Diagnostics(f.read, f.write).command(0)
        self.assertEqual(f.seq, 0)

    def test_no_retry_on_missing_reply(self):
        f = Fake(); f.finished = False
        with self.assertRaises(TimeoutError):
            Diagnostics(f.read, f.write, timeout=0).command(0)
        self.assertEqual(len(f.writes), 1)

    def test_pending_previous_command_is_not_overwritten(self):
        f = Fake(); f.words[BASE + 16] = 1
        with self.assertRaises(TimeoutError):
            Diagnostics(f.read, f.write, timeout=0).command(3)
        self.assertEqual(f.writes, [])

    def test_invalid_control_is_rejected(self):
        f = Fake()
        with self.assertRaises(ValueError):
            Diagnostics(f.read, f.write).command(13)
        self.assertEqual(f.writes, [])

    def test_event_edge_rejection_is_not_decoded_as_status(self):
        f = Fake()
        def write(offset, value):
            f.write(offset, value)
            f.reply = 0xE0000003
        with self.assertRaisesRegex(RuntimeError, 'Status unavailable'):
            Diagnostics(f.read, write).status()


if __name__ == '__main__':
    unittest.main()
