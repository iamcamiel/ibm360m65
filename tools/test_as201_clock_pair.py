"""Check AS201 complementary outputs against actual emitted CLOCK equations."""
from pathlib import Path
import argparse,json,re,subprocess
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--generated',type=Path,default=R/'gen/ald');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=False);G=a.generated.resolve()
cpp=(G/'360_as.cpp').read_text();vhd=(G/'360_as.vhd').read_text();noclock,clock=cpp.split('void process_AS_clock()',1)
for target in ['AS.clock_p2','AS_INT._clock_p3']:
 assert 'newstate.'+target+' =' not in noclock
 assert len(re.findall(re.escape('newstate.'+target+' ='),clock.split('void init_AS()',1)[0]))==1
names=['P_clock_p2','M_clock_p3'];expr=[]
for n in names:
 assert n+'_first <=' not in vhd
 matches=re.findall(r'\b'+n+r' <= ([^;]+);',vhd);assert len(matches)==2 and matches[0] in ["'0'","'1'"];expr.append(matches[1])
source=r'''#include "360_struc.h"
#include <cstdio>
DATA360 oldstate,newstate;
int main(){init_AS();for(unsigned i=0;i<1024;i++){
 bool source=i&1,previous_p2=i&2,previous_p3=i&4;
 oldstate.AS._clock_p1=source;oldstate.AS.clock_p2=previous_p2;oldstate.AS_INT._clock_p3=previous_p3;newstate=oldstate;
 process_AS();if(newstate.AS.clock_p2!=previous_p2||newstate.AS_INT._clock_p3!=previous_p3)return 1;
 process_AS_clock();if(newstate.AS.clock_p2!=!source||newstate.AS_INT._clock_p3!=source)return 2;
 oldstate=newstate;process_AS();if(newstate.AS.clock_p2!=!source||newstate.AS_INT._clock_p3!=source)return 3;
 printf("%u %u %u\n",source,!source,source);
}return 0;}'''
(O/'oracle.cpp').write_text(source)
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(R/'hercules')+'" /I"'+str(G)+'" oracle.cpp "'+str(G/'360_as.cpp')+'" /Fe:oracle.exe\nexit /b %errorlevel%\n')
tb='''library ieee;use ieee.std_logic_1164.all;use std.textio.all;use std.env.all;
entity as201_test is end;architecture sim of as201_test is
signal clk:std_logic:='0';signal M_clock_p1:std_logic:='0';signal P_clock_p2:std_logic:='0';signal M_clock_p3:std_logic:='1';begin
process(clk)begin if rising_edge(clk)then EQUATIONS end if;end process;
process file f:text open read_mode is "vectors.txt";variable l:line;variable v:integer_vector(0 to 2);variable n:natural:=0;
function bv(x:integer)return std_logic is begin if x=0 then return '0';else return '1';end if;end;
begin while not endfile(f)loop readline(f,l);for i in 0 to 2 loop read(l,v(i));end loop;
M_clock_p1<=bv(v(0));wait for 4 ns;clk<='1';wait for 1 ns;
assert P_clock_p2=bv(v(1)) and M_clock_p3=bv(v(2)) report "AS201 complementary core-edge clocks differ" severity failure;
clk<='0';wait for 5 ns;n:=n+1;end loop;assert n=1024 severity failure;report "AS201_PASS 1024 core edges";stop;wait;end process;end;
'''.replace('EQUATIONS','\n'.join(n+' <= '+e+';' for n,e in zip(names,expr)))
(O/'as201_test.vhd').write_text(tb)
def run(name,cmd):
 z=subprocess.run(list(map(str,cmd)),cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(z.stdout+z.stderr);z.check_returncode();return z
run('build',['cmd','/c','build.cmd']);z=run('cpp',[O/'oracle.exe']);(O/'vectors.txt').write_text(z.stdout)
ghdl=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe';run('analyze',[ghdl,'-a','--std=08','as201_test.vhd']);run('vhdl',[ghdl,'-r','--std=08','as201_test','--assert-level=error'])
result=dict(passed=True,core_edges=1024,unique_source_and_previous_output_cases=8,cpp_both_noclock_passes_do_not_advance_clocks=True,actual_emitted_vhdl_clock_equations=expr,scope='Original AS201 P2D/P3G complement, arbitrary previous output values; actual C++ section and emitted VHDL CLOCK expressions. No scheduling change or whole CPU/routed timing claim.')
(O/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
