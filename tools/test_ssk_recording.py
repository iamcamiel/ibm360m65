"""Exercise the actual SSK recorder with offsets, mark bits and wrong-key controls."""
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
    functions = '\n'.join(function_source('360_ros.cpp', name) for name in (
        'void record_65_set_key(int sec, int sea)',
        'void record_herc_set_key(int addr, char data)'))
    fixture = r'''#include <map>
#include <cstdio>
std::map<int,unsigned char> sk65,skh;
FUNCTIONS
int main() {
 unsigned blocks[]={0,0x800,0x74e000,0x767000,0x7ff800};
 unsigned marks[]={0,0xff000000,0xf0000000,0x0f000000};
 unsigned cases=0,fail=0;
 for(unsigned base:blocks) for(unsigned offset=0;offset<2048;++offset)
 for(unsigned key=0;key<32;++key) for(unsigned sequence=0;sequence<4;++sequence)
 for(unsigned mark:marks) {
  unsigned sea=mark|base|offset,sec=(sequence<<30)|(key<<25)|2;
  sk65.clear();skh.clear();record_65_set_key(sec,sea);record_herc_set_key(base,key<<3);
  ++cases;fail+=(sk65!=skh);
  skh.clear();record_herc_set_key(base^0x800,key<<3);fail+=(sk65==skh);
  skh.clear();record_herc_set_key(base,(key<<3)^8);fail+=(sk65==skh);
 }
 std::printf("SSK_RECORDER cases=%u failures=%u; different blocks/keys rejected\n",cases,fail);
 return fail!=0;
}
'''.replace('FUNCTIONS', functions)
    (out/'fixture.cpp').write_text(fixture)
    cmd = out/'build.cmd'
    cmd.write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 fixture.cpp /Fe:fixture.exe\nexit /b %errorlevel%\n')
    for label, command in [('build', ['cmd','/c',str(cmd)]),
                           ('test', [str(out/'fixture.exe')])]:
        result = subprocess.run(command,cwd=out,capture_output=True,text=True,timeout=90)
        (out/(label+'.log')).write_text(result.stdout+result.stderr)
        print(result.stdout[-1500:]+result.stderr[-1500:],end='')
        if result.returncode: raise SystemExit(result.returncode)
    (out/'result.json').write_text(json.dumps({'passed':True,'cases':5242880,
        'scope':'Actual recorder; 2 KiB offsets, key bits, sequence bits, marks; wrong block/key controls.'},indent=2)+'\n')


if __name__ == '__main__': main()
