"""Advance a clean MVT checkpoint under the ALD/Hercules comparator.

Leaves a running terminal after logon, LISTCAT and a submitted test job.
Each operator reply waits for a fresh settled model wait. No elapsed-time
estimate substitutes for CPU readiness. Failure pauses the CPU and saves
the evidence. Use only private disks copied from a clean stopped checkpoint.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from urllib.request import Request, urlopen


def http(base, path='/state', data=None):
    request = Request(base.rstrip('/') + path,
                      None if data is None else json.dumps(data).encode(),
                      {} if data is None else {'Content-Type': 'application/json'})
    with urlopen(request, timeout=10) as response:
        body = response.read()
    return json.loads(body) if body else None


class Validation:
    def __init__(self, args):
        self.args = args
        self.run = args.run_dir.resolve()
        self.phase = 0
        self.offset = 0
        self.remainder = ''
        self.generation = 0
        self.consumed = 0
        self.waiting = False
        self.wait_since = 0
        self.clear_remaining = 0
        self.clear_limit = 19
        self.tso_proc = None
        self.events = []
        self.timer_disabled_verified = False
        self.result = {'system': 'OS/360 21.8F MVT, Model 65',
                       'cpu_mode': 'ALD C++ compared with Hercules',
                       'interval_timer': False, 'complete': False,
                       'checkpoint': str(args.checkpoint.resolve())}

    def observe(self):
        path = self.run / 'm65.log'
        if not path.exists():
            return
        with path.open('rb') as log:
            log.seek(self.offset)
            chunk = log.read()
            self.offset = log.tell()
        lines = (self.remainder + chunk.decode('latin1')).split('\n')
        self.remainder = lines.pop()
        for line in lines:
            if line.startswith('M65TIMER '):
                if line.strip() != 'M65TIMER disable_key=1 clock_enable=0':
                    raise RuntimeError('ALD interval timer is not disabled: ' + line.strip())
                self.timer_disabled_verified = True
            if line.startswith('M65WAIT entered '):
                self.generation += 1
                self.waiting = True
                self.wait_since = time.monotonic()
            elif line.startswith('M65WAIT left'):
                self.waiting = False
            if 'Execution gone astray' in line:
                raise RuntimeError(line.strip())
            if line.startswith('M65FAULT '):
                raise RuntimeError('First fault requires investigation: ' + line.strip())

    def ready(self, model):
        return (self.waiting and self.generation > self.consumed
                and time.monotonic() - self.wait_since >= 1
                and 'CPU paused' not in model['status'])

    def send(self, base, path, data, model):
        http(base, path, data)
        self.consumed = self.generation
        self.events.append({'instructions': model['instructions'],
                            'phase': self.phase, 'endpoint': base,
                            'action': data})
        print(json.dumps(self.events[-1]), flush=True)

    def save(self, model, stage):
        self.result.update(stage=stage, phase=self.phase,
                           instructions=model['instructions'],
                           comparison_errors=model['comparison_errors'],
                           model_wait_generation=self.generation,
                           events=self.events)
        temporary = self.run / 'validation-progress.tmp'
        temporary.write_text(json.dumps(self.result, indent=2))
        for attempt in range(10):
            try:
                temporary.replace(self.run / 'validation-progress.json')
                break
            except PermissionError:
                # A Windows reader can briefly hold the destination open.
                if attempt == 9:
                    raise
                time.sleep(0.05 * (attempt + 1))

    def attach_tso(self):
        if self.tso_proc is None:
            directory = self.run.parent / (self.run.name + '-tso')
            output = (self.run / 'tso-helper.log').open('wb')
            command = [sys.executable, str(Path(__file__).with_name('tso_console.py')),
                       '--exe', str(self.args.ws3270.resolve()),
                       '--console-port', str(self.args.console_port),
                       '--web-port', str(self.args.tso_web_port),
                       '--run-dir', str(directory), '--control-url', self.args.backend]
            self.tso_proc = subprocess.Popen(command, stdin=subprocess.DEVNULL,
                                            stdout=output, stderr=subprocess.STDOUT)
            output.close()
            self.result['tso_helper_pid'] = self.tso_proc.pid
        if self.tso_proc.poll() is not None:
            raise RuntimeError('TSO terminal helper stopped; inspect tso-helper.log')
        try:
            return http(self.args.tso)
        except OSError:
            return None

    def step(self, model, primary, guest):
        screen = primary['console']
        if not self.timer_disabled_verified:
            return 'Waiting for ALD timer-disable confirmation'
        combined = screen + '\n' + guest
        ready = self.ready(model)
        # A full display blocks further WTOs with the interval timer off.
        # Clear its actual displayed range twice, with a separate CPU wait
        # between commands, before proceeding to the next startup action.
        if ready and self.phase >= 3 and (self.clear_remaining or 'IEE159E' in screen):
            if not self.clear_remaining:
                numbers = re.findall(r'^\s*(\d{2})\s', screen, re.M)
                self.clear_limit = max(map(int, numbers)) if numbers else 19
                self.clear_remaining = 2
            self.send(self.args.primary, '/input',
                      {'text': f'K E,1,{self.clear_limit}'}, model)
            self.clear_remaining -= 1
            return 'Clearing pending console messages'
        command = None
        stage = ['Waiting for NIP system parameters', 'Waiting for dump-tape request',
                 'Waiting for MVT READY', 'Waiting for startup to finish',
                 'Waiting for HASP options', 'Waiting for HASP initialization',
                 'Waiting for TCAM', 'Waiting for TSO initialization',
                 'Waiting for TSO logon prompt', 'Waiting for CAMIEL logon',
                 'Waiting for catalog listing', 'Waiting for TSOTEST completion'][self.phase]
        if not ready:
            return stage
        elif self.phase == 0 and 'SPECIFY SYSTEM PARAMETERS' in screen:
            command = ''  # Wait until the NIP 3270 handler is installed.
        elif self.phase == 1 and 'IEA135A' in screen:
            command = "R 00,'NO'"
        elif self.phase == 2 and 'IEE101A READY' in combined:
            command = 'T DATE=75.279'
        elif self.phase == 3 and 'IEE351I' in combined:
            command = 'S HASP'
        elif self.phase == 4:
            match = re.search(r'\*?(\d\d)\s+\$ SPECIFY HASP OPTIONS', screen)
            if match:
                command = f'R {match[1]},NOREQ'
        elif self.phase == 5 and 'ALL AVAILABLE FUNCTIONS COMPLETE' in combined:
            if self.attach_tso() is not None:
                command = 'S TCAM'
        elif self.phase == 6 and re.search(r'IEF403I TCAM\s+STARTED', combined):
            command = 'S TSO'
        elif self.phase == 7 and 'IKJ019I TSO HAS BEEN INITIALIZED' in combined:
            self.send(self.args.tso, '/key', {'key': 'clear'}, model)
            self.result['hasp_started'] = True
            self.result['tso_initialized'] = True
            self.phase += 1
        elif self.phase >= 8:
            terminal = self.attach_tso()
            if terminal is None:
                return stage
            text = terminal['console']
            if not terminal['status'].endswith('ready for input'):
                return stage
            tso_command = None
            if self.phase == 8 and 'ENTER LOGON' in text:
                tso_command = 'LOGON CAMIEL'
            elif self.phase == 9 and 'READY' in text and re.search(r'IEF125I CAMIEL\s+LOGGED ON', guest):
                self.result['tso_logon'] = True
                tso_command = 'LISTCAT'
            elif self.phase == 10 and all(x in text for x in ('CHECK.CNTL', 'TEST.DATA', 'READY')):
                self.result['catalog_datasets_verified'] = True
                tso_command = 'SUBMIT CHECK.CNTL'
            elif self.phase == 11:
                listing = (self.run / 'prt00e.txt').read_text(errors='replace') if (self.run / 'prt00e.txt').exists() else ''
                codes = re.findall(r'IEF142I\s+TSOTEST\s+.*?COND CODE\s+(\d{4})', listing)
                if codes and 'JOB TSOTEST' in text and 'SUBMITTED' in text:
                    self.result['test_job_condition_codes'] = codes
                    if any(x != '0000' for x in codes):
                        raise RuntimeError('TSOTEST returned ' + ','.join(codes))
                    self.result['complete'] = True
                    return 'MVT, HASP, TSO logon, catalog and submitted job verified'
            if tso_command is not None:
                self.send(self.args.tso, '/input', {'text': tso_command}, model)
                self.phase += 1
        if command is not None:
            self.send(self.args.primary, '/input', {'text': command}, model)
            self.phase += 1
        return stage

    def run_validation(self):
        model = {'instructions': 0, 'comparison_errors': 0}
        try:
            while True:
                model = http(self.args.backend)
                if model['cpu_mode'] != 'comparison':
                    raise RuntimeError('Expected the ALD/Hercules comparison backend')
                self.observe()
                if model['comparison_errors']:
                    raise RuntimeError('Comparator reported a mismatch')
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
            try:
                self.save(model, 'Validation stopped: ' + str(error))
            except OSError as save_error:
                print('Could not save failure status: ' + str(save_error),
                      file=sys.stderr, flush=True)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', required=True, type=Path)
    parser.add_argument('--checkpoint', required=True, type=Path)
    parser.add_argument('--backend', default='http://127.0.0.1:8153')
    parser.add_argument('--primary', default='http://127.0.0.1:8152')
    parser.add_argument('--tso', default='http://127.0.0.1:8154')
    parser.add_argument('--console-port', type=int, default=3289)
    parser.add_argument('--tso-web-port', type=int, default=8154)
    parser.add_argument('--ws3270', type=Path, default=Path('gen/wc3270/ws3270.exe'))
    Validation(parser.parse_args()).run_validation()


if __name__ == '__main__':
    main()
