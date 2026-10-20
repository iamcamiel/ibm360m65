"""Run generated two-pass ALD VHDL against generated C++ snapshot evaluation.

Use: python tools/test_ald_settle.py --compiler PATH --ghdl PATH --output DIR
Requires the Visual C++ command environment; preserves the production model.
"""
import argparse, subprocess, json
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--ghdl',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[1]
    fixture=repo/'tools/ald_compiler/tests'
    root=args.output.resolve(); root.mkdir(parents=True,exist_ok=True)
    generated=root/'generated'; generated.mkdir(exist_ok=True)
    logs={}
    def run(name,command,cwd=root):
        result=subprocess.run([str(a) for a in command],cwd=cwd,capture_output=True,text=True)
        logs[name]=result.stdout+result.stderr
        (root/(name+'.log')).write_text(logs[name])
        assert result.returncode==0, logs[name][-3000:]
        return result
    run('generation',[args.compiler.resolve(),'-O1',fixture,generated])
    (generated/'360_struc.h').write_text('#pragma once\n#include "ald.h"\nextern _ALD oldstate, newstate;\n')
    build=root/'build-oracle.cmd'
    files=[fixture/'settle_oracle.cpp',*sorted(generated.glob('*.cpp'))]
    build.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
        +'cl /nologo /std:c++17 /EHsc /O2 /I"'+str(generated)+'" /Fe:"'+str(root/'oracle.exe')+'" '
        +' '.join('"'+str(p)+'"' for p in files)+'\nexit /b %errorlevel%\n')
    run('oracle-build',['cmd','/c',build])
    oracle=run('oracle',[root/'oracle.exe'])
    (root/'settle-vectors.txt').write_text(oracle.stdout)
    run('analyze',[args.ghdl.resolve(),'-a','--std=08',*sorted(generated.glob('settle_*.vhd')),generated/'ald.vhd',fixture/'test_ald_settle.vhd'])
    result=run('simulation',[args.ghdl.resolve(),'-r','--std=08','test_ald_settle','--assert-level=error'])
    assert 'ALD_SETTLE_TEST_PASS' in result.stdout
    summary={'passed':True,'reference':'generated C++ process_ald/process_ald_clock/copy/process_ald',
             'cycles':1024,'one_pass_divergences':int(oracle.stderr.strip().split('=')[1])}
    (root/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary))

if __name__=='__main__': main()
