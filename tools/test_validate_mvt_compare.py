"""Check readiness gates and failure detection in the slow MVT controller."""
from pathlib import Path
import io
import json
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import time
import unittest
from unittest.mock import patch

from validate_mvt_compare import Validation
from software_ipl import Session


class ComparisonStartupTests(unittest.TestCase):
    def setUp(self):
        self.directory = TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.validation = Validation(SimpleNamespace(
            run_dir=Path(self.directory.name), checkpoint=Path(self.directory.name),
            primary='primary', backend='backend', tso='tso'))
        self.validation.timer_disabled_verified = True
        self.model = {'instructions': 100, 'comparison_errors': 0, 'status': 'Booting'}

    def wait(self):
        self.validation.waiting = True
        self.validation.generation += 1
        self.validation.wait_since = time.monotonic() - 2

    @patch('validate_mvt_compare.http')
    def test_displayed_prompt_needs_a_fresh_wait_for_each_reply(self, request):
        self.validation.phase = 1
        screen = {'console': 'IEA135A SPECIFY SYS1.DUMP TAPE UNIT ADDRESS OR NO\nIEE101A READY'}
        self.validation.step(self.model, screen, '')
        request.assert_not_called()
        self.wait()
        self.validation.step(self.model, screen, '')
        request.assert_called_once_with('primary', '/input', {'text': "R 00,'NO'"})
        self.validation.step(self.model, screen, '')
        self.assertEqual(request.call_count, 1)
        self.wait()
        self.validation.step(self.model, screen, '')
        self.assertEqual(request.call_args.args, ('primary', '/input', {'text': 'T DATE=75.279'}))

    @patch('validate_mvt_compare.http')
    def test_full_console_uses_actual_range_and_separate_waits(self, request):
        self.validation.phase = 3
        screen = {'console': '01 | MESSAGE\n18 | LAST MESSAGE\n IEE159E MESSAGE WAITING'}
        self.wait()
        self.validation.step(self.model, screen, '')
        request.assert_called_once_with('primary', '/input', {'text': 'K E,1,18'})
        self.validation.step(self.model, screen, '')
        self.assertEqual(request.call_count, 1)
        self.wait()
        self.validation.step(self.model, {'console': 'DELETE CONFIRMATION'}, '')
        self.assertEqual(request.call_count, 2)
        self.assertEqual(self.validation.phase, 3)

    def test_memory_comparison_failure_is_not_missed(self):
        (self.validation.run / 'm65.log').write_text(
            'M65WAIT entered ic=123456\nExecution gone astray: memory writes do not match\n')
        with self.assertRaisesRegex(RuntimeError, 'memory writes'):
            self.validation.observe()

    def test_enabled_model_timer_stops_validation(self):
        (self.validation.run / 'm65.log').write_text('M65TIMER disable_key=0 clock_enable=1\n')
        with self.assertRaisesRegex(RuntimeError, 'not disabled'):
            self.validation.observe()

    def test_ce_indicator_alone_does_not_stop_validation(self):
        (self.validation.run / 'm65.log').write_text(
            'M65FAULT reason=ce-check runtime=0.000000200 ROSAR=003 IC=000000\n')
        self.validation.observe()

    def test_comparison_fault_still_stops_validation(self):
        (self.validation.run / 'm65.log').write_text(
            'M65FAULT reason=comparison-write runtime=0.000000200 ROSAR=003 IC=000000\n')
        with self.assertRaisesRegex(RuntimeError, 'comparison-write'):
            self.validation.observe()

    def test_taken_ce_ros_branch_stops_validation(self):
        (self.validation.run / 'm65.log').write_text(
            'M65CEBRANCH request_rosar=234 from=567 to=019 runtime=0.123\n'
            'M65FAULT reason=ce-ros-branch runtime=0.123 ROSAR=019 IC=000000\n')
        with self.assertRaisesRegex(RuntimeError, 'ce-ros-branch'):
            self.validation.observe()

    def test_session_records_ce_observation_without_pausing(self):
        session = Session(SimpleNamespace(run_dir=self.validation.run, auto_mft=True))
        session.observe_fault('M65FAULT reason=ce-check runtime=0.000000200 ')
        self.assertEqual(session.ce_checks, 1)
        self.assertIsNone(session.first_fault)
        self.assertTrue(session.controls.empty())
        self.assertTrue(session.auto)
        self.assertFalse((self.validation.run / 'first-failure.txt').exists())
        session.observe_fault('M65FAULT reason=comparison-write runtime=0.000000210 ')
        self.assertIn('comparison-write', session.first_fault)
        self.assertEqual(session.controls.get_nowait(), 'stop')
        self.assertFalse(session.auto)
        self.assertTrue((self.validation.run / 'first-failure.txt').exists())

    @patch('validate_mvt_compare.http')
    def test_timer_confirmation_is_required_before_answering_nip(self, request):
        self.validation.timer_disabled_verified = False
        self.validation.step(self.model, {'console': 'SPECIFY SYSTEM PARAMETERS'}, '')
        request.assert_not_called()
        (self.validation.run / 'm65.log').write_text('M65TIMER disable_key=1 clock_enable=0\n')
        self.validation.observe()
        self.validation.step(self.model, {'console': 'SPECIFY SYSTEM PARAMETERS'}, '')
        request.assert_not_called()
        self.wait()
        self.validation.step(self.model, {'console': 'SPECIFY SYSTEM PARAMETERS'}, '')
        request.assert_called_once_with('primary', '/input', {'text': ''})

    @patch('validate_mvt_compare.http')
    def test_checkpoint_startup_and_tso_job_sequence(self, request):
        steps = [
            ('IEA101A SPECIFY SYSTEM PARAMETERS', ''),
            ('IEA135A SPECIFY SYS1.DUMP TAPE UNIT ADDRESS OR NO', ''),
            ('IEE101A READY', ''),
            ('', 'IEE351I SMF SYS1.MAN RECORDING NOT BEING USED'),
            ('00 | *00 $ SPECIFY HASP OPTIONS -- HASP-II VERSION 4.009762', ''),
            ('', 'ALL AVAILABLE FUNCTIONS COMPLETE'),
            ('', 'IEF403I TCAM     STARTED'),
            ('', 'IKJ019I TSO HAS BEEN INITIALIZED'),
            ('', ''),
            ('', 'IEF125I CAMIEL   LOGGED ON'),
            ('', ''),
            ('', ''),
        ]
        terminals = ['Hercules welcome', 'IKJ54012A ENTER LOGON -',
                     'LOGON CAMIEL\nREADY', 'LISTCAT\n CHECK.CNTL\n TEST.DATA\n READY',
                     'JOB TSOTEST SUBMITTED\nREADY']
        (self.validation.run / 'prt00e.txt').write_text(
            'IEF142I TSOTEST CHECK - STEP WAS EXECUTED - COND CODE 0000\n')
        with patch.object(self.validation, 'attach_tso', side_effect=[
                {'console': t, 'status': 'TSO terminal · ready for input'} for t in terminals]):
            for screen, guest in steps:
                self.wait()
                self.validation.step(self.model, {'console': screen}, guest)
        actions = [call.args[2] for call in request.call_args_list]
        self.assertEqual(actions, [
            {'text': ''}, {'text': "R 00,'NO'"}, {'text': 'T DATE=75.279'},
            {'text': 'S HASP'}, {'text': 'R 00,NOREQ'}, {'text': 'S TCAM'},
            {'text': 'S TSO'}, {'key': 'clear'}, {'text': 'LOGON CAMIEL'},
            {'text': 'LISTCAT'}, {'text': 'SUBMIT CHECK.CNTL'}])
        self.assertTrue(self.validation.result['complete'])
        self.assertEqual(self.validation.result['test_job_condition_codes'], ['0000'])

    @patch('validate_mvt_compare.http')
    def test_manual_pause_prevents_new_operator_commands(self, request):
        self.validation.phase = 2
        self.wait()
        self.model['status'] = 'CPU paused'
        self.validation.step(self.model, {'console': 'IEE101A READY'}, '')
        request.assert_not_called()

    @patch('validate_mvt_compare.time.sleep')
    def test_progress_save_retries_a_temporary_windows_lock(self, sleep):
        original_replace = Path.replace
        attempts = []

        def replace(path, destination):
            attempts.append(destination)
            if len(attempts) == 1:
                raise PermissionError('Destination held by a reader')
            return original_replace(path, destination)

        with patch.object(Path, 'replace', autospec=True, side_effect=replace):
            self.validation.save(self.model, 'Checking storage')
        saved = json.loads((self.validation.run / 'validation-progress.json').read_text())
        self.assertEqual(saved['instructions'], 100)
        self.assertEqual(saved['stage'], 'Checking storage')
        self.assertEqual(len(attempts), 2)
        sleep.assert_called_once_with(0.05)

    @patch('validate_mvt_compare.http')
    def test_failure_pauses_cpu_even_when_failure_status_cannot_be_saved(self, request):
        request.return_value = dict(self.model, cpu_mode='comparison')
        with patch.object(self.validation, 'step', side_effect=RuntimeError('Boundary mismatch')), \
             patch.object(self.validation, 'save', side_effect=PermissionError('Status locked')), \
             patch('validate_mvt_compare.sys.stderr', new_callable=io.StringIO):
            with self.assertRaisesRegex(RuntimeError, 'Boundary mismatch'):
                self.validation.run_validation()
        request.assert_any_call('backend', '/control', {'command': 'stop'})


if __name__ == '__main__':
    unittest.main()
