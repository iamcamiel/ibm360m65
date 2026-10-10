"""Install the tested IOCE-1 TCH recording filter in one existing Win32 process.

Only record_herc_io is replaced. Verify symbols and original machine code, suspend
the process for the write, preserve its bytes, and roll back on any failure.
Guest state, architectural instruction functions and comparison checks are untouched.
"""
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

CDB = Path(r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe')


def replacement_code(record_address):
    # x86 cdecl: EAX/EDX and flags are caller-saved. Skip only TCH requests
    # whose channel selects IOCE 2..4; keep all other recording unchanged.
    return (bytes.fromhex('8b442404 8bd0 81e2000000ff 81fa00000007 7507 '
                          'a9000c0000 7505 a3')
            + struct.pack('<I', record_address) + bytes.fromhex('c3'))


def install(pid, executable, report_path, original_factory=None, replacement_factory=None):
    report_path = Path(report_path)
    if report_path.exists():
        raise RuntimeError('A patch record already exists; do not apply twice')
    debug = subprocess.run([str(CDB), '-y', str(Path(executable).resolve().parent), '-pv', '-p', str(pid), '-c',
        '.reload /f Hercules.exe; x Hercules!*record_herc_io*; '
        'x Hercules!io_herc; qd'], capture_output=True, text=True, timeout=20)
    report_path.with_suffix('.symbols.log').write_text(debug.stdout + debug.stderr)
    if debug.returncode:
        raise RuntimeError('Cannot resolve original process symbols')
    entry_match = re.search(r'^([0-9a-fA-F]{8})\s+hercules!record_herc_io\s', debug.stdout, re.M | re.I)
    data_match = re.search(r'^([0-9a-fA-F]{8})\s+hercules!io_herc\s', debug.stdout, re.M | re.I)
    if not entry_match or not data_match:
        raise RuntimeError('Missing recorder symbols')
    entry, data = (int(m[1], 16) for m in (entry_match, data_match))
    original = (original_factory(data) if original_factory else
                bytes.fromhex('558bec8b4508a3') + struct.pack('<I', data) + bytes.fromhex('5dc3'))
    code = (replacement_factory or replacement_code)(data)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    nt = C.WinDLL('ntdll')
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    kernel.WriteProcessMemory.argtypes = kernel.ReadProcessMemory.argtypes
    kernel.VirtualAllocEx.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, W.DWORD]
    kernel.VirtualAllocEx.restype = C.c_void_p
    kernel.VirtualProtectEx.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, C.POINTER(W.DWORD)]
    kernel.VirtualFreeEx.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD]
    kernel.FlushInstructionCache.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t]
    kernel.CloseHandle.argtypes = [W.HANDLE]
    nt.NtSuspendProcess.argtypes = nt.NtResumeProcess.argtypes = [W.HANDLE]
    handle = kernel.OpenProcess(0xC38, False, pid)
    if not handle:
        raise C.WinError(C.get_last_error())

    def read(address, count):
        buffer = C.create_string_buffer(count)
        size = C.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buffer, count, C.byref(size)) or size.value != count:
            raise C.WinError(C.get_last_error())
        return buffer.raw

    def write(address, value):
        size = C.c_size_t()
        if not kernel.WriteProcessMemory(handle, address, value, len(value), C.byref(size)) or size.value != len(value):
            raise C.WinError(C.get_last_error())
        if read(address, len(value)) != value:
            raise RuntimeError('Process write did not verify')

    def protect(address, count, mode):
        old = W.DWORD()
        if not kernel.VirtualProtectEx(handle, address, count, mode, C.byref(old)):
            raise C.WinError(C.get_last_error())
        return old.value

    suspended, remote, installed, old_protection = False, None, False, None
    record = {'pid': pid, 'executable': str(Path(executable).resolve()),
              'executable_sha256': hashlib.sha256(Path(executable).read_bytes()).hexdigest(),
              'entry': entry, 'record_address': data, 'original_bytes': original.hex(),
              'replacement_bytes': code.hex(), 'installed': False}
    try:
        if nt.NtSuspendProcess(handle) != 0:
            raise RuntimeError('Cannot suspend process for an atomic recorder replacement')
        suspended = True
        if read(entry, len(original)) != original:
            raise RuntimeError('Recorder machine code differs from the verified Win32 build')
        record['io_herc_before'] = int.from_bytes(read(data, 4), 'little')
        remote = kernel.VirtualAllocEx(handle, None, len(code), 0x3000, 0x04)
        if not remote or remote >= 0x100000000:
            raise RuntimeError('Cannot allocate a Win32 replacement')
        write(remote, code)
        protect(remote, len(code), 0x20)
        jump = b'\xe9' + struct.pack('<I', (remote - entry - 5) & 0xffffffff) + b'\x90'
        record.update(remote_address=remote, entry_patch=jump.hex())
        report_path.write_text(json.dumps(record, indent=2))
        old_protection = protect(entry, len(jump), 0x40)
        installed = True  # Roll back even if a partial entry write fails.
        write(entry, jump)
        protect(entry, len(jump), old_protection)
        if not kernel.FlushInstructionCache(handle, remote, len(code)) or not kernel.FlushInstructionCache(handle, entry, len(jump)):
            raise C.WinError(C.get_last_error())
        record['installed'] = True
        record['io_herc_after'] = int.from_bytes(read(data, 4), 'little')
        report_path.write_text(json.dumps(record, indent=2))
    except Exception:
        if installed:
            protect(entry, 6, 0x40)
            write(entry, original[:6])
            protect(entry, 6, old_protection)
            kernel.FlushInstructionCache(handle, entry, 6)
        if remote:
            kernel.VirtualFreeEx(handle, remote, 0, 0x8000)
        record['installed'] = False
        record['rolled_back'] = installed
        report_path.write_text(json.dumps(record, indent=2))
        raise
    finally:
        if suspended and nt.NtResumeProcess(handle) != 0:
            raise RuntimeError('Recorder written, but process suspension could not be released')
        kernel.CloseHandle(handle)
    return record
