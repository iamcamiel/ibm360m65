"""AP321 scan bit32 and zero FAA PSBAR reads in the plain 360/65 model."""
from pathlib import Path
import argparse,json,re,subprocess,sys
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--native',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(exist_ok=False);N=a.native.resolve();G=R/'gen/ald'
sys.path.insert(0,str(R/'tools'));from audit_ald_logic import normalize_cpp
cpp=r'''#include "360_struc.h"
#include <cstdio>
extern "C" {double runtime=0;void logmsg(char*,...){}void trace_probe(const char*){}}
void process_AP();void process_BF();void process_DR_clock();void process_LS_clock();void init_BF();void init_DR();void init_LS();
unsigned parity(){return oldstate.LS.local_store_bus_p00M07<<3|oldstate.LS.local_store_bus_p08M15<<2|oldstate.LS.local_store_bus_p16M23<<1|oldstate.LS.local_store_bus_p24M31;}
int main(){unsigned ap=0,ps=0;
for(unsigned v=0;v<2048;++v){oldstate={};newstate={};oldstate.KT.scan_out_bus_bit.B32=(v>>0)&1;oldstate.AP_INT.enable_scan_bypass=(v>>1)&1;oldstate.AP_INT.sum.B32=(v>>2)&1;oldstate.AP_INT.sum.B36=(v>>3)&1;oldstate.AP_INT.sum.B28=(v>>4)&1;oldstate.AP_INT.zero_shift_or_enable_scan=(v>>5)&1;oldstate.AP_INT.left_4_shift=(v>>6)&1;oldstate.AP_INT.right_4_shift=(v>>7)&1;oldstate.AP_INT._clock_p3_extended=(v>>8)&1;oldstate.AP_INT.clock_p2_extended=(v>>9)&1;oldstate.AP.paddl.B32=(v>>10)&1;
bool want=!(((v&3)==3)||(((v>>2)&1)&&((v>>5)&1)&&((v>>8)&1))||(((v>>3)&1)&&((v>>6)&1)&&((v>>8)&1))||(((v>>4)&1)&&((v>>7)&1)&&((v>>8)&1))||(((v>>9)&1)&&((v>>10)&1)));process_AP();if(newstate.AP_INT._latched_sum.B32!=want)return 2;++ap;}
for(unsigned h=0;h<128;++h)for(unsigned par=0;par<16;++par)for(unsigned data=0;data<2;++data){oldstate={};newstate={};init_BF();init_DR();init_LS();oldstate.RY.ros_reg_bit.F=h;oldstate.RY._ros_reg_bit.F=(~(unsigned long long)h)&0x7ffffffffffULL;oldstate.RY._P9020_regs_to_ls_bus_out=!(h>=64&&h<96);for(int slot=0;slot<32;++slot){oldstate.ls_mem[slot].F=data?0xce:0;oldstate.ls_par[slot].F=par;}oldstate.FE.ext_reg.F=0;oldstate.FE.ext_reg_p00M07=oldstate.FE.ext_reg_p08M15=oldstate.FE.ext_reg_p16M23=oldstate.FE.ext_reg_p24M31=1;oldstate.RG._g_reg_bit.F=255;oldstate.RG._g_reg_bit_p0M7=0;newstate=oldstate;
for(int step=0;step<16;++step){newstate=oldstate;process_DR_clock();process_BF();process_LS_clock();oldstate=newstate;}
bool selected=h==66;if(oldstate.BF_INT.gate_psbar_to_ls_bus!=selected||oldstate.DR._select_psbar_reg!=(h!=2&&h!=66))return 3;
if(selected&&(oldstate.LS.local_store_bus.F!=0||parity()!=15)){printf("h=%u data=%08x p=%x\n",h,oldstate.LS.local_store_bus.F,parity());return 4;}
if(h==0&&(oldstate.LS.local_store_bus.F!=(data?0xce:0)||parity()!=par))return 5;
if(h==65&&(oldstate.LS.local_store_bus.F!=0||parity()!=15))return 6;++ps;}
printf("AP321_PSbar_PASS ap_pin_cases=%u source_cases=%u state_bytes=%zu\n",ap,ps,sizeof(DATA360));return 0;}
'''
(O/'native.cpp').write_text(cpp);flags='/nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(R/'hercules')+'" /I"'+str(G)+'" '
objects=[N/'cpu.obj',*N.glob('360_*.obj')]
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl '+flags+'native.cpp '+' '.join('"'+str(x)+'"'for x in objects)+' /Fe:native.exe\nexit /b %errorlevel%\n')
for name,args in [('build',['cmd','/c','build.cmd']),('native',[str(O/'native.exe')])]:
 z=subprocess.run(args,cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(z.stdout+z.stderr);print(name,z.returncode,(z.stdout+z.stderr)[-600:],flush=True);z.check_returncode()
state_bytes=int(re.search(r'state_bytes=(\d+)',z.stdout)[1])
# Extract actual emitted C++ and both VHDL Boolean-pass equations, including
# the new BF read gate and all four parity products. DR select remains CLOCK.
checks=[]
for section,target in [('AP','AP_INT._latched_sum.B32'),('BF','BF_INT.gate_psbar_to_ls_bus'),('BF','BF_INT._gate_psbar_to_ls_bus'),('DR','DR._select_psbar_reg')]:
 c=(G/('360_'+section.lower()+'.cpp')).read_text();e=re.search(r'newstate\.'+re.escape(target)+r' = (.+);',c)[1];checks.append(dict(section=section,target=target,cpp=e))
# AP321's independent 11-pin oracle, tested against both emitted VHDL passes.
source=(G/'360_ap.vhd').read_text();eqs=[]
mapping={'KT_P_scan_out_bus_bit(32)':0,'P_enable_scan_bypass':1,'P_sum(32)':2,'P_sum(36)':3,'P_sum(28)':4,'P_sum_first(32)':2,'P_sum_first(36)':3,'P_sum_first(28)':4,'P_zero_shift_or_enable_scan':5,'P_left_4_shift':6,'P_right_4_shift':7,'M_clock_p3_extended':8,'P_clock_p2_extended':9,'P_paddl(32)':10,'P_paddl_first(32)':10}
for target in ['M_latched_sum_first(32)','M_latched_sum(32)']:
 e=re.search(r'^\s*'+re.escape(target)+r' <= (.*);',source,re.M)[1].split(' when ')[0];original=e
 e=e.replace('KT_P_scan_out_bus_bit_first(32)','x(0)')
 for ref,bit in sorted(mapping.items(),key=lambda kv:-len(kv[0])):e=e.replace(ref,'x('+str(bit)+')')
 assert not re.search('[A-Z]_',e),e;eqs.append(e);checks.append(dict(target=target,vhdl=original))
tb='''library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;use std.env.all;
entity ap321 is end;architecture sim of ap321 is signal x:std_logic_vector(10 downto 0);signal a,b:std_logic;begin
a<=FIRST;b<=SECOND;
process variable v:std_logic_vector(10 downto 0);variable want:std_logic;begin
for i in 0 to 2047 loop v:=std_logic_vector(to_unsigned(i,11));x<=v;want:=not ((v(0) and v(1)) or (v(2) and v(5) and v(8)) or (v(3) and v(6) and v(8)) or (v(4) and v(7) and v(8)) or (v(9) and v(10)));wait for 1 ns;assert a=want and b=want severity failure;end loop;
report "AP321_PASS 2048 pin cases, both Boolean passes";stop;wait;end process;end;
'''.replace('FIRST',eqs[0]).replace('SECOND',eqs[1]);(O/'ap321.vhd').write_text(tb)
g=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,args in [('analyze',[str(g),'-a','--std=08','ap321.vhd']),('vhdl',[str(g),'-r','--std=08','ap321','--assert-level=error'])]:
 z=subprocess.run(args,cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(z.stdout+z.stderr);print(name,z.returncode,(z.stdout+z.stderr)[-500:],flush=True);z.check_returncode()
(O/'result.json').write_text(json.dumps(dict(passed=True,ap321_pin_cases=2048,vhdl_passes=2,psbar_native_source_cases=4096,state_bytes=state_bytes,equations=checks,scope='PSBAR zero data and normal odd parity F in plain360/65; original DR CLOCK select/read gate, H2 write and H65 external-source isolation, arbitrary stored LS parity. Native pin/source and VHDL bit32 tests; not MVT completion or full FAA/hardware equivalence.'),indent=2))
