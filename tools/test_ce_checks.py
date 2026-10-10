"""Check actual C++ initialization and the generated PK/KW CE switch path."""
import argparse
from pathlib import Path
import re
import subprocess
from test_hercules_write_recording import VCVARS, function_source


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();gen=args.generated.resolve();out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=True)
    # These unrelated pulse primitives are boundary stubs; this test observes
    # the contact/inverter path only, not SPECIAL timing or IPL sequencing.
    header='#pragma once\n#include "ald.h"\nextern _ALD oldstate,newstate;\n'
    for name in ('INT','TD','TD10NS','RLY_PT','SSN5500US','NSSN3US',
                 'NSSN2800NS','NSSN525NS','NSSN1000MS','NSSN2500MS'):
        header+=f'template<class... T> inline char {name}(char a,T...){{return a;}}\n'
    (out/'360_struc.h').write_text(header)
    source=r'''#include "360_struc.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
_ALD oldstate,newstate; bool mon_cycle,mon_ros;
void init_ros(){}
INIT_SECTION_FUNCTIONS
INIT_FUNCTION
void settle(){for(int i=0;i<24;++i){process_PK_PL();process_KW();oldstate=newstate;}}
int main(){
 _putenv_s("M65_INTERVAL_TIMER","0");full_init();oldstate=newstate;settle();
 bool enabled=newstate.EXTERNAL_.switches_7.B11 && !newstate.PK_PL.disable_check_key && newstate.KW_INT._disable_ce_check_switch;
 bool timer_disabled=!newstate.EXTERNAL_.switches_7.B13;
 oldstate.EXTERNAL_.switches_7.B11=newstate.EXTERNAL_.switches_7.B11=0;settle();
 bool control=newstate.PK_PL.disable_check_key && !newstate.KW_INT._disable_ce_check_switch;
 std::printf("CE_CHECKS initialized_enabled=%u decoded_enabled=%u timer_disabled=%u disable_control=%u\n",enabled,enabled,timer_disabled,control);
 return !(enabled&&timer_disabled&&control);
}
'''.replace('INIT_FUNCTION',function_source('360_ros.cpp','void full_init()'))
    initializers=[]
    for path in sorted(gen.glob('360_*.cpp')):
        if path.name in ('360_pk.cpp','360_kw.cpp'):continue
        match=re.search(r'void init_\w+\(\)\s*\{[\s\S]*?\n\}',path.read_text())
        if match:initializers.append(match[0])
    source=source.replace('INIT_SECTION_FUNCTIONS','\n'.join(initializers))
    (out/'fixture.cpp').write_text(source)
    cmd=out/'build.cmd';cmd.write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{out}" /I"{gen}" fixture.cpp "{gen / "360_pk.cpp"}" "{gen / "360_kw.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n')
    for label,command in [('build',['cmd','/c',str(cmd)]),('test',[str(out/'fixture.exe')])]:
        result=subprocess.run(command,cwd=out,capture_output=True,text=True,timeout=60)
        (out/(label+'.log')).write_text(result.stdout+result.stderr)
        print(result.stdout[-1800:]+result.stderr[-1800:],end='')
        if result.returncode:raise SystemExit(result.returncode)


if __name__=='__main__':main()
