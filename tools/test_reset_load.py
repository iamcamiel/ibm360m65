"""Exercise the production RX081 gate and native reset/LOAD input contract.

The IPL fixture observes the real load_ipl function with stubbed host I/O. It
does not boot a guest or supply synthetic responses to a live CPU.
"""
import argparse
import json
import subprocess
from pathlib import Path

from test_hercules_write_recording import ROOT, VCVARS, function_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generated = args.generated.resolve()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    fixture = r'''
#include "360_struc.h"
#include <cstdlib>
#include <cstdio>
extern "C" {
DATA360 oldstate{},newstate{};
FILE* lf; double runtime=0; bool logging=true,mon_cycle=false,mon_ros=false;
_ROS ros[4096]{};
int listing(int){return 0;}
void logmsg(char*,...){}
void init_ros(){}
bool m65_ce_ros_branch_taken(bool){return false;}
FULL_INIT
}
using U16=unsigned short;
enum {M65_REG_IO_CMD,M65_REG_IO_RESP};
unsigned current_io_num=0,held_calls=0,checks=0,bad=0,reference_calls=0;
const unsigned idle_unit=0xabc;
struct {void* regs=nullptr;} sysblk;
void check(bool condition){++checks;if(!condition)++bad;}
unsigned read_m65_reg(unsigned){
 return held_calls==50&&!newstate.EXTERNAL_.switches_0.B14?0x40000000:0;
}
void write_m65_reg(unsigned,unsigned){}
void twenty_cycle(void* =nullptr){
 check(newstate.EXTERNAL_.switches_0.B14==1);
 check(newstate.EXTERNAL_.switches_0.B13==0);
 unsigned unit=0;for(int i=0;i<12;++i)
  unit|=(!((newstate.EXTERNAL_.switches_0.F>>(23-i))&1))<<i;
 check(unit==idle_unit);++held_calls;
}
int s370_load_ipl(U16,U16,int,int){
 check(!newstate.EXTERNAL_.switches_0.B14);++reference_calls;return 17;
}
LOAD_IPL
int main(){
 lf=fopen("fixture.log","w");_putenv_s("M65_INTERVAL_TIMER","0");
 full_init();
 check(!newstate.EXTERNAL_.switches_0.B14);
 check(!newstate.EXTERNAL_.switches_0.B13);
 check(newstate.EXTERNAL_.switches_7.B11); // CE remains enabled.
 check(!newstate.EXTERNAL_.switches_7.B13); // Timer-disable policy unchanged.
 check(newstate.RX.rosar.F==0x00b);
 check(newstate.CA.ic_parity_00M07==0&&newstate.CA.ic_parity_08M15==0&&newstate.CA.ic_parity_16M23==0);
 check(M65_NATIVE_POWER_RESET_CYCLES>320);
 check(M65_NATIVE_STARTUP_CYCLES>=10000);
 // KW321 contact decoding must agree with both released/pressed flags.
 for(int pressed=0;pressed<2;pressed++){
  oldstate=newstate;oldstate.EXTERNAL_.switches_0.B14=pressed;
  oldstate.EXTERNAL_.switches_0.B13=pressed;process_KW();
  check(newstate.KW_INT._subsystem_ipl==!pressed);
  check(newstate.KW_INT._interrupt_pb_ce==!pressed);
 }
 // Original RX081: either active-low request sets address bit 8.
 for(int reset=0;reset<2;reset++)for(int scan=0;scan<2;scan++){
  oldstate={};newstate={};
  oldstate.RX_INT._pwr_on_reset_gated=!reset;
  oldstate.DS._set_rosar_scan.B8=!scan;process_RX();
  check(newstate.RX_INT.temp1217==(reset||scan));
  oldstate={};newstate={};
  oldstate.RX_INT.temp1217=reset||scan;oldstate.RX_INT.p4_gate=1;
  process_RX();check(newstate.RX._rosar.B8==!(reset||scan));
 }
 full_init();newstate.KW_INT._reset_delay_ss=1;
 check(load_ipl(0,idle_unit,0,0)==17);
 check(held_calls==50&&reference_calls==1);
 check(!newstate.EXTERNAL_.switches_0.B14);
 check(current_io_num==0x40000000);
 fclose(lf);printf("RESET_LOAD checks=%u bad=%u\n",checks,bad);return bad!=0;
}
'''.replace('FULL_INIT', function_source('360_ros.cpp', 'void full_init()'))
    fixture = fixture.replace('LOAD_IPL', function_source('ipl.c', 'int load_ipl ('))
    (out / 'fixture.cpp').write_text(fixture)
    files = [out / 'fixture.cpp', *sorted(generated.glob('360_*.cpp'))]
    (out / 'build.cmd').write_text(
        f'@echo off\ncall "{VCVARS}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"{ROOT / "hercules"}" '
        f'/I"{generated}" ' + ' '.join(f'"{p}"' for p in files) +
        ' /Fe:fixture.exe\nexit /b %errorlevel%\n')
    for name, command in [('build', ['cmd', '/c', str(out / 'build.cmd')]),
                          ('run', [str(out / 'fixture.exe')])]:
        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        print(name, result.returncode, (result.stdout + result.stderr)[-1200:])
        result.check_returncode()
    (out / 'result.json').write_text(json.dumps({
        'passed': True, 'rx081_truth_table_cases': 4,
        'native_ipl_source_exercised': True, 'pressed_batches': 50,
        'idle_load_interrupt': False, 'ce_enabled': True,
        'ic_default_initialization_preserved': True,
        'scope': 'Production equations and host contact contract; stubbed I/O, no guest boot.'
    }, indent=2))


if __name__ == '__main__':
    main()
