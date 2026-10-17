"""Linux PCIe diagnostic reader/control for FPGA1.38/interface1.7.

Default reads frozen evidence only. Arming, capture, halt and release require
explicit flags. Never run against an older image; no reset or IPL operations.
"""
import argparse
import json
import mmap
import os
from pathlib import Path
import struct
import time

BASE = 0x600
WORDS = ['rosar_flags', 'ic', 'kw_checks', 'mc_controls', 'ap_checks', 'sab',
         'sdbi_hi', 'sdbi_lo', 'marks', 'ls_bus', 'bf_mb_checks', 'as_state',
         'd_register', 'st_hi', 'st_lo', 'ku_controls', 'se_response',
         'read_data_hi', 'read_data_lo', 'io_response', 'io_interrupt',
         'ar_ros_check', 'ap_first_checks', 'kw_km_controls', 'se_command',
         'se_address', 'write_data_hi', 'write_data_lo', 'io_command',
         'external_register', 'storage_size', 'reset_ready']


class Diagnostics:
    def __init__(self, read32, write32, timeout=2):
        self.read = read32
        self.write = write32
        self.timeout = timeout
        # Check compatibility before the first command write, including reads.
        if read32(0) != 0x03602065 or read32(0x7F8) != 0x10026 or read32(0x7FC) != 0x10007:
            raise ValueError('Requires FPGA1.38/interface1.7; no commands issued')
        if read32(BASE) != 0x44424731:
            raise ValueError('Missing DBG1 diagnostic signature')

    def command(self, op, arg=0):
        if not 0 <= op <= 12 or not 0 <= arg < 1 << 24:
            raise ValueError('Invalid diagnostic command')
        deadline = time.monotonic() + self.timeout
        while self.read(BASE + 16) & 1:
            if time.monotonic() >= deadline:
                raise TimeoutError('Previous diagnostic command is still pending')
            time.sleep(.001)
        before = self.read(BASE + 4)
        expected = (before + 1) & 0xFFFFFFFF
        self.write(BASE, ((0xA0 + op) << 24) | arg)
        while self.read(BASE + 8) != expected:
            if time.monotonic() >= deadline:
                raise TimeoutError('Diagnostic reply not received; no retry/control recovery attempted')
            time.sleep(.001)
        result = self.read(BASE + 12)
        # Snapshot/history data may legitimately equal an error-code bit pattern.
        if 1 <= op <= 4 or op in (8, 9, 10, 12):
            if result:
                raise RuntimeError(f'Diagnostic control rejected: {result:08X}')
        return result

    def status(self):
        v = self.command(0)
        if v & 0xFF800000:
            raise RuntimeError(f'Status unavailable or event-edge command rejection: {v:08X}')
        return dict(armed=bool(v & 1), frozen=bool(v & 2), halted=bool(v & 4),
                    halt_on_event=bool(v & 8), check_mask=(v >> 4) & 15,
                    changed_checks=(v >> 8) & 15, history_count=(v >> 12) & 511, stepping=bool(v & 1 << 21),
                    step_mode='p0' if v & 1 << 22 else 'ros-change')

    @staticmethod
    def decode_breakpoint(config, target, hits):
        if config & ~0x3FFF or not 1 <= target <= 0xFFFFFF or not 0 <= hits <= 0xFFFFFF:
            raise RuntimeError('Invalid breakpoint metadata or event-edge rejection')
        return dict(enabled=bool(config & 0x1000), rosar=f'{config & 0xFFF:03X}',
                    mode='p0' if config & 0x2000 else 'entry', hit_target=target, hits=hits)

    def breakpoint(self):
        return self.decode_breakpoint(*(self.command(11, i) for i in range(3)))

    def configure_breakpoint(self, address, hit=1, mode='p0'):
        # Validate everything before issuing either command. Neither command arms,
        # discards evidence or releases a halt. Hardware rejects changes while armed.
        if not 0 <= address <= 0xFFF or not 1 <= hit <= 0xFFFFFF or mode not in ('p0', 'entry'):
            raise ValueError('Invalid ROSAR breakpoint address, hit target or mode')
        self.command(10, hit)
        self.command(9, address | 0x1000 | (0x2000 if mode == 'p0' else 0))

    def dump(self):
        result = dict(schema=1, status=self.status(), breakpoint=self.breakpoint())
        if not result['status']['frozen']:
            return result
        generation = self.command(7, 0)
        result['generation'] = generation
        reason = self.command(7, 6)
        if reason & ~31:
            raise RuntimeError('Invalid capture reason')
        result['event_reason'] = dict(check=bool(reason & 1), breakpoint=bool(reason & 2), manual=bool(reason & 4),
                                      step_ros_change=bool(reason & 8), step_p0=bool(reason & 16))
        result['event_step_start_rosar'] = f'{self.command(7, 10):03X}'
        result['event_breakpoint'] = self.decode_breakpoint(self.command(7, 8), self.command(7, 9), self.command(7, 7))
        result['event_core_tick'] = self.command(7, 1) | self.command(7, 2) << 32
        for label, base in [('before', 0), ('at_event', 32)]:
            result[label] = {name: f'{self.command(5, base + i):08X}' for i, name in enumerate(WORDS)}
        count = self.command(7, 3)
        if count > 256:
            raise ValueError('Corrupt history count')
        history = []
        for i in range(count):
            flags = self.command(6, i * 4)
            ticks = self.command(6, i * 4 + 1) | self.command(6, i * 4 + 2) << 32
            history.append(dict(core_tick=ticks, rosar=f'{flags & 4095:03X}',
                                proc_check=bool(flags & 1 << 12), stor_check=bool(flags & 1 << 13),
                                proc_first=bool(flags & 1 << 14), stor_first=bool(flags & 1 << 15),
                                event=bool(flags & 1 << 24), flags=f'{flags:08X}'))
        result['history'] = history
        if generation != self.command(7, 0) or not self.status()['frozen']:
            raise RuntimeError('Capture changed during read; evidence is not coherent')
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resource', type=Path, required=True, help='M65 sysfs PCI resource0')
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--arm', action='store_true', help='Discard prior evidence and arm checks/ring')
    action.add_argument('--capture', action='store_true', help='Freeze current evidence without halting')
    action.add_argument('--halt', action='store_true', help='Stop CPU ALD/SPECIAL updates')
    action.add_argument('--release', action='store_true', help='Explicitly release diagnostic halt')
    action.add_argument('--disarm', action='store_true', help='Stop recording without clearing evidence')
    action.add_argument('--step', choices=['ros-change', 'p0'], help='From halted+armed only: run to next boundary and capture/halt')
    parser.add_argument('--halt-on-event', action='store_true', help='Only with --arm')
    parser.add_argument('--mask', type=lambda x: int(x, 0), default=15, help='PROC/STOR/current/first mask0..15')
    config = parser.add_mutually_exclusive_group()
    config.add_argument('--breakpoint', type=lambda x: int(x, 0), help='Configure ROSAR0..0xFFF; requires disarmed recorder')
    config.add_argument('--disable-breakpoint', action='store_true', help='Disable ROSAR trigger; requires disarmed recorder')
    parser.add_argument('--breakpoint-hit', type=lambda x: int(x, 0), default=1, help='One-based matching hit target1..16777215')
    parser.add_argument('--breakpoint-mode', choices=['p0', 'entry'], default='p0', help='Count matching positive P0 edges or address entries')
    args = parser.parse_args()
    if args.breakpoint is not None:
        if not 0 <= args.breakpoint <= 0xFFF or not 1 <= args.breakpoint_hit <= 0xFFFFFF:
            parser.error('Invalid ROSAR address or hit target')
    elif args.breakpoint_hit != 1 or args.breakpoint_mode != 'p0':
        parser.error('Breakpoint hit/mode options require --breakpoint')
    if (args.breakpoint is not None or args.disable_breakpoint) and (args.capture or args.halt or args.release or args.disarm or args.step):
        parser.error('Breakpoint configuration may be combined only with --arm')
    if args.halt_on_event and not args.arm:
        parser.error('--halt-on-event requires --arm')
    if not 0 <= args.mask <= 15:
        parser.error('--mask must be0..15')
    fd = os.open(args.resource, os.O_RDWR | os.O_SYNC)
    try:
        with mmap.mmap(fd, 2048, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE) as mm:
            d = Diagnostics(lambda off: struct.unpack_from('<I', mm, off)[0],
                            lambda off, value: struct.pack_into('<I', mm, off, value))
            if args.breakpoint is not None:
                d.configure_breakpoint(args.breakpoint, args.breakpoint_hit, args.breakpoint_mode)
            elif args.disable_breakpoint:
                d.command(9, 0)
            if args.arm:
                d.command(1, args.mask | (16 if args.halt_on_event else 0))
            for chosen, op in [(args.capture, 2), (args.halt, 3), (args.release, 4), (args.disarm, 8)]:
                if chosen:
                    d.command(op)
            if args.step:
                d.command(12, 1 if args.step == 'p0' else 0)
            print(json.dumps(d.dump(), indent=2))
    finally:
        os.close(fd)


if __name__ == '__main__':
    main()
