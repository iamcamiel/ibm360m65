"""Local browser TSO terminal backed by the portable ws3270 client."""
import argparse, json, subprocess, threading, time
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.request import Request, urlopen
from software_ipl import PAGE

class Terminal:

    def __init__(self, args):
        self.args = args
        self.lock = threading.RLock()
        args.run_dir.mkdir(parents=True, exist_ok=True)
        self.proc = subprocess.Popen([str(args.exe.resolve()), '-model', '3278-2', '-tn', 'IBM-3278-2@' + args.device, '-clear', 'aidWait', '-trace', '-tracefile', str((args.run_dir / 'protocol.trace').resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=(args.run_dir / 'client-errors.txt').open('w'), text=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        self.action('Connect(N:S:127.0.0.1:' + str(args.console_port) + ')')

    def action(self, command):
        with self.lock:
            self.proc.stdin.write(command + '\n')
            self.proc.stdin.flush()
            rows = []
            while True:
                line = self.proc.stdout.readline().rstrip('\r\n')
                if not line and self.proc.poll() is not None:
                    raise RuntimeError('3270 client stopped')
                if line in ('ok', 'error'):
                    if line == 'error':
                        raise ValueError('\n'.join(rows))
                    return rows
                rows.append(line)

    def state(self):
        with self.lock:
            rows = self.action('Ascii()')
            screen = '\n'.join((x[6:].rstrip() for x in rows if x.startswith('data: ')))
            status = rows[-1]
            value = {'console': screen, 'status': 'TSO terminal · ' + ('ready for input' if status.startswith('U ') else 'processing'),
                     'cursor': status.split()[8:10]}
            path = self.args.run_dir / 'screen.txt'
            old = path.read_text() if path.exists() else None
            if screen != old:
                path.write_text(screen)
                with (self.args.run_dir / 'screens.jsonl').open('a') as output:
                    output.write(json.dumps(value) + '\n')
            return value

    def input(self, text):
        with self.lock:
            # TCAM may unlock before its final screen/cursor write. Wait for
            # a stable screen so input does not land in a previous page.
            previous, stable = None, 0
            for _ in range(20):
                state = self.state()
                current = (state['console'], state['cursor'], state['status'])
                stable = stable + 1 if current == previous and state['status'].endswith('ready for input') else 0
                if stable >= 3:
                    break
                previous = current
                time.sleep(.1)
            else:
                raise ValueError('Wait for the host to finish, or use Reset')
            if 'ENTER LOGON' in state['console']:
                self.action('MoveCursor(0,0)')
            if text:
                self.action('String(' + json.dumps(text.upper()) + ')')
                self.action('EraseEOF()')
            self.action('Enter()')
            with (self.args.run_dir / 'operator-input.jsonl').open('a') as output:
                output.write(json.dumps({'text': text}) + '\n')

    def run(self):
        terminal = self

        class Handler(BaseHTTPRequestHandler):

            def log_message(self, *_):
                pass

            def do_GET(self):
                try:
                    if self.path == '/state':
                        data = json.dumps(terminal.state()).encode()
                        mime = 'application/json'
                    elif self.path == '/':
                        page = PAGE.replace('IBM 360 · OS console', 'OS/360 · TSO terminal').replace('Operator command', 'TSO command').replace('Entering a command takes over from automatic startup. Resume the CPU before continuing a paused session.', 'CAMIEL is available without a password. Clear requests the logon screen; PA2 continues paged output.')
                        buttons = ''.join(('<button type="button" onclick="post(\'/key\',{key:\'' + key + '\'}).catch(e=>alert(e.message))">' + label + '</button>' for key, label in [('clear', 'Clear'), ('pa1', 'Attention'), ('pa2', 'PA2'), ('reset', 'Reset')]))
                        page = page.replace('maxlength="148"', 'maxlength="72"').replace('PA2 continues paged output.', 'PA2 continues paged output. Keep each input line within 72 characters.')
                        data = page.replace('<button>Send</button>', '<button>Send</button>' + buttons).encode()
                        mime = 'text/html; charset=utf-8'
                    else:
                        self.send_error(404)
                        return
                    self.send_response(200)
                    self.send_header('Content-Type', mime)
                    self.send_header('Content-Length', str(len(data)))
                    self.send_header('Cache-Control', 'no-store')
                    self.end_headers()
                    self.wfile.write(data)
                except Exception as e:
                    self.send_error(500, str(e))

            def do_POST(self):
                try:
                    length = int(self.headers.get('Content-Length', 0))
                    if not 0 < length <= 2048:
                        raise ValueError('Invalid input size')
                    v = json.loads(self.rfile.read(length))
                    if self.path == '/input':
                        text = v['text']
                        if not isinstance(text, str) or len(text) > 72 or any((not 32 <= ord(c) < 127 for c in text)):
                            raise ValueError('Use one ASCII command, at most 72 characters')
                        terminal.input(text)
                    elif self.path == '/key':
                        terminal.action({'clear': 'Clear()', 'pa1': 'PA(1)', 'pa2': 'PA(2)', 'reset': 'Reset()'}[v['key']])
                    elif self.path == '/control' and v['command'] in ('start', 'stop'):
                        with urlopen(Request(terminal.args.control_url + '/control', json.dumps(v).encode(), {'Content-Type': 'application/json'}), timeout=5):
                            pass
                    else:
                        raise ValueError('Unknown action')
                    self.send_response(204)
                    self.end_headers()
                except Exception as e:
                    self.send_error(400, str(e))
        print(f'TSO terminal: http://127.0.0.1:{self.args.web_port}/', flush=True)
        try:
            ThreadingHTTPServer(('127.0.0.1', self.args.web_port), Handler).serve_forever()
        finally:
            self.proc.terminate()
            self.proc.wait()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--exe', type=Path, required=True)
    p.add_argument('--console-port', type=int, required=True)
    p.add_argument('--web-port', type=int, required=True)
    p.add_argument('--device', default='00C1')
    p.add_argument('--run-dir', type=Path, required=True)
    p.add_argument('--control-url', required=True)
    Terminal(p.parse_args()).run()
if __name__ == '__main__':
    main()
