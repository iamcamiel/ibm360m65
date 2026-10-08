"""Run actual Hercules store/instruction bodies against a storage fixture.

The fixture supplies decoding, storage translation and atomic operations. The
functions under test are extracted from the current sources, so missing records,
wrong lengths, advanced pointers and records left by access faults are observable.
Both comparison-enabled and ordinary builds are exercised.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'gen' / 'hercules-write-tests'
VCVARS = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat')


def function_source(filename, marker):
    text = (ROOT / 'hercules' / filename).read_text()
    clean = re.sub(r'/\*.*?\*/|//[^\n]*', lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)
    at = clean.index(marker)
    start = text.rfind('\n', 0, at) + 1
    body = clean.index('{', at)
    depth = 0
    for end in range(body, len(text)):
        depth += (clean[end] == '{') - (clean[end] == '}')
        if depth == 0:
            return text[start:end + 1]
    raise ValueError(f'Unclosed function: {filename} {marker}')


FIXTURE = r'''
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <cstdint>
using BYTE = unsigned char;
using U16 = uint16_t;
using U32 = uint32_t;
using U64 = uint64_t;
using VADR = unsigned;
using VADR_L = unsigned;
struct REGS {
    struct { int pkey = 0, cc = 0; } psw;
    struct { BYTE* storkey; } dat;
    BYTE* mainstor;
    U32 gr[16]{}, cr[16]{}, fpr[8]{};
};
struct { unsigned cpus = 1; } sysblk;
BYTE memory[16384], key;
std::map<int, BYTE> writes;
unsigned target = 0, first_reg = 0, last_reg = 0, source = 0, length = 1;
BYTE immediate = 0x0f;
int denied = -1;
bool noncontiguous = false;
unsigned tests = 0, failures = 0;
#ifdef COMPARE_M65
constexpr bool recording = true;
#else
constexpr bool recording = false;
#endif
struct AccessFault {};
unsigned physical(unsigned addr) {
    addr &= 0x1fff;
    return noncontiguous && (addr & 0x1800) == 0x800 ? addr + 0x1800 : addr;
}
BYTE* address(unsigned addr, REGS* regs) {
    if (int(addr) == denied) throw AccessFault{};
    regs->dat.storkey = &key;
    return memory + physical(addr);
}
void record_herc_write(int addr, int len, char* data) {
    for (int i = 0; i < len; ++i) writes[addr + i] = BYTE(data[i]);
}
void record_herc_write(int addr, int len, BYTE* data) {
    record_herc_write(addr, len, reinterpret_cast<char*>(data));
}
void record_herc_write_char(int addr, char value) { writes[addr] = BYTE(value); }
void put(void* p, U64 value, unsigned length) {
    for (unsigned i = 0; i < length; ++i)
        static_cast<BYTE*>(p)[i] = BYTE(value >> (8 * (length - i - 1)));
}
U32 swap32(U32 value) {
    return ((value & 255) << 24) | ((value & 0xff00) << 8) |
           ((value >> 8) & 0xff00) | (value >> 24);
}
int cmpxchg4(U32* old, U32 value, BYTE* ptr) {
    U32 current; std::memcpy(&current, ptr, 4);
    if (current != *old) { *old = current; return 1; }
    std::memcpy(ptr, &value, 4); return 0;
}
int cmpxchg1(BYTE* old, BYTE value, BYTE* ptr) {
    if (*ptr != *old) { *old = *ptr; return 1; }
    *ptr = value; return 0;
}
#define ARCH_DEP(name) name
#define DEF_INST(name) void name(BYTE* inst, REGS* regs)
#define _VSTORE_C_STATIC static
#define _VSTORE_FULL_C_STATIC static
#define GR_L(i) gr[(i)]
#define CR_L(i) cr[(i)]
#define RS(inst, regs, r1, r3, b2, addr2) \
    do { r1 = first_reg; r3 = last_reg; b2 = 0; addr2 = target; } while (0)
#define RX_(inst, regs, r1, b2, addr2) \
    do { r1 = first_reg; b2 = 0; addr2 = target; } while (0)
#define S(inst, regs, b2, addr2) do { b2 = 0; addr2 = target; } while (0)
#define SI(inst, regs, i2, b1, addr1) do { i2 = immediate; b1 = 0; addr1 = target; } while (0)
#define SS_L(inst, regs, len, b1, addr1, b2, addr2) \
    do { len = length - 1; b1 = b2 = 0; addr1 = target; addr2 = source; } while (0)
#define HFPREG_CHECK(...) ((void)0)
#define FPR2I(r) (r)
#define FW_CHECK(...) ((void)0)
#define PRIV_CHECK(...) ((void)0)
#define PTT(...) ((void)0)
#define sched_yield() ((void)0)
#define PERFORM_SERIALIZATION(...) ((void)0)
#define OBTAIN_MAINLOCK(...) ((void)0)
#define RELEASE_MAINLOCK(...) ((void)0)
#define ITIMER_SYNC(...) ((void)0)
#define ITIMER_UPDATE(...) ((void)0)
#define STORKEY_REF 4
#define STORKEY_CHANGE 2
#define ADDRESS_MAXWRAP(regs) 0x1fff
#define likely(x) (x)
#define unlikely(x) (x)
#define MADDR(addr, arn, regs, access, pkey) address(addr, regs)
#define STORE_HW(ptr, value) put(ptr, value, 2)
#define STORE_FW(ptr, value) put(ptr, value, 4)
#define store_fw(ptr, value) STORE_FW(ptr, value)
#define STORE_DW(ptr, value) put(ptr, value, 8)
#define CSWAP32(value) swap32(value)

ABSOLUTE_FIELD_MACRO
FUNCTIONS

void reset() {
    std::memset(memory, 0xcc, sizeof(memory));
    writes.clear(); key = 0; denied = -1; noncontiguous = false;
}
void check(const char* name, const std::map<int, BYTE>& expected, bool other = true) {
    bool passed = other && writes == (recording ? expected : std::map<int, BYTE>{});
    for (auto byte : expected) passed &= memory[physical(byte.first)] == byte.second;
    ++tests; if (!passed) ++failures;
    std::printf("%s: %s (%zu recorded bytes)\n", passed ? "PASS" : "FAIL", name, writes.size());
}
std::map<int, BYTE> expected_value(unsigned addr, U64 value, unsigned length) {
    std::map<int, BYTE> result;
    for (unsigned i = 0; i < length; ++i)
        result[(addr + i) & 0x1fff] = BYTE(value >> (8 * (length - i - 1)));
    return result;
}
int main() {
    REGS regs{}; regs.mainstor = memory; BYTE inst[6]{};
    for (unsigned addr : {0x100u, 0x103u}) {
        for (U64 value : {U64(0), U64(0x8123456789abcdef)}) {
            reset(); vstore8(value, addr, 0, &regs);
            check("eight-byte aligned/unaligned zero/nonzero", expected_value(addr, value, 8));
        }
    }
    for (unsigned addr : {0x7fcu, 0x1ffcu}) {
        reset(); noncontiguous = true;
        vstore8_full(0x8123456789abcdef, addr, 0, &regs);
        check("eight-byte split page/address wrap", expected_value(addr, 0x8123456789abcdef, 8), key == 6);
    }
    reset(); regs.fpr[0] = 0x81234567; regs.fpr[1] = 0x89abcdef;
    target = 0xb8; first_reg = 0;
    store_float_long(inst, &regs);
    check("STD through real vstore8", expected_value(target, 0x8123456789abcdef, 8));
    for (unsigned width : {1u, 2u, 4u, 8u}) {
        reset(); bool fault = false; denied = 0x123;
        try {
            if (width == 1) vstoreb(0, denied, 0, &regs);
            if (width == 2) vstore2(0, denied, 0, &regs);
            if (width == 4) vstore4(0, denied, 0, &regs);
            if (width == 8) vstore8(0, denied, 0, &regs);
        } catch (AccessFault&) { fault = true; }
        check("access fault leaves no phantom record", {}, fault && memory[0x123] == 0xcc);
    }
    reset(); denied = 0x800; bool fault = false;
    try { vstore8_full(0, 0x7fc, 0, &regs); } catch (AccessFault&) { fault = true; }
    check("second-page access fault before eight-byte store", {}, fault && memory[0x7fc] == 0xcc);
    reset(); vstoreb(0x80, 0x123, 0, &regs);
    check("single-byte value", expected_value(0x123, 0x80, 1));
    reset(); vstore2(0x89ab, 0x123, 0, &regs);
    check("halfword value", expected_value(0x123, 0x89ab, 2));
    for (BYTE before : {BYTE(0), BYTE(0x80), BYTE(0xff)}) {
        reset(); target = 0x123; memory[target] = before;
        test_and_set(inst, &regs);
        check("TS both condition codes, including unchanged FF", expected_value(target, 255, 1), regs.psw.cc == before >> 7);
    }
    for (bool succeeds : {false, true}) {
        reset(); target = 0x124; first_reg = 1; last_reg = 3;
        STORE_FW(memory + target, 0x12345678);
        regs.gr[1] = succeeds ? 0x12345678 : 0x87654321;
        regs.gr[3] = 0x89abcdef;
        compare_and_swap(inst, &regs);
        if (succeeds) check("successful CS records store", expected_value(target, 0x89abcdef, 4), regs.psw.cc == 0);
        else check("failed CS records no store", {}, regs.psw.cc == 1 && regs.gr[1] == 0x12345678 && memory[target] == 0x12);
    }
    for (unsigned addr : {0x200u, 0x7fcu}) {
        reset(); noncontiguous = true; target = addr; first_reg = 14; last_reg = 1;
        std::map<int, BYTE> expected;
        for (unsigned i = 0; i < 16; ++i) regs.cr[i] = 0x81234500 + i;
        for (unsigned i = 0; i < 4; ++i) {
            auto word = expected_value(target + i * 4, regs.cr[(14 + i) & 15], 4);
            expected.insert(word.begin(), word.end());
        }
        store_control(inst, &regs);
        check("STCTL register wrap, contiguous/split physical pages", expected);
    }
    reset(); target = 0x7fc; first_reg = 14; last_reg = 1; denied = 0x800; fault = false;
    try { store_control(inst, &regs); } catch (AccessFault&) { fault = true; }
    check("STCTL second-page access fault records no store", {}, fault && memory[0x7fc] == 0xcc);
    reset(); BYTE (&field)[8] = *reinterpret_cast<BYTE(*)[8]>(memory + 0x128);
    STORE_DW(field, 0x8123456789abcdef);
#ifdef COMPARE_M65
    RECORD_HERC_ABSOLUTE_FIELD(&regs, field);
#endif
    check("absolute CPU field recording", expected_value(0x128, 0x8123456789abcdef, 8));
    for (unsigned count : {1u, 256u}) {
        reset(); BYTE bytes[256];
        for (unsigned i = 0; i < count; ++i) bytes[i] = BYTE(i);
        vstorec(bytes, count - 1, 0x100, 0, &regs);
        std::map<int, BYTE> expected;
        for (unsigned i = 0; i < count; ++i) expected[0x100 + i] = bytes[i];
        check("character-store minimum/maximum length", expected);
    }
    struct Op { const char* name; void (*run)(BYTE*, REGS*); BYTE result; bool string; };
    const Op ops[] = {
        {"NI", and_immediate, 0, false}, {"OI", or_immediate, 0xff, false},
        {"XI", exclusive_or_immediate, 0xff, false},
        {"NC", and_character, 0, true}, {"OC", or_character, 0xff, true},
        {"XC", exclusive_or_character, 0xff, true},
        {"MVN", move_numerics, 0xff, true}, {"MVZ", move_zones, 0, true},
        {"TR", translate, 0x81, true}
    };
    for (const auto& op : ops) {
        for (unsigned count : {1u, 9u}) {
            if (!op.string && count != 1) continue;
            reset(); target = 0x100; source = 0x500; length = count;
            std::memset(memory + target, 0xf0, count);
            std::memset(memory + source, 0x0f, count);
            memory[source + 0xf0] = 0x81;
            op.run(inst, &regs);
            std::map<int, BYTE> expected;
            for (unsigned i = 0; i < count; ++i) expected[target + i] = op.result;
            check(op.name, expected);
        }
    }
    for (unsigned count : {1u, 4u, 16u}) {
        reset(); target = 0x100; first_reg = 14; last_reg = (14 + count - 1) & 15;
        std::map<int, BYTE> expected;
        for (unsigned i = 0; i < 16; ++i) regs.gr[i] = 0x81234500 + i;
        for (unsigned i = 0; i < count; ++i) {
            auto word = expected_value(target + i * 4, regs.gr[(14 + i) & 15], 4);
            expected.insert(word.begin(), word.end());
        }
        store_multiple(inst, &regs);
        check("STM one/four/all registers including register wrap", expected);
    }
    std::printf("%u cases, %u failures, comparison %s\n", tests, failures, recording ? "enabled" : "disabled");
    return failures ? 1 : 0;
}
'''


def main():
    bodies = [function_source('vstore.h', f'ARCH_DEP({name})')
              for name in ('vstoreb', 'vstore2', 'vstore4', 'vstore8_full', 'vstore8', 'vstorec')]
    source = (ROOT / 'hercules' / 'general1.c').read_text()
    for macro in ('MOVE_NUMERIC_BUMP', 'MOVE_ZONE_BUMP'):
        start = source.index(f'#define {macro}')
        bodies.append(source[start:source.index('\n/*---', start)])
    for filename, name in [('float.c', 'store_float_long'), ('general1.c', 'compare_and_swap'),
                           ('general2.c', 'test_and_set'), ('control.c', 'store_control')]:
        bodies.append(function_source(filename, f'DEF_INST({name})'))
    for filename, names in [('general1.c', ('and_immediate', 'and_character', 'exclusive_or_immediate',
                                         'exclusive_or_character', 'move_numerics', 'move_zones')),
                            ('general2.c', ('or_immediate', 'or_character', 'translate', 'store_multiple'))]:
        for name in names:
            bodies.append(function_source(filename, f'DEF_INST({name})'))
    header = (ROOT / 'hercules' / '360_minstruc.h').read_text()
    macro = header[header.index('#define RECORD_HERC_ABSOLUTE_FIELD'):header.index('void record_herc_set_key')]
    BUILD.mkdir(parents=True, exist_ok=True)
    (BUILD / 'fixture.cpp').write_text(FIXTURE.replace('FUNCTIONS', '\n'.join(bodies))
                                     .replace('ABSOLUTE_FIELD_MACRO', macro))
    for comparison in (True, False):
        variant = 'compare' if comparison else 'ordinary'
        (BUILD / 'build.cmd').write_text(
            f'@echo off\ncall "{VCVARS}" >nul\ncd /d "{BUILD}"\n'
            f'cl /nologo /O2 /EHsc {"/DCOMPARE_M65" if comparison else ""} '
            f'/Fe:{variant}.exe fixture.cpp >{variant}-build.log 2>&1\nexit /b %errorlevel%\n')
        subprocess.run(['cmd.exe', '/c', str(BUILD / 'build.cmd')], check=True)
        subprocess.run([str(BUILD / f'{variant}.exe')], check=True)


if __name__ == '__main__':
    main()
