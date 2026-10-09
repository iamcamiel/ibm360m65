"""Verified recorder-only repair for the unchanged ISK comparison executable."""
import hashlib
from pathlib import Path
import struct
from live_tch_recording_patch import install as install_recorder

EXE_SHA256 = 'ea03ce33b1b4182e769b464be244185de00e0d75fe15565138baacbbe837a64e'

def original_code(data):
    # Exact 36-byte Win32 TCH-only recorder, confirmed by passive disassembly.
    return (bytes.fromhex('558bec8b4d088bc125000000ff3d000000077508'
                          'f7c1000c00007506890d') + struct.pack('<I', data)
            + bytes.fromhex('5dc3'))

def replacement_code(data):
    # cdecl: caller-saved EAX/EDX/flags only; TIO/TCH outside IOCE 1 skip.
    return (bytes.fromhex('8b4424048bd081e2000000ff81fa000000077408'
                          '81fa000000057507a9000c00007505a3')
            + struct.pack('<I', data) + bytes.fromhex('c3'))

def install(pid, executable, report_path):
    if hashlib.sha256(Path(executable).read_bytes()).hexdigest() != EXE_SHA256:
        raise RuntimeError('Executable differs from the tested ISK comparison build')
    return install_recorder(pid, executable, report_path, original_code, replacement_code)
