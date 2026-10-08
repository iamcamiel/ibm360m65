import unittest
from tn3270_console import Screen, encode_address


class ConsoleInputTests(unittest.TestCase):
    def screen(self):
        screen = Screen()
        screen.apply(bytes.fromhex('f5c3115d7f1d60115ad11d40133c5d7f40'))
        return screen

    def test_nip_read_buffer_contains_operator_reply(self):
        screen = self.screen()
        reply = "R 00,'NO'"
        screen.enter(reply)
        screen.apply(bytes.fromhex('f1c3115ad1'))
        data = screen.read_buffer()
        # NIP positions its channel read at field 1681, skips the three-byte
        # header and SF/attribute, then reads the reply from the terminal buffer.
        self.assertEqual(data[3 + 1681:3 + 1681 + 2], b'\x1d\x40')
        self.assertEqual(data[3 + 1681 + 2:3 + 1681 + 2 + len(reply)], reply.encode('cp037'))
        self.assertEqual(data[:3], b'\x7d' + encode_address(1682 + len(reply)))

    def test_shorter_reply_clears_old_input_and_preserves_fields(self):
        screen = self.screen()
        screen.enter('LONG REPLY')
        screen.enter('NO')
        self.assertEqual(screen.cells[1682:1692], 'NO'.encode('cp037') + b'\x40' * 8)
        self.assertEqual(screen.fields, {1919: 0x60, 1681: 0x40})

    def test_empty_enter_is_blank_in_nip_read_area(self):
        screen = self.screen()
        screen.enter('')
        self.assertEqual(screen.cells[1682:1762], b'\x40' * 80)

    def test_enter_submits_host_prefilled_command(self):
        screen = self.screen()
        screen.apply(b'\xf1\xc3\x11' + encode_address(1682) + 'K E,1'.encode('cp037'))
        packet = screen.enter('')
        self.assertTrue(packet.endswith(b'\x11' + encode_address(1682) + 'K E,1'.encode('cp037')))
        self.assertEqual(screen.cells[1682:1687], 'K E,1'.encode('cp037'))

    def test_clear_is_short_read_and_clears_locked_screen(self):
        screen = self.screen()
        self.assertEqual(screen.clear(), b'\x6d')
        self.assertFalse(screen.fields)
        self.assertEqual(screen.cells, bytearray(1920))

    def test_tso_unformatted_input_replaces_logon_prompt(self):
        screen = Screen(tso=True)
        screen.apply(b'\xf5\xc3' + 'IKJ54012A ENTER LOGON -'.encode('cp037'))
        packet = screen.enter('logon ibmuser')
        self.assertEqual(packet, b'\x7d' + encode_address(13) + 'LOGON IBMUSER'.encode('cp037'))
        self.assertEqual(screen.cells[13:], bytearray(1907))

    def test_keyboard_restore_resets_previous_aid(self):
        screen = self.screen()
        screen.tso = True
        screen.enter('TEST')
        screen.apply(b'\xf1\xc3')
        self.assertEqual(screen.aid, 0x60)

    def test_tso_solicited_read_retains_modified_input(self):
        screen = self.screen()
        screen.tso = True
        entered = screen.enter('LISTCAT')
        self.assertEqual(screen.read_modified(), entered)
        screen.apply(b'\xf1\xc1')
        self.assertEqual(screen.read_modified(), b'\x7d' + encode_address(screen.cursor))


if __name__ == '__main__':
    unittest.main()
