"""Check the production CE-caused logout-branch guard and its cycle boundary."""
import argparse
import json
from pathlib import Path
import subprocess
from test_hercules_write_recording import ROOT, VCVARS, function_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    out = parser.parse_args().output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = r'''
#include "360_struc.h"
#include <cstdio>
extern "C" {
DATA360 oldstate{}, newstate{};
FILE* lf; double runtime=0; bool diagnostic_fault=false;
int passes=0; bool transfer=false;
void fixture_settle(){++passes;if(transfer)newstate.RX.rosar.F=0x019;}
void fixture_clock(){}
void cycle_mon(){}
FUNCTIONS
}
int cases=0,bad=0;
void check(bool value){++cases;if(!value)++bad;}
void reset(){
 oldstate={};newstate={};m65_ce_ros_branch_taken(true);
 oldstate.KW._error_log_required=1;
 oldstate.KU_INT._pulsed_split_log_to_soros_set=1;
 oldstate.KU_INT._soros_tgr=1;
 oldstate.RX.rosar.F=0x345;newstate=oldstate;
}
void accept(){
 oldstate.KW._error_log_required=0;oldstate.KU_INT.temp704=1;
 oldstate.KU_INT._clock_p0M3=oldstate.KU_INT._clock_p1=1;
 newstate=oldstate;newstate.KU_INT._soros_tgr=0;
 check(!m65_ce_ros_branch_taken(false));oldstate=newstate;
}
bool branch(bool gate=true,bool phase=true,bool forced=true){
 oldstate.RX.gate_rosar_scan_or_mc=gate;oldstate.RX_INT.p4_gate=phase;
 oldstate.KU_INT._temp_ku511_3d_ag=!forced;
 newstate=oldstate;newstate.RX.rosar.F=0x019;
 return m65_ce_ros_branch_taken(false);
}
int main(){
 lf=fopen("m65.log","w");
 reset();newstate.KW_INT._check_reg_1_error=0;check(!branch());
 reset();oldstate.KW._error_log_required=0;check(!branch());
 reset();accept();check(branch());check(!branch());
 reset();oldstate.KU_INT.console_log_out_latch=1;
 oldstate.KU_INT.logout_pb_gated=oldstate.KU_INT.short_ss_pulse=1;
 accept();check(!branch());
 reset();oldstate.KU_INT._pulsed_split_log_to_soros_set=0;accept();check(!branch());
 reset();oldstate.KU_INT.temp705=oldstate.KU_INT.clock_p0=1;accept();check(!branch());
 reset();accept();newstate=oldstate;newstate.KW_INT.por_ss=1;
 check(!m65_ce_ros_branch_taken(false));oldstate=newstate;check(!branch());
 reset();accept();newstate=oldstate;newstate.KU_INT._soros_tgr=1;
 check(!m65_ce_ros_branch_taken(false));oldstate=newstate;check(!branch());
 reset();accept();check(!branch(false));check(branch());
 reset();accept();check(!branch(true,false));check(branch());
 reset();accept();check(!branch(true,true,false));check(branch());
 reset();accept();oldstate.RX.rosar.F=0x019;check(!branch());
 reset();oldstate.KU_INT.temp704=0;
 oldstate.KW._error_log_required=0;
 oldstate.KU_INT._clock_p0M3=oldstate.KU_INT._clock_p1=1;
 newstate=oldstate;newstate.KU_INT._soros_tgr=0;
 check(!m65_ce_ros_branch_taken(false));oldstate=newstate;check(!branch());
 // Exercise the actual production single_cycle: capture after BOTH passes.
 reset();accept();oldstate.RX.gate_rosar_scan_or_mc=1;
 oldstate.RX_INT.p4_gate=1;oldstate.KU_INT._temp_ku511_3d_ag=0;
 newstate=oldstate;transfer=true;passes=0;single_cycle();
 check(m65_fault_pending());check(passes==2);
 double held=runtime;single_cycle();check(passes==2&&runtime==held);
 fclose(lf);printf("CE_ROS_BRANCH cases=%d bad=%d\n",cases,bad);return bad!=0;
}
'''
    markers = ['bool m65_fault_pending()', 'void m65_latch_fault(',
               'bool m65_ce_ros_branch_taken(bool reset) {', 'void single_cycle()']
    body = '\n'.join(function_source('360_ros.cpp', marker) for marker in markers)
    body = body.replace('process_ald();', 'fixture_settle();').replace('process_ald_clock();', 'fixture_clock();')
    (out / 'fixture.cpp').write_text(source.replace('FUNCTIONS', body))
    (out / 'build.cmd').write_text(
        f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /DCOMPARE_M65 '
        f'/I"{ROOT / "hercules"}" /I"{ROOT / "gen/ald"}" fixture.cpp /Fe:fixture.exe\nexit /b %errorlevel%\n')
    for name, command in [('build', ['cmd', '/c', str(out / 'build.cmd')]),
                          ('run', [str(out / 'fixture.exe')])]:
        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        print(name, result.returncode, (result.stdout + result.stderr)[-1500:])
        result.check_returncode()
    state = json.loads((out / 'first-fault-state.json').read_text())
    assert state['reason'] == 'ce-ros-branch' and state['rosar'] == 0x019
    assert 'M65CEBRANCH request_rosar=345 from=345 to=019' in (out / 'm65.log').read_text()
    (out / 'result.json').write_text(json.dumps({'passed': True,
        'production_guard_tested': True, 'captured_after_two_passes': True,
        'sticky_stop_verified': True, 'negative_controls':
        ['indicator only', 'request without acceptance', 'manual logout', 'split logout',
         'cycle counter', 'power-on reset', 'logout reset', 'missing force/gate/phase',
         'unrelated or unchanged ROS019', 'unselected SOROS input']}, indent=2))


if __name__ == '__main__':
    main()
