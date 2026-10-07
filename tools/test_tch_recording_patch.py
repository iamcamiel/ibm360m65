"""Exercise actual source and the exact live replacement as Win32 native code."""
from pathlib import Path
import subprocess
from live_tch_recording_patch import replacement_code
from test_hercules_write_recording import function_source, ROOT

BUILD = ROOT / 'gen/tch-investigation/recording-tests'
BUILD.mkdir(exist_ok=True)
code = replacement_code(0)
source = r'''
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
int io_herc = 0;
SOURCE_FUNCTION
int main() {
    unsigned char code[] = {BYTES};
    const uint32_t output = reinterpret_cast<uint32_t>(&io_herc);
    std::memcpy(code + sizeof(code) - 5, &output, 4);
    void* buffer = VirtualAlloc(nullptr, sizeof(code), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) return 2;
    std::memcpy(buffer, code, sizeof(code));
    DWORD old;
    if (!VirtualProtect(buffer, sizeof(code), PAGE_EXECUTE_READ, &old)) return 2;
    FlushInstructionCache(GetCurrentProcess(), buffer, sizeof(code));
    auto live = reinterpret_cast<void(__cdecl*)(int)>(buffer);
    unsigned tests = 0;
    for (unsigned op = 0; op < 256; ++op)
        for (unsigned channel = 0; channel < 256; ++channel)
            for (unsigned unit : {0u, 0x51u, 0xffu}) {
                unsigned request = (op << 24) | (channel << 8) | unit;
                unsigned expected = (op == 7 && (channel & 12)) ? 0x13579bdf : request;
                io_herc = 0x13579bdf;
                record_herc_io(request);
                if (unsigned(io_herc) != expected) return 3;
                io_herc = 0x13579bdf;
                live(request);
                if (unsigned(io_herc) != expected) return 4;
                ++tests;
            }
    VirtualFree(buffer, 0, MEM_RELEASE);
    std::printf("%u source and live-code recording cases passed\n", tests);
}
'''
source = source.replace('#include <cstdint>', '#include <cstdint>\n#include <initializer_list>')
source = source.replace('SOURCE_FUNCTION', function_source('360_ros.cpp', 'void record_herc_io(int i)'))
source = source.replace('BYTES', ','.join(f'0x{x:02x}' for x in code))
(BUILD / 'fixture.cpp').write_text(source)
(BUILD / 'build.cmd').write_text(
    '@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat" >nul\n'
    'cl /nologo /EHsc /O2 fixture.cpp /Fe:fixture.exe\n')
subprocess.run(['cmd', '/c', str(BUILD / 'build.cmd')], cwd=BUILD, check=True)
subprocess.run([str(BUILD / 'fixture.exe')], check=True)
