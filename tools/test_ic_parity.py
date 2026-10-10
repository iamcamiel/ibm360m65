"""Check production IC default initialization and odd-parity transfers.

Software initialization is tested separately from ALD load/hold behavior; this
does not establish an FPGA power-up preset or whole-CPU startup equivalence.
"""
import argparse, json, re, subprocess
from pathlib import Path
from test_hercules_write_recording import VCVARS, function_source


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); gen = a.generated.resolve(); out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out/'360_struc.h').write_text('#pragma once\n#include "ald.h"\nextern _ALD oldstate,newstate;\n')
    initializers=[]
    for path in sorted(gen.glob('360_*.cpp')):
        if path.name == '360_ca.cpp': continue
        initializers.append(re.search(r'void init_\w+\(\)\s*\{[\s\S]*?\n\}', path.read_text()).group(0))
    source=r'''#include "360_struc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
_ALD oldstate,newstate; bool mon_cycle,mon_ros;
void init_ros(){}
INITIALIZERS
FULL_INIT
unsigned odd(unsigned x){unsigned p=0;while(x){p^=x&1;x>>=1;}return p;}
int main(){
 _putenv_s("M65_INTERVAL_TIMER","0");full_init();
 unsigned bad=0, transfers=0, holds=0;
 for(auto *s : {&oldstate,&newstate}){
  bad+=s->CA.ic.F!=0;
  bad+=s->CA.ic_parity_00M07!=0 || s->CA.ic_parity_08M15!=0 || s->CA.ic_parity_16M23!=0;
 }
 for(unsigned byte=0;byte<256;++byte){
  unsigned word=(byte<<16)|(byte<<8)|byte,p=1^odd(byte);
  for(unsigned hold=0;hold<2;++hold){
   for(int pass=0;pass<8;++pass){
    oldstate.CA_INT.gt_pal_to_ic=!hold;oldstate.CA_INT._rst_ic_for_pal=hold;
    oldstate.CA_INT.set_ic_21M22Ppar=0;oldstate.CA_INT._rst_ic_21M22Ppar=1;
    oldstate.AP.paddl.F=(unsigned long long)(hold?~word:word)<<4;
    oldstate.AP.paddl_parity_40M47=oldstate.AP.paddl_parity_48M55=oldstate.AP.paddl_parity_56M63=hold?!p:p;
    newstate=oldstate;process_CA();oldstate=newstate;
   }
   bad+=oldstate.CA.ic.F!=word;
   bad+=oldstate.CA.ic_parity_00M07!=p || oldstate.CA.ic_parity_08M15!=p || oldstate.CA.ic_parity_16M23!=p;
   hold?++holds:++transfers;
  }
 }
 printf("IC_PARITY transfers=%u holds=%u bad=%u\n",transfers,holds,bad);
 return bad!=0;
}
'''.replace('INITIALIZERS', '\n'.join(initializers)).replace('FULL_INIT', function_source('360_ros.cpp','void full_init()'))
    source=source.replace('#include <cstring>','#include <cstring>\n#include <initializer_list>')
    (out/'fixture.cpp').write_text(source)
    (out/'build.cmd').write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{out}" /I"{gen}" fixture.cpp "{gen / "360_ca.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n')
    for name, cmd in [('build',['cmd','/c',str(out/'build.cmd')]),('run',[str(out/'fixture.exe')])]:
        r=subprocess.run(cmd,cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr)
        print(name,r.returncode,(r.stdout+r.stderr)[-1000:],flush=True);r.check_returncode()
    (out/'result.json').write_text(json.dumps({'passed':True,'explicit_ic_parity_seed':False,'default_latch_initialization_preserved':True,'transfers':256,'holds':256,'fpga_initialization_verified':False},indent=2))


if __name__=='__main__':main()
