"""Exercise actual ROS register CLOCK outputs and both emitted decoder passes."""
from pathlib import Path
import argparse,json,re,subprocess
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--generated',type=Path,default=R/'gen/ald');p.add_argument('--original',type=Path,default=R/'gen/logout-repair-r125-20261012/source-snapshot/gen/ald');a=p.parse_args();O=a.output.resolve();O.mkdir(exist_ok=False,parents=True);G=a.generated.resolve()
v=(G/'360_ry.vhd').read_text();equations=[]
for i in range(43):
 for prefix in ['P','M']:
  name=f'{prefix}_ros_reg_bit({i})';expr=re.findall(re.escape(name)+r' <= ([^;]+);',v)[-1];assert '_first' not in expr;equations.append(name+' <= '+expr+';')
decode=[]
for target in ['P_write_9020_reg','M_P9020_regs_to_ls_bus_out']:
 for suffix in ['_first','']:
  name=target+suffix;expr=re.findall(r'\b'+re.escape(name)+r' <= ([^;]+);',v)[-1].split(' when ')[0];decode.append(name+' <= '+expr+';')
source=r'''#include "360_struc.h"
#include <cstdio>
DATA360 oldstate,newstate;
static const unsigned long long mask=(1ULL<<43)-1;
static unsigned long long pattern(bool b){unsigned long long v=0;for(unsigned i=0;i<43;++i)v=(v<<1)|(b^(i%2));return v;}
int main(){unsigned errors=0;init_RY();
 for(unsigned j=0;j<64;++j){
  unsigned long long expected=0;const bool load=j&1,hold=j&2;
  oldstate.RY.ros_reg_bit.F=pattern(j&4);oldstate.RY._ros_reg_bit.F=pattern(j&8);
  SENSE_BITS
  oldstate.RY.clock_p0_forced=load;oldstate.RY_INT._clock_p0M1Onot_block=hold;
  const auto loaded=(load?pattern(j&16):0ULL),held=(hold?oldstate.RY._ros_reg_bit.F:0ULL);
  expected=(~(loaded|held))&mask;
  for(unsigned step=0;step<5;++step){
   if(step){oldstate.RY.clock_p0_forced=0;oldstate.RY_INT._clock_p0M1Onot_block=1;}
   newstate=oldstate;process_RY_clock();
   if((newstate.RY.ros_reg_bit.F&mask)!=expected||(newstate.RY._ros_reg_bit.F&mask)!=(mask^expected))++errors;
   for(unsigned pass=0;pass<2;++pass){oldstate=newstate;process_RY();
    if((newstate.RY.ros_reg_bit.F&mask)!=expected||(newstate.RY._ros_reg_bit.F&mask)!=(mask^expected))++errors;
    const bool control=!(oldstate.RY.ros_reg_bit.B36&&oldstate.RY._ros_reg_bit.B37);
    if(newstate.RY.write_9020_reg!=control||newstate.RY._P9020_regs_to_ls_bus_out!=control)++errors;
   }
   oldstate=newstate;
  }
 }
 printf("RY_OUTPUTS edges=320 bits=43 errors=%u\n",errors);return errors?2:0;
}'''.replace('SENSE_BITS','\n'.join(f'oldstate.EF._cros_bit.B{i}=bool(j&16)^bool({i}%2);' for i in range(43)))
(O/'oracle.cpp').write_text(source)
flags='/nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(R/'hercules')+'" /I"'+str(G)+'" '
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl '+flags+'oracle.cpp "'+str(G/'360_ry.cpp')+'" /Fe:oracle.exe\nif errorlevel 1 exit /b %errorlevel%\ncl '+flags+'oracle.cpp "'+str(a.original.resolve()/'360_ry.cpp')+'" /Fe:original.exe\nexit /b %errorlevel%\n')
tb='''library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;use std.env.all;
entity ry_outputs is end;architecture sim of ry_outputs is
signal clk,seed_enable:std_logic:='0';signal seed_p,seed_m,EF_M_cros_bit:std_logic_vector(0 to 42);
signal P_ros_reg_bit,M_ros_reg_bit:std_logic_vector(0 to 42);
signal P_clock_p0_forced,M_clock_p0M1Onot_block:std_logic;
signal P_write_9020_reg_first,P_write_9020_reg,M_P9020_regs_to_ls_bus_out_first,M_P9020_regs_to_ls_bus_out:std_logic;
function bv(x:natural;b:natural)return std_logic is begin if (x/(2**b)) mod 2=0 then return '0';else return '1';end if;end;
function pattern(x:std_logic)return std_logic_vector is variable a:std_logic_vector(0 to 42);begin for i in a'range loop if i mod 2=0 then a(i):=x;else a(i):=not x;end if;end loop;return a;end;
begin
process(clk)begin if rising_edge(clk)then if seed_enable='1' then P_ros_reg_bit<=seed_p;M_ros_reg_bit<=seed_m;else EQUATIONS end if;end if;end process;
DECODERS
process variable expected:std_logic_vector(0 to 42);variable control:std_logic;variable n:natural:=0;
begin
for j in 0 to 63 loop
 seed_enable<='1';seed_p<=pattern(bv(j,2));seed_m<=pattern(bv(j,3));EF_M_cros_bit<=pattern(bv(j,4));P_clock_p0_forced<=bv(j,0);M_clock_p0M1Onot_block<=bv(j,1);
 wait for 4 ns;clk<='1';wait for 1 ns;clk<='0';wait for 5 ns;seed_enable<='0';
 for i in 0 to 42 loop expected(i):=not((EF_M_cros_bit(i) and P_clock_p0_forced) or(M_ros_reg_bit(i) and M_clock_p0M1Onot_block));end loop;
 for step in 0 to 4 loop
  if step>0 then P_clock_p0_forced<='0';M_clock_p0M1Onot_block<='1';end if;
  wait for 4 ns;clk<='1';wait for 1 ns;
  assert P_ros_reg_bit=expected and M_ros_reg_bit=not expected report "ROS register complement/capture mismatch" severity failure;
  control:=not(expected(36) and not expected(37));
  assert P_write_9020_reg_first=control and P_write_9020_reg=control and M_P9020_regs_to_ls_bus_out_first=control and M_P9020_regs_to_ls_bus_out=control report "RY131 decoder pass mismatch" severity failure;
  clk<='0';wait for 5 ns;n:=n+1;
 end loop;
end loop;assert n=320 severity failure;report "RY_OUTPUTS_PASS 320 core edges,43 bits,both decoder passes";stop;wait;end process;end;
'''.replace('EQUATIONS','\n'.join(equations)).replace('DECODERS','\n'.join(decode));(O/'ry_outputs.vhd').write_text(tb)
def run(name,cmd,expected=0):
 q=subprocess.run(list(map(str,cmd)),cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(q.stdout+q.stderr);print(name,q.returncode,flush=True);assert q.returncode==expected,(name,q.stdout,q.stderr);return q
run('build',['cmd','/c','build.cmd']);fixed=run('cpp',[O/'oracle.exe']);original=run('original',[O/'original.exe'],2)
ghdl=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe';run('analyze',[ghdl,'-a','--std=08','ry_outputs.vhd']);run('vhdl',[ghdl,'-r','--std=08','ry_outputs','--assert-level=error'])
record=dict(passed=True,core_edges=320,register_bits=43,bit_pair_checks=13760,cpp_two_boolean_passes_hold_CLOCK_outputs=True,vhdl_both_decode_passes=True,original_failure_reproduced=True,original_output=original.stdout,fixed_output=fixed.stdout,scope='Actual generated RY C++ CLOCK function and all43 emitted VHDL CLOCK pairs; load/hold transitions and arbitrary prior polarities. Both RY131 emitted decoder passes checked. Excludes full hardware timing equivalence.')
(O/'result.json').write_text(json.dumps(record,indent=2));print(json.dumps(record))
