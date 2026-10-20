"""Check readiness gates and failure detection in the slow MVT controller."""
from pathlib import Path
import io
import json
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import time
import unittest
from unittest.mock import patch

from validate_mvt_compare import Validation, test_job_codes
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
            ('', 'IEE351I SMF SYS1.MAN RECORDING NOT BEING USED'),
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
            '//TSOTEST JOB CLASS=A\n'
            'IEF142I TSOTEST CHECK - STEP WAS EXECUTED - COND CODE 0000\n'
            'IEF376I JOB /TSOTEST / STOP 75279.0000\n')
        with patch.object(self.validation, 'attach_tso', side_effect=[
                {'console': t, 'status': 'TSO terminal · ready for input'} for t in terminals]):
            for screen, guest in steps:
                self.wait()
                self.validation.step(self.model, {'console': screen}, guest)
        actions = [call.args[2] for call in request.call_args_list]
        self.assertEqual(actions, [
            {'text': ''}, {'text': "R 00,'NO'"}, {'text': 'T DATE=75.279'},
            {'text': 'MN JOBNAMES,T'}, {'text': 'MN STATUS'},
            {'text': 'S HASP'}, {'text': 'R 00,NOREQ'}, {'text': 'S TCAM'},
            {'text': 'S TSO'}, {'key': 'clear'}, {'text': 'LOGON CAMIEL'},
            {'text': 'LISTCAT'}, {'text': 'SUBMIT CHECK.CNTL'}])
        self.assertTrue(self.validation.result['complete'])
        self.assertEqual(self.validation.result['test_job_condition_codes'], ['0000'])

    def test_anonymous_mvt_result_belongs_to_completed_job(self):
        listing = ('//OTHER JOB CLASS=A\n'
                   'IEF142I - STEP WAS EXECUTED - COND CODE 0012\n'
                   'IEF376I JOB /OTHER / STOP 75279.0000\n'
                   '//TSOTEST JOB CLASS=A\n'
                   '//CHECK EXEC PGM=IEFBR14\n'
                   'IEF142I - STEP WAS EXECUTED - COND CODE 0000\n'
                   'IEF376I  JOB /TSOTEST / STOP 75279.0000\n'
                   '//ANOTHER JOB CLASS=A\n'
                   'IEF142I - STEP WAS EXECUTED - COND CODE 0008\n')
        self.assertEqual(test_job_codes(listing), ['0000'])

    def test_partial_or_unscoped_results_do_not_complete_job(self):
        code = 'IEF142I - STEP WAS EXECUTED - COND CODE 0000\n'
        for listing in (code, '//TSOTEST JOB CLASS=A\n' + code,
                        '//OTHER JOB CLASS=A\n' + code +
                        'IEF376I JOB /TSOTEST / STOP 75279.0000\n',
                        '//TSOTEST JOB CLASS=A\n' + code +
                        '//OTHER JOB CLASS=A\nIEF376I JOB /TSOTEST / STOP 75279.0000\n'):
            with self.subTest(listing=listing):
                self.assertEqual(test_job_codes(listing), [])

    def test_named_results_must_match_the_enclosing_job(self):
        self.assertEqual(test_job_codes(
            '//TSOTEST JOB CLASS=A\n'
            'IEF142I OTHER CHECK - STEP WAS EXECUTED - COND CODE 0000\n'
            'IEF376I JOB /TSOTEST / STOP 75279.0000\n'), [])

    @patch('validate_mvt_compare.http')
    def test_final_verification_needs_no_fresh_wait_or_guest_commands(self, request):
        self.validation.phase = 11
        self.validation.final_only = True
        self.model['status'] = 'CPU paused'
        request.return_value = {'console': 'JOB TSOTEST SUBMITTED\nREADY'}
        (self.validation.run / 'prt00e.txt').write_text(
            '//TSOTEST JOB CLASS=A\n'
            'IEF142I - STEP WAS EXECUTED - COND CODE 0000\n'
            'IEF376I JOB /TSOTEST / STOP 75279.0000\n')
        with patch('validate_mvt_compare.subprocess.Popen') as spawn:
            self.validation.step(self.model, {'console': 'IEE159E MESSAGE WAITING'}, '')
        spawn.assert_not_called()
        request.assert_called_once_with('tso')
        self.assertTrue(self.validation.result['complete'])
        self.assertEqual(self.validation.result['test_job_condition_codes'], ['0000'])

    @patch('validate_mvt_compare.http')
    def test_nonzero_step_is_not_hidden_by_successful_step(self, request):
        self.validation.phase = 11
        self.validation.final_only = True
        request.return_value = {'console': 'JOB TSOTEST SUBMITTED\nREADY'}
        (self.validation.run / 'prt00e.txt').write_text(
            '//TSOTEST JOB CLASS=A\n'
            'IEF142I - STEP WAS EXECUTED - COND CODE 0000\n'
            'IEF142I - STEP WAS EXECUTED - COND CODE 0008\n'
            'IEF376I JOB /TSOTEST / STOP 75279.0000\n')
        with self.assertRaisesRegex(RuntimeError, '0000,0008'):
            self.validation.step(self.model, {'console': ''}, '')
        self.assertFalse(self.validation.result['complete'])

    @patch('validate_mvt_compare.http')
    def test_pending_job_still_clears_a_full_console_at_a_fresh_wait(self, request):
        self.validation.phase = 11
        self.wait()
        with patch.object(self.validation, 'attach_tso', return_value={
                'console': 'JOB TSOTEST SUBMITTED\nREADY'}):
            self.validation.step(self.model, {'console': 'IEE159E MESSAGE WAITING'}, '')
        request.assert_called_once_with('primary', '/input', {'text': 'K E,1,19'})
        self.assertFalse(self.validation.result['complete'])

    def test_final_resume_preserves_submission_and_rejects_earlier_or_failed_runs(self):
        saved = dict(self.validation.result, phase=11, comparison_errors=0,
                     hasp_started=True, tso_initialized=True, tso_logon=True,
                     catalog_datasets_verified=True, events=[
                         {'phase': 10, 'endpoint': 'tso',
                          'action': {'text': 'SUBMIT CHECK.CNTL'}}])
        path = self.validation.run / 'validation-progress.json'
        path.write_text(json.dumps(saved))
        self.validation.restore_final()
        self.assertTrue(self.validation.final_only)
        self.assertEqual(self.validation.result, saved)
        for changes in ({'phase': 10}, {'failure': 'mismatch'},
                        {'comparison_errors': 1}, {'complete': True},
                        {'events': []}, {'interval_timer': True}):
            with self.subTest(changes=changes):
                path.write_text(json.dumps(dict(saved, **changes)))
                with self.assertRaisesRegex(RuntimeError, 'healthy submitted'):
                    self.validation.restore_final()

    @patch('validate_mvt_compare.http')
    def test_final_resume_cannot_send_guest_commands(self, request):
        self.validation.final_only = True
        with self.assertRaisesRegex(RuntimeError, 'cannot send guest commands'):
            self.validation.send('primary', '/input', {'text': 'S TSO'}, self.model)
        request.assert_not_called()

    def test_log_observer_retains_partial_lines_across_reads(self):
        path = self.validation.run / 'm65.log'
        path.write_bytes(b'M65WAIT entered ic=123456\nM65FAULT reason=comparison-')
        self.validation.observe()
        self.assertEqual(self.validation.generation, 1)
        with path.open('ab') as log:
            log.write(b'cc runtime=0.123\n')
        with self.assertRaisesRegex(RuntimeError, 'comparison-cc'):
            self.validation.observe()

    @patch('validate_mvt_compare.http')
    def test_monitoring_precedes_job_start_and_requires_separate_waits(self, request):
        self.validation.phase = 3
        screen = {'console': 'IEE351I SMF SYS1.MAN RECORDING NOT BEING USED'}
        expected = ['MN JOBNAMES,T', 'MN STATUS', 'S HASP']
        for index, command in enumerate(expected):
            self.validation.step(self.model, screen, '')
            self.assertEqual(request.call_count, index)
            self.wait()
            self.validation.step(self.model, screen, '')
            self.assertEqual(request.call_count, index + 1)
            self.assertEqual(request.call_args.args,
                             ('primary', '/input', {'text': command}))
        self.assertEqual(self.validation.phase, 4)
        self.assertEqual(self.validation.result['startup_monitor_commands_sent'],
                         expected[:2])

    @patch('validate_mvt_compare.http')
    def test_missing_tcam_started_message_does_not_send_tso(self, request):
        self.validation.phase = 6
        self.wait()
        self.validation.step(self.model, {'console': 'S TCAM'}, '')
        request.assert_not_called()
        self.validation.step(self.model, {'console': 'S TCAM'},
                             'IEF403I TCAM     STARTED TIME=00.00.00')
        request.assert_called_once_with('primary', '/input', {'text': 'S TSO'})
        self.validation.step(self.model, {'console': 'S TCAM'},
                             'IEF403I TCAM     STARTED TIME=00.00.00')
        self.assertEqual(request.call_count, 1)

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
