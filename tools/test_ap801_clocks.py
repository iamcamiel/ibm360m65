"""Check the AP801 BJ/BK complement at each core edge in emitted C++/VHDL."""
from pathlib import Path
import argparse, json, re, subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--generated', type=Path, default=ROOT/'gen/ald')
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
gen = a.generated.resolve()
cpp = (gen/'360_ap.cpp').read_text(); vhdl = (gen/'360_ap.vhd').read_text()
targets = ['clock_p2_extended_or_error', '_clock_p3_extended_O_no_error']
body, clock = cpp.split('void process_AP_clock()', 1)
for target in targets:
    assert 'newstate.AP_INT.'+target+' =' not in body
    assert len(re.findall(r'newstate\.AP_INT\.'+target+r' =', clock.split('void init_AP()', 1)[0])) == 1
names = ['P_clock_p2_extended_or_error', 'M_clock_p3_extended_O_no_error']
expressions = []
for name in names:
    assert name+'_first <=' not in vhdl
    matches = re.findall(r'\b'+name+r' <= ([^;]+);', vhdl)
    assert len(matches) == 2 and matches[0] in ["'0'", "'1'"]
    expressions.append(matches[1])

source = r'''#include "360_struc.h"
#include <cstdio>
DATA360 oldstate,newstate;
int main(){ init_AP();
 for(unsigned i=0;i<1024;++i){
  const bool p1=i&1,p3=i&2,inhibit=i&4;
  // Deliberately arbitrary previous outputs detect dependence on a stale BJ.
  oldstate.AP_INT.clock_p2_extended_or_error=bool(i&8);
  oldstate.AP_INT._clock_p3_extended_O_no_error=bool(i&16);
  oldstate.AP_INT._clock_p1=p1;oldstate.AP_INT._clock_p3=p3;
  oldstate.AP._inhibit_clock_padd_hs_error=inhibit;
  newstate=oldstate;process_AP();
  if(newstate.AP_INT.clock_p2_extended_or_error!=bool(i&8)||newstate.AP_INT._clock_p3_extended_O_no_error!=bool(i&16))return 1;
  process_AP_clock();
  const bool positive=!(p1&&p3&&inhibit),negative=!positive;
  if(newstate.AP_INT.clock_p2_extended_or_error!=positive||newstate.AP_INT._clock_p3_extended_O_no_error!=negative)return 2;
  oldstate=newstate;process_AP();
  if(newstate.AP_INT.clock_p2_extended_or_error!=positive||newstate.AP_INT._clock_p3_extended_O_no_error!=negative)return 3;
  printf("%u %u %u %u %u\n",p1,p3,inhibit,positive,negative);
 }
}'''
(out/'oracle.cpp').write_text(source)
(out/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
    +'cl /nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(ROOT/'hercules')+'" /I"'+str(gen)+'" oracle.cpp "'+str(gen/'360_ap.cpp')+'" /Fe:oracle.exe\nexit /b %errorlevel%\n')
tb='''library ieee;use ieee.std_logic_1164.all;use std.textio.all;use std.env.all;
entity ap801_clock_test is end;architecture sim of ap801_clock_test is
signal clk:std_logic:='0';signal M_clock_p1,M_clock_p3,M_inhibit_clock_padd_hs_error:std_logic:='0';
signal P_clock_p2_extended_or_error:std_logic:='0';signal M_clock_p3_extended_O_no_error:std_logic:='1';
begin
process(clk) begin if rising_edge(clk) then EQUATIONS end if;end process;
process file f:text open read_mode is "vectors.txt";variable l:line;variable values:integer_vector(0 to 4);variable n:natural:=0;
function bv(n:integer)return std_logic is begin if n=0 then return '0';else return '1';end if;end;
begin while not endfile(f) loop
readline(f,l);for i in 0 to 4 loop read(l,values(i));end loop;
M_clock_p1<=bv(values(0));M_clock_p3<=bv(values(1));M_inhibit_clock_padd_hs_error<=bv(values(2));
wait for 4 ns;clk<='1';wait for 1 ns;
assert P_clock_p2_extended_or_error=bv(values(3)) and M_clock_p3_extended_O_no_error=bv(values(4)) report "AP801 complement mismatch" severity failure;
clk<='0';wait for 5 ns;n:=n+1;
end loop;assert n=1024 severity failure;report "AP801_CLOCK_PASS 1024 core edges";stop;wait;end process;end;
'''.replace('EQUATIONS','\n'.join(n+' <= '+e+';' for n,e in zip(names,expressions)))
(out/'ap801_clock_test.vhd').write_text(tb)
def run(name,command):
    r=subprocess.run(list(map(str,command)),cwd=out,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r
run('build',['cmd','/c','build.cmd']); result=run('cpp',[out/'oracle.exe']);(out/'vectors.txt').write_text(result.stdout)
ghdl=ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
run('analyze',[ghdl,'-a','--std=08','ap801_clock_test.vhd'])
run('vhdl',[ghdl,'-r','--std=08','ap801_clock_test','--assert-level=error'])
record=dict(passed=True,core_edges=1024,input_truth_combinations=8,previous_output_combinations=4,
            cpp_second_pass_does_not_advance_clocks=True,
            scope='Actual AP C++ module and emitted AP801 VHDL clock expressions; full CPU and routed timing excluded')
(out/'result.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
