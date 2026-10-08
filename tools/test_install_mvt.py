import unittest

from install_mvt import completion_status, configure_stage1


class Stage1Tests(unittest.TestCase):
    def test_model65_selects_360_cpu_and_recovery_in_card_columns(self):
        deck = ('    CENPROCS   MODEL=158,'.ljust(71) + 'X\n' +
                '               SER=MCH,'.ljust(71) + 'X\n' +
                '    CHANNEL    ADDRESS=1,TYPE=BLKMPXR\n')
        cards = configure_stage1(deck, '65').splitlines()
        self.assertIn('MODEL=65,', cards[0][:71])
        self.assertIn('SER=SER1,', cards[1][:71])
        self.assertTrue(all(len(card) == 72 and card[71] == 'X' for card in cards[:2]))
        self.assertIn('TYPE=SELECTOR', cards[2])


class CompletionStatusTests(unittest.TestCase):
    def test_source_comments_do_not_count_as_failures(self):
        source = '* ABEND CODE FOR INVALID HASP\n* THE COND CODE 3 MAY BE THE RESULT\n'
        self.assertEqual(completion_status(source), ([], []))

    def test_supervisor_return_codes_are_read(self):
        listing = '\fIEF142I - STEP WAS EXECUTED - COND CODE 0000\nIEF142I - STEP WAS EXECUTED - COND CODE 0008\n'
        self.assertEqual(completion_status(listing), (['0000', '0008'], []))

    def test_real_abend_message_is_reported(self):
        listing = 'IEF450I TEST STEP - ABEND S806 U0000\n'
        self.assertEqual(completion_status(listing), ([], [listing.strip()]))

    def test_system_dump_completion_code_is_reported(self):
        listing = 'COMPLETION CODE - SYSTEM=B37  USER=0000\n'
        self.assertEqual(completion_status(listing), ([], [listing.strip()]))


if __name__ == '__main__':
    unittest.main()
