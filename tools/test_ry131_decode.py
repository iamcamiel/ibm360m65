from pathlib import Path
import json,re,subprocess,sys,itertools
import argparse
R=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);a.add_argument('--generated',type=Path,default=R/'gen/ald');a.add_argument('--original',type=Path,default=R/'gen/logout-repair-r124-20261012/source-snapshot/gen/ald');args=a.parse_args()
O=args.output.resolve();O.mkdir(exist_ok=False,parents=True)
sys.path.insert(0,str(R/'tools'));from audit_ald_logic import normalize_cpp
refpat=r'oldstate\.\w+\.\w+(?:\.B\d+)?'
targets=['RY.write_9020_reg','RY._P9020_regs_to_ls_bus_out']
leaves={'oldstate.RY.ros_reg_bit.B36':'b36','oldstate.RY._ros_reg_bit.B36':'n36','oldstate.RY._ros_reg_bit.B37':'n37','oldstate.RY.clock_p0_forced':'clk'}
expressions={}
for kind,G in [('original',args.original),('fixed',args.generated)]:
 ct=(G/'360_ry.cpp').read_text();vt=(G/'360_ry.vhd').read_text()
 def expand_cpp(ref,depth=0):
  assert depth<8,ref
  if ref in leaves:return leaves[ref]
  e=re.search(r'newstate\.'+re.escape(ref.removeprefix('oldstate.'))+r' = (.+);',ct)[1]
  return '('+re.sub(refpat,lambda m:expand_cpp(m[0],depth+1),e)+')'
 def expand_vhdl(ref,passno,depth=0):
  assert depth<8,ref
  if ref in leaves:return leaves[ref]
  name=normalize_cpp(ref,'RY'); selected=name+'_first' if passno==0 else name
  matches=re.findall(r'\b'+re.escape(selected)+r' <= (.+?);',vt,re.S)
  if not matches:matches=re.findall(r'\b'+re.escape(name)+r' <= (.+?);',vt,re.S)
  e=matches[-1].split(' when ')[0].strip().replace('_first','')
  ce=re.search(r'newstate\.'+re.escape(ref.removeprefix('oldstate.'))+r' = (.+);',ct)[1]
  refs=set(re.findall(refpat,ce));mapping={normalize_cpp(x,'RY').replace('[','(').replace(']',')'):x for x in refs}
  pat='|'.join(re.escape(x) for x in sorted(mapping,key=len,reverse=True))
  return '('+re.sub(r'(?<![\w])(?:'+pat+r')(?![\w])',lambda m:expand_vhdl(mapping[m[0]],passno,depth+1),e)+')'
 for target in targets:expressions[kind,target]=(expand_cpp('oldstate.'+target),[expand_vhdl('oldstate.'+target,p) for p in range(2)])
cpp=['#include <cstdio>','int main(){int old_errors=0;bool b36,n36,n37,clk;']
vhdl=['library ieee;use ieee.std_logic_1164.all;use std.env.all;entity ry131_decode is end;architecture sim of ry131_decode is begin process variable b36,n36,n37,clk:std_logic;variable old_errors:integer:=0;begin']
vectors=[]
for i,(a,b,c) in enumerate(itertools.product((0,1),repeat=3)):
 values=dict(b36=a,n36=1-a,n37=1-b,clk=c)
 # Open triangles: BE negative output; BA negative input; BF/BQ negative output. BA and BF/BQ preserve voltage, so both outputs equal BE NAND. No P0 input.
 write=1-(a&(1-b));expected=[write,write]
 vectors.append(dict(bit36=a,bit37=b,clock_p0=c,write=write,memory_enable=write))
 cpp.extend(f'{k}={val};' for k,val in values.items());vhdl.extend(f"{k}:='{val}';" for k,val in values.items())
 for kind in ['original','fixed']:
  for target,want in zip(targets,expected):
   ce,ve=expressions[kind,target]
   if kind=='fixed':cpp.append(f'if(({ce})!={want}){{puts("fixed case {i} {target} failed");return 1;}}')
   else:cpp.append(f'if(({ce})!={want})++old_errors;')
   for passno,e in enumerate(ve):
    if kind=='fixed':vhdl.append(f'assert ({e})=\'{want}\' report "fixed case {i} {target} pass{passno}" severity failure;')
    else:vhdl.append(f'if ({e})/=\'{want}\' then old_errors:=old_errors+1;end if;')
cpp+=['printf("RY131_PASS 8 input combinations, original_errors=%d\\n",old_errors);return old_errors==8?0:2;}']
vhdl+=['assert old_errors=16 report "original error-count differs" severity failure; report "RY131_PASS 8 input combinations, both VHDL passes; original_errors=16";stop;wait;end process;end;']
(O/'gate.cpp').write_text('\n'.join(cpp));(O/'gate.vhd').write_text('\n'.join(vhdl))
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc gate.cpp /Fe:gate.exe\nexit /b %errorlevel%\n')
g=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,cmd in [('build',['cmd','/c','build.cmd']),('cpp',[str(O/'gate.exe')]),('analyze',[str(g),'-a','--std=08','gate.vhd']),('vhdl',[str(g),'-r','--std=08','ry131_decode','--assert-level=error'])]:
 p=subprocess.run(cmd,cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(p.stdout+p.stderr);print(name,p.returncode,p.stdout+p.stderr,flush=True);p.check_returncode()
(O/'result.json').write_text(json.dumps(dict(passed=True,combinations=8,targets=targets,original_cpp_errors=8,original_vhdl_errors_each_pass=8,corrected_errors=0,vhdl_passes=2,vectors=vectors,scope='Full-sheet static wire decode, four coherent bit pairs each with both P0 levels; no added P0 qualification; not full hardware timing proof'),indent=2))
