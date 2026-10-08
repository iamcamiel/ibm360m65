"""Check that automatic WTOR replies cannot outrun OS reply-buffer setup."""
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import time
import unittest

from software_ipl import Session


class AutomaticReplyTests(unittest.TestCase):
    def setUp(self):
        self.directory = TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.session = Session(SimpleNamespace(
            run_dir=Path(self.directory.name), auto_mft=True, reference=False))
        self.session.connected.set()

    def prompt(self, text):
        (self.session.run / 'guest-console.log').write_text(text)

    def wait(self, seconds=2):
        self.session.cpu_waiting = True
        self.session.wait_since = time.monotonic() - seconds

    def test_definition_reply_waits_for_settled_cpu_wait(self):
        self.prompt('*00 IEE802A ENTER DEFINITION')
        self.session.automate()
        self.assertTrue(self.session.commands.empty())
        self.wait(.1)
        self.session.automate()
        self.assertTrue(self.session.commands.empty())
        self.wait()
        self.session.automate()
        self.assertEqual(self.session.commands.get_nowait(),
                         'r 00,p0=(a,256k),p1=(a,256k),end')
        self.assertFalse(self.session.cpu_waiting)
        self.wait()
        self.session.automate()
        self.assertTrue(self.session.commands.empty())

    def test_two_visible_prompts_need_separate_waits(self):
        self.prompt('*00 IEE801D CHANGE PARTITIONS\n*00 IEE802A ENTER DEFINITION')
        self.wait()
        self.session.automate()
        self.assertEqual(self.session.commands.get_nowait(), 'r 00,yes')
        self.assertTrue(self.session.commands.empty())
        self.wait()
        self.session.automate()
        self.assertEqual(self.session.commands.get_nowait(),
                         'r 00,p0=(a,256k),p1=(a,256k),end')

    def test_initial_read_inquiry_can_receive_enter_without_cpu_wait(self):
        self.prompt('SPECIFY SYSTEM PARAMETERS')
        self.session.automate()
        self.assertEqual(self.session.commands.get_nowait(), '')

    def test_native_reference_does_not_require_model_wait_markers(self):
        self.session.args.reference = True
        self.prompt('*00 IEE802A ENTER DEFINITION')
        self.session.automate()
        self.assertEqual(self.session.commands.get_nowait(),
                         'r 00,p0=(a,256k),p1=(a,256k),end')


if __name__ == '__main__':
    unittest.main()
