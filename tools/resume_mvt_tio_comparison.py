"""Continue the same ISK MVT boot after its proven TIO recorder repair.

Acknowledge exactly three complete historical records. Every subsequent
discrepancy remains fatal; no dynamic exception filter or counter reset.
"""
import argparse
from datetime import datetime, timezone
import hashlib, json, re, time
from pathlib import Path
from validate_mvt_compare import Validation, http
from live_tio_recording_patch import EXE_SHA256
from verify_live_recorder import verify_live_patch

PAUSED_AT = 10684557
EXPECTED = [(10684489, 0x700), (10684512, 0x701), (10684535, 0x702)]
HEADER = 'Execution gone astray: I/O operations do not match'

def verify_history(model, progress, log, patch=None, live_identity=None):
    if not (model['cpu_mode'] == 'comparison' and 'CPU paused' in model['status']
            and model['instructions'] == PAUSED_AT and model['comparison_errors'] == 3):
        raise RuntimeError('Expected the original paused ISK comparison state')
    if progress['phase'] != 5 or progress['complete'] or len(progress['events']) != 7:
        raise RuntimeError('Unexpected HASP checkpoint; do not replay guest replies')
    log = log.replace('\r\n','\n')
    if [line for line in log.splitlines() if 'Execution gone astray' in line] != [HEADER]*3:
        raise RuntimeError('Historical discrepancies are not exactly the three TIO reports')
    blocks = re.findall(r'^7db126\(7db136\)\s+(\d+)\([^\n]*?\)\(\s*\d+\)\s+'
                        r'9d00\.5000\s+([^\n]+)\n' + re.escape(HEADER) +
                        r'\nM65:\n  00000000\nHerc:\n  (0500070[012])\n', log, re.M)
    records = []
    for count, registers, request in blocks:
        registers = registers.split()
        if len(registers) != 16 or int(registers[5],16) != (int(request,16)&0xffff):
            raise RuntimeError('Historical TIO device/register evidence changed')
        records.append((int(count), int(request,16)&0xffff))
    if records != EXPECTED:
        raise RuntimeError('Historical instruction boundaries or I/O records changed')
    if patch is not None and not (live_identity and patch['installed'] and patch['pid']==live_identity['pid']
            and patch['executable_sha256']==EXE_SHA256
            and patch['io_herc_before']==patch['io_herc_after']==0):
        raise RuntimeError('Expected verified recorder-only patch of the current process')
    return records

class ResumedValidation(Validation):
    acknowledged = 3

    def prepare(self, patch_path):
        model = http(self.args.backend)
        raw = (self.run/'validation-progress.json').read_bytes()
        progress = json.loads(raw)
        with (self.run/'m65.log').open('rb') as f:
            f.seek(0,2)
            self.offset = f.tell()
            f.seek(max(0,self.offset-262144))
            tail = f.read().decode('latin1')
            f.seek(0)
            head = f.read(262144).decode('latin1')
        patch = json.loads(patch_path.read_text())
        identity = verify_live_patch(self.args.backend, patch)
        verify_history(model, progress, tail, patch, identity)
        if re.findall(r'^M65TIMER .*$',head.replace('\r\n','\n'),re.M) != ['M65TIMER disable_key=1 clock_enable=0']:
            raise RuntimeError('Missing timer-disable evidence')
        manifest = json.loads((self.run/'restart-manifest.json').read_text())
        if manifest['ipl_requests']!=1 or manifest['executable_sha256']!=EXE_SHA256:
            raise RuntimeError('Original single-IPL executable identity changed')
        if hashlib.sha256(Path(manifest['executable']).read_bytes()).hexdigest()!=EXE_SHA256:
            raise RuntimeError('On-disk executable changed')
        marker = self.run/'tio-controller-resume.json'
        if marker.exists():
            raise RuntimeError('Controller recovery already attempted')
        (self.run/'validation-progress-before-tio-recovery.json').write_bytes(raw)
        (self.run/'tio-historical-evidence.log').write_text(tail)
        self.phase, self.events = progress['phase'], progress['events']
        self.generation = self.consumed = progress['model_wait_generation']
        self.waiting = False  # Only a fresh settled wait can trigger a reply.
        self.timer_disabled_verified = True
        self.last_instruction = PAUSED_AT
        self.result = {key:value for key,value in progress.items() if key!='failure'}
        self.result.update(acknowledged_comparison_errors=3,
                           acknowledged_error_instructions=[n for n,_ in EXPECTED],
                           acknowledged_error_kind='Verified TIO recorder reports outside IOCE 1',
                           recording_recovery_at=PAUSED_AT,live_patch=str(patch_path),
                           whole_run_clean_comparison=False)
        self.result['verified_live_patch_identity'] = identity
        marker.write_text(json.dumps(dict(time_utc=datetime.now(timezone.utc).isoformat(),
            before=model, phase=self.phase,log_offset=self.offset,live_patch=str(patch_path),
            repeated_ipl=False,user_authorized_resume=True),indent=2)+'\n')
        self.save(model,'TIO recorder repaired; resuming existing HASP initialization')

    def save(self, model, stage):
        self.result.update(cumulative_reported_mismatches=model['comparison_errors'],
                           unacknowledged_comparison_errors=model['comparison_errors']-3)
        super().save(model,stage)  # Keep the raw count visible in progress.json.

    def run_validation(self):
        model = http(self.args.backend)
        try:
            http(self.args.backend,'/control',{'command':'start'})
            while True:
                model = http(self.args.backend)
                if model['cpu_mode']!='comparison' or model['instructions']<self.last_instruction:
                    raise RuntimeError('Comparison session restarted or changed')
                self.last_instruction=model['instructions']
                self.observe()  # Every new mismatch, of any type, is fatal.
                if model['comparison_errors'] != self.acknowledged:
                    raise RuntimeError('Comparator reported a new mismatch or reset')
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
            except Exception as stop_error:
                self.result['failure_stop_error']=str(stop_error)
            try:
                model=http(self.args.backend)
                self.result['failure_status_refreshed']=True
            except Exception as status_error:
                self.result['failure_status_refreshed']=False
                self.result['failure_status_error']=str(status_error)
            self.save(model,'Validation stopped: '+str(error))
            raise

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--run-dir',required=True,type=Path)
    p.add_argument('--patch-record',required=True,type=Path)
    p.add_argument('--checkpoint',required=True,type=Path)
    p.add_argument('--backend',default='http://127.0.0.1:8233')
    p.add_argument('--primary',default='http://127.0.0.1:8232')
    p.add_argument('--tso',default='http://127.0.0.1:8234')
    p.add_argument('--console-port',type=int,default=3300)
    p.add_argument('--tso-web-port',type=int,default=8234)
    p.add_argument('--ws3270',type=Path,default=Path('gen/wc3270/ws3270.exe'))
    args=p.parse_args()
    validation=ResumedValidation(args)
    validation.prepare(args.patch_record.resolve())
    validation.run_validation()

if __name__=='__main__':
    main()
