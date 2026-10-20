"""Compile all 48 native sections and check SRDA CC at the production exit.

Windows/MSVC Win32, matching the comparison executable. Every case runs in a
fresh process with private memory. --negative expects the R139 zero-result bug.
No guest, comparison run or hardware is controlled by this test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

R = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--negative', action='store_true')
    a = p.parse_args()
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    generated = a.generated.resolve()
    sources = sorted(generated.glob('360_*.cpp'))
    assert len(sources) == 48
    for f in [*sources, *generated.glob('*.h')]: shutil.copy2(f, out/f.name)
    for name in ['360_struc.h', '360_minstruc.h', '360_ros.cpp', 'm65_fault_signals.inc']:
        shutil.copy2(R/'hercules'/name, out/name)
    shutil.copy2(R/'tools/test_srda_boundary.cpp', out/'probe.cpp')
    cpu = (R/'hercules/cpu.c').read_text()
    begin = cpu.index('        if (((newstate.RX.rosar.F == 0x02e)')
    end = cpu.index('        if (newstate.RX.rosar.F == 0x180)', begin)
    predicate = cpu[begin:end]
    # Omit logging only; retain the production guards and exact return boundary.
    predicate = '\n'.join(line for line in predicate.splitlines() if 'D_fprintf' not in line)
    assert predicate.count('return ic;') == 1
    (out/'exit.inc').write_text(predicate.replace('return ic;', 'return true;').replace('ic_set = false;', ''))
    files = ['probe.cpp', '360_ros.cpp', *[f.name for f in sources]]
    (out/'compile.rsp').write_text('/nologo /std:c++17 /EHsc /O2 /DCOMPARE_M65 /I. '
        + ' '.join(files) + ' /Fe:probe.exe\n')
    (out/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars32.bat" >nul\ncl @compile.rsp\nexit /b %errorlevel%\n')
    def run(name, cmd, cwd):
        result = subprocess.run(list(map(str, cmd)), cwd=cwd, capture_output=True, text=True)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        result.check_returncode()
        return result
    run('build', ['cmd', '/c', 'build.cmd'], out)
    cases = [('r46',32,0,0x7816e8,1,0)] if a.negative else [
        ('r46',32,0,0x7816e8,1,0), ('zero32',32,0,0,0,3),
        ('out31',31,0,0x7816e8,0,2), ('out63',63,0,1,0,1),
        ('pos0',0,0,0x7816e8,0,0), ('pos32',32,1,0,0,3),
        ('neg32',32,0xffffffff,0x7816e8,0,2),
        *[(f'r46-cc{cc}',32,0,0x7816e8,1,cc) for cc in (1,2,3)]]
    records = {}
    for name, count, hi, lo, context, cc in cases:
        d = out/name; d.mkdir(exist_ok=True); shutil.copy2(R/'src/ros.txt', d/'ros.txt')
        result = run(name,[out/'probe.exe',count,f'{hi:x}',f'{lo:x}',context,cc],d)
        got = json.loads(result.stdout.strip().splitlines()[-1])
        signed = (hi<<32)|lo
        if hi & 0x80000000: signed -= 1<<64
        expected = (signed>>count)&((1<<64)-1)
        expected_cc = 0 if expected==0 else 1 if expected>>63 else 2
        valid = (got['exit_count']==1 and got['exit_data']==f'{expected:016x}'
            and got['stored']==f'{expected:016x}' and got['checks']==[1]*4
            and got['writes']==1 and got['reads']>0 and got['wait'] and not got['fault'])
        valid &= got['exit_cc']==(2 if a.negative else expected_cc)
        records[name] = dict(passed=bool(valid), expected_cc=expected_cc, **got)
        (out/'cases.json').write_text(json.dumps(records,indent=2))
        assert valid, records[name]
        print(name, 'negative control reproduced' if a.negative else 'PASS', flush=True)
    files = [*out.glob('*.cpp'),*out.glob('*.h'),out/'exit.inc']
    summary = dict(passed=True,negative_control=a.negative,sections=48,cases=records,
        hashes={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files},
        scope='Actual native 48-section CPU and production instruction-exit predicate; private storage, no exact historical MVT replay or hardware timing claim')
    (out/'result.json').write_text(json.dumps(summary,indent=2)+'\n')

if __name__ == '__main__': main()
