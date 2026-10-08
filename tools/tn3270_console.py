"""Minimal 3270 model-2 console for the local OS/360 installation.

Order and address definitions follow hercules/console.c. Unsupported host
orders fail visibly rather than silently corrupting the displayed screen.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import queue
import socket
import threading
from urllib.request import Request, urlopen

from software_ipl import PAGE

ADDRESS = bytes.fromhex('40c1c2c3c4c5c6c7c8c94a4b4c4d4e4f50d1d2d3d4d5d6d7d8d95a5b5c5d5e5f6061e2e3e4e5e6e7e8e96a6b6c6d6e6ff0f1f2f3f4f5f6f7f8f97a7b7c7d7e7f')


def encode_address(address):
    return bytes((ADDRESS[(address >> 6) & 63], ADDRESS[address & 63]))


def decode_address(high, low):
    return ((high & (63 if high & 0xc0 else 255)) << 6 | (low & 63)) if high & 0xc0 else (high << 8 | low)


class Screen:
    def __init__(self, tso=False):
        self.tso = tso
        self.cells = bytearray(1920)
        self.fields = {}
        self.cursor = 0
        self.aid = 0x60
        self.unlocked = False

    def field(self, position):
        if not self.fields:
            return None
        starts = sorted(self.fields)
        return max((start for start in starts if start <= position), default=starts[-1])

    def protected(self, position):
        start = self.field(position)
        return start is not None and bool(self.fields[start] & 0x20)

    def text(self):
        chars = []
        for index, value in enumerate(self.cells):
            chars.append(' ' if index in self.fields or value < 0x40 else bytes((value,)).decode('cp037'))
        return '\n'.join(''.join(chars[row:row + 80]).rstrip() for row in range(0, 1920, 80))

    def apply(self, data):
        command = data[0]
        if command in (0xf2, 0xf6, 0x6e):
            return
        if command not in (0xf1, 0xf5, 0x7e, 0x6f):
            raise ValueError(f'Unsupported 3270 command {command:02x}')
        if command == 0x6f:
            for position in range(1920):
                if not self.protected(position) and position not in self.fields:
                    self.cells[position] = 0
            self.unlocked = True
            return
        if command in (0xf5, 0x7e):
            self.cells = bytearray(1920)
            self.fields = {}
        if data[1] & 2:
            self.unlocked = True
            if self.tso:
                self.aid = 0x60
        if self.tso and data[1] & 1:
            self.fields = {p: a & ~1 for p, a in self.fields.items()}
        index, position = 2, 0
        while index < len(data):
            value = data[index]
            index += 1
            if value == 0x11:
                position = decode_address(*data[index:index + 2]) % 1920
                index += 2
            elif value == 0x1d:
                self.fields[position] = data[index]
                self.cells[position] = 0
                index += 1
                position = (position + 1) % 1920
            elif value == 0x13:
                self.cursor = position
            elif value in (0x3c, 0x12):
                end = decode_address(*data[index:index + 2]) % 1920
                index += 2
                repeated = 0
                if value == 0x3c:
                    repeated = data[index]
                    index += 1
                while position != end:
                    if value == 0x3c or not self.protected(position):
                        self.cells[position] = repeated
                        self.fields.pop(position, None)
                    position = (position + 1) % 1920
            elif value == 0x08:
                self.cells[position] = data[index]
                index += 1
                position = (position + 1) % 1920
            elif value == 0x28:
                index += 2
            elif value in (0x29, 0x2c):
                count = data[index]
                index += 1
                for _ in range(count):
                    kind, attribute = data[index:index + 2]
                    index += 2
                    if kind == 0xc0:
                        self.fields[position] = attribute
                if value == 0x29:
                    self.cells[position] = 0
                    position = (position + 1) % 1920
            elif value == 0x05:
                starts = sorted(start for start, attribute in self.fields.items() if not attribute & 0x20)
                if starts:
                    position = (next((start for start in starts if start > position), starts[0]) + 1) % 1920
            elif value < 0x40 and value not in (0, 0x1c, 0x1e):
                raise ValueError(f'Unsupported 3270 order {value:02x}')
            else:
                self.fields.pop(position, None)
                self.cells[position] = value
                position = (position + 1) % 1920

    def enter(self, text):
        start = self.field(self.cursor)
        if start is None or self.fields[start] & 0x20:
            start = next((p for p, a in sorted(self.fields.items()) if not a & 0x20), None)
        if start is None and not self.tso:
            raise ValueError('No input field on the current screen')
        position = (start + 1) % 1920 if start is not None else 0
        capacity = 0
        while (position + capacity) % 1920 not in self.fields and capacity < 1920:
            capacity += 1
        if len(text) > capacity:
            raise ValueError(f'Input field holds {capacity} characters')
        encoded = text.upper().encode('cp037')
        if text:
            if self.tso and start is not None:
                self.fields[start] |= 1
            for offset in range(capacity):
                # NIP consumes a fixed 80-byte Read Buffer area; pad with
                # EBCDIC blanks for its parameter checks.
                self.cells[(position + offset) % 1920] = encoded[offset] if offset < len(encoded) else (0 if self.tso else 0x40)
            self.cursor = (position + len(encoded)) % 1920
        self.aid = 0x7d
        packet = bytes((self.aid,)) + encode_address(self.cursor)
        # Enter alone submits any command prefilled by the host (for example
        # the second Enter confirming a console deletion request).
        field_text = bytes(self.cells[(position + offset) % 1920] for offset in range(capacity)).rstrip(b'\x00\x40')
        if field_text:
            packet += (b'\x11' + encode_address(position) if self.fields else b'') + field_text.replace(b'\x00', b'')
        self.unlocked = False
        return packet

    def read_modified(self):
        if self.aid in (0x6d, 0x6c, 0x6e, 0x6b):
            return bytes((self.aid,))
        packet = bytes((self.aid,)) + encode_address(self.cursor)
        if not self.fields:
            return packet + bytes(c for c in self.cells if c)
        for start, attribute in sorted(self.fields.items()):
            if not attribute & 1:
                continue
            position = (start + 1) % 1920
            packet += b'\x11' + encode_address(position)
            while position not in self.fields:
                if self.cells[position]:
                    packet += bytes((self.cells[position],))
                position = (position + 1) % 1920
        return packet

    def clear(self):
        self.cells = bytearray(1920)
        self.fields = {}
        self.cursor = 0
        self.aid = 0x6d
        self.unlocked = False
        return bytes((self.aid,))

    def read_buffer(self):
        # Hercules positions channel reads within this complete terminal buffer.
        packet = bytearray(bytes((self.aid,)) + encode_address(self.cursor))
        for position, cell in enumerate(self.cells):
            packet.extend((0x1d, self.fields[position]) if position in self.fields else (cell,))
        return bytes(packet)


class Terminal:
    def __init__(self, args):
        self.args = args
        self.screen = Screen(args.tso)
        self.commands = queue.Queue()
        self.status = 'Connecting to MVT console'
        self.lock = threading.Lock()
        self.connected = threading.Event()
        self.failed = None
        args.run_dir.mkdir(parents=True, exist_ok=True)

    def state(self):
        with self.lock:
            return {'console': self.screen.text(), 'status': self.status}

    def network(self):
        try:
            with socket.create_connection(('127.0.0.1', self.args.console_port), timeout=5) as sock:
                def exact(length):
                    value = b''
                    while len(value) < length:
                        part = sock.recv(length - len(value))
                        if not part:
                            raise ConnectionError('Console disconnected')
                        value += part
                    return value
                for expected, answer in [(b'\xff\xfd\x18', b'\xff\xfb\x18'),
                                         (b'\xff\xfa\x18\x01\xff\xf0', b'\xff\xfa\x18\x00IBM-3278-2@' + self.args.device.encode() + b'\xff\xf0'),
                                         (b'\xff\xfd\x19\xff\xfb\x19', b'\xff\xfb\x19\xff\xfd\x19'),
                                         (b'\xff\xfd\x00\xff\xfb\x00', b'\xff\xfb\x00\xff\xfd\x00')]:
                    if exact(len(expected)) != expected:
                        raise ValueError('Unexpected terminal negotiation')
                    sock.sendall(answer)
                sock.settimeout(.2)
                self.connected.set()
                pending, record, telnet = None, bytearray(), False
                while True:
                    # Clear cancels a queued command and is allowed with the
                    # keyboard locked, as on a physical 3270.
                    with self.commands.mutex:
                        clear = next((c for c in self.commands.queue if c.get('key') == 'clear'), None)
                        if clear:
                            self.commands.queue.clear()
                            pending = clear
                    if pending is None and not self.commands.empty():
                        pending = self.commands.get_nowait()
                    if pending is not None and (self.screen.unlocked or pending.get('key') == 'clear'):
                        with self.lock:
                            packet = self.screen.clear() if pending.get('key') == 'clear' else self.screen.enter(pending['text'])
                            self.status = ('TSO terminal' if self.args.tso else 'MVT console') + ' · processing'
                        sock.sendall(packet.replace(b'\xff', b'\xff\xff') + b'\xff\xef')
                        with (self.args.run_dir / 'operator-input.jsonl').open('a') as output:
                            output.write(json.dumps(pending) + '\n')
                        pending = None
                    try:
                        data = sock.recv(65536)
                    except socket.timeout:
                        continue
                    if not data:
                        raise ConnectionError('Console disconnected')
                    for value in data:
                        if telnet:
                            telnet = False
                            if value == 255:
                                record.append(value)
                            elif value == 239:
                                with self.lock:
                                    self.screen.apply(record)
                                    self.status = ('TSO terminal' if self.args.tso else 'MVT console') + ' · ' + ('ready for input' if self.screen.unlocked else 'processing')
                                    screen = self.screen.text()
                                    response = None
                                    if record and record[0] in (0xf2, 0xf6, 0x6e):
                                        response = b'\x60' + encode_address(self.screen.cursor)
                                        if record[0] == 0xf2:
                                            response = self.screen.read_buffer()
                                        elif self.args.tso:
                                            response = self.screen.read_modified()
                                if response is not None:
                                    sock.sendall(response.replace(b'\xff', b'\xff\xff') + b'\xff\xef')
                                (self.args.run_dir / 'screen.txt').write_text(screen)
                                with (self.args.run_dir / 'screens.jsonl').open('a') as output:
                                    output.write(json.dumps({'record': record.hex(), 'screen': screen}) + '\n')
                                record.clear()
                            else:
                                raise ValueError(f'Unexpected telnet command {value:02x}')
                        elif value == 255:
                            telnet = True
                        else:
                            record.append(value)
        except Exception as error:
            self.failed = str(error)
            self.status = 'Console stopped: ' + str(error)
            print(self.status, flush=True)

    def run(self):
        terminal = self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass
            def do_GET(self):
                if self.path == '/state':
                    data, mime = json.dumps(terminal.state()).encode(), 'application/json'
                elif self.path == '/':
                    page = PAGE
                    if terminal.args.tso:
                        page = page.replace('IBM 360 · OS console', 'OS/360 · TSO terminal')
                        page = page.replace('<button>Send</button>', '<button>Send</button><button type="button" onclick="post(\'/key\',{key:\'clear\'}).catch(e=>alert(e.message))">Clear</button>')
                        page = page.replace('Operator command', 'TSO command').replace('Entering a command takes over from automatic startup. Resume the CPU before continuing a paused session.', 'Use Clear to request the logon prompt. Enter a TSO command and press Send.')
                    data, mime = page.encode(), 'text/html; charset=utf-8'
                else:
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(data)))
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                self.wfile.write(data)
            def do_POST(self):
                try:
                    length = int(self.headers.get('Content-Length', 0))
                    if not 0 < length <= 2048:
                        raise ValueError('Invalid command size')
                    value = json.loads(self.rfile.read(length))
                    if self.path == '/input':
                        text = value['text']
                        if not isinstance(text, str) or len(text) > 148 or any(not 32 <= ord(c) < 127 for c in text):
                            raise ValueError('Use a single ASCII command')
                        if terminal.failed:
                            raise ValueError(terminal.failed)
                        terminal.commands.put({'text': text})
                    elif self.path == '/key' and value.get('key') == 'clear':
                        terminal.commands.put({'key': 'clear'})
                    elif self.path == '/control' and value['command'] in ('start', 'stop'):
                        request = Request(terminal.args.control_url.rstrip('/') + '/control', json.dumps(value).encode(),
                                          {'Content-Type': 'application/json'})
                        with urlopen(request, timeout=5):
                            pass
                    else:
                        raise ValueError('Unknown action')
                    self.send_response(204)
                    self.end_headers()
                except Exception as error:
                    self.send_error(400, str(error))
        threading.Thread(target=self.network, daemon=True).start()
        print(f'Console: http://127.0.0.1:{self.args.web_port}/', flush=True)
        ThreadingHTTPServer(('127.0.0.1', self.args.web_port), Handler).serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--console-port', required=True, type=int)
    parser.add_argument('--web-port', required=True, type=int)
    parser.add_argument('--device', default='00C0')
    parser.add_argument('--run-dir', required=True, type=Path)
    parser.add_argument('--control-url', required=True)
    parser.add_argument('--tso', action='store_true', help='Enable TSO input and the terminal Clear key')
    Terminal(parser.parse_args()).run()


if __name__ == '__main__':
    main()
