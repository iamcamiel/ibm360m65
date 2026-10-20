"""Check emitted AP635 left-four parity against independent byte odd parity.

AL B09 receives positive nibble parity and inverted bus parity (D04).
AM B03 receives negative nibble parity and positive bus parity (D05).
The ALD scan is the wiring reference; integer bit counts are the truth oracle.
"""
from pathlib import Path
import argparse, itertools, json, re, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--generated', type=Path, default=ROOT/'gen/ald')
args = p.parse_args()
G = args.generated.resolve(); OUT = args.output.resolve(); OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT/'tools'))
from audit_ald_logic import normalize_cpp

refpat = r'oldstate\.[\w]+\.[\w]+(?:\.B\d+)?'
target = 'AP_INT._temp94'
expr = re.search(r'newstate\.'+re.escape(target)+r' = (.+);', (G/'360_ap.cpp').read_text())[1]
name = normalize_cpp('oldstate.'+target, 'AP')
vtext = (G/'360_ap.vhd').read_text()
ve = [re.findall(r'\b'+re.escape(n)+r' <= (.+?);', vtext, re.S)[-1].split(' when ')[0].strip() for n in [name+'_first', name]]
refs = sorted(set(re.findall(refpat, expr)))
tests = []
def vector(lo,hi,p3=1,p2=0,previous=0,inhibit=0):
    d = dict.fromkeys(refs,0)
    odd_lo = 1^(lo.bit_count()%2); odd_hi = 1^(hi.bit_count()%2)
    d.update({'oldstate.AP_INT.left_4_shift':1,
              'oldstate.AP_INT._clock_p3_extended':p3,
              'oldstate.AP_INT.clock_p2_extended':p2,
              'oldstate.AP.paddl_parity_56M63':previous,
              'oldstate.AP_INT._clock_p3_extendedOshift_r1':inhibit,
              'oldstate.AP_INT.bits_60M63_odd':odd_lo,
              'oldstate.AP_INT._bits_60M63_odd':1-odd_lo,
              'oldstate.AP_INT.bus_parity_64M67':odd_hi,
              'oldstate.AP_INT._temp95':1-odd_hi})
    assert set(d)==set(refs), set(d)-set(refs)
    odd_output = 1^(((lo<<4)|hi).bit_count()%2)
    expected = int(not (inhibit or (p2 and previous) or (p3 and not odd_output)))
    return d,expected
for lo,hi in itertools.product(range(16),repeat=2):tests.append(vector(lo,hi))
for p3,p2,previous,inhibit in itertools.product((0,1),repeat=4):
    for lo,hi in itertools.product((0,1),repeat=2):tests.append(vector(lo,hi,p3,p2,previous,inhibit))

ids = {r:'s'+str(i) for i,r in enumerate(refs)}
cpp = ['#include <stdio.h>','int main(){',*(f'bool {s}=0;' for s in ids.values())]
vhdl = ['library ieee;use ieee.std_logic_1164.all;use std.env.all;',
        'entity ap635_parity is end;architecture sim of ap635_parity is begin process',
        *(f'variable {s}:std_logic;' for s in ids.values()),'begin']
for i,(values,expected) in enumerate(tests):
    cpp.extend(f'{ids[r]}={v};' for r,v in values.items())
    vhdl.extend(f"{ids[r]}:='{v}';" for r,v in values.items())
    ce = re.sub(refpat,lambda m:ids[m[0]],expr)
    cpp.append(f'if(({ce})!={expected}){{printf("case {i} failed\\n");return 1;}}')
    names = {normalize_cpp(r,'AP').replace('[','(').replace(']',')'):ids[r] for r in values}
    pattern = '|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))
    for passno,vexpr in enumerate(ve):
        vexpr = re.sub(r'(?<![\w])(?:'+pattern+r')(?![\w])',lambda m:names[m[0]],vexpr.replace('_first',''))
        vhdl.append(f'assert ({vexpr})=\'{expected}\' report "case {i} pass {passno}" severity failure;')
cpp += [f'printf("AP635_PARITY_PASS {len(tests)} cases\\n");return 0;}}']
vhdl += [f'report "AP635_PARITY_PASS {len(tests)} cases";stop;wait;end process;end;']
(OUT/'ap635_parity.cpp').write_text('\n'.join(cpp))
(OUT/'ap635_parity.vhd').write_text('\n'.join(vhdl))
(OUT/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj ap635_parity.cpp /Fe:ap635_parity.exe\nexit /b %errorlevel%\n')
ghdl = ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,command in [('build',['cmd','/c','build.cmd']),('cpp',[str(OUT/'ap635_parity.exe')]),
                     ('analyze',[str(ghdl),'-a','--std=08','ap635_parity.vhd']),
                     ('vhdl',[str(ghdl),'-r','--std=08','ap635_parity','--assert-level=error'])]:
    r = subprocess.run(command,cwd=OUT,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
    print(name,r.returncode,(r.stdout+r.stderr)[-500:],flush=True);r.check_returncode()
(OUT/'result.json').write_text(json.dumps(dict(passed=True,total=len(tests),nibble_pairs=256,
    clock_hold_inhibit_cases=64,vhdl_passes=2,
    scope='Actual AP635 emitted gate; independent integer byte odd parity; clocks unchanged'),indent=2)+'\n')
