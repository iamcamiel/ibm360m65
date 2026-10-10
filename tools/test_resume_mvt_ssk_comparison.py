"""Check selective SSK encoding recovery and fail-closed comparison handling."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from resume_mvt_ssk_comparison import SskEncodingFilter, ResumedValidation


def record(model='1e', native='f0', address='7e3800', opcode='0872'):
    return (f'014fee(014ffe) 10136133(0.000000)(3389) {opcode}           00000000\n'
            'Execution gone astray: storage key writes do not match\nM65:\n'
            f'  7e3800: {model}\nHerc:\n  {address}: {native}\n')


class SskFilterTests(unittest.TestCase):
    def feed(self, text, parser=None):
        parser=parser or SskEncodingFilter()
        for line in text.splitlines():
            parser.feed(line)
        return parser

    def test_all_five_bit_values_match_only_their_byte_encoding(self):
        for value in range(32):
            with self.subTest(value=value):
                parser=self.feed(record(f'{value:02x}',f'{value<<3:02x}'))
                self.assertEqual(parser.ignored,1)
                self.assertIsNone(parser.pending)

    def test_data_address_instruction_and_reference_bit_differences_rejected(self):
        for text in (record(native='e0'),record(address='7e4000'),record(opcode='0972'),
                     record(native='f4'),record(model='f0')):
            with self.subTest(text=text),self.assertRaises(RuntimeError):
                self.feed(text)

    def test_other_comparator_errors_are_never_ignored(self):
        with self.assertRaisesRegex(RuntimeError,'memory writes'):
            self.feed('Execution gone astray: memory writes do not match\n')

    def test_partial_record_remains_unclassified(self):
        parser=self.feed(record().rsplit('  7e3800: f0',1)[0])
        self.assertEqual(parser.ignored,0)
        self.assertIsNotNone(parser.pending)

    def test_extra_key_rows_fail_closed(self):
        with self.assertRaisesRegex(RuntimeError,'extra storage-key row'):
            self.feed(record()+'  7e4000: f0\n')

    def validation(self, root):
        return ResumedValidation(SimpleNamespace(run_dir=root,checkpoint=root,
            backend='backend',primary='primary',tso='tso'))

    def test_new_memory_error_pauses_existing_cpu(self):
        with TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'m65.log').write_text('Execution gone astray: memory writes do not match\n')
            validation=self.validation(root)
            model=dict(cpu_mode='comparison',status='CPU running',instructions=10136133,
                       comparison_errors=25)
            with patch('resume_mvt_ssk_comparison.http',return_value=model) as request:
                with self.assertRaisesRegex(RuntimeError,'memory writes'):
                    validation.run_validation()
            request.assert_any_call('backend','/control',{'command':'stop'})
            saved=json.loads((root/'validation-progress.json').read_text())
            self.assertEqual(saved['comparison_errors'],1)
            self.assertEqual(saved['cumulative_reported_mismatches'],25)

    def test_known_record_retains_raw_count_and_allows_progress(self):
        with TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'m65.log').write_text(record())
            validation=self.validation(root)
            model=dict(cpu_mode='comparison',status='CPU running',instructions=10136133,
                       comparison_errors=25)
            def complete(*args):
                validation.result['complete']=True
                return 'test complete'
            with patch('resume_mvt_ssk_comparison.http',return_value=model) as request, \
                 patch.object(validation,'step',side_effect=complete):
                validation.run_validation()
            self.assertFalse(any(call.args==('backend','/control',{'command':'stop'})
                                 for call in request.call_args_list))
            saved=json.loads((root/'validation-progress.json').read_text())
            self.assertEqual(saved['comparison_errors'],0)
            self.assertEqual(saved['acknowledged_comparison_errors'],25)
            self.assertEqual(saved['newly_ignored_ssk_records'],1)
            self.assertEqual(saved['cumulative_reported_mismatches'],25)
            self.assertTrue((root/'ignored-ssk-records.jsonl').exists())

    def test_unclassified_count_and_session_restart_stop(self):
        for errors,instructions in ((25,10136133),(23,10136133),(24,1)):
            with self.subTest(errors=errors,instructions=instructions),TemporaryDirectory() as directory:
                root=Path(directory)
                (root/'m65.log').write_text('')
                validation=self.validation(root)
                model=dict(cpu_mode='comparison',status='CPU running',instructions=instructions,
                           comparison_errors=errors)
                with patch('resume_mvt_ssk_comparison.http',return_value=model) as request:
                    with self.assertRaises(RuntimeError):
                        validation.run_validation()
                request.assert_any_call('backend','/control',{'command':'stop'})


if __name__=='__main__':
    unittest.main()
