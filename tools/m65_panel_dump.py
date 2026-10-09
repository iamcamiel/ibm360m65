#!/usr/bin/env python3
"""Inspect read-only panel snapshots in M65 interface 1.4 (Linux PCIe BAR0)."""
import argparse
import datetime
import json
import mmap
import os
from pathlib import Path
import struct
import sys
import time

MAGIC = 0x504E4C31
BASE = 0x400


def capture(read32):
    """Magic captures a bank; generation checks detect another reader recapturing it."""
    identity = read32(0)
    interface = read32(0x7FC)
    if identity != 0x03602065:
        raise ValueError(f"Unexpected M65 identity {identity:08x}")
    if interface >> 16 != 1 or interface & 0xFFFF < 4:
        raise ValueError("Panel readback requires interface 1.4; the loaded image has "
                         f"{interface >> 16}.{interface & 0xFFFF}")
    for _ in range(10):
        if read32(BASE) != MAGIC:
            raise ValueError("Panel snapshot signature PNL1 is missing")
        generation = read32(BASE + 4)
        words = [read32(BASE + 4*n) for n in range(2, 24)]
        if generation == read32(BASE + 4):
            flags, divider = words[:2]
            sw = words[2:10]
            led = [words[10+2*b] | (words[11+2*b] & 0xFF) << 32 for b in range(6)]
            return {
                "checked_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "generation": generation,
                "frame_valid": bool(flags & 8),
                "configured": bool(flags & 1),
                "power_on": bool(flags & 2),
                "cpu_panel_reset": bool(flags & 4),
                "waiting_blink_on": bool(flags & 128),
                "buttons_pressed": {name: bool(flags & (1 << bit)) for name, bit in
                                    (("power_on", 4), ("power_off", 5), ("load", 6))},
                "enable_divider": divider,
                "switch_serial_sync": f"{(flags >> 8) & 255:02x}",
                "switch_serial_sample": f"{(flags >> 16) & 255:02x}",
                "switch_banks_active_low": [f"{v & 0xFFFFFF:06x}" for v in sw],
                "switch_pressed_bits": [[n for n in range(24) if not (v >> n) & 1] for v in sw],
                "led_banks_serialized": [f"{v:010x}" for v in led],
                "led_asserted_bits": [[n for n in range(40) if (v >> n) & 1] for v in led],
            }
    raise ValueError("Another reader repeatedly changed the captured bank; retry with one reader")


def format_snapshot(s):
    if not s["frame_valid"]:
        return "No complete panel frame yet (reset or stopped core clock)."
    pressed = ", ".join(k for k, v in s["buttons_pressed"].items() if v) or "none"
    lines = [f"Frame {s['generation']}  configured={s['configured']}  power_on={s['power_on']}  "
             f"panel_reset={s['cpu_panel_reset']}  waiting_blink={s['waiting_blink_on']}",
             f"Buttons pressed: {pressed}"]
    for b, (raw, bits) in enumerate(zip(s['switch_banks_active_low'], s['switch_pressed_bits'])):
        lines.append(f"SW{b}: {raw}  low bits: {','.join(map(str,bits)) or '-'}")
    for b, (raw, bits) in enumerate(zip(s['led_banks_serialized'], s['led_asserted_bits'])):
        lines.append(f"LED{b}: {raw}  asserted bits: {','.join(map(str,bits)) or '-'}")
    lines.append("LED values are serialized FPGA commands, not electrical lamp feedback.")
    return '\n'.join(lines)


def find_resource():
    candidates = []
    for device in Path('/sys/bus/pci/devices').iterdir():
        if (device / 'vendor').read_text().strip() == '0x0360' and \
           (device / 'device').read_text().strip() == '0x2065':
            candidates.append(device / 'resource0')
    if len(candidates) != 1:
        raise ValueError(f"Found {len(candidates)} M65 PCIe devices; specify --resource")
    return candidates[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resource', type=Path, help='PCIe resource0 path (otherwise discover M65)')
    parser.add_argument('--watch', type=float, default=0, metavar='SECONDS', help='repeat at this interval')
    parser.add_argument('--json', action='store_true', help='one JSON object per snapshot')
    args = parser.parse_args()
    if args.watch < 0:
        parser.error('--watch must be nonnegative')
    try:
        fd = os.open(args.resource or find_resource(), os.O_RDONLY | os.O_SYNC)
        try:
            with mmap.mmap(fd, 2048, mmap.MAP_SHARED, mmap.PROT_READ) as mm:
                while True:
                    snapshot = capture(lambda offset: struct.unpack_from('<I', mm, offset)[0])
                    print(json.dumps(snapshot) if args.json else format_snapshot(snapshot), flush=True)
                    if not args.watch:
                        break
                    time.sleep(args.watch)
        finally:
            os.close(fd)
    except (OSError, ValueError) as error:
        print(f"Panel inspection: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
