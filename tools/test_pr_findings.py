"""Regressions for empty audits, terminal-status races and live patch identity."""
import json
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from audit_ald_schedule import audit
from resume_mvt_ssk_comparison import ResumedValidation
from verify_live_recorder import verify_live_patch
from live_tio_recording_patch import EXE_SHA256, replacement_code


class ReviewTests(unittest.TestCase):
    def test_empty_and_missing_schedule_directories_fail(self):
        with TemporaryDirectory() as folder:
            for directory in (Path(folder), Path(folder)/'absent'):
                result = audit(directory)
                self.assertEqual(result['sections'], 0)
                self.assertTrue(result['differences'])

    def test_sections_without_assignments_fail(self):
        with TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'360_ab.cpp').write_text('void process_AB() {}\nvoid process_AB_clock() {}')
            (root/'360_ab.vhd').write_text('-- ALD_NOCLOCK_SECOND_BEGIN\n-- ALD_NOCLOCK_SECOND_END')
            self.assertTrue(audit(root)['differences'])

    def failure(self, refresh_fails=False, stop_fails=False):
        with TemporaryDirectory() as folder:
            root=Path(folder)
            (root/'m65.log').write_text('Execution gone astray: memory writes do not match\n')
            validator=ResumedValidation(SimpleNamespace(run_dir=root,checkpoint=root,
                backend='backend',primary='primary',tso='tso'))
            stale=dict(cpu_mode='comparison',status='CPU running',instructions=10136133,comparison_errors=24)
            final=dict(stale,status='CPU paused',instructions=10136134,comparison_errors=26)
            states=iter([stale,stale])
            def request(base,path='/state',data=None):
                if path=='/control':
                    if data['command']=='stop' and stop_fails:raise OSError('stop unreachable')
                    return None
                try:return next(states)
                except StopIteration:
                    if refresh_fails:raise OSError('status unreachable')
                    return final
            with patch('resume_mvt_ssk_comparison.http',side_effect=request):
                with self.assertRaisesRegex(RuntimeError,'memory writes'):
                    validator.run_validation()
            return json.loads((root/'validation-progress.json').read_text())

    def test_failure_saves_final_paused_counts(self):
        saved=self.failure()
        self.assertEqual(saved['cumulative_reported_mismatches'],26)
        self.assertEqual(saved['comparison_errors'],2)
        self.assertEqual(saved['instructions'],10136134)
        self.assertTrue(saved['failure_status_refreshed'])

    def test_status_failure_preserves_original_error_and_marks_fallback(self):
        saved=self.failure(refresh_fails=True)
        self.assertEqual(saved['cumulative_reported_mismatches'],24)
        self.assertFalse(saved['failure_status_refreshed'])
        self.assertIn('status unreachable',saved['failure_status_error'])
        self.assertIn('memory writes',saved['failure'])

    def test_stop_failure_still_refreshes(self):
        saved=self.failure(stop_fails=True)
        self.assertEqual(saved['cumulative_reported_mismatches'],26)
        self.assertIn('stop unreachable',saved['failure_stop_error'])

    def verify_patch(self, stale_pid=False, bad_bytes=False):
        record=dict(installed=True,pid=9001,executable='Hercules.exe',executable_sha256=EXE_SHA256,
                    entry=0x1000,remote_address=0x2000,record_address=0x3000)
        record['entry_patch']=(b'\xe9'+struct.pack('<I',0xffb)+b'\x90').hex()
        record['replacement_bytes']=replacement_code(record['record_address']).hex()
        identity=dict(pid=9002 if stale_pid else 9001,executable='Hercules.exe',creation_filetime=100,parent_pid=123)
        def memory(pid,address,size):
            if bad_bytes:return bytes(size)
            return bytes.fromhex(record['entry_patch'] if address==0x1000 else record['replacement_bytes'])
        with patch('verify_live_recorder.discover_emulator',return_value=identity), \
             patch('verify_live_recorder.read_memory',side_effect=memory), \
             patch('verify_live_recorder.Path.read_bytes',return_value=b'executable'), \
             patch('verify_live_recorder.hashlib.sha256') as digest:
            digest.return_value.hexdigest.return_value=EXE_SHA256
            return verify_live_patch('backend',record)

    def test_discovered_nonhistorical_pid_is_accepted_with_live_bytes(self):
        self.assertEqual(self.verify_patch()['pid'],9001)

    def test_changed_pid_and_recycled_pid_without_patch_are_rejected(self):
        for kwargs in (dict(stale_pid=True),dict(bad_bytes=True)):
            with self.assertRaises(RuntimeError):self.verify_patch(**kwargs)


if __name__=='__main__':unittest.main()
