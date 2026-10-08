"""Run reviewed job decks on the local native MVT installation console."""
import argparse
import json
from pathlib import Path
import re
import time
from urllib.request import Request, urlopen

from install_mvt import completion_status


def job_listings(listing, names):
    sections = re.split(r'(?=\f(?:IEF298I[^\r\n]*\r?\n)?//[A-Z0-9#$@]{1,8}\s+JOB\b)', listing)
    return '\n'.join(section for section in sections
                     if re.search(r'(?:^|\f)//(' + '|'.join(map(re.escape, names)) + r')\s+JOB\b', section, re.M))


class MVTJobs:
    def __init__(self, run, console_url, control_url):
        self.run = Path(run).resolve()
        self.console_url, self.control_url = console_url.rstrip('/'), control_url.rstrip('/')
        # A resumed runner must not act on tape requests from completed jobs.
        self.handled = {m.start() for m in re.finditer(r'IEF(?:233|234|504|533)A[^\r\n]*', self.console())}
        self.servicing = False
        self.report_path = self.run / 'extensions.json'
        self.results = json.loads(self.report_path.read_text()) if self.report_path.exists() else {'jobs': []}

    def save(self):
        self.report_path.write_text(json.dumps(self.results, indent=2))

    def post(self, url, value):
        with urlopen(Request(url, json.dumps(value).encode(), {'Content-Type': 'application/json'}), timeout=5):
            pass

    def state(self):
        with urlopen(self.console_url + '/state', timeout=5) as response:
            return json.load(response)

    def console(self):
        path = self.run / 'mvtlog.txt'
        return path.read_text(errors='replace') if path.exists() else ''

    def wait(self, predicate, purpose, timeout=120):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            if predicate():
                return
            self.service()
            time.sleep(.2)
        raise TimeoutError(purpose)

    def control(self, command):
        path = self.run / 'console.log'
        offset = path.stat().st_size
        if command in ('start', 'stop', 'quit'):
            self.post(self.control_url + '/control', {'command': command})
        else:
            (self.run / 'control.rc').write_text(command + '\n')
        def acknowledged():
            with path.open('rb') as output:
                output.seek(offset)
                return command in output.read().decode('latin1').splitlines()
        until = time.monotonic() + 15
        while not acknowledged():
            if time.monotonic() > until:
                raise TimeoutError('Hercules command ' + command)
            time.sleep(.2)
        if command not in ('start', 'stop', 'quit'):
            (self.run / 'control.rc').write_text('')
        time.sleep(.3)

    def operator(self, command):
        offset = len(self.console())
        self.post(self.console_url + '/input', {'text': command})
        self.wait(lambda: command.upper() in self.console()[offset:], 'MVT command ' + command)
        time.sleep(.4)

    def service(self):
        if self.servicing:
            return
        self.servicing = True
        try:
            screen = self.state()['console']
            if 'IEE159E MESSAGE WAITING' in screen:
                lines = screen.splitlines()
                # An active DISPLAY area can shrink the message area.
                area = next((i for i, line in enumerate(lines) if ' FRAME ' in line), 19)
                numbered = re.findall(r'^\s*(\d{1,2})[ |*\-]', screen, re.M)
                if numbered:
                    area = max(map(int, numbered))
                erase = 'K E,1,' + str(area)
                self.post(self.console_url + '/input', {'text': erase})
                until = time.monotonic() + 5
                while time.monotonic() < until and 'IEE157I' not in self.state()['console']:
                    time.sleep(.1)
                self.post(self.console_url + '/input', {'text': erase})
                time.sleep(.4)
            for match in re.finditer(r'IEF(?:233|234|504|533)A[^\r\n]*', self.console()):
                if match.start() in self.handled:
                    continue
                tape = next((name for name in ('HASP4', 'H4SUPB') if name in match[0]), None)
                if tape:
                    self.handled.add(match.start())
                    print('Mounting tape ' + tape, flush=True)
                    self.control('devinit 280 tapes/' + ('hasp4.aws' if tape == 'HASP4' else 'h4supb.aws'))
                    self.results.setdefault('tape_mounts', []).append(match[0])
                    self.save()
        finally:
            self.servicing = False

    def submit(self, deck, max_rc=0, timeout=600):
        path = self.run / deck
        names = re.findall(r'^//([A-Z0-9#$@]{1,8})\s+JOB\b', path.read_text(), re.M)
        if not names:
            raise ValueError('No job in ' + str(path))
        self.results['phase'] = 'Running ' + deck
        self.save()
        print(self.results['phase'], flush=True)
        offset = len(self.console())
        printer = self.run / 'prt00e.txt'
        print_offset = printer.stat().st_size if printer.exists() else 0
        self.control('devinit 00c ' + deck)
        self.wait(lambda: re.search(r'IEF404I\s+' + re.escape(names[-1]) + r'\b', self.console()[offset:]),
                  'Job completion ' + names[-1], timeout)
        # Wait for output to settle before assessing the supervisor messages.
        self.wait(lambda: printer.exists() and printer.stat().st_size > print_offset,
                  'Printed output ' + names[-1], 60)
        size = -1
        until = time.monotonic() + 60
        stable = time.monotonic()
        while time.monotonic() < until:
            current = printer.stat().st_size
            if current != size:
                size, stable = current, time.monotonic()
            elif time.monotonic() - stable > 2:
                break
            self.service()
            time.sleep(.2)
        with printer.open('rb') as output:
            output.seek(print_offset)
            listing = output.read().decode('latin1')
        listing = job_listings(listing, names)
        (self.run / 'job-output' / (path.name + '.txt')).write_text(listing, encoding='utf-8')
        codes, abends = completion_status(listing)
        abends.extend(re.findall(r'IEF(?:450|472)I[^\r\n]*', self.console()[offset:]))
        result = {'deck': deck, 'job_names': names, 'condition_codes': codes, 'abends': abends}
        self.results['jobs'].append(result)
        self.save()
        if not codes or abends or any(int(code) > max_rc for code in codes):
            self.control('stop')
            raise RuntimeError('Job needs diagnosis: ' + json.dumps(result))
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', required=True)
    parser.add_argument('--console-url', required=True)
    parser.add_argument('--control-url', required=True)
    parser.add_argument('--max-rc', type=int, default=0)
    parser.add_argument('decks', nargs='+')
    args = parser.parse_args()
    installation = MVTJobs(args.run_dir, args.console_url, args.control_url)
    installation.control('start')
    for deck in args.decks:
        installation.submit(deck, args.max_rc)
    installation.results['phase'] = 'Requested jobs complete'
    installation.save()


if __name__ == '__main__':
    main()
