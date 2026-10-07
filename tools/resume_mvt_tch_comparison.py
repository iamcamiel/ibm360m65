"""Resume the existing MVT boot after the verified TCH recording-only repair.

Keep all historical logs and cumulative error counts. Acknowledge exactly the
three diagnosed TCH records, restore controller phase 2, and stop on any new
error. This runner neither launches an emulator nor issues another IPL.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time

from validate_mvt_compare import Validation, http

EXPECTED = [(565188, '07000400'), (565224, '07000500'), (565260, '07000600')]
PAUSED_AT = 565290


def verify_recovery(model, progress, log, patch):
    if not (model['cpu_mode'] == 'comparison' and 'CPU paused' in model['status']
            and model['instructions'] == PAUSED_AT and model['comparison_errors'] == 3):
        raise RuntimeError('Existing paused CPU state changed')
    if not (progress['phase'] == 2 and not progress['complete'] and len(progress['events']) == 2):
        raise RuntimeError('Unexpected controller checkpoint; do not replay guest replies')
    if not (patch['installed'] and patch['pid'] == 46384
            and patch['io_herc_before'] == patch['io_herc_after'] == 0):
        raise RuntimeError('Verified live recorder replacement is missing')
    if re.findall(r'^M65TIMER .*$', log, re.M) != ['M65TIMER disable_key=1 clock_enable=0']:
        raise RuntimeError('Timer-disable initialization is missing or ambiguous')
    errors = [line for line in log.splitlines() if 'Execution gone astray' in line]
    if errors != ['Execution gone astray: I/O operations do not match'] * 3:
        raise RuntimeError('Historical errors include an undiagnosed discrepancy')
    blocks = re.findall(
        r'^0342cc\(0342d4\)\s+(\d+)\([^\n]*\)\s+9f00\.7000[^\n]*\n'
        r'Execution gone astray: I/O operations do not match\nM65:\n  00000000\nHerc:\n  (07000[456]00)',
        log, re.M)
    if [(int(n), io) for n, io in blocks] != EXPECTED:
        raise RuntimeError('The three historical reports are not the verified TCH probes')


class ResumedValidation(Validation):
    acknowledged = 3

    def prepare(self, patch_path):
        model = http(self.args.backend)
        progress_path = self.run / 'validation-progress.json'
        progress_bytes = progress_path.read_bytes()
        progress = json.loads(progress_bytes)
        log_bytes = (self.run / 'm65.log').read_bytes()
        log = log_bytes.decode('latin1').replace('\r\n', '\n')
        patch = json.loads(patch_path.read_text())
        verify_recovery(model, progress, log, patch)
        marker = self.run / 'tch-controller-resume.json'
        if marker.exists():
            raise RuntimeError('Controller recovery was already attempted')
        (self.run / 'validation-progress-before-tch-recovery.json').write_bytes(progress_bytes)
        self.phase = progress['phase']
        self.events = progress['events']
        self.result = {key: value for key, value in progress.items() if key != 'failure'}
        self.result.update(acknowledged_comparison_errors=self.acknowledged,
                           acknowledged_error_instructions=[n for n, _ in EXPECTED],
                           recording_recovery_at=PAUSED_AT, live_patch=str(patch_path))
        self.generation = 0
        for line in log.splitlines():
            if line.startswith('M65WAIT entered '):
                self.generation += 1
                self.waiting = True
            elif line.startswith('M65WAIT left'):
                self.waiting = False
        self.consumed = self.generation  # Only a fresh wait may trigger a reply.
        self.timer_disabled_verified = True
        self.offset = len(log_bytes)
        self.remainder = ''
        self.save(model, 'TCH recording fixed; resuming existing MVT startup')
        marker.write_text(json.dumps({'time_utc': datetime.now(timezone.utc).isoformat(),
                                     'before': model, 'phase': self.phase,
                                     'log_offset': self.offset, 'live_patch': str(patch_path)}, indent=2))

    def save(self, model, stage):
        self.result['cumulative_reported_mismatches'] = model['comparison_errors']
        active = dict(model, comparison_errors=model['comparison_errors'] - self.acknowledged)
        super().save(active, stage)

    def run_validation(self):
        model = http(self.args.backend)
        try:
            # prepare() and its durable evidence must precede this one Start.
            http(self.args.backend, '/control', {'command': 'start'})
            while True:
                model = http(self.args.backend)
                if model['cpu_mode'] != 'comparison':
                    raise RuntimeError('Expected the existing comparison backend')
                self.observe()  # Any new log discrepancy stops immediately.
                if model['comparison_errors'] != self.acknowledged:
                    raise RuntimeError('Comparator reported a new mismatch or restarted')
                primary = http(self.args.primary)
                path = self.run / 'mvtlog.txt'
                guest = path.read_text(errors='replace') if path.exists() else ''
                stage = self.step(model, primary, guest)
                self.save(model, stage)
                if self.result['complete']:
                    print(stage, flush=True)
                    return
                time.sleep(1)
        except Exception as error:
            self.result['failure'] = str(error)
            try:
                http(self.args.backend, '/control', {'command': 'stop'})
            except OSError:
                pass
            self.save(model, 'Validation stopped: ' + str(error))
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', required=True, type=Path)
    parser.add_argument('--patch-record', required=True, type=Path)
    parser.add_argument('--checkpoint', required=True, type=Path)
    parser.add_argument('--backend', default='http://127.0.0.1:8193')
    parser.add_argument('--primary', default='http://127.0.0.1:8192')
    parser.add_argument('--tso', default='http://127.0.0.1:8194')
    parser.add_argument('--console-port', type=int, default=3295)
    parser.add_argument('--tso-web-port', type=int, default=8194)
    parser.add_argument('--ws3270', type=Path, default=Path('gen/wc3270/ws3270.exe'))
    args = parser.parse_args()
    validation = ResumedValidation(args)
    validation.prepare(args.patch_record.resolve())
    validation.run_validation()


if __name__ == '__main__':
    main()
