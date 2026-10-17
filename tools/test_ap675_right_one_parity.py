"""Check AP675 AN and AA against independent odd parity in emitted C++/both VHDL passes."""
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
cpptext = (G/'360_ap.cpp').read_text(); vtext = (G/'360_ap.vhd').read_text()
targets = ['AP_INT.temp_ap675_1a_an_not', 'AP_INT._paddl_parity_64M67']
expressions = {}
for target in targets:
    ce = re.search(r'newstate\.'+re.escape(target)+r' = (.+);', cpptext)[1]
    name = normalize_cpp('oldstate.'+target, 'AP')
    ve = [re.findall(r'\b'+re.escape(n)+r' <= (.+?);', vtext, re.S)[-1].split(' when ')[0].strip()
          for n in [name+'_first', name]]
    expressions[target] = (ce, ve)
refs = sorted(set(re.findall(refpat, ' '.join(e[0] for e in expressions.values()))))
ids = {r:'s'+str(i) for i,r in enumerate(refs)}
cpp = ['#include <stdio.h>', 'int main(){', *(f'bool {s}=0;' for s in ids.values())]
vhdl = ['library ieee;use ieee.std_logic_1164.all;use std.env.all;',
        'entity ap675_right_one is end;architecture sim of ap675_right_one is begin process',
        *(f'variable {s}:std_logic;' for s in ids.values()), 'begin']

tests = list(itertools.product((0,1), repeat=4))
for i,(sum63,sample,hold,previous) in enumerate(tests):
    values = dict.fromkeys(refs, 0)
    # AP671 shifts SUM63 into bit64 and zeros bits65..67. Independent odd parity.
    odd_parity = 1 ^ ((sum63 << 3).bit_count() % 2)
    values.update({'oldstate.AP_INT.sum.B63':sum63,
                   'oldstate.AP_INT.temp_ap675_1a_an_not':odd_parity,
                   'oldstate.AP_INT._clock_p3_extendedOshift_r1':sample,
                   'oldstate.AP_INT.clock_p2_extended':hold,
                   'oldstate.AP.paddl_parity_64M67':previous})
    assert set(values) == set(refs)
    expected = [odd_parity, int(not ((sample and odd_parity) or (hold and previous)))]
    cpp.extend(f'{ids[r]}={v};' for r,v in values.items())
    vhdl.extend(f"{ids[r]}:='{v}';" for r,v in values.items())
    names = {normalize_cpp(r,'AP').replace('[','(').replace(']',')'):ids[r] for r in values}
    pattern = '|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))
    for target,want in zip(targets,expected):
        ce, ve = expressions[target]
        ce = re.sub(refpat,lambda m:ids[m[0]],ce)
        cpp.append(f'if(({ce})!={want}){{printf("case {i} {target} failed\\n");return 1;}}')
        for passno,vexpr in enumerate(ve):
            vexpr = re.sub(r'(?<![\w])(?:'+pattern+r')(?![\w])',lambda m:names[m[0]],vexpr.replace('_first',''))
            vhdl.append(f'assert ({vexpr})=\'{want}\' report "case {i} {target} pass {passno}" severity failure;')
cpp += [f'printf("AP675_RIGHT_ONE_PASS {len(tests)} cases\\n");return 0;}}']
vhdl += [f'report "AP675_RIGHT_ONE_PASS {len(tests)} cases";stop;wait;end process;end;']
(OUT/'ap675_right_one.cpp').write_text('\n'.join(cpp))
(OUT/'ap675_right_one.vhd').write_text('\n'.join(vhdl))
(OUT/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj ap675_right_one.cpp /Fe:ap675_right_one.exe\nexit /b %errorlevel%\n')
ghdl = ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,command in [('build',['cmd','/c','build.cmd']),('cpp',[str(OUT/'ap675_right_one.exe')]),
                     ('analyze',[str(ghdl),'-a','--std=08','ap675_right_one.vhd']),
                     ('vhdl',[str(ghdl),'-r','--std=08','ap675_right_one','--assert-level=error'])]:
    r = subprocess.run(command,cwd=OUT,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
    print(name,r.returncode,(r.stdout+r.stderr)[-500:],flush=True);r.check_returncode()
(OUT/'result.json').write_text(json.dumps(dict(passed=True,total=len(tests),vhdl_passes=2,
    tested_targets=targets,scope='AP675 AN inversion and AA right-one/sample/hold against independent AP671 nibble odd parity; stable input pins; no timing or whole CPU equivalence claim'),indent=2)+'\n')
