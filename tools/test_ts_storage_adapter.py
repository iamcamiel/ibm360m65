"""Exercise the actual TS memory handler in comparison and standalone modes."""
from pathlib import Path
import subprocess
from test_hercules_write_recording import function_source, ROOT

BUILD = ROOT / 'gen/ts-fix/storage-tests'
BUILD.mkdir(parents=True, exist_ok=True)
fixture = r'''
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <map>
#include <cstdlib>
using BYTE = unsigned char;
struct REGS {};
BYTE memory[4096];
struct { BYTE* mainstor = memory; } sysblk;
unsigned current_se_num = 0, command, address, responses[8], locks = 0, unlocks = 0;
bool mon_stor = false;
FILE* lf;
std::map<unsigned, BYTE> writes;
enum { M65_REG_SE_CMD, M65_REG_SE_ADDR, M65_REG_SE_WDATA_HI, M65_REG_SE_WDATA_LO,
       M65_REG_SE_RDATA_HI, M65_REG_SE_RDATA_LO, M65_REG_SE_RESP };
#define D_fprintf(...) ((void)0)
#define fflush(...) ((void)0)
#define OBTAIN_MAINLOCK(regs) (++locks)
#define RELEASE_MAINLOCK(regs) (++unlocks)
#define STORAGE_KEY(...) key
#define STORAGE_KEY_INVALIDATE(...) ((void)0)
#define STORKEY_BADFRM 0
#define STORKEY_INVALIDATE(...) ((void)0)
BYTE key;
unsigned read_m65_reg(unsigned reg) { return reg == M65_REG_SE_CMD ? command : reg == M65_REG_SE_ADDR ? address : 0; }
void write_m65_reg(unsigned reg, unsigned value) { responses[reg] = value; }
unsigned htonl(unsigned n) { return _byteswap_ulong(n); }
void record_65_write(unsigned sea, unsigned high, unsigned low) {
    for (unsigned i = 0; i < 8; ++i)
        if (sea & (0x80000000u >> i)) {
            unsigned word = i < 4 ? high : low;
            writes[(sea & 0x7ffff8) + i] = BYTE(word >> (8 * (3 - i % 4)));
        }
}
void record_herc_write(unsigned, unsigned, BYTE*) {}
void record_65_set_key(unsigned, unsigned) {}
ACTUAL_FUNCTION
void require(bool ok, const char* message) {
    if (!ok) { std::printf("FAIL: %s\n", message); std::exit(1); }
}
unsigned tests;
void exercise(unsigned marks, unsigned selected_value, bool cancelled) {
    REGS regs;
    BYTE before[8];
    for (unsigned i = 0; i < 8; ++i) before[i] = memory[0x700 + i] = BYTE(17 * i + 3);
    for (unsigned i = 0; i < 8; ++i) if (marks & (0x80u >> i)) before[i] = memory[0x700 + i] = BYTE(selected_value);
    std::memset(responses, 0xcc, sizeof(responses));
    writes.clear(); locks = unlocks = 0;
    current_se_num = 0;
    command = 0x80000010u | (cancelled ? 8 : 0);
    address = (marks << 24) | 0x700;
    process_memory(&regs);
    require(responses[M65_REG_SE_RESP] == 0x80000000u, "request acknowledgement");
    require(locks == unlocks && locks == (cancelled ? 0 : 1), "lock coverage");
    if (cancelled) {
        require(responses[M65_REG_SE_RDATA_HI] == 0xccccccccu, "cancelled request returned data");
        require(writes.empty(), "cancelled request recorded a write");
    } else {
        BYTE returned[8];
        unsigned high = htonl(responses[M65_REG_SE_RDATA_HI]);
        unsigned low = htonl(responses[M65_REG_SE_RDATA_LO]);
        std::memcpy(returned, &high, 4); std::memcpy(returned + 4, &low, 4);
        require(!std::memcmp(returned, before, 8), "TS did not return the original doubleword");
    }
    unsigned recorded = 0;
    for (unsigned i = 0; i < 8; ++i) {
        bool marked = !cancelled && (marks & (0x80u >> i));
#ifdef COMPARE_M65
        require(memory[0x700 + i] == before[i], "comparison mode changed native TS input");
        require(writes.count(0x700 + i) == unsigned(marked), "wrong model byte mark");
        if (marked) { ++recorded; require(writes[0x700 + i] == 255, "TS record was not FF"); }
#else
        require(memory[0x700 + i] == (marked ? 255 : before[i]), "standalone TS stored wrong byte");
#endif
    }
    require(writes.size() == recorded, "extra model writes");
    // The same completed command must not be serviced a second time.
    unsigned previous_locks = locks;
    process_memory(&regs);
    require(locks == previous_locks, "repeated command executed twice");
    ++tests;
}
int main() {
    for (unsigned position = 0; position < 8; ++position)
        for (unsigned value = 0; value < 256; ++value) exercise(0x80u >> position, value, false);
    for (unsigned marks = 0; marks < 256; ++marks) exercise(marks, 0x55, false);
    for (unsigned position = 0; position < 8; ++position) exercise(0x80u >> position, 0x80, true);
    std::printf("%u TS storage cases passed\n", tests);
}
'''.replace('ACTUAL_FUNCTION', function_source('cpu.c', 'void process_memory(REGS* regs)'))
(BUILD / 'fixture.cpp').write_text(fixture)
for mode in ('COMPARE_M65', 'SOFTWARE_M65'):
    script = BUILD / f'build-{mode}.cmd'
    script.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat" >nul\n'
        f'cl /nologo /EHsc /O2 /D{mode} fixture.cpp /Fe:fixture-{mode}.exe\n')
    subprocess.run(['cmd', '/c', str(script)], cwd=BUILD, check=True)
    subprocess.run([str(BUILD / f'fixture-{mode}.exe')], check=True)
