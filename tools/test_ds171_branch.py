"""Exercise DS171 -> DS111 -> RX111 using the production generated equations."""
import argparse,json,subprocess
from pathlib import Path
from test_hercules_write_recording import ROOT,VCVARS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();g=args.generated.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    cpp=r'''
#include "360_struc.h"
#include <cstdio>
extern "C" { DATA360 oldstate{},newstate{}; FILE* lf; double runtime=0; }
int main(){
 unsigned cases=0,bad=0;
 for(int j=0;j<128;++j)for(int inputs=0;inputs<16;++inputs){
  oldstate={};newstate={};init_DS();init_RX();
  oldstate.DS.roslth_62M68_eq_j.F=0;
  // BIT_0_127 spans two words; assign through its declared individual bits.
  if(j==40)oldstate.DS.roslth_62M68_eq_j.B40=1;
  if(j==41)oldstate.DS.roslth_62M68_eq_j.B41=1;
  if(j==42)oldstate.DS.roslth_62M68_eq_j.B42=1;
  if(j==43)oldstate.DS.roslth_62M68_eq_j.B43=1;
  if(j==44)oldstate.DS.roslth_62M68_eq_j.B44=1;
  oldstate.RS_RT.st_bus.B63=inputs&1;
  oldstate.JA.write_direct_ioce_operation=(inputs>>1)&1;
  oldstate.KM.any_ioce_mck=(inputs>>2)&1;
  oldstate.KW.register_set_pb_gated=(inputs>>3)&1;
  const bool expected=(j==40&&(inputs&1))||(j==41&&(inputs&2))||
   (j==42&&(inputs&4))||j==43||(j==44&&(inputs&8));
  process_DS_clock();++cases;if(newstate.DS_INT._set_rosar_11!=!expected){if(bad<5)printf("DS171 j=%d inputs=%d got=%d expected=%d\n",j,inputs,newstate.DS_INT._set_rosar_11,!expected);++bad;}
  // Follow the actual DS171 wire through the declared CLOCK staging.
  oldstate.DS_INT._set_rosar_11=newstate.DS_INT._set_rosar_11;
  process_DS_clock();++cases;if(bool(newstate.DS.rosar_11_fast_input_11)!=expected){if(bad<5)printf("DS111 j=%d inputs=%d got=%d expected=%d\n",j,inputs,newstate.DS.rosar_11_fast_input_11,expected);++bad;}
  for(int gate=0;gate<2;++gate){
   oldstate.DS.rosar_11_fast_input_11=newstate.DS.rosar_11_fast_input_11;
   oldstate.RX_INT.p4_gate=gate;oldstate.RX_INT._inhibit_next_address=1;
   process_RX();++cases;if(newstate.RX._rosar.B11!=!(expected&&gate)){if(bad<5)printf("RX111 j=%d inputs=%d gate=%d got=%d expected=%d\n",j,inputs,gate,newstate.RX._rosar.B11,!(expected&&gate));++bad;}
  }
 }
 printf("DS171_BRANCH cases=%u bad=%u\n",cases,bad);return bad!=0;
}
'''
    (out/'fixture.cpp').write_text(cpp)
    cmd=f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{ROOT/"hercules"}" /I"{g}" fixture.cpp "{g/"360_ds.cpp"}" "{g/"360_rx.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n'
    (out/'build.cmd').write_text(cmd)
    for name,command in [('build',['cmd','/c',str(out/'build.cmd')]),('run',[str(out/'fixture.exe')])]:
        r=subprocess.run(command,cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr)
        print(name,r.returncode,(r.stdout+r.stderr)[-1200:]);r.check_returncode()
    (out/'result.json').write_text(json.dumps({'passed':True,'cases':8192,'scope':'Generated DS171/DS111 wire and RX111 phase gate; no full logout or routed timing proof.'},indent=2))

if __name__=='__main__':main()
