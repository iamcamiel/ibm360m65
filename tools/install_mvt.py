"""Build OS/360 MVT on private disk copies using the supplied MFT starter."""
import argparse
import json
from pathlib import Path
import re
import shutil
import threading
import time
from types import SimpleNamespace
from urllib.request import Request, urlopen

from software_ipl import Session


def completion_status(listing):
    """Read supervisor completion messages, excluding printed program source."""
    conditions = re.findall(r'IEF142I[^\r\n]*COND CODE\s+(\d{4})\b', listing)
    abends = re.findall(r'IEF(?:450|472)I[^\r\n]*|^COMPLETION CODE - SYSTEM=(?!000)[^\r\n]*', listing, re.M)
    return conditions, abends


def configure_stage1(deck, model):
    if model == '158':
        return deck
    lines = deck.splitlines()
    cpu = next(i for i, line in enumerate(lines) if 'CENPROCS   MODEL=158,' in line)
    recovery = next(i for i, line in enumerate(lines) if 'SER=MCH,' in line)
    lines[cpu] = '    CENPROCS   MODEL=65,'.ljust(42) + '360/65 CPU'.ljust(29) + 'X'
    lines[recovery] = '               SER=SER1,'.ljust(42) + '360 ERROR RECOVERY'.ljust(29) + 'X'
    lines = [line.replace('TYPE=BLKMPXR', 'TYPE=SELECTOR') for line in lines]
    return '\n'.join(lines) + '\n'


class RemoteSession:
    def __init__(self, run, url):
        self.run, self.url = run, url.rstrip('/')
        self.finished = threading.Event()
        self.controls = self

    @property
    def milestones(self):
        # The live helper rewrites this file as console events arrive.
        for attempt in range(20):
            try:
                return json.loads((self.run / 'milestones.json').read_text())
            except (json.JSONDecodeError, PermissionError):
                if attempt == 19:
                    raise
                time.sleep(.05)

    def post(self, path, value):
        request = Request(self.url + path, json.dumps(value).encode(),
                          {'Content-Type': 'application/json'})
        with urlopen(request, timeout=5):
            pass

    def send(self, text):
        self.post('/input', {'text': text})

    def put(self, command):
        if command in ('start', 'stop', 'quit'):
            self.post('/control', {'command': command})
        else:
            (self.run / 'control.rc').write_text(command + '\n')


class Installation:
    def __init__(self, args):
        self.args = args
        self.run = args.run_dir.resolve()
        self.results = {'phase': 'Starting MFT', 'jobs': [], 'cpu_model': args.cpu_model,
                        'interval_clock': 'disabled in Hercules build'}
        self.handled_mounts = set()
        self.servicing_mount = False
        self.session = Session(SimpleNamespace(
            run_dir=self.run, system_dir=args.system_dir.resolve(), exe=args.exe.resolve(),
            ros=args.ros.resolve(), console_port=args.console_port, web_port=args.web_port,
            auto_mft=True, reference=True, config='gen.cnf', ipl_device='150', console_device='0009'))

    def save(self):
        (self.run / 'installation.json').write_text(json.dumps(self.results, indent=2))

    def wait(self, predicate, purpose, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            if self.session.finished.is_set():
                raise RuntimeError('Hercules exited while ' + purpose)
            self.service_mounts()
            time.sleep(.2)
        raise TimeoutError('Timed out while ' + purpose)

    def service_mounts(self):
        if self.servicing_mount:
            return
        volumes = {'150': ('SYSRES', 'private'), '151': ('WORK01', 'public'),
                   '350': ('MVTRES', 'private'), '351': ('DLIB01', 'private'),
                   '352': ('WORK02', 'public')}
        for request in re.finditer(r'IEF(?:233|533)A\s+M\s+([0-9A-F]{3}),[^\r\n]*', self.console()):
            address = request[1]
            if request.start() in self.handled_mounts or address not in volumes:
                continue
            self.handled_mounts.add(request.start())
            self.servicing_mount = True
            try:
                volume, use = volumes[address]
                print('Mounting ' + volume + ' on ' + address, flush=True)
                self.operator('m ' + address + ',vol=(sl,' + volume + '),use=' + use)
                self.control('i ' + address)
                self.results.setdefault('mounts', []).append({'device': address, 'volume': volume,
                                                              'request': request[0]})
                self.save()
            finally:
                self.servicing_mount = False

    def console(self):
        path = self.run / 'guest-console.log'
        return path.read_text(errors='replace') if path.exists() else ''

    def control(self, command):
        path = self.run / 'console.log'
        offset = path.stat().st_size
        self.session.controls.put(command)
        def acknowledged():
            with path.open('rb') as output:
                output.seek(offset)
                return command in output.read().decode('latin1').splitlines()
        self.wait(acknowledged, 'sending Hercules command ' + command, 15)
        if isinstance(self.session, RemoteSession) and command not in ('start', 'stop', 'quit'):
            for attempt in range(20):
                try:
                    (self.run / 'control.rc').write_text('')
                    break
                except PermissionError:
                    if attempt == 19:
                        raise
                    time.sleep(.1)
        time.sleep(.4)

    def operator(self, text):
        before = len(self.session.milestones)
        self.session.send(text)
        self.wait(lambda: any(event['event'] == 'read' for event in self.session.milestones[before:]),
                  'reading operator command ' + text)
        time.sleep(.6)

    def printer(self):
        path = self.run / 'prt00e.txt'
        return path.read_text(errors='replace') if path.exists() else ''

    def submit(self, filename, timeout=240):
        deck = self.run / 'jcl' / filename
        names = re.findall(r'^//([A-Z0-9#$@]{1,8})\s+JOB\b', deck.read_text(), re.M)
        if not names:
            raise ValueError('No jobs in ' + str(deck))
        self.results['phase'] = 'Running ' + filename
        self.save()
        print(self.results['phase'], flush=True)
        offset = len(self.console())
        printer_offset = len(self.printer())
        self.control('devinit 00c jcl/' + filename)
        final_job = names[-1]
        self.wait(lambda: re.search(r'IEF404I\s+' + re.escape(final_job) + r'\b', self.console()[offset:]),
                  'completing ' + final_job, timeout)
        # WTR drains SYSOUT asynchronously after the console end message.
        self.wait(lambda: final_job in self.printer()[printer_offset:], 'printing ' + final_job, 60)
        time.sleep(2)
        listing = self.printer()[printer_offset:]
        (self.run / 'job-output' / (filename + '.txt')).write_text(listing)
        conditions, abends = completion_status(listing)
        abends.extend(re.findall(r'IEF(?:450|472)I[^\r\n]*', self.console()[offset:]))
        failures = [int(code) for code in conditions if int(code) > (4 if filename.startswith('stage2') else 0)]
        self.results['jobs'].append({'deck': filename, 'job_names': names,
                                    'condition_codes': conditions, 'abends': abends})
        self.save()
        if failures or abends or not conditions:
            raise RuntimeError('Review job output for ' + filename + ': ' + repr(failures or abends[:3]))

    def generate(self):
        punch_name = 'pch-stage1-' + str(time.time_ns()) + '.txt'
        self.control('devinit 00d ' + punch_name + ' ascii')
        self.submit('stage1.jcl')
        punched = self.run / punch_name
        self.wait(lambda: punched.exists() and punched.stat().st_size > 10000,
                  'receiving generated stage 2 jobstream')
        shutil.copyfile(punched, self.run / 'jcl/stage2.generated.jcl')
        self.submit('stage2.generated.jcl', timeout=1200)
        self.submit('mvtsetup.jcl')
        self.results['phase'] = 'MVT generated; ready for first IPL'
        self.save()
        self.operator('p 00e')
        self.operator('z eod')
        self.control('stop')
        print(json.dumps(self.results), flush=True)

    def resume(self):
        self.session = RemoteSession(self.run, self.args.resume_url)
        self.results = json.loads((self.run / 'installation.json').read_text())
        # A rejected stage-1 definition has not changed the target volume.
        # Preserve that attempt and regenerate after correcting the deck.
        if self.results['jobs'] and self.results['jobs'][-1]['deck'] == 'stage1.jcl':
            attempt = self.results['jobs'][-1]
            if any(int(code) > 0 for code in attempt['condition_codes']):
                self.results.setdefault('failed_attempts', []).append(self.results['jobs'].pop())
                shutil.copyfile(self.run / 'job-output/stage1.jcl.txt',
                                self.run / 'job-output/stage1.previous.jcl.txt')
                stage1 = self.args.system_dir / 'jcl/stage1.jcl'
                (self.run / 'jcl/stage1.jcl').write_text(configure_stage1(stage1.read_text(), self.args.cpu_model))
        # Re-evaluate existing listings with the supervisor-message parser.
        for job in self.results['jobs']:
            listing = (self.run / 'job-output' / (job['deck'] + '.txt')).read_text()
            conditions, abends = completion_status(listing)
            job.update(condition_codes=conditions, abends=abends)
            if any(int(code) > 0 for code in conditions) or abends:
                raise RuntimeError('Existing job needs diagnosis: ' + job['deck'])
        self.results.pop('error', None)
        self.save()
        self.control('start')
        try:
            self.generate()
        except Exception as error:
            self.results.update(phase='Paused for diagnosis', error=str(error))
            self.save()
            self.session.put('stop')
            raise

    def run_installation(self):
        thread = threading.Thread(target=self.session.run_cpu, name='MFT console')
        thread.start()
        self.wait(lambda: self.run.exists(), 'creating private working directory')
        try:
            shutil.copytree(self.args.system_dir / 'jcl', self.run / 'jcl')
            stage1 = self.run / 'jcl/stage1.jcl'
            stage1.write_text(configure_stage1(stage1.read_text(), self.args.cpu_model))
            (self.run / 'job-output').mkdir()
            self.save()
            self.wait(lambda: 'WTR WAITING FOR WORK' in self.console(), 'starting MFT writer', 120)
            self.wait(lambda: not self.session.auto, 'pausing completed MFT startup', 15)
            self.control('t-0009')
            self.control('start')
            for command in ['mn jobnames,t', 'mn status',
                            'm 151,vol=(sl,work01),use=public',
                            'm 352,vol=(sl,work02),use=public',
                            'm 350,vol=(sl,mvtres),use=private',
                            'm 351,vol=(sl,dlib01),use=private',
                            's init.p1,,,a']:
                self.operator(command)
            self.control('devinit 00c jcl/ctlgwrk.jcl')
            start_offset = len(self.console())
            self.operator('s rdr.p1,00c')
            self.wait(lambda: re.search(r'IEF404I\s+CTLGWRK\b', self.console()[start_offset:]),
                      'cataloging work datasets')
            self.wait(lambda: 'CTLGWRK' in self.printer(), 'printing work catalog job')
            time.sleep(2)
            listing = self.printer()
            (self.run / 'job-output/ctlgwrk.jcl.txt').write_text(listing)
            conditions, abends = completion_status(listing)
            self.results['jobs'].append({'deck': 'ctlgwrk.jcl', 'job_names': ['CTLGWRK'],
                                        'condition_codes': conditions, 'abends': abends})
            if any(int(code) > 0 for code in conditions) or abends:
                raise RuntimeError('Review CTLGWRK output')
            for filename in ['ctlg3330.jcl', 'fixnip.jcl', 'fixgenlb.jcl', 'hasphook.jcl']:
                self.submit(filename)
            if self.args.cpu_model == '65':
                shutil.copyfile(Path(__file__).parent / 'mvt-jcl/fix65ios.jcl',
                                self.run / 'jcl/fix65ios.jcl')
                self.submit('fix65ios.jcl')
            self.generate()
        except Exception as error:
            self.results['phase'] = 'Paused for diagnosis'
            self.results['error'] = str(error)
            self.save()
            self.session.auto = False
            self.session.controls.put('stop')
            print(json.dumps(self.results), flush=True)
        # Keep the console and disk state available for diagnosis or first IPL.
        thread.join()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', required=True, type=Path)
    parser.add_argument('--system-dir', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--ros', type=Path, default=Path(__file__).resolve().parents[1] / 'src/ros.txt')
    parser.add_argument('--console-port', type=int, default=3277)
    parser.add_argument('--web-port', type=int, default=8145)
    parser.add_argument('--resume-url', help='Continue after verified preparation jobs using an existing console')
    parser.add_argument('--cpu-model', choices=('65', '158'), default='65',
                        help='CPU target for the system generation (default: 360/65)')
    args = parser.parse_args()
    installation = Installation(args)
    if args.resume_url:
        installation.resume()
    else:
        installation.run_installation()


if __name__ == '__main__':
    main()
