from pathlib import Path
import json,re,subprocess,sys,itertools
import argparse
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Regression for CE181 hold, RT777 complemented forced-ones parity and coherent ST load/hold pins.')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--generated',type=Path,default=ROOT/'gen/ald')
args=parser.parse_args();OUT=args.output.resolve();OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'tools'))
from audit_ald_logic import normalize_cpp
G=args.generated.resolve();work=OUT/'three-repairs';work.mkdir(exist_ok=True)
refpat=r'oldstate\.[\w]+\.[\w]+(?:\.B\d+)?'
def rhs(file,target):
 s=(G/file).read_text();m=re.search(r'newstate\.'+re.escape(target)+r' = (.+);',s);assert m,target
 return m[1]
def vhdl_rhs(file,target,section):
 name=normalize_cpp('oldstate.'+target,section).replace('[','(').replace(']',')')
 text=(G/file).read_text();names=[name.replace('(', '_first(',1) if '(' in name else name+'_first',name]
 result=[]
 for n in names:
  matches=re.findall(r'\b'+re.escape(n)+r' <= (.+?);',text,re.S);assert matches,n
  result.append(matches[-1].split(' when ')[0].strip())
 return result
tests=[];counts={}
def add(kind,file,vfile,target,section,cases):
 expr=rhs(file,target);ve=vhdl_rhs(vfile,target,section);refs=sorted(set(re.findall(refpat,expr)))
 for values,expected in cases:
  assert all(ref in values for ref in refs),(target,refs,values)
  tests.append((expr,ve,section,values,int(expected),kind))
 counts[kind]=counts.get(kind,0)+len(cases)
expr=rhs('360_ce.cpp','CE_INT._temp253');refs=sorted(set(re.findall(refpat,expr)));assert len(refs)==12,refs
cases=[]
for bits in itertools.product([0,1],repeat=len(refs)):
 d=dict(zip(refs,bits));a=lambda field:d['oldstate.'+field]
 prediction=(a('CE_INT._block_predicted_parity') and (a('RE.e_reg_bits_8M15_even') ^ a('CE_INT.change_parity_of_e8M11') ^ a('CE_INT.change_parity_of_e12M15')))
 prediction|=a('CE_INT.gate_d18M21_to_incr12M15') and (a('RE._e_reg_bits_8M11_even') ^ a('RD.d_reg_bits_18M21_odd'))
 prediction|=a('CE_INT.gate_cone_to_incr12M15') and a('RE.e_reg_bits_8M11_even')
 expected=not ((a('RE._clock_p3') and prediction) or (a('RE.clock_p2_incr_reset') and a('CE.incr_bits_8M15_even')))
 cases.append((d,expected))
add('CE181','360_ce.cpp','360_ce.vhd','CE_INT._temp253','CE',cases)
expr=rhs('360_rs.cpp','RS_RT._st_padda_bus_parity_56M63');refs=sorted(set(re.findall(refpat,expr)));cases=[]
for byte in range(256):
 for comp in range(2):
  for ones in range(2):
   d=dict.fromkeys(refs,0)
   for bit in range(16,21):d[f'oldstate.RD.d_bit.B{bit}']=(byte>>(23-bit))&1
   d['oldstate.RD.d_parity_16M23']=1^(byte.bit_count()%2)
   d['oldstate.RS_RT_INT.gt_d_padda_40M63_c']=comp;d['oldstate.RS_RT_INT.gt_d_padda_40M63_t']=1-comp
   d['oldstate.RS_RT_INT._block_padda_bus_pty_56M63']=1
   d['oldstate.RS_RT_INT.gt_onesMMpadda_61M63']=ones;d['oldstate.RS_RT_INT._gt_onesMMpadda_61M63']=1-ones
   data=byte^(255 if comp else 0)
   if ones:data|=7
   cases.append((d,data.bit_count()%2))
add('RT777','360_rs.cpp','360_rs.vhd','RS_RT._st_padda_bus_parity_56M63','RS_RT',cases)
text=(G/'360_rs.cpp').read_text()
targets=re.findall(r'^  newstate\.(RS_RT\._st_bit\.B\d+|RS_RT_INT\._st_parity_\d+M\d+) =',text,re.M)
assert len(targets)==72,len(targets)
for target in targets:
 expr=rhs('360_rs.cpp',target);refs=sorted(set(re.findall(refpat,expr)));reset=[r for r in refs if '._rst_' in r]
 held=[r for r in refs if '.st_bit.' in r or '.st_parity_' in r];assert len(held)==1,(target,held)
 sources=[r for r in refs if r not in reset+held];assert len(sources)==len(reset),(target,sources,reset)
 assert '.gt_' not in expr,expr
 cases=[]
 for gates in itertools.product([0,1],repeat=len(reset)):
  for data in range(2):
   for previous in range(2):
    d=dict(zip(reset,gates));d.update(dict.fromkeys(sources,data));d[held[0]]=previous
    expected=not(data if not all(gates) else previous)
    cases.append((d,expected))
 # Each individual wired source must capture on its own matching pin.
 pairs=re.findall(r'\((oldstate\.[\w.]+) && !(oldstate\.[\w.]+)\)',expr)
 assert len(pairs)==len(sources),(target,pairs)
 for source,pin in pairs:
  for previous in range(2):
   for data in range(2):
    d=dict.fromkeys(refs,0);d.update(dict.fromkeys(reset,1));d[pin]=0;d[source]=data;d[held[0]]=previous
    cases.append((d,not data))
 add('ST capture','360_rs.cpp','360_rs.vhd',target,'RS_RT',cases)
# Emit both languages using their actual generated equations and shared vectors.
allrefs=sorted(set(r for _,_,_,values,_,_ in tests for r in values));ids={r:'s'+str(i) for i,r in enumerate(allrefs)}
cpp=['#include <stdio.h>','int main(){',*(f'bool {s}=0;' for s in ids.values())]
vhdl=['library ieee;use ieee.std_logic_1164.all;use std.env.all;entity repairs is end;architecture sim of repairs is','begin process',*(f'variable {s}:std_logic;' for s in ids.values()),'begin']
for index,(expr,ve,section,values,expected,kind) in enumerate(tests):
 cpp.extend(f'{ids[r]}={v};' for r,v in values.items());vhdl.extend(f"{ids[r]}:='{v}';" for r,v in values.items())
 ce=re.sub(refpat,lambda m:ids[m[0]],expr)
 names={normalize_cpp(r,section).replace('[','(').replace(']',')'):ids[r] for r in values}
 # Replace longer vector-indexed names before their prefixes.
 pattern='|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))
 cpp.append(f'if(({ce})!={expected}){{printf("case {index} {kind} failed\\n");return 1;}}')
 for passno,vexpr in enumerate(ve):
  vexpr=re.sub(r'(?<![\w])(?:'+pattern+r')(?![\w])',lambda m:names[m[0]],vexpr.replace('_first',''))
  vhdl.append(f'assert ({vexpr})=\'{expected}\' report "case {index} {kind} pass {passno}" severity failure;')
cpp+=['printf("REPAIRS_PASS %d cases\\n",'+str(len(tests))+');return 0;}']
vhdl+=['report "REPAIRS_PASS '+str(len(tests))+' cases";stop;wait;end process;end;']
(work/'repairs.cpp').write_text('\n'.join(cpp));(work/'repairs.vhd').write_text('\n'.join(vhdl))
(work/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj repairs.cpp /Fe:repairs.exe\nexit /b %errorlevel%\n')
commands=[('build',['cmd','/c','build.cmd']),('cpp',[str(work/'repairs.exe')]),('analyze',[str(ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'),'-a','--std=08','repairs.vhd']),('vhdl',[str(ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'),'-r','--std=08','repairs','--assert-level=error'])]
for name,command in commands:
 r=subprocess.run(command,cwd=work,capture_output=True,text=True);(work/(name+'.log')).write_text(r.stdout+r.stderr);print(name,r.returncode,(r.stdout+r.stderr)[-700:]);r.check_returncode()
(work/'result.json').write_text(json.dumps(dict(passed=True,cases_per_language=counts,total=len(tests),st_equations=72,scope='Actual emitted CE181/RT777/ST latch equations; independent parity and capture expectations. Full CPU and SPECIAL histories are separate limitations.'),indent=2))
