"""Exercise emitted AP752 precheck/AP751 hold logic using the original positive bit63 pin.

Stable equivalence, deliberately skewed positive/negative pins, and retained-check
transition are independent of the production expression. This is an isolated
pin/hold regression, not replay of a live MVT instruction history.
"""
from pathlib import Path
import argparse,json,re,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--generated',type=Path,default=ROOT/'gen/ald');p.add_argument('--original',type=Path,default=ROOT/'gen/logout-repair-r126-20261012/source-snapshot/gen/ald');a=p.parse_args()
O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);G=a.generated.resolve();B=a.original.resolve()
sys.path.insert(0,str(ROOT/'tools'));from audit_ald_logic import normalize_cpp
targets=['AP_INT._half_sum_error_56M63','AP_INT._half_sum_error_56M67','AP_INT.temp_ap751_7g_bd_andnot']
refpat=r'oldstate\.[\w]+\.[\w]+(?:\.B\d+)?'
def emitted(g):
 c=(g/'360_ap.cpp').read_text();v=(g/'360_ap.vhd').read_text();out={}
 for t in targets:
  ce=re.search(r'newstate\.'+re.escape(t)+r' = (.+);',c)[1];n=normalize_cpp('oldstate.'+t,'AP')
  ve=[re.findall(r'\b'+re.escape(k)+r' <= (.+?);',v,re.S)[-1].split(' when ')[0].strip() for k in [n+'_first',n]]
  out[t]=(ce,ve)
 return out
old=emitted(B);fixed=emitted(G);raw=targets[0]
assert 'oldstate.AP_INT.half_sum.B63' in fixed[raw][0] and 'oldstate.AP_INT._half_sum.B63' not in fixed[raw][0]
for t in targets[1:]:assert old[t]==fixed[t],t
refs=sorted(set(re.findall(refpat,' '.join(x[0] for m in [old,fixed] for x in m.values()))));ids={r:'s'+str(i) for i,r in enumerate(refs)}
vn={normalize_cpp(r,'AP').replace('[','(').replace(']',')'):ids[r] for r in refs}
vp='|'.join(re.escape(n) for n in sorted(vn,key=len,reverse=True))
def cpp(e):return re.sub(refpat,lambda m:ids[m[0]],e)
def vhd(e):return re.sub(r'(?<![\w])(?:'+vp+r')(?![\w])',lambda m:vn[m[0]],e.replace('_first',''))
C=['#include <stdio.h>','int main(){',*(f'bool {x}=0;' for x in ids.values()),'bool q[2]={1,1},f[2]={0,0},r[2],nq[2],nf[2];']
V=['library ieee;use ieee.std_logic_1164.all;use std.env.all;','entity ap752_pin is end;architecture sim of ap752_pin is begin process',*(f'variable {x}:std_logic;' for x in ids.values()),'type bits is array(0 to 3) of std_logic;variable q,f,r,nq,nf:bits;','begin']
def assign(values):
 assert set(values)==set(refs)
 C.extend(f'{ids[k]}={v};' for k,v in values.items());V.extend(f"{ids[k]}:='{v}';" for k,v in values.items())
def check(e,w,label):
 C.append(f'if(({cpp(e[0])})!={w}){{puts("{label}");return 1;}}')
 for i,x in enumerate(e[1]):V.append(f"assert ({vhd(x)})='{w}' report \"{label} pass{i}\" severity failure;")
for byte in range(256):
 for pa in range(2):
  for pb in range(2):
   d=dict.fromkeys(refs,0)
   for i in range(8):d[f'oldstate.AP_INT._half_sum.B{56+i}']=1^((byte>>(7-i))&1)
   d['oldstate.AP_INT.half_sum.B63']=byte&1;d['oldstate.AP_INT._padda_bus_parity_56M63']=pa;d['oldstate.AP_INT.paddb_bus_parity_56M63']=pb
   want=(byte.bit_count()%2)^pa^pb
   assign(d);check(old[raw],want,'old settled');check(fixed[raw],want,'fixed settled')
   d['oldstate.AP_INT._half_sum.B63']^=1
   assign(d);check(old[raw],1^want,'old skew negative control');check(fixed[raw],want,'fixed skew')
C.append('for(int i=0;i<2;++i){q[i]=1;f[i]=0;}');V.append("q:=(others=>'1');f:=(others=>'0');")
for step in range(8):
 d=dict.fromkeys(refs,0)
 for i in range(8):d[f'oldstate.AP_INT._half_sum.B{56+i}']=1 if i!=7 or step<2 else 0
 # PADDA=0 has odd parity1, hence negative PADDA parity0; PADDB=1 odd parity0.
 d.update({'oldstate.AP_INT.half_sum.B63':1,'oldstate.AP_INT._padda_bus_parity_56M63':0,'oldstate.AP_INT.paddb_bus_parity_56M63':0,'oldstate.AP_INT._clock_p3_extended_O_no_error':int(step<2),'oldstate.AP_INT.clock_p2_extended_or_error':int(step>=2),'oldstate.AP_INT._half_sum_error_64M67':1})
 assign(d)
 for model,m in enumerate([old,fixed]):
  C.append(f'r[{model}]={cpp(m[raw][0])};')
  for passno,e in enumerate(m[raw][1]):V.append(f'r({model*2+passno}):={vhd(e)};')
 # Read the same prior snapshot for latch and feedback; commit together.
 for model,m in enumerate([old,fixed]):
  cm={'oldstate.AP_INT._half_sum_error_56M63':f'r[{model}]','oldstate.AP_INT._half_sum_error_56M67':f'q[{model}]','oldstate.AP_INT._half_sum_error':f'q[{model}]','oldstate.AP_INT.temp_ap751_7g_bd_andnot':f'f[{model}]'}
  for target,var in [(targets[1],'nq'),(targets[2],'nf')]:
   ce=m[target][0]
   ce=re.sub(refpat,lambda z:cm.get(z[0],z[0]),ce)
   C.append(f'{var}[{model}]={cpp(ce)};')
  for passno in range(2):
   j=model*2+passno
   for target,var in [(targets[1],'nq'),(targets[2],'nf')]:
    ve=vhd(m[target][1][passno])
    vm={ids[k]:v.replace('[','(').replace(']',')').replace(f'({model})',f'({j})') for k,v in cm.items()}
    ve=re.sub(r'\bs\d+\b',lambda z:vm.get(z[0],z[0]),ve)
    V.append(f'{var}({j}):={ve};')
 C.append('for(int i=0;i<2;++i){q[i]=nq[i];f[i]=nf[i];}')
 C.append(f'printf("step{step} raw=%u/%u q=%u/%u f=%u/%u\\n",r[0],r[1],q[0],q[1],f[0],f[1]);')
 V.append('q:=nq;f:=nf;')
 if step>=1:
  C.append('if(q[0]||!q[1]||!f[0]||f[1])return 2;')
  V.append('assert q="0011" and f="1100" report "AP751 transient retain mismatch" severity failure;')
C+=['puts("AP752_PASS stable1024 skew1024 transient8 old failure reproduced");return 0;}']
V+=['report "AP752_PASS stable1024 skew1024 transient8 old failure reproduced";stop;wait;end process;end;']
(O/'ap752.cpp').write_text('\n'.join(C));(O/'ap752.vhd').write_text('\n'.join(V))
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj ap752.cpp /Fe:ap752.exe\nexit /b %errorlevel%\n')
ghdl=ROOT/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for n,c in [('build',['cmd','/c','build.cmd']),('cpp',[str(O/'ap752.exe')]),('analyze',[str(ghdl),'-a','--std=08','ap752.vhd']),('vhdl',[str(ghdl),'-r','--std=08','ap752_pin','--assert-level=error'])]:
 r=subprocess.run(c,cwd=O,capture_output=True,text=True);(O/(n+'.log')).write_text(r.stdout+r.stderr);print(n,r.returncode,(r.stdout+r.stderr)[-700:],flush=True);r.check_returncode()
(O/'result.json').write_text(json.dumps(dict(passed=True,stable_cases=1024,skew_cases=1024,transient_steps=8,vhdl_passes=2,original_failure_reproduced=True,emitted_cpp=fixed[raw][0],scope='Actual emitted AP752/AP751 equations; positive/negative pin skew and shared prior snapshot for hold; private transition not exact r33 replay, separate MB source unresolved'),indent=2))
