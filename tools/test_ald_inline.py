"""Exercise explicit wire cones in actual generated C++ and two-pass VHDL."""
import argparse
import json
from pathlib import Path
import subprocess

FIXTURE = '''SECTION CONE
INLINE
+blocked
  OR
    +reset(*)
    +phase
-reset
  NOT
    +blocked
+positive
  NOT
    -negative
NOCLOCK
-negative(*)
  NOR
    +set(*)
    AND
      -reset
      +positive
+raw(*)
  AND
    -reset
    +gate(*)
CLOCK
+phase
  NOT
    +phase
+sampled(*)
  AND
    -reset
    +gate(*)
'''
CPP = r'''#include "360_struc.h"
#include <cstdio>
_ALD oldstate{},newstate{};
int main(){
 init_ald(); bool n=true; unsigned rng=0x7816e8;
 for(int i=0;i<512;++i){
  rng=rng*1664525u+1013904223u;
  bool reset=(rng>>30)&1,set=(rng>>29)&1,gate=(rng>>28)&1;
  bool rst=i%97==0,halt=i%23<3;
  newstate.EXTERNAL_.reset=reset; newstate.EXTERNAL_.set=set;
  newstate.EXTERNAL_.gate=gate;
  if(rst){init_ald();n=true;}
  else if(!halt){
   oldstate=newstate;
   bool first=!(set||(!(reset||oldstate.CONE_INT.phase)&&!n));
   bool sampled=oldstate.CONE_INT._reset&&gate;
   process_ald();
   if(newstate.CONE._negative!=first) return 2;
   process_ald_clock();
   if(newstate.CONE.sampled!=sampled) return 3;
   n=!(set||(!(reset||newstate.CONE_INT.phase)&&!first));
   oldstate=newstate; process_ald();
   if(newstate.CONE._negative!=n || newstate.CONE.raw!=(!(reset||newstate.CONE_INT.phase)&&gate)) return 4;
  }
  printf("%d %d %d %d %d %d %d %d\n",rst,halt,reset,set,gate,
      newstate.CONE._negative,newstate.CONE.raw,newstate.CONE.sampled);
 }
}
'''
VHDL = '''library ieee;use ieee.std_logic_1164.all;use std.textio.all;
entity test_inline is end;
architecture test of test_inline is
signal clk,rst,hlt,r,s,g,n,raw,sampled:std_logic:='0';
signal done:boolean:=false;
begin
clk<=not clk after 5 ns when not done else '0';
dut:entity work.ald port map(clk=>clk,rst=>rst,hlt=>hlt,P_reset=>r,P_set=>s,
P_gate=>g,M_negative=>n,P_raw=>raw,P_sampled=>sampled);
process
file vectors:text open read_mode is "vectors.txt";
variable row:line;variable rv,hv,riv,siv,giv,nv,rawv,sv:integer;
variable count:integer:=0;
procedure check is begin
assert n=std_logic'val(nv+2) and raw=std_logic'val(rawv+2)
and sampled=std_logic'val(sv+2) report "INLINE cycle " & integer'image(count) severity failure;
end;
begin
while not endfile(vectors) loop
readline(vectors,row);read(row,rv);read(row,hv);read(row,riv);read(row,siv);
read(row,giv);read(row,nv);read(row,rawv);read(row,sv);
wait until falling_edge(clk);
rst<=std_logic'val(rv+2);hlt<=std_logic'val(hv+2);r<=std_logic'val(riv+2);
s<=std_logic'val(siv+2);g<=std_logic'val(giv+2);
wait until rising_edge(clk);wait for 1 ns;check;
r<=not r;s<=not s;g<=not g;wait for 2 ns;check;
count:=count+1;
end loop;
report "INLINE_PASS 512 cycles, nested cones, feedback, CLOCK transition, halt/reset, sampled inputs";
done<=true;wait;end process;end;
'''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',type=Path,required=True)
    p.add_argument('--ghdl',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    src=out/'source';src.mkdir(exist_ok=True)
    gen=out/'generated';gen.mkdir(exist_ok=True)
    compiler=a.compiler.resolve();ghdl=a.ghdl.resolve()
    def run(name,cmd,expected=0):
        r=subprocess.run(list(map(str,cmd)),cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr)
        assert (r.returncode==0)==(expected==0),r.stdout[-3000:]+r.stderr[-1000:]
        return r
    (src/'cone.ald').write_text(FIXTURE)
    run('generate',[compiler,'-O1',src,gen])
    (gen/'360_struc.h').write_text('#pragma once\n#include "ald.h"\nextern _ALD oldstate,newstate;\n')
    (out/'oracle.cpp').write_text(CPP);(out/'test.vhd').write_text(VHDL)
    (out/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /std:c++17 /EHsc /O2 /Igenerated oracle.cpp generated/*.cpp /Fe:oracle.exe\nexit /b %errorlevel%\n')
    run('build',['cmd','/c','build.cmd'])
    oracle=run('oracle',[out/'oracle.exe']);(out/'vectors.txt').write_text(oracle.stdout)
    run('analyze',[ghdl,'-a','--std=08',gen/'cone.vhd',gen/'ald.vhd',out/'test.vhd'])
    run('vhdl',[ghdl,'-r','--std=08','test_inline','--assert-level=error'])
    # Reject recursion even if the invalid cone is never used by a consumer.
    for name,body,message in [
        ('recursive','SECTION CYCLE\nINLINE\n+a\n  NOT\n    +b\n+b\n  NOT\n    +a\n','Recursive INLINE cone'),
        ('vector','SECTION CYCLE\nINLINE\n+a[0..1]\n  XOR\n    +b[0..1](*)\n    +c[0..1](*)\n','local scalar Boolean'),
        ('external','SECTION CYCLE\nINLINE\n+a(*)\n  NOT\n    +b(*)\n','local scalar Boolean'),
        ('special','SECTION CYCLE\nINLINE\n+a\n  SS200NS\n    +b(*)\n','pure Boolean')]:
        d=out/name;d.mkdir(exist_ok=True);(d/'cycle.ald').write_text(body)
        target=d/'generated';target.mkdir(exist_ok=True)
        r=run(name,[compiler,'-O1',d,target],expected=1)
        assert message in r.stdout,r.stdout
    (out/'result.json').write_text(json.dumps(dict(passed=True,cycles=512,
        scope='Actual generated C++ and VHDL wire-cone feedback, both passes, CLOCK snapshot, halt/reset and four rejected invalid definitions'),indent=2))
    print('INLINE_PASS')

if __name__=='__main__':main()
