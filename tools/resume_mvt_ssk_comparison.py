"""Continue the existing M17 MVT run, filtering only proven SSK key encoding reports.

The running executable remains unchanged. Never suppress a storage-key data or
address difference, another instruction's report, or any other comparator error.
"""
import argparse
from collections import deque
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import time

from validate_mvt_compare import Validation, http

PAUSED_AT = 10136130
HISTORICAL_COUNT = 24
HEADER = 'Execution gone astray: storage key writes do not match'
INSTRUCTION = re.compile(r'^[0-9a-f]{6}\([0-9a-f]{6}\)\s+(\d+)\([^\n]*?\)\s+([0-9a-f.]+)\s')
KEY_ROW = re.compile(r'^\s+([0-9a-f]{6}): ([0-9a-f]{2})$')


class SskEncodingFilter:
    def __init__(self, accepted=None):
        self.accepted = accepted or (lambda record: None)
        self.instruction = None
        self.pending = None
        self.ignored = 0
        self.pending_since = None

    def feed(self, line):
        line = line.rstrip('\r\n')
        match = INSTRUCTION.match(line)
        if match:
            if self.pending:
                raise RuntimeError('Incomplete SSK discrepancy record')
            self.instruction = (int(match[1]), match[2])
        if 'Execution gone astray' in line:
            if self.pending:
                raise RuntimeError('Incomplete SSK discrepancy before another failure')
            if line != HEADER:
                raise RuntimeError(line)
            if not self.instruction or not re.fullmatch(r'08[0-9a-f]{2}', self.instruction[1]):
                raise RuntimeError('Storage-key report did not follow SSK')
            self.pending = {'instructions': self.instruction[0], 'lines': [], 'state': 0}
            self.pending_since = time.monotonic()
            return
        if not self.pending:
            if KEY_ROW.match(line):
                raise RuntimeError('Unexpected extra storage-key row')
            return
        record = self.pending
        record['lines'].append(line)
        state = record['state']
        row = KEY_ROW.fullmatch(line)
        if state == 0 and line == 'M65:':
            record['state'] = 1
        elif state == 1 and row:
            record.update(model_address=int(row[1],16), model_key=int(row[2],16), state=2)
        elif state == 2 and line == 'Herc:':
            record['state'] = 3
        elif state == 3 and row:
            address, key = int(row[1],16), int(row[2],16)
            if address != record['model_address'] or record['model_key'] > 31 or key != record['model_key'] << 3:
                raise RuntimeError('New storage-key data/address discrepancy: ' + repr(record['lines']))
            record.update(hercules_address=address, hercules_key=key)
            record.pop('state')
            self.ignored += 1
            self.accepted(record)
            self.pending = self.pending_since = None
        else:
            raise RuntimeError('Unexpected storage-key report format: ' + line)


class ResumedValidation(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.filter = SskEncodingFilter(self.record_ignored)
        self.historical = HISTORICAL_COUNT
        self.last_count = HISTORICAL_COUNT
        self.last_instruction = PAUSED_AT

    def record_ignored(self, record):
        with (self.run/'ignored-ssk-records.jsonl').open('a') as output:
            output.write(json.dumps(record)+'\n')

    def prepare(self):
        model = http(self.args.backend)
        if not (model['cpu_mode']=='comparison' and 'CPU paused' in model['status']
                and model['instructions']==PAUSED_AT and model['comparison_errors']==HISTORICAL_COUNT):
            raise RuntimeError('Expected the original paused M17 CPU state')
        marker = self.run/'ssk-controller-resume.json'
        if marker.exists():
            raise RuntimeError('SSK controller recovery was already attempted')
        raw = (self.run/'validation-progress.json').read_bytes()
        progress = json.loads(raw)
        if progress['phase'] != 4 or progress['complete'] or len(progress['events']) != 4:
            raise RuntimeError('Unexpected startup phase; do not replay operator replies')
        manifest = json.loads((self.run/'restart-manifest.json').read_text())
        if not manifest.get('interval_timer_disabled_verified') or manifest['verified_ipl_acknowledgements'] != 1:
            raise RuntimeError('Verified timer-disabled single IPL evidence missing')
        executable = Path(manifest['executable'])
        if hashlib.sha256(executable.read_bytes()).hexdigest() != manifest['executable_sha256']:
            raise RuntimeError('The current executable identity changed')
        records = []
        history = SskEncodingFilter(records.append)
        with (self.run/'m65.log').open('rb') as log:
            log.seek(0,2)
            self.offset = log.tell()
            log.seek(max(0,self.offset-65536))
            lines = log.read().decode('latin1').splitlines()
        for line in lines[1:]:
            history.feed(line)
        if history.pending or len(records)!=HISTORICAL_COUNT:
            raise RuntimeError('Historical discrepancies are not the 24 complete diagnosed SSK reports')
        if records[0]['instructions']!=10136061 or records[-1]['instructions']!=PAUSED_AT:
            raise RuntimeError('Historical SSK instruction boundaries changed')
        (self.run/'validation-progress-before-ssk-recovery.json').write_bytes(raw)
        (self.run/'ssk-historical-records.json').write_text(json.dumps(records,indent=2)+'\n')
        self.phase = progress['phase']
        self.events = progress['events']
        self.generation = self.consumed = progress['model_wait_generation']
        self.waiting = False  # Require a new settled wait before an operator reply.
        self.timer_disabled_verified = True
        self.result = {k:v for k,v in progress.items() if k!='failure'}
        self.result.update(recording_recovery_at=PAUSED_AT,
                           ignored_discrepancy='Verified SSK five-bit versus high-five-bit byte encoding only',
                           running_executable_unchanged=True,
                           whole_run_clean_comparison=False)
        marker.write_text(json.dumps(dict(time_utc=datetime.now(timezone.utc).isoformat(),
            before=model, phase=self.phase, log_offset=self.offset,
            executable_sha256=manifest['executable_sha256'], user_authorized_resume=True),indent=2)+'\n')
        self.save(model,'Known SSK encoding reports acknowledged; restoring HASP startup')

    def observe(self):
        path = self.run/'m65.log'
        if path.stat().st_size < self.offset:
            raise RuntimeError('Comparison log was truncated')
        with path.open('rb') as log:
            log.seek(self.offset)
            chunk = log.read()
            self.offset = log.tell()
        lines = (self.remainder+chunk.decode('latin1')).split('\n')
        self.remainder = lines.pop()
        for line in lines:
            self.filter.feed(line)
            line = line.rstrip('\r')
            if line.startswith('M65TIMER ') and line!='M65TIMER disable_key=1 clock_enable=0':
                raise RuntimeError('ALD interval timer changed')
            if line.startswith('M65WAIT entered '):
                self.generation += 1
                self.waiting = True
                self.wait_since = time.monotonic()
            elif line.startswith('M65WAIT left'):
                self.waiting = False
        if self.filter.pending and time.monotonic()-self.filter.pending_since > 5:
            raise RuntimeError('Timed out reading complete SSK discrepancy evidence')

    def save(self, model, stage):
        ignored = self.historical+self.filter.ignored
        self.result.update(cumulative_reported_mismatches=model['comparison_errors'],
                           acknowledged_comparison_errors=ignored,
                           historical_ignored_ssk_records=self.historical,
                           newly_ignored_ssk_records=self.filter.ignored)
        active = dict(model,comparison_errors=max(0,model['comparison_errors']-ignored))
        super().save(active,stage)

    def run_validation(self):
        model = http(self.args.backend)
        try:
            http(self.args.backend,'/control',{'command':'start'})
            while True:
                model = http(self.args.backend)
                if model['cpu_mode']!='comparison' or model['instructions']<self.last_instruction or model['comparison_errors']<self.last_count:
                    raise RuntimeError('Existing comparison session restarted or changed')
                self.last_instruction, self.last_count = model['instructions'], model['comparison_errors']
                self.observe()
                unclassified = model['comparison_errors']-self.historical-self.filter.ignored
                if unclassified > int(self.filter.pending is not None):
                    raise RuntimeError('Comparator reported an unclassified mismatch')
                if self.filter.pending:
                    self.save(model,'Checking storage-key discrepancy evidence')
                    time.sleep(.25)
                    continue
                primary = http(self.args.primary)
                path = self.run/'mvtlog.txt'
                guest = path.read_text(errors='replace') if path.exists() else ''
                stage = self.step(model,primary,guest)
                self.save(model,stage)
                if self.result['complete']:
                    print(stage,flush=True)
                    return
                time.sleep(1)
        except Exception as error:
            self.result['failure']=str(error)
            try:
                http(self.args.backend,'/control',{'command':'stop'})
            finally:
                self.save(model,'Validation stopped: '+str(error))
            raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir',required=True,type=Path)
    parser.add_argument('--checkpoint',required=True,type=Path)
    parser.add_argument('--backend',default='http://127.0.0.1:8233')
    parser.add_argument('--primary',default='http://127.0.0.1:8232')
    parser.add_argument('--tso',default='http://127.0.0.1:8234')
    parser.add_argument('--console-port',type=int,default=3300)
    parser.add_argument('--tso-web-port',type=int,default=8234)
    parser.add_argument('--ws3270',type=Path,default=Path('gen/wc3270/ws3270.exe'))
    validation=ResumedValidation(parser.parse_args())
    validation.prepare()
    validation.run_validation()


if __name__=='__main__':
    main()
