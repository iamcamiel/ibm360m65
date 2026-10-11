"""Check AR582/AS072/AS075 emitted equations against scan gates and byte parity."""
from pathlib import Path
import argparse, itertools, json, re, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--generated', type=Path, default=ROOT/'gen/ald')
args = p.parse_args()
G = args.generated.resolve()
OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(ROOT/'tools'))
from audit_ald_logic import normalize_cpp

refpat = r'oldstate\.[\w]+\.[\w]+(?:\.B\d+)?'
def rhs(file, target):
    m = re.search(r'newstate\.'+re.escape(target)+r' = (.+);', (G/file).read_text())
    assert m, target
    return m[1]

def vhdl_rhs(file, target, section):
    name = normalize_cpp('oldstate.'+target, section)
    names = [name+'_first', name]
    result = []
    text = (G/file).read_text()
    for n in names:
        matches = re.findall(r'\b'+re.escape(n)+r' <= (.+?);', text, re.S)
        assert matches, n
        result.append(matches[-1].split(' when ')[0].strip())
    return result

tests = []
counts = {}
def add(kind, section, target, cases):
    expr = rhs('360_'+section.lower()+'.cpp', target)
    ve = vhdl_rhs('360_'+section.lower()+'.vhd', target, section)
    refs = set(re.findall(refpat, expr))
    for values, expected in cases:
        assert refs <= values.keys(), (kind, refs-values.keys())
        tests.append((expr, ve, section, values, int(expected), kind))
    counts[kind] = len(cases)

def scan_pair(p, q):
    # AR582 U03AM: positive-input NAND. U03AI negative-input/output OR
    # becomes physical AND. U03AL has negative inputs and positive output.
    common = not (p and q)
    left, right = p and common, q and common
    return not left and not right

arrefs = ['oldstate.AR_INT.'+n for n in
          ('_sadda_parity_adjust','_sadda_parity','_saddb_parity_adjust','_saddb_parity')]
cases = []
for bits in itertools.product((0,1), repeat=4):
    # First two pairs pass through AY3L/AZ3M, then BC4L/BH5L/BJ5M/BM6L.
    expected = scan_pair(scan_pair(*bits[:2]), scan_pair(*bits[2:]))
    cases.append((dict(zip(arrefs,bits)), expected))
add('AR582 scan gates', 'AR', 'AR._oe_par_aMpar_b_O_par_adj', cases)

k2target = 'AS_INT._k2_bits_04M07'
k2refs = sorted(set(re.findall(refpat, rhs('360_as.cpp',k2target))))
cases = []
for bits in itertools.product((0,1), repeat=len(k2refs)):
    d = dict(zip(k2refs,bits))
    a = lambda n: d['oldstate.AS_INT.'+n]
    # AS072 positive INVT pin feeds its buffer, then the NOR directly.
    term = a('bit_carry.B5') and a('_bit_carry.B6') and a('bit_transmit.B6')
    term |= a('_bit_carry.B6') and a('_bit_transmit.B5') and a('bit_transmit.B6')
    term |= a('bit_carry.B7') or a('_bit_transmit.B7')
    expected = not (a('invt_predicted_parity') or (a('hot_carry_into_bit_07') and term))
    cases.append((d,expected))
add('AS072 K2 gates','AS',k2target,cases)

target = 'AS_INT._saddl_parity'
refs = sorted(set(re.findall(refpat, rhs('360_as.cpp',target))))
def base():
    d = dict.fromkeys(refs,0)
    d['oldstate.AR._force_sal_ones_blk_err'] = 1
    d['oldstate.AS_INT._clock_p3'] = 1
    return d

# Check all A/B byte pairs in Python using ordinary arithmetic bit counts.
# Collapse identical Boolean inputs into vectors; no byte pair is skipped.
vectors = {}
for a in range(256):
    for b in range(256):
        pa, pb = 1^(a.bit_count()%2), 1^(b.bit_count()%2)
        arnegative = scan_pair(scan_pair(1,pa),scan_pair(1,pb))
        transmit = (a|b).bit_count()%2
        for kind, value in [('or',a|b),('oe',a^b),('and',a&b)]:
            d = base()
            d['oldstate.AS_INT.'+kind+'_function'] = 1
            d['oldstate.AR._oe_par_aMpar_b_O_par_adj'] = int(arnegative)
            d['oldstate.AR.oe_par_aMpar_b_O_par_adj'] = int(not arnegative)
            d['oldstate.AS_INT.oe_transmits_bits_00M07'] = transmit
            d['oldstate.AS_INT._oe_transmits_bits_00M07'] = 1-transmit
            expected = value.bit_count()%2  # Negative odd-parity output.
            key = tuple(d.items())
            if key in vectors: assert vectors[key] == expected
            vectors[key] = expected
add('AS075 logical byte parity','AS',target,[(dict(k),v) for k,v in vectors.items()])

cases = []
for force,p3,p2,previous in itertools.product((0,1),repeat=4):
    for kind in ('none','arithmetic','or','oe','and'):
        for bits in itertools.product((0,1),repeat=4):
            d = base()
            d.update({'oldstate.AR._force_sal_ones_blk_err':force,
                      'oldstate.AS_INT._clock_p3':p3,
                      'oldstate.AS.clock_p2':p2,'oldstate.AS.saddl_parity':previous})
            arneg, tx = bits[:2]
            d['oldstate.AR._oe_par_aMpar_b_O_par_adj'] = arneg
            d['oldstate.AR.oe_par_aMpar_b_O_par_adj'] = 1-arneg
            d['oldstate.AS_INT.oe_transmits_bits_00M07'] = tx
            d['oldstate.AS_INT._oe_transmits_bits_00M07'] = 1-tx
            for n,v in zip(('even_parity_bits_00M03','saddl_even_parity_bits_04M07',
                            'k3_bits_00M03','_k3_bits_04M07'),bits):
                d['oldstate.AS_INT.'+n] = v
            d['oldstate.AS_INT._temp_p3_not_logical'] = int(kind=='arithmetic')
            if kind in ('or','oe','and'): d['oldstate.AS_INT.'+kind+'_function'] = 1
            odd = (not force and p3) or (p2 and previous)
            if kind=='arithmetic': odd |= sum(bits)%2
            if kind=='or': odd |= p3 and not tx
            if kind=='oe': odd |= p3 and arneg
            if kind=='and': odd |= p3 and ((1-arneg)==tx)
            cases.append((d,not odd))
add('AS075 force hold and prediction','AS',target,cases)

allrefs = sorted(set(r for _,_,_,v,_,_ in tests for r in v))
ids = {r:'s'+str(i) for i,r in enumerate(allrefs)}
cpp = ['#include <stdio.h>','int main(){',*(f'bool {s}=0;' for s in ids.values())]
vhdl = ['library ieee;use ieee.std_logic_1164.all;use std.env.all;',
        'entity serial_parity is end;architecture sim of serial_parity is begin process',
        *(f'variable {s}:std_logic;' for s in ids.values()),'begin']
for i,(expr,ve,section,values,expected,kind) in enumerate(tests):
    cpp.extend(f'{ids[r]}={v};' for r,v in values.items())
    vhdl.extend(f"{ids[r]}:='{v}';" for r,v in values.items())
    ce = re.sub(refpat,lambda m:ids[m[0]],expr)
    cpp.append(f'if(({ce})!={expected}){{printf("case {i} {kind} failed\\n");return 1;}}')
    names = {normalize_cpp(r,section).replace('[','(').replace(']',')'):ids[r] for r in values}
    pattern = '|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))
    for passno,vexpr in enumerate(ve):
        vexpr = re.sub(r'(?<![\w])(?:'+pattern+r')(?![\w])',lambda m:names[m[0]],vexpr.replace('_first',''))
        vhdl.append(f'assert ({vexpr})=\'{expected}\' report "case {i} {kind} pass {passno}" severity failure;')
cpp += [f'printf("SERIAL_PARITY_PASS {len(tests)} cases\\n");return 0;}}']
vhdl += [f'report "SERIAL_PARITY_PASS {len(tests)} cases";stop;wait;end process;end;']
(OUT/'serial_parity.cpp').write_text('\n'.join(cpp))
(OUT/'serial_parity.vhd').write_text('\n'.join(vhdl))
(OUT/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj serial_parity.cpp /Fe:serial_parity.exe\nexit /b %errorlevel%\n')
ghdl = ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
commands = [('build',['cmd','/c','build.cmd']),('cpp',[str(OUT/'serial_parity.exe')]),
            ('analyze',[str(ghdl),'-a','--std=08','serial_parity.vhd']),
            ('vhdl',[str(ghdl),'-r','--std=08','serial_parity','--assert-level=error'])]
for name,command in commands:
    r = subprocess.run(command,cwd=OUT,capture_output=True,text=True)
    (OUT/(name+'.log')).write_text(r.stdout+r.stderr)
    print(name,r.returncode,(r.stdout+r.stderr)[-700:],flush=True)
    r.check_returncode()
(OUT/'result.json').write_text(json.dumps(dict(passed=True,cases_per_language=counts,
    total=len(tests),functional_byte_pairs=65536,logical_operations=3,vhdl_passes=2,
    scope='Actual emitted equations, independent scan-gate and byte-parity expectations; no full CPU equivalence claim.'),indent=2))
