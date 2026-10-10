"""Exercise the production cycle guard and first-fault capture with injected faults."""
from pathlib import Path
import argparse
import subprocess
from test_hercules_write_recording import VCVARS, function_source, ROOT

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
OUT = parser.parse_args().output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
source = r'''
#include "360_struc.h"
#include <cstdio>
extern "C" {
DATA360 oldstate{},newstate{};
FILE* lf; double runtime=0; bool diagnostic_fault=false;
int passes=0; bool inject=false;
void fixture_settle(){++passes;if(inject)newstate.KW_INT._check_reg_1_error=0;}
void fixture_clock(){}
void cycle_mon(){}
GUARD_FUNCTIONS
}
int main(int argc,char**argv){
 lf=fopen("m65.log","w");
 newstate.KW_INT._check_reg_1_error=newstate.KW_INT._check_reg_2_error=1;
 single_cycle();if(m65_fault_pending()||passes!=2)return 1;
 inject=true;single_cycle();
 if(m65_fault_pending()||passes!=4)return 2;
 if(argc==1){
  single_cycle();if(m65_fault_pending()||passes!=6)return 3;
  fclose(lf);printf("CE indicator observed; cycles continue\n");return 0;
 }
 m65_latch_fault("comparison-test");
 if(!m65_fault_pending())return 2;
 int held=passes;double held_time=runtime;
 single_cycle();m65_latch_fault("must-not-overwrite");
 if(passes!=held||runtime!=held_time)return 3;
 fclose(lf);printf("first-fault captured and subsequent cycles held\n");return 0;
}
'''
source=source.replace('GUARD_FUNCTIONS','\n'.join(function_source('360_ros.cpp',marker) for marker in
 ['bool m65_fault_pending()', 'void m65_latch_fault(', 'bool m65_ce_ros_branch_taken(bool reset) {', 'void single_cycle()'])
 .replace('process_ald();','fixture_settle();').replace('process_ald_clock();','fixture_clock();'))
(OUT/'fixture.cpp').write_text(source)
(OUT/'build.cmd').write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /DCOMPARE_M65 /I"{ROOT / "hercules"}" /I"{ROOT / "gen/ald"}" fixture.cpp /Fe:fixture.exe\nexit /b %errorlevel%\n')
result=subprocess.run(['cmd','/c',str(OUT/'build.cmd')],cwd=OUT,capture_output=True,text=True)
(OUT/'build.log').write_text(result.stdout+result.stderr)
print(result.stdout[-1600:]);result.check_returncode()
for mode in ('ce','comparison'):
 folder=OUT/mode;folder.mkdir(exist_ok=True)
 result=subprocess.run([str(OUT/'fixture.exe')]+([] if mode=='ce' else ['compare']),cwd=folder,capture_output=True,text=True)
 print(mode,result.stdout);result.check_returncode()
 import json
 if mode=='ce':
  assert not (folder/'first-fault-state.json').exists()
 else:
  state=json.loads((folder/'first-fault-state.json').read_text())
  assert state['reason']=='comparison-test'
 assert 'must-not-overwrite' not in (folder/'m65.log').read_text()
