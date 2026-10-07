"""Ensure recovery acknowledges only diagnosed history and still stops on failure."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from resume_mvt_tch_comparison import ResumedValidation, verify_recovery


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.model = {'cpu_mode': 'comparison', 'status': 'CPU paused',
                      'instructions': 565290, 'comparison_errors': 3}
        self.progress = {'phase': 2, 'complete': False, 'events': [{}, {}]}
        self.patch = {'installed': True, 'pid': 46384, 'io_herc_before': 0, 'io_herc_after': 0}
        self.log = 'M65TIMER disable_key=1 clock_enable=0\n' + ''.join(
            f'0342cc(0342d4) {n}(0.0)( 97) 9f00.7000 registers\n'
            'Execution gone astray: I/O operations do not match\nM65:\n  00000000\nHerc:\n'
            f'  {io}\n' for n, io in [(565188, '07000400'), (565224, '07000500'), (565260, '07000600')])

    def test_exact_diagnosed_history_is_accepted(self):
        verify_recovery(self.model, self.progress, self.log, self.patch)

    def test_architectural_fault_is_not_acknowledged(self):
        with self.assertRaisesRegex(RuntimeError, 'undiagnosed'):
            verify_recovery(self.model, self.progress, self.log.replace(
                'I/O operations do not match', 'CC does not match', 1), self.patch)

    def test_other_channel_record_is_not_acknowledged(self):
        with self.assertRaisesRegex(RuntimeError, 'verified TCH'):
            verify_recovery(self.model, self.progress, self.log.replace('07000400', '07000100'), self.patch)

    def test_moving_cpu_or_missing_patch_is_rejected(self):
        for model, patch_record in [(dict(self.model, status='CPU running'), self.patch),
                                    (self.model, dict(self.patch, installed=False))]:
            with self.assertRaises(RuntimeError):
                verify_recovery(model, self.progress, self.log, patch_record)

    def test_new_error_stops_cpu_and_keeps_historical_count(self):
        with TemporaryDirectory() as directory:
            run = Path(directory)
            validation = ResumedValidation(SimpleNamespace(run_dir=run, checkpoint=run,
                backend='backend', primary='primary', tso='tso'))
            with patch('resume_mvt_tch_comparison.http', return_value=dict(self.model, comparison_errors=4)) as http:
                with self.assertRaisesRegex(RuntimeError, 'new mismatch'):
                    validation.run_validation()
                http.assert_any_call('backend', '/control', {'command': 'stop'})
            result = json.loads((run / 'validation-progress.json').read_text())
            self.assertEqual(result['comparison_errors'], 1)
            self.assertEqual(result['cumulative_reported_mismatches'], 4)

    def test_new_log_error_stops_even_before_backend_count_updates(self):
        with TemporaryDirectory() as directory:
            run = Path(directory)
            (run / 'm65.log').write_text('Execution gone astray: memory writes do not match\n')
            validation = ResumedValidation(SimpleNamespace(run_dir=run, checkpoint=run,
                backend='backend', primary='primary', tso='tso'))
            with patch('resume_mvt_tch_comparison.http', return_value=self.model) as http:
                with self.assertRaisesRegex(RuntimeError, 'memory writes'):
                    validation.run_validation()
                http.assert_any_call('backend', '/control', {'command': 'stop'})


if __name__ == '__main__':
    unittest.main()
