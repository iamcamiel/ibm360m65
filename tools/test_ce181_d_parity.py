"""CE181 D-source parity: emitted C++ and both VHDL passes against independent scan gates and byte even parity."""
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
ce_source = (G/'360_ce.cpp').read_text()
target = re.search(r'newstate\.CE\.incr_bits_8M15_even = !oldstate\.([\w.]+);', ce_source)[1]
expr = re.search(r'newstate\.'+re.escape(target)+r' = (.+);', (G/'360_ce.cpp').read_text())[1]
name = normalize_cpp('oldstate.'+target, 'CE')
vtext = (G/'360_ce.vhd').read_text()
ve = [re.findall(r'\b'+re.escape(n)+r' <= (.+?);', vtext, re.S)[-1].split(' when ')[0].strip() for n in [name+'_first', name]]
refs = sorted(set(re.findall(refpat, expr)))
tests = []
def vector(upper,lower,p3=1,hold=0,previous=0):
 d=dict.fromkeys(refs,0)
 e=1^(upper.bit_count()%2);odd=lower.bit_count()%2
 d.update({'oldstate.RE.e_reg_bits_8M11_even':e,'oldstate.RD.d_reg_bits_18M21_odd':odd,'oldstate.RE._clock_p3':p3,'oldstate.CE_INT.gate_d18M21_to_incr12M15':1,'oldstate.RE.clock_p2_incr_reset':hold,'oldstate.CE.incr_bits_8M15_even':previous})
 assert set(d)==set(refs)
 aq=1-odd;ak=not(e and aq);an=e and ak;ap=aq and ak;ar=not(an or ap)
 expected_even=1^(((upper<<4)|lower).bit_count()%2)
 assert ar==expected_even
 return d,int(not ((p3 and expected_even) or (hold and previous)))
for upper,lower in itertools.product(range(16),repeat=2):tests.append(vector(upper,lower))
for p3,hold,previous in itertools.product((0,1),repeat=3):
 for upper,lower in itertools.product((0,1),repeat=2):tests.append(vector(upper,lower,p3,hold,previous))

ids = {r:'s'+str(i) for i,r in enumerate(refs)}
cpp = ['#include <stdio.h>','int main(){',*(f'bool {s}=0;' for s in ids.values())]
vhdl = ['library ieee;use ieee.std_logic_1164.all;use std.env.all;',
        'entity ce181_d_parity is end;architecture sim of ce181_d_parity is begin process',
        *(f'variable {s}:std_logic;' for s in ids.values()),'begin']
for i,(values,expected) in enumerate(tests):
    cpp.extend(f'{ids[r]}={v};' for r,v in values.items())
    vhdl.extend(f"{ids[r]}:='{v}';" for r,v in values.items())
    ce = re.sub(refpat,lambda m:ids[m[0]],expr)
    cpp.append(f'if(({ce})!={expected}){{printf("case {i} failed\\n");return 1;}}')
    names = {normalize_cpp(r,'CE').replace('[','(').replace(']',')'):ids[r] for r in values}
    pattern = '|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))
    for passno,vexpr in enumerate(ve):
        vexpr = re.sub(r'(?<![\w])(?:'+pattern+r')(?![\w])',lambda m:names[m[0]],vexpr.replace('_first',''))
        vhdl.append(f'assert ({vexpr})=\'{expected}\' report "case {i} pass {passno}" severity failure;')
cpp += [f'printf("CE181_D_PARITY_PASS {len(tests)} cases\\n");return 0;}}']
vhdl += [f'report "CE181_D_PARITY_PASS {len(tests)} cases";stop;wait;end process;end;']
(OUT/'ce181_d_parity.cpp').write_text('\n'.join(cpp))
(OUT/'ce181_d_parity.vhd').write_text('\n'.join(vhdl))
(OUT/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj ce181_d_parity.cpp /Fe:ce181_d_parity.exe\nexit /b %errorlevel%\n')
ghdl = ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,command in [('build',['cmd','/c','build.cmd']),('cpp',[str(OUT/'ce181_d_parity.exe')]),
                     ('analyze',[str(ghdl),'-a','--std=08','ce181_d_parity.vhd']),
                     ('vhdl',[str(ghdl),'-r','--std=08','ce181_d_parity','--assert-level=error'])]:
    r = subprocess.run(command,cwd=OUT,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
    print(name,r.returncode,(r.stdout+r.stderr)[-500:],flush=True);r.check_returncode()
(OUT/'result.json').write_text(json.dumps(dict(passed=True,total=len(tests),nibble_pairs=256,
    clock_hold_inhibit_cases=32,vhdl_passes=2,
    scope='Actual CE181_D emitted gate; independent byte even parity and CE181 AQ/AK/AN/AP/AR scan gates; clocks unchanged'),indent=2)+'\n')
