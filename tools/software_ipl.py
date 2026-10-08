"""Run the generated CPU against Hercules using private OS disk copies.

The browser console stays available when the CPU is paused, allowing an IPL
session to continue without repeating the storage clearing and module loading.
Only the Python standard library is required.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import queue
import re
import shutil
import socket
import subprocess
import threading
import time


PAGE = '''<!doctype html><html><head><meta charset="utf-8"><title>IBM 360 console</title>
<style>body{margin:0;background:#111713;color:#d6e8d7;font:16px Consolas,monospace}header{padding:20px 24px;border-bottom:1px solid #344638}h1{font:20px system-ui;margin:0 0 8px}#status{font:14px system-ui;color:#a1b9a6}pre{padding:24px;white-space:pre-wrap;line-height:1.5}form{display:flex;gap:8px;padding:16px 24px;position:sticky;bottom:0;background:#111713}input{flex:1;background:#1b261e;color:#e0efe2;border:1px solid #546b58;padding:10px;font:inherit}button{background:#283b2c;color:#e0efe2;border:1px solid #546b58;padding:10px;cursor:pointer}#controls{float:right;display:flex;gap:8px}#note{font:13px system-ui;color:#a1b9a6;padding:0 24px 20px}</style></head>
<body><header><div id="controls"><button onclick="control('stop')">Pause CPU</button><button onclick="control('start')">Resume CPU</button></div><h1>IBM 360 · OS console</h1><div id="status">Connecting…</div></header><pre id="console"></pre>
<form id="entry"><input id="command" maxlength="148" aria-label="Operator command" placeholder="Operator command — leave blank to send Enter"><button>Send</button></form><div id="note">Entering a command takes over from automatic startup. Resume the CPU before continuing a paused session.</div>
<script>async function post(path,value){const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(value)});if(!r.ok)throw new Error(await r.text())}async function control(command){try{await post('/control',{command})}catch(e){alert(e.message)}}document.getElementById('entry').onsubmit=async e=>{e.preventDefault();const input=document.getElementById('command');try{await post('/input',{text:input.value});input.value=''}catch(e){alert(e.message)}};async function refresh(){try{const s=await(await fetch('/state',{cache:'no-store'})).json();const view=document.getElementById('console');const changed=view.textContent!==s.console;view.textContent=s.console;document.getElementById('status').textContent=s.status;if(changed)window.scrollTo(0,document.body.scrollHeight)}catch(e){document.getElementById('status').textContent='Console disconnected'}}refresh();setInterval(refresh,1500);</script></body></html>'''


class Session:
    def __init__(self, args):
        self.args = args
        self.run = args.run_dir.resolve()
        self.commands = queue.Queue()
        self.controls = queue.Queue()
        self.finished = threading.Event()
        self.connected = threading.Event()
        self.read_ready = threading.Event()
        self.log_lock = threading.Lock()
        self.auto = args.auto_mft
        self.stage = 'Booting'
        self.steps = set()
        self.queue_reply_at = None
        self.instructions = 0
        self.errors = 0
        self.milestones = []
        self.started = time.monotonic()
        self.cpu_waiting = False
        self.wait_since = None

    def log_console(self, text):
        with self.log_lock, (self.run / 'guest-console.log').open('ab') as output:
            output.write(text.encode('utf-8', errors='replace'))

    def send(self, text):
        self.commands.put(text)

    def state(self):
        path = self.run / 'guest-console.log'
        text = path.read_text(errors='replace') if path.exists() else ''
        detail = (f'last console event at {self.instructions:,} Hercules instructions' if self.args.reference
                  else f'{self.instructions:,} compared instructions · {self.errors} reported mismatches')
        return {'console': text, 'status': f'{self.stage} · {detail}',
                'instructions': self.instructions, 'comparison_errors': self.errors,
                'cpu_waiting': self.cpu_waiting,
                'automatic_startup': self.auto, 'cpu_mode': 'hercules' if self.args.reference else 'comparison'}

    def save_status(self):
        data = self.state()
        data.update(elapsed_seconds=round(time.monotonic()-self.started, 1),
                    operator_prompt_reached='SPECIFY SYSTEM PARAMETERS' in data['console'],
                    mft_writer_ready='WTR WAITING FOR WORK' in data['console'])
        (self.run / 'session.json').write_text(json.dumps(data, indent=2))
        if self.args.reference:
            (self.run / 'milestones.json').write_text(json.dumps(self.milestones, indent=2))

    def console_client(self):
        sock = None
        try:
            for attempt in range(60):
                if self.finished.is_set():
                    return
                try:
                    sock = socket.create_connection(('127.0.0.1', self.args.console_port), timeout=1)
                    break
                except OSError:
                    time.sleep(.25)
            if sock is None:
                raise ConnectionError('Hercules console did not become available')
            sock.settimeout(5)
            def exact(count):
                data = b''
                while len(data) < count:
                    part = sock.recv(count-len(data))
                    if not part:
                        raise ConnectionError('Console disconnected during negotiation')
                    data += part
                return data
            if exact(3) != b'\xff\xfd\x18':
                raise ConnectionError('Unexpected terminal negotiation')
            sock.sendall(b'\xff\xfb\x18')
            if exact(6) != b'\xff\xfa\x18\x01\xff\xf0':
                raise ConnectionError('Unexpected terminal type request')
            terminal = 'ASCII@' + getattr(self.args, 'console_device', '0009')
            sock.sendall(b'\xff\xfa\x18\x00' + terminal.encode('ascii') + b'\xff\xf0')
            sock.settimeout(.2)
            self.connected.set()
            if self.args.reference and not getattr(self.args, 'no_ipl', False):
                # Native Hercules boots quickly enough to reach console writes
                # before this terminal connects. Attach before starting IPL.
                self.controls.put('ipl ' + getattr(self.args, 'ipl_device', '150'))
            pending = None
            while not self.finished.is_set():
                if pending is None and not self.commands.empty():
                    pending = self.commands.get_nowait()
                # The legacy tty device cannot buffer a zero-length line
                # before Read Inquiry. Wait for its CCW before sending Enter.
                if pending is not None and (pending or self.read_ready.is_set()):
                    sock.sendall(pending.encode('ascii') + b'\r\n')
                    self.log_console('\n> ' + (pending or '[Enter]') + '\n')
                    pending = None
                    self.read_ready.clear()
                try:
                    data = sock.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                self.log_console(data.decode('latin1'))
        except OSError as error:
            if not self.finished.is_set() and self.stage != 'Stopping session':
                self.log_console(f'\n[Console connection: {error}]\n')
        finally:
            self.connected.clear()
            if sock:
                sock.close()

    def automate(self):
        if not self.auto or not self.connected.is_set():
            return
        text = (self.run / 'guest-console.log').read_text(errors='replace')
        def once(step, command, stage):
            if step not in self.steps:
                # A displayed WTOR is not yet safe to answer on the generated
                # CPU: IEEDFIN1 can still clear its reply buffer and ECB. Wait
                # for the settled CPU wait reported by the model. The initial
                # Read Inquiry needs Enter to complete, so it is exempt.
                if not self.args.reference and step != 'parameters':
                    if not self.cpu_waiting or self.wait_since is None or time.monotonic() - self.wait_since < 1:
                        self.stage = 'Waiting for CPU before ' + stage.lower()
                        return False
                self.steps.add(step)
                self.stage = stage
                self.send(command)
                if not self.args.reference and step != 'parameters':
                    self.cpu_waiting = False
                    self.wait_since = None
                return True
            return False
        if 'SPECIFY SYSTEM PARAMETERS' in text:
            once('parameters', '', 'Accepting system settings')
        match = re.search(r'\*?(\d\d) IEE801D', text)
        if match:
            once('partitions', 'r ' + match[1] + ',yes', 'Defining partitions')
        match = re.search(r'\*?(\d\d) IEE802A', text)
        if match:
            once('definition', 'r ' + match[1] + ',p0=(a,256k),p1=(a,256k),end', 'Defining partitions')
        if 'IEE101A' in text:
            once('date', 't date=75.279,q=(,f)', 'Initializing job queue')
        match = re.search(r'\*?(\d\d) IEF423A', text)
        if match and once('queue', 'r ' + match[1] + ',u', 'Initializing job queue'):
            self.queue_reply_at = time.monotonic()
        if self.queue_reply_at and time.monotonic() - self.queue_reply_at > 45:
            # A 1052 read accepts one line. Sending several commands together
            # can discard the extra lines, so wait for this task's response.
            once('tasks', 's wtr.p0,00e', 'Starting writer')
        if 'WTR WAITING FOR WORK' in text and 'ready' not in self.steps:
            self.steps.add('ready')
            self.stage = 'MFT writer ready — pausing CPU'
            self.controls.put('stop')
            self.auto = False
            self.save_status()
            print(json.dumps(self.state()), flush=True)

    def serve(self):
        session = self
        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path == '/':
                    data, mime = PAGE.encode(), 'text/html; charset=utf-8'
                elif self.path == '/state':
                    data, mime = json.dumps(session.state()).encode(), 'application/json'
                else:
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header('Content-Type', mime)
                self.send_header('Cache-Control', 'no-store')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            def do_POST(self):
                if self.headers.get('Content-Type') != 'application/json':
                    self.send_error(400)
                    return
                try:
                    length = int(self.headers.get('Content-Length', 0))
                    if not 0 < length <= 2048:
                        raise ValueError('Invalid command size')
                    data = json.loads(self.rfile.read(length))
                    if self.path == '/input':
                        text = data['text']
                        if not isinstance(text, str) or len(text) > 148 or any(not 32 <= ord(c) < 127 for c in text):
                            raise ValueError('Use a single ASCII command of at most 148 characters')
                        if not session.connected.is_set():
                            raise ValueError('OS console is not connected')
                        session.auto = False
                        session.send(text)
                    elif self.path == '/control':
                        command = data['command']
                        if command not in ('start', 'stop', 'quit'):
                            raise ValueError('Unknown CPU control')
                        session.controls.put(command)
                    else:
                        self.send_error(404)
                        return
                except (ValueError, KeyError, TypeError) as error:
                    self.send_error(400, str(error))
                    return
                self.send_response(204)
                self.end_headers()
            def log_message(self, *_):
                pass
        return ThreadingHTTPServer(('127.0.0.1', self.args.web_port), Handler)

    def run_cpu(self):
        self.run.mkdir(parents=True, exist_ok=False)
        shutil.copytree(self.args.system_dir / 'dasd', self.run / 'dasd')
        shutil.copyfile(self.args.ros, self.run / 'ros.txt')
        config = (self.args.system_dir / getattr(self.args, 'config', 'gen.cnf')).read_text()
        config = '\n'.join(line for line in config.splitlines()
                           if not re.match(r'^\s*(HTTPPORT|CNSLPORT|LOADPARM)\b', line, re.I))
        config = f'CNSLPORT 127.0.0.1:{self.args.console_port}\n' + config + '\n'
        (self.run / 'gen.cnf').write_text(config)
        (self.run / 'control.rc').write_text('')
        # Each nested script reopens the control file after the outer pause.
        # Static Windows daemon mode does not accept commands on stdin.
        (self.run / 'hercules.rc').write_text('t+' + getattr(self.args, 'console_device', '0009') + '\n' +
            ('' if self.args.reference or getattr(self.args, 'no_ipl', False)
             else 'ipl ' + getattr(self.args, 'ipl_device', '150') + '\n') +
            f'pause 1\nscript control.rc\n' * getattr(self.args, 'control_hours', 4) * 3600 + 'stop\nquit\n')
        env = dict(os.environ, HERCULES_RC=str(self.run / 'hercules.rc'))
        if not self.args.reference:
            # feat370.h disables the Hercules timer; operate the model's
            # active-low console switch too, so both CPUs use the same setup.
            env['M65_INTERVAL_TIMER'] = '0'
        server = self.serve()
        threading.Thread(target=server.serve_forever, daemon=True).start()
        print(f'Console: http://127.0.0.1:{self.args.web_port}/', flush=True)
        offsets = {'m65.log': 0, 'console.log': 0}
        remainder = ''
        console_remainder = ''
        pending_control_until = 0
        pending_control = None
        def observe_model(line):
            if line.startswith('M65WAIT entered '):
                self.cpu_waiting = True
                self.wait_since = time.monotonic()
            elif line.startswith('M65WAIT left'):
                self.cpu_waiting = False
                self.wait_since = None
        with (self.run / 'console.log').open('wb') as output:
            proc = subprocess.Popen([str(self.args.exe.resolve()), '-d', '-f', 'gen.cnf'],
                cwd=self.run, env=env, stdin=subprocess.DEVNULL,
                stdout=output, stderr=subprocess.STDOUT)
            threading.Thread(target=self.console_client, daemon=True).start()
            try:
                while proc.poll() is None:
                    now = time.monotonic()
                    if pending_control_until and now > pending_control_until:
                        (self.run / 'control.rc').write_text('')
                        pending_control_until = 0
                        pending_control = None
                    if not pending_control_until and not self.controls.empty():
                        command = self.controls.get_nowait()
                        (self.run / 'control.rc').write_text(command + '\n')
                        pending_control_until = now + 2
                        pending_control = command
                        self.stage = {'stop': 'CPU paused', 'start': 'CPU running', 'quit': 'Stopping session'}.get(command, self.stage)
                    path = self.run / 'm65.log'
                    if path.exists():
                        with path.open('rb') as log:
                            log.seek(offsets['m65.log'])
                            data = log.read().decode('latin1')
                            offsets['m65.log'] = log.tell()
                        lines = (remainder + data).split('\n')
                        remainder = lines.pop()
                        for line in lines:
                            observe_model(line)
                            match = re.match(r'^[0-9a-f]{6}\([0-9a-f]{6}\)\s+(\d+)\(', line)
                            if match:
                                self.instructions = int(match[1])
                            if 'Execution gone astray' in line:
                                self.errors += 1
                                if self.errors == 1:
                                    self.auto = False
                                    self.controls.put('stop')
                                    (self.run / 'first-failure.txt').write_text(line + '\n')
                                    print('Comparison failure; pausing CPU', flush=True)
                    with (self.run / 'console.log').open('rb') as log:
                        log.seek(offsets['console.log'])
                        data = log.read().decode('latin1')
                        offsets['console.log'] = log.tell()
                    lines = (console_remainder + data).split('\n')
                    console_remainder = lines.pop()
                    if pending_control and any(line.strip() == pending_control for line in lines):
                        (self.run / 'control.rc').write_text('')
                        pending_control_until = 0
                        pending_control = None
                    device = getattr(self.args, 'console_device', '0009')
                    if any(re.search(r'HHCCP048I\s+' + re.escape(device) + r':CCW=0A', line, re.I) for line in lines):
                        self.read_ready.set()
                    if self.args.reference:
                        for line in lines:
                            match = re.match(r'M65REF (WRITE|READ) dev=([0-9A-F]+) instructions=(\d+) (.*)', line)
                            if match:
                                self.instructions = int(match[3])
                                self.milestones.append({'event': match[1].lower(), 'device': match[2],
                                    'instructions': self.instructions,
                                    'elapsed_seconds': round(time.monotonic() - self.started, 3),
                                    'detail': match[4].rstrip('\r')})
                            if 'HHCCP011I' in line and 'Disabled wait state' in line:
                                self.stage = 'Hercules entered a disabled wait'
                                self.auto = False
                    self.automate()
                    self.save_status()
                    time.sleep(.25)
            finally:
                if proc.poll() is None:
                    (self.run / 'control.rc').write_text('stop\nquit\n')
                    try:
                        proc.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        proc.terminate()
                        proc.wait(timeout=10)
                self.finished.set()
                self.stage = f'Session stopped (exit {proc.returncode})'
                self.save_status()
                server.shutdown()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', required=True, type=Path)
    parser.add_argument('--system-dir', required=True, type=Path)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--ros', type=Path, default=Path(__file__).resolve().parents[1] / 'src/ros.txt')
    parser.add_argument('--console-port', type=int, default=3270)
    parser.add_argument('--web-port', type=int, default=8138)
    parser.add_argument('--auto-mft', action='store_true', help='Answer MFT startup prompts on the copied disks')
    parser.add_argument('--reference', action='store_true', help='Use Hercules only; read console milestone counters from an instrumented reference build')
    parser.add_argument('--config', default='gen.cnf', help='Configuration filename inside the system directory')
    parser.add_argument('--ipl-device', default='150', help='IPL device address in hexadecimal')
    parser.add_argument('--console-device', default='0009', help='Four-digit hexadecimal ASCII console address')
    parser.add_argument('--no-ipl', action='store_true', help='Attach the console and wait for a manual IPL')
    parser.add_argument('--control-hours', type=int, default=4,
                        help='Hours of Hercules control-file polling before stopping (default: 4)')
    Session(parser.parse_args()).run_cpu()


if __name__ == '__main__':
    main()
