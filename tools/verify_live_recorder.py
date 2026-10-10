"""Read-only verification of a recorder patch in the backend's live child.

Old patch records lack a creation time. Verify the exact entry jump and remote
replacement bytes in addition to path, hash, PID and backend-parent identity;
a matching recycled PID alone is never sufficient. No patch is installed here.
"""
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from urllib.parse import urlparse


def discover_emulator(backend, executable):
    port = urlparse(backend).port
    if not port or urlparse(backend).hostname not in ('127.0.0.1', 'localhost'):
        raise RuntimeError('Live patch verification requires a local backend')
    # The backend listener is the Python launcher; Hercules is its child.
    result = subprocess.run(['netstat', '-ano', '-p', 'tcp'], capture_output=True,
                            text=True, check=True, timeout=15)
    owners = {int(row[-1]) for line in result.stdout.splitlines()
              if len(row := line.split()) == 5 and row[0] == 'TCP'
              and row[1].rsplit(':', 1)[-1] == str(port) and row[3] == 'LISTENING'}
    if len(owners) != 1:
        raise RuntimeError('Backend listener identity is missing or ambiguous')
    parent = owners.pop()
    result = subprocess.run(['powershell', '-NoProfile', '-Command',
        'Get-Process Hercules -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id | ConvertTo-Json'],
        capture_output=True, text=True, check=True, timeout=15)
    pids = json.loads(result.stdout or '[]')
    if isinstance(pids, int):
        pids = [pids]
    matches = []
    for pid in pids:
        identity = process_identity(pid)
        if identity['parent_pid'] == parent and Path(identity['executable']).resolve() == Path(executable).resolve():
            matches.append(identity)
    if len(matches) != 1:
        raise RuntimeError('Backend must own exactly one matching emulator')
    return matches[0]


def process_identity(pid):
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)]
    kernel.GetProcessTimes.argtypes = [W.HANDLE, *([C.POINTER(W.FILETIME)] * 4)]
    class Basic(C.Structure):
        _fields_ = [('exit_status', C.c_void_p), ('peb', C.c_void_p),
                    ('affinity', C.c_void_p), ('priority', C.c_void_p),
                    ('pid', C.c_size_t), ('parent', C.c_size_t)]
    nt = C.WinDLL('ntdll')
    nt.NtQueryInformationProcess.argtypes = [W.HANDLE, W.ULONG, C.c_void_p, W.ULONG, C.c_void_p]
    handle = kernel.OpenProcess(0x400, False, pid)
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        path = C.create_unicode_buffer(32768); size = W.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, C.byref(size)):
            raise C.WinError(C.get_last_error())
        times = [W.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle, *[C.byref(t) for t in times]):
            raise C.WinError(C.get_last_error())
        info = Basic()
        if nt.NtQueryInformationProcess(handle, 0, C.byref(info), C.sizeof(info), None):
            raise RuntimeError('Cannot discover emulator parent')
        return dict(pid=pid, executable=path.value, parent_pid=info.parent,
                    creation_filetime=(times[0].dwHighDateTime << 32) | times[0].dwLowDateTime)
    finally:
        kernel.CloseHandle(handle)


def read_memory(pid, address, count):
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]; kernel.OpenProcess.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    handle = kernel.OpenProcess(0x410, False, pid)
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        buffer = C.create_string_buffer(count); size = C.c_size_t()
        if not kernel.ReadProcessMemory(handle, address, buffer, count, C.byref(size)) or size.value != count:
            raise C.WinError(C.get_last_error())
        return buffer.raw
    finally:
        kernel.CloseHandle(handle)


def verify_live_patch(backend, patch):
    from live_tio_recording_patch import EXE_SHA256, replacement_code
    identity = discover_emulator(backend, patch['executable'])
    if (not patch['installed'] or identity['pid'] != patch['pid']
            or patch['executable_sha256'] != EXE_SHA256
            or hashlib.sha256(Path(identity['executable']).read_bytes()).hexdigest() != EXE_SHA256):
        raise RuntimeError('Patch does not identify the live emulator')
    if 'creation_filetime' in patch and patch['creation_filetime'] != identity['creation_filetime']:
        raise RuntimeError('Patch belongs to a different process lifetime')
    entry, remote = patch['entry'], patch['remote_address']
    jump = b'\xe9' + struct.pack('<I', (remote - entry - 5) & 0xffffffff) + b'\x90'
    code = replacement_code(patch['record_address'])
    if (bytes.fromhex(patch['entry_patch']) != jump or bytes.fromhex(patch['replacement_bytes']) != code
            or read_memory(identity['pid'], entry, len(jump)) != jump
            or read_memory(identity['pid'], remote, len(code)) != code):
        raise RuntimeError('Live recorder bytes do not match the verified patch')
    if identity != discover_emulator(backend, patch['executable']):
        raise RuntimeError('Emulator identity changed during verification')
    return dict(identity, recorder_bytes_verified=True)
