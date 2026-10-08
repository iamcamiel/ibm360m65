"""Exercise the real MVZ instruction body and its comparison write recording.

Run with Python on Windows with Visual Studio's C++ compiler installed. The
fixture supplies storage and instruction-decoding inputs; it extracts the
instruction itself from general1.c rather than duplicating its implementation.
"""
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'gen' / 'mvz-comparison-test'
VCVARS = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')


def instruction_source():
    source = (ROOT / 'hercules' / 'general1.c').read_text()
    start = source.index('#define MOVE_ZONE_BUMP')
    end = source.index('\n/*-------------------------------------------------------------------*/',
                       source.index('DEF_INST(move_zones)', start))
    return source[start:end]


FIXTURE = r'''
#include <cstdio>
#include <cstring>
#include <vector>
#include <utility>
using BYTE = unsigned char;
using VADR = unsigned;
struct REGS { struct { int pkey = 0; } psw; struct { BYTE* storkey; } dat; };
BYTE memory[65536], storage_key;
unsigned destination, source, length;
std::vector<std::pair<unsigned, BYTE>> writes;
BYTE* address(unsigned addr, REGS* regs) {
    regs->dat.storkey = &storage_key;
    return &memory[addr];
}
void record_herc_write(int addr, int len, char* data) {
    for (int i = 0; i < len; ++i) writes.emplace_back(addr + i, BYTE(data[i]));
}
void record_herc_write(int addr, int len, BYTE* data) {
    record_herc_write(addr, len, reinterpret_cast<char*>(data));
}
#define COMPARE_M65
#define DEF_INST(name) void name(BYTE* inst, REGS* regs)
#define SS_L(inst, regs, len, arn1, addr1, arn2, addr2) \
    do { len = length - 1; arn1 = arn2 = 0; addr1 = destination; addr2 = source; } while (0)
#define ITIMER_SYNC(...) ((void)0)
#define ITIMER_UPDATE(...) ((void)0)
#define MADDR(addr, arn, regs, access, key) address(addr, regs)

INSTRUCTION_SOURCE

int main() {
    unsigned failures = 0;
    struct Case { const char* name; unsigned dst, src, len; bool zeros; };
    const Case cases[] = {
        {"one zero byte", 0x2ee8, 0x21, 1, true},
        {"second reported zero byte", 0x48, 0x2ee8, 1, true},
        {"nonzero zones and preserved digits", 0x500, 0x700, 7, false},
        {"maximum 256-byte length", 0x1000, 0x2000, 256, false},
        {"overlapping operands", 0x301, 0x300, 16, false}
    };
    for (const auto& c : cases) {
        for (unsigned i = 0; i < sizeof(memory); ++i)
            memory[i] = c.zeros ? 0 : BYTE(i * 37 + 0xa5);
        BYTE expected[sizeof(memory)];
        std::memcpy(expected, memory, sizeof(memory));
        for (unsigned i = 0; i < c.len; ++i)
            expected[c.dst + i] = (expected[c.dst + i] & 15) | (expected[c.src + i] & 240);
        destination = c.dst; source = c.src; length = c.len;
        writes.clear();
        REGS regs{}; BYTE inst[6]{};
        move_zones(inst, &regs);
        bool passed = std::memcmp(memory, expected, sizeof(memory)) == 0 && writes.size() == c.len;
        if (writes.size() == c.len)
            for (unsigned i = 0; i < c.len; ++i)
                passed &= writes[i].first == c.dst + i && writes[i].second == expected[c.dst + i];
        std::printf("%s: %s (%zu recorded bytes, expected %u)\n", passed ? "PASS" : "FAIL", c.name, writes.size(), c.len);
        if (!passed) ++failures;
    }
    return failures ? 1 : 0;
}
'''


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / 'fixture.cpp').write_text(FIXTURE.replace('INSTRUCTION_SOURCE', instruction_source()))
    (BUILD / 'build.cmd').write_text(
        f'@echo off\ncall "{VCVARS}" >nul\ncd /d "{BUILD}"\n'
        'cl /nologo /O2 /EHsc /Fe:fixture.exe fixture.cpp >build.log 2>&1\nexit /b %errorlevel%\n')
    subprocess.run(['cmd.exe', '/c', str(BUILD / 'build.cmd')], check=True)
    subprocess.run([str(BUILD / 'fixture.exe')], check=True)


if __name__ == '__main__':
    main()
