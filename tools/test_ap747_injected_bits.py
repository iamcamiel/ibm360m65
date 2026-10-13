"""Check AP671 physical BQ4/BP4 hot-bit fan-in in emitted AP747 C++/both VHDL passes."""
from pathlib import Path
import argparse,json,re,subprocess,sys
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--generated',type=Path,default=R/'gen/ald');p.add_argument('--original',type=Path,default=R/'gen/logout-repair-r127-20261013/source-snapshot/gen/ald');a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(R/'tools'));from audit_ald_logic import normalize_cpp
t='AP_INT._half_sum_error_64M67'
def expressions(g):
 c=(g/'360_ap.cpp').read_text();v=(g/'360_ap.vhd').read_text();ce=re.search(r'newstate\.'+re.escape(t)+r' = (.+);',c)[1];n=normalize_cpp('oldstate.'+t,'AP')
 ve=[re.findall(r'\b'+re.escape(k)+r' <= (.+?);',v,re.S)[-1].split(' when ')[0].strip() for k in [n+'_first',n]];return ce,ve
old=expressions(a.original.resolve());fixed=expressions(a.generated.resolve())
assert all('oldstate.AP_INT.padd_bit_b.B'+str(i) in fixed[0] for i in range(64,68))
pat=r'oldstate\.[\w]+\.[\w]+(?:\.B\d+)?';refs=sorted(set(re.findall(pat,old[0]+' '+fixed[0])));ids={r:'s'+str(i) for i,r in enumerate(refs)}
vn={normalize_cpp(r,'AP').replace('[','(').replace(']',')'):ids[r] for r in refs};vp='|'.join(re.escape(n) for n in sorted(vn,key=len,reverse=True))
cpp=lambda e:re.sub(pat,lambda m:ids[m[0]],e)
vhd=lambda e:re.sub(r'(?<![\w])(?:'+vp+r')(?![\w])',lambda m:vn[m[0]],e.replace('_first',''))
C=['#include <cstdio>','int main(){unsigned rejected=0;',*(f'bool {v}=0;' for v in ids.values())]
V=['library ieee;use ieee.std_logic_1164.all;use std.env.all;','entity ap747_pin is end;architecture sim of ap747_pin is begin process',*(f'variable {v}:std_logic;' for v in ids.values()),'begin']
count=bad=0
for byte in range(16):
 for hot in range(4):
  for controls in range(16):
   d=dict.fromkeys(refs,0);aval=[(byte>>i)&1 for i in range(3,-1,-1)];bval=[aval[0]|((hot>>1)&1),aval[1]|(hot&1),aval[2],aval[3]]
   for side,values in [('a',aval),('b',bval)]:
    for i,value in enumerate(values):
     key=f'oldstate.AP_INT.padd_bit_{side}.B{64+i}'
     if key in d:d[key]=value
   ctrl=[(controls>>i)&1 for i in range(3,-1,-1)]
   for key,value in zip(['left_shift_par_adj','ab_64M65_odd_on_l2_shift','P2_comp_64M67_par_adj','bus_parity_64M67'],ctrl):d['oldstate.AP_INT.'+key]=value
   want=(sum(bval)+sum(ctrl))%2;legacy=(sum(aval)+sum(ctrl))%2;bad+=want!=legacy;count+=1
   C.extend(f'{ids[k]}={v};' for k,v in d.items());V.extend(f"{ids[k]}:='{v}';" for k,v in d.items())
   C.append(f'if(({cpp(old[0])})!={legacy})return 2;if(({cpp(fixed[0])})!={want})return 3;if(({cpp(old[0])})!={want})++rejected;')
   for i,e in enumerate(old[1]):V.append(f"assert ({vhd(e)})='{legacy}' report \"original pass{i} case{count}\" severity failure;")
   for i,e in enumerate(fixed[1]):V.append(f"assert ({vhd(e)})='{want}' report \"corrected pass{i} case{count}\" severity failure;")
assert count==1024 and bad==384
C+=['if(rejected!=384)return 4;puts("AP747_PASS 1024 physical pin cases; original384 failures corrected0");return 0;}'];V+=['report "AP747_PASS 1024 physical pin cases; original384 failures corrected0; both passes";stop;wait;end process;end;']
(O/'ap747.cpp').write_text('\n'.join(C));(O/'ap747.vhd').write_text('\n'.join(V))
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /Od /bigobj ap747.cpp /Fe:ap747.exe\nexit /b %errorlevel%\n')
g=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,cmd in [('build',['cmd','/c','build.cmd']),('cpp',[str(O/'ap747.exe')]),('analyze',[str(g),'-a','--std=08','ap747.vhd']),('vhdl',[str(g),'-r','--std=08','ap747_pin','--assert-level=error'])]:
 z=subprocess.run(cmd,cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(z.stdout+z.stderr);print(name,z.returncode,(z.stdout+z.stderr)[-700:],flush=True);z.check_returncode()
(O/'result.json').write_text(json.dumps(dict(passed=True,physical_pin_cases=1024,original_output_errors=384,corrected_output_errors=0,vhdl_passes=2,original_failure_reproduced=True,emitted_cpp=fixed[0],scope='Independent AP671 BQ4/BP4 hot-bit pin fan-in oracle against actual emitted AP747 C++ and both VHDL Boolean passes; not whole CPU or routed timing proof'),indent=2))
