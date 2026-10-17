"""Verify neutral raw-bit decoding and held-frame retry without hardware."""
import importlib.util,pathlib,unittest
spec=importlib.util.spec_from_file_location('reader',pathlib.Path(__file__).with_name('panel_input_reader.py'))
reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(reader)
class ReaderTest(unittest.TestCase):
    def test_all_bits_and_banks(self):
        for b in range(8):
            for n in range(24):
                regs={0:0x03602065,0x3FC:0x50444931,0x7F8:0,0x7FC:0x10004,
                      0x400:0x504E4C31,0x404:42,0x408:8,0x40C:1024}
                regs[0x410+b*4]=1<<n
                s=reader.capture(lambda o:regs.get(o,0))
                self.assertEqual(s['banks'],[1<<n if k==b else 0 for k in range(8)])
                self.assertFalse(s['polarity_assumed']);self.assertTrue(s['frame_valid'])
    def test_reject_cpu_image(self):
        with self.assertRaises(ValueError):reader.capture(lambda o:0x03602065 if o==0 else 0x10003)
    def test_unstable_capture(self):
        count=0
        def read(o):
            nonlocal count
            if o==0x404:count+=1;return count
            return {0:0x03602065,0x3FC:0x50444931,0x7F8:0,0x7FC:0x10004,0x400:0x504E4C31}.get(o,0)
        with self.assertRaises(ValueError):reader.capture(read)
    def test_invalid_frame_preserved(self):
        regs={0:0x03602065,0x3FC:0x50444931,0x7F8:0,0x7FC:0x10004,0x400:0x504E4C31}
        self.assertFalse(reader.capture(lambda o:regs.get(o,0))['frame_valid'])
if __name__=='__main__':unittest.main()
