"""Production AP equations at held capture/check boundaries, plus fault injection.

This isolates arithmetic, parity prediction and error latches. It does not prove
complete CPU boot or equivalence of physical clock phases.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from test_hercules_write_recording import VCVARS


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); gen=args.generated.resolve(); out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    (out/'360_struc.h').write_text('#pragma once\n#include "ald.h"\nextern _ALD oldstate,newstate;\n')
    groups=[(4,7),*[(i,i+7) for i in range(8,64,8)],(64,67)]
    tests=[]
    for lo,hi in groups:
        group=f'{lo:02}M{hi:02}'; width=hi-lo+1
        clamp='\n'.join(f'oldstate.AP.paddl.B{i}=(data>>{hi-i})&1;' for i in range(lo,hi+1))
        tests.append(f'''for(unsigned data=0;data<{1<<width};++data) for(unsigned p=0;p<2;++p) {{
 reset();
 for(int cycle=0;cycle<32;++cycle) {{
  {clamp}
  oldstate.AP.paddl_parity_{group}=p;
  oldstate.AP_INT.clock_p2_extended=1;
  oldstate.AP_INT._clock_p1_and_no_error=0;
  newstate=oldstate;process_AP();oldstate=newstate;
 }}
 unsigned want=!(parity(data)^p),got=!oldstate.AP._ind_full_sum_error_{group};
 checker_cases++;checker_fail+=(got!=want);
 if(got!=want && checker_fail<3) std::printf("CHECK_FAIL {group} data=%x parity=%u want=%u got=%u\\n",data,p,want,got);
}}''')
    predictors=[]
    aggregate_groups=['04M07','08M15','16M23','24M31','32M39','40M47','48M55','56M67']
    aggregate_inputs='\n'.join(f'oldstate.AP_INT._half_sum_error_{group}=(mask>>{i})&1;' for i,group in enumerate(aggregate_groups))
    aggregate=r'''for(unsigned mask=0;mask<256;++mask){
 reset();
 for(int cycle=0;cycle<16;++cycle){
  AGGREGATE_INPUTS
  oldstate.AP_INT.clock_p2=1;
  oldstate.AP_INT._temp_ap801_2m_bx_not=1;
  oldstate.AP_INT._left_shift=0;
  oldstate.AP_INT._clock_p1_and_no_error=0;
  newstate=oldstate;process_AP();oldstate=newstate;
 }
 unsigned want=parity(mask),got=oldstate.AP_INT.half_sum_error;
 aggregate_cases++;aggregate_fail+=(got!=want);
 aggregate_latch_fail+=(!oldstate.AP._inhibit_clock_padd_hs_error!=want);
 if(got!=want&&aggregate_fail<3)std::printf("AGGREGATE_FAIL mask=%02x want=%u got=%u\n",mask,want,got);
}'''.replace('AGGREGATE_INPUTS',aggregate_inputs)
    for lo in range(4,64,4):
        hi=lo+3;group=f'{lo:02}M{hi:02}'; cg=15-(lo-4)//4
        # Clamp the incoming group carry at the prediction boundary.
        predictors.append(f'''for(unsigned a=0;a<16;++a) for(unsigned b=0;b<16;++b) for(unsigned carry=0;carry<2;++carry) {{
 reset();
 for(int cycle=0;cycle<32;++cycle) {{
  oldstate.AP_INT.temp_padda.F=(unsigned long long)a<<{63-hi};
  oldstate.AP_INT.temp_paddb.F=(unsigned long long)b<<{63-hi};
  oldstate.AP_INT._carry_into_section.F=-1;
  oldstate.AP_INT.carry_into_group.B{cg}=carry;
  newstate=oldstate;process_AP();oldstate=newstate;
 }}
 unsigned want=parity((a+b+carry)&15),got=oldstate.AP_INT.bits_{group}_odd;
 predictor_cases++;predictor_fail+=(got!=want);
 if(got!=want && predictor_fail<3) std::printf("PREDICT_FAIL {group} a=%x b=%x carry=%u want=%u got=%u\\n",a,b,carry,want,got);
}}''')
    fixture=r'''#include "360_struc.h"
#include <cstdio>
#include <cstring>
_ALD oldstate,newstate;
unsigned parity(unsigned n) {unsigned p=0;for(;n;n>>=1)p^=n&1;return p;}
void reset(){std::memset(&oldstate,0,sizeof(oldstate));std::memset(&newstate,0,sizeof(newstate));init_AP();}
void phase(unsigned a,unsigned b,bool hold){
 for(int cycle=0;cycle<32;++cycle){
  oldstate.AP_INT.temp_padda.F=(unsigned long long)a<<24;
  oldstate.AP_INT.temp_paddb.F=(unsigned long long)b<<24;
  oldstate.AP_INT._carry_into_section.F=-1;
  oldstate.AP_INT._clock_p3_extended=!hold;
  oldstate.AP_INT._clock_p3_extendedOshift_r1=0;
  oldstate.AP_INT.clock_p2_extended=hold;
  oldstate.AP_INT.zero_shift_or_enable_scan=1;
  oldstate.AP_INT.left_4_shift=oldstate.AP_INT.right_4_shift=0;
  oldstate.AP_INT.enable_scan_bypass=0;
  oldstate.AP_INT._clock_p1_and_no_error=0;
  newstate=oldstate;process_AP();oldstate=newstate;
 }
}
int main(){
 unsigned arithmetic_cases=0,arithmetic_fail=0,odd_fail=0,valid_check_fail=0;
 unsigned checker_cases=0,checker_fail=0,predictor_cases=0,predictor_fail=0;
 unsigned aggregate_cases=0,aggregate_fail=0,aggregate_latch_fail=0;
 for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
  reset();phase(a,b,false);
  unsigned data=(oldstate.AP.paddl.F>>28)&255,p=oldstate.AP.paddl_parity_32M39;
  arithmetic_cases++;arithmetic_fail+=(data!=((a+b)&255));odd_fail+=((parity(data)^p)!=1);
  phase(a,b,true);valid_check_fail+=!oldstate.AP._ind_full_sum_error_32M39;
 }
 CHECKERS
 PREDICTORS
 AGGREGATE
 std::printf("AP_TEST arithmetic_cases=%u arithmetic_fail=%u odd_fail=%u valid_check_fail=%u checker_cases=%u checker_fail=%u predictor_cases=%u predictor_fail=%u\n",arithmetic_cases,arithmetic_fail,odd_fail,valid_check_fail,checker_cases,checker_fail,predictor_cases,predictor_fail);
 std::printf("AP_AGGREGATE cases=%u detector_fail=%u latch_fail=%u\n",aggregate_cases,aggregate_fail,aggregate_latch_fail);
 return arithmetic_fail||odd_fail||valid_check_fail||checker_fail||predictor_fail||aggregate_fail||aggregate_latch_fail;
}
'''.replace('CHECKERS','\n'.join(tests)).replace('PREDICTORS','\n'.join(predictors)).replace('\n AGGREGATE\n','\n'+aggregate+'\n')
    (out/'fixture.cpp').write_text(fixture)
    cmd=out/'build.cmd';cmd.write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{out}" /I"{gen}" fixture.cpp "{gen / "360_ap.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n')
    result=subprocess.run(['cmd','/c',str(cmd)],cwd=out,capture_output=True,text=True,timeout=90)
    (out/'build.log').write_text(result.stdout+result.stderr)
    if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
    result=subprocess.run([str(out/'fixture.exe')],cwd=out,capture_output=True,text=True,timeout=180)
    (out/'test.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='')
    (out/'result.json').write_text(json.dumps({'passed':result.returncode==0,
        'generated_cpp_sha256':hashlib.sha256((gen/'360_ap.cpp').read_bytes()).hexdigest(),
        'scope':__doc__,'result':result.stdout.strip()},indent=2)+'\n')
    raise SystemExit(result.returncode)


if __name__=='__main__':main()
