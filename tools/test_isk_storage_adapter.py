"""Test the actual ISK storage handler and generated WA/MC/RF return path.

Uses isolated fixtures, never the active emulator or its disks.
Usage: python tools/test_isk_storage_adapter.py --generated DIR
"""
import argparse
import json
import subprocess
from pathlib import Path
from test_hercules_write_recording import function_source, ROOT, VCVARS

FIXTURE = r'''
#include "360_struc.h"
#include <cstdlib>
#include <cstdint>
extern "C" { DATA360 oldstate{}, newstate{}; }
using BYTE = unsigned char;
struct REGS {};
BYTE memory[8 * 1024 * 1024], keys[4096];
struct { BYTE* mainstor = memory; } sysblk;
unsigned current_se_num, command, address, responses[8], calls, key_reads, last_key_address;
bool mon_stor = false;
FILE* lf;
enum { M65_REG_SE_CMD, M65_REG_SE_ADDR, M65_REG_SE_WDATA_HI, M65_REG_SE_WDATA_LO,
    M65_REG_SE_RDATA_HI, M65_REG_SE_RDATA_LO, M65_REG_SE_RESP };
#undef D_fprintf
#define D_fprintf(...) ((void)0)
#define fflush(...) ((void)0)
#define OBTAIN_MAINLOCK(...) ((void)0)
#define RELEASE_MAINLOCK(...) ((void)0)
BYTE& storage_key(unsigned addr) { ++key_reads; last_key_address=addr; return keys[addr >> 11]; }
#define STORAGE_KEY(addr, regs) storage_key(addr)
#define STORKEY_BADFRM 1
#define STORKEY_INVALIDATE(...) ((void)0)
unsigned read_m65_reg(unsigned reg) { return reg == M65_REG_SE_CMD ? command : reg == M65_REG_SE_ADDR ? address : 0; }
void write_m65_reg(unsigned reg, unsigned value) { ++calls; responses[reg]=value; }
unsigned htonl(unsigned n) { return _byteswap_ulong(n); }
void record_65_write(unsigned, unsigned, unsigned) {}
void record_herc_write(unsigned, unsigned, BYTE*) {}
void record_65_set_key(unsigned, unsigned) {}
ACTUAL_FUNCTION
void require(bool ok, const char* message) {
    if (!ok) { printf("FAIL: %s (cmd=%08x addr=%08x response=%08x)\n",message,command,address,responses[M65_REG_SE_RESP]); exit(1); }
}
void settle(unsigned response, unsigned sequence, bool insert, bool busy) {
    for (unsigned pass=0; pass<16; ++pass) {
        // Boundary controls model the live storage request; interior equations
        // and all key/parity/gate/ingating signals are actual production ALDs.
        oldstate.EXTERNAL_.reg_se_resp.F=response;
        oldstate.WA_INT.insert_key=insert;
        oldstate.WA_INT.se_counter_out.F=sequence;
        oldstate.WA_INT.se_busy=busy;
        oldstate.WA_INT.adv_del=0;
        oldstate.WA_INT.do_sel=oldstate.WA_INT.inc_se_counter=0;
        oldstate.DR.insert_key=insert;
        oldstate.MC_INT.clock_b0=1;
        newstate=oldstate;
        process_WA(); process_MC(); process_RF();
        oldstate=newstate;
    }
}
unsigned cases=0, gate_cases=0;
int main() {
    REGS regs;
    for (unsigned key=0; key<256; ++key)
    for (unsigned sequence=0; sequence<4; ++sequence)
    for (unsigned odd=0; odd<2; ++odd) {
        keys[0x7cd000 >> 11]=BYTE(key);
        // Marks and an arbitrary doubleword within the 2 KiB block must not
        // affect which storage key is returned. Cover both interleaved requests.
        address=0xff7cd600u + odd*8;
        command=(sequence<<30)|4;
        current_se_num=((sequence+3)&3)<<30;
        std::memset(responses,0xcc,sizeof responses);
        key_reads=calls=0;
        process_memory(&regs);
        unsigned expected=(sequence<<30)|0x01000000u|((key&0xf8)<<22);
        require(responses[M65_REG_SE_RESP]==expected,"ISK packed response/key/sequence");
        require(key_reads==1 && last_key_address==0x7cd000,"ISK 2 KiB key lookup");
        require(responses[M65_REG_SE_RDATA_HI]==0xccccccccu && responses[M65_REG_SE_RDATA_LO]==0xccccccccu,"ISK must not read a data word");
        unsigned count=calls;
        process_memory(&regs);
        require(calls==count && key_reads==1,"duplicate request not executed twice");
        oldstate={}; newstate={};
        settle(expected,sequence,true,true);
        require(oldstate.WA.key_advance==1,"matching valid ISK advances");
        require(oldstate.RF.f_bit.F==(key&0xf8),"production WA/MC/RF returns the exact key byte");
        require(oldstate.WA.out_key_parity==!(bool(key&128)^bool(key&64)^bool(key&32)^bool(key&16)^bool(key&8)),"key parity");
        // The observed failing operand uses R1=R2 and nonzero upper bytes.
        unsigned operand=0x007cd600;
        require(((operand&0xffffff00)|oldstate.RF.f_bit.F)==(operand|(key&0xf8)),"preserved operand bytes");
        ++cases;
    }
    for (unsigned sequence=0; sequence<4; ++sequence) {
        unsigned good=(sequence<<30)|0x3d000000u;
        for (unsigned variant=0; variant<4; ++variant) {
            oldstate={}; newstate={};
            unsigned response=variant==0 ? good^0x40000000u : variant==1 ? good&~0x01000000u : good;
            settle(response,sequence,variant!=2,variant!=3);
            require(oldstate.WA.key_advance==0,"stale/invalid/non-ISK/inactive responses cannot advance");
            require(oldstate.RF.f_bit.F==0,"rejected response cannot alter F");
            ++gate_cases;
        }
        current_se_num=((sequence+3)&3)<<30;
        command=(sequence<<30)|12; address=0xff7cd608u;
        key_reads=0;
        process_memory(&regs);
        require(responses[M65_REG_SE_RESP]==(sequence<<30) && key_reads==0,"cancelled ISK must not publish key-valid");
        current_se_num=((sequence+3)&3)<<30;
        command=sequence<<30;
        process_memory(&regs);
        require(responses[M65_REG_SE_RESP]==(sequence<<30),"normal read clears key-valid");
#ifndef COMPARE_M65
        for (unsigned five=0; five<32; ++five) {
            current_se_num=((sequence+3)&3)<<30;
            command=(sequence<<30)|(five<<25)|2;
            keys[0x7cd000>>11]=1;
            process_memory(&regs);
            require(keys[0x7cd000>>11]==((five<<3)|1),"standalone SSK key packing preserves bad-frame bit");
        }
#endif
    }
    printf("ISK_STORAGE_PASS: %u actual handler/WA/MC/RF cases; %u rejected-response cases; cancellation, duplicate requests, ordinary reads and standalone SSK round trips passed\n",cases,gate_cases);
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=ROOT/'gen/isk-fix/storage-tests')
    args=parser.parse_args()
    generated=args.generated.resolve()
    build=args.output.resolve(); build.mkdir(parents=True,exist_ok=True)
    (build/'fixture.cpp').write_text(FIXTURE.replace('ACTUAL_FUNCTION',function_source('cpu.c','void process_memory(REGS* regs)')))
    results={}
    for mode in ('COMPARE_M65','SOFTWARE_M65'):
        script=build/f'build-{mode}.cmd'
        script.write_text(f'@echo off\ncall "{VCVARS}" >nul\n'
            f'cl /nologo /EHsc /O2 /w /D{mode} /I"{generated}" /I"{ROOT / "hercules"}" '
            f'fixture.cpp "{generated / "360_wa.cpp"}" "{generated / "360_mc.cpp"}" "{generated / "360_rf.cpp"}" '
            f'/Fe:fixture-{mode}.exe >build-{mode}.log 2>&1\nexit /b %errorlevel%\n')
        subprocess.run(['cmd','/c',str(script)],cwd=build,check=True)
        result=subprocess.run([str(build/f'fixture-{mode}.exe')],cwd=build,capture_output=True,text=True)
        (build/f'simulation-{mode}.log').write_text(result.stdout+result.stderr)
        print(result.stdout,end='')
        assert result.returncode==0
        results[mode]={'passed':True,'key_response_cases':2048,'rejected_response_cases':16}
    (build/'result.json').write_text(json.dumps(results,indent=2)+'\n')

if __name__=='__main__':
    main()
