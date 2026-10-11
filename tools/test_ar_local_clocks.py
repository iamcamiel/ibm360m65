"""Verify AR311/AR801 inverter timing in emitted C++ and VHDL clock banks."""
from pathlib import Path
import argparse, json, re, subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--generated', type=Path, default=ROOT/'gen/ald')
a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
gen = a.generated.resolve()
cpp = (gen/'360_ar.cpp').read_text(); vhdl = (gen/'360_ar.vhd').read_text()
targets = ['clock_p2_forced_O_not_ros_ck', '_clock_p1_O_not_ros_ck', 'clock_p2_O_not_ros_ck']
body, clock = cpp.split('void process_AR_clock()', 1)
for target in targets:
    assert not re.search(r'newstate\.AR_INT\.'+target+r' =', body), target
    assert len(re.findall(r'newstate\.AR_INT\.'+target+r' =', clock.split('void init_AR()', 1)[0])) == 1
names = ['P_clock_p2_forced_O_not_ros_ck', 'M_clock_p1_O_not_ros_ck', 'P_clock_p2_O_not_ros_ck']
expressions = []
for name in names:
    assert name+'_first <=' not in vhdl, name
    matches = re.findall(r'\b'+name+r' <= ([^;]+);', vhdl)
    assert len(matches) == 2 and matches[0] in ["'0'", "'1'"], (name, matches)
    assert ' when ' not in matches[1], name
    expressions.append(matches[1])

source = r'''#include "360_struc.h"
#include <cstdio>
DATA360 oldstate,newstate;
int main(){ init_AR();
 for(unsigned i=0;i<1024;++i){
  const bool p0=i%20<8, enable=(i/80)%2==0, reset=i%79!=0;
  oldstate.AS.clock_p0=p0;oldstate.AP._inhibit_local_clock_p0=enable;oldstate.KU._reset_ros_m_field=reset;
  newstate=oldstate;
  const bool p1=oldstate.AR_INT._clock_p1_O_not_ros_ck;
  const bool p2=oldstate.AR_INT.clock_p2_O_not_ros_ck,forced=oldstate.AR_INT.clock_p2_forced_O_not_ros_ck;
  process_AR();
  if(newstate.AR_INT._clock_p1_O_not_ros_ck!=p1||newstate.AR_INT.clock_p2_O_not_ros_ck!=p2||newstate.AR_INT.clock_p2_forced_O_not_ros_ck!=forced)return 1;
  process_AR_clock();
  const bool expected_p1=!(enable&&p0), expected_p2=!p1, expected_forced=!p1||!reset;
  if(newstate.AR_INT._clock_p1_O_not_ros_ck!=expected_p1||newstate.AR_INT.clock_p2_O_not_ros_ck!=expected_p2||newstate.AR_INT.clock_p2_forced_O_not_ros_ck!=expected_forced)return 2;
  oldstate=newstate;process_AR();
  if(newstate.AR_INT._clock_p1_O_not_ros_ck!=expected_p1||newstate.AR_INT.clock_p2_O_not_ros_ck!=expected_p2||newstate.AR_INT.clock_p2_forced_O_not_ros_ck!=expected_forced)return 3;
  printf("%u %u %u %u %u %u\n",p0,enable,reset,expected_forced,expected_p1,expected_p2);
  oldstate=newstate;
 }
}'''
(out/'oracle.cpp').write_text(source)
(out/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
    +'cl /nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I"'+str(ROOT/'hercules')+'" /I"'+str(gen)+'" oracle.cpp "'+str(gen/'360_ar.cpp')+'" /Fe:oracle.exe\nexit /b %errorlevel%\n')
tb='''library ieee;use ieee.std_logic_1164.all;use ieee.std_logic_textio.all;use std.textio.all;use std.env.all;
entity ar_clock_test is end;architecture sim of ar_clock_test is
signal clk:std_logic:='0';signal AS_P_clock_p0,AP_M_inhibit_local_clock_p0,KU_M_reset_ros_m_field:std_logic:='0';
signal P_clock_p2_forced_O_not_ros_ck,P_clock_p2_O_not_ros_ck:std_logic:='0';signal M_clock_p1_O_not_ros_ck:std_logic:='1';
begin
process(clk) begin if rising_edge(clk) then EQUATIONS end if;end process;
process file f:text open read_mode is "vectors.txt";variable l:line;variable values:integer_vector(0 to 5);variable n:natural:=0;
function bit_value(n:integer)return std_logic is begin if n=0 then return '0';else return '1';end if;end;
begin while not endfile(f) loop
readline(f,l);for i in 0 to 5 loop read(l,values(i));end loop;
AS_P_clock_p0<=bit_value(values(0));AP_M_inhibit_local_clock_p0<=bit_value(values(1));KU_M_reset_ros_m_field<=bit_value(values(2));
wait for 4 ns;clk<='1';wait for 1 ns;
assert P_clock_p2_forced_O_not_ros_ck=bit_value(values(3)) and M_clock_p1_O_not_ros_ck=bit_value(values(4)) and P_clock_p2_O_not_ros_ck=bit_value(values(5)) report "Local clock stage timing mismatch" severity failure;
clk<='0';wait for 5 ns;n:=n+1;
end loop;assert n=1024 severity failure;report "AR_LOCAL_CLOCK_PASS 1024 core edges";stop;wait;end process;end;
'''.replace('EQUATIONS','\n'.join(n+' <= '+e+';' for n,e in zip(names,expressions)))
(out/'ar_clock_test.vhd').write_text(tb)
def run(name,command):
    r=subprocess.run(list(map(str,command)),cwd=out,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r
run('build',['cmd','/c','build.cmd']); result=run('cpp',[out/'oracle.exe']);(out/'vectors.txt').write_text(result.stdout)
ghdl=ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
run('analyze',[ghdl,'-a','--std=08','ar_clock_test.vhd'])
run('vhdl',[ghdl,'-r','--std=08','ar_clock_test','--assert-level=error'])
record=dict(passed=True,core_edges=1024,clock_stages=3,cpp_second_pass_does_not_advance_clocks=True,
            scope='Actual AR C++ module and emitted VHDL clock expressions; normal, inhibited and forced-reset inputs; full CPU and routed timing excluded')
(out/'result.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
