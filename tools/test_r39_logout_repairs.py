"""Original RT701 and AP561/AP571/AP581 gate oracles against emitted C++/both VHDL passes."""
from pathlib import Path
import argparse,re,subprocess,json,sys
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--generated',type=Path,default=R/'gen/ald');p.add_argument('--original',type=Path,default=R/'gen/logout-repair-r131-20261013/source-snapshot/gen/ald');a=p.parse_args()
O=a.output.resolve();O.mkdir(parents=True,exist_ok=False);G=a.generated.resolve();P=a.original.resolve()
sys.path.insert(0,str(R/'tools'));from audit_ald_logic import normalize_cpp
refpat=r'oldstate\.\w+\.\w+(?:\.B\d+)?'
targets=[('RS_RT','RS_RT._d_padda_bus_ov.B7')]+[('AP','AP_INT._latched_sum.B'+str(b)) for b in (56,57,58)]
results={}
for section,target in targets:
 file_section='rs' if section=='RS_RT' else section.lower()
 def expr(g):return re.search(r'newstate\.'+re.escape(target)+r' = (.+);',(g/('360_'+file_section+'.cpp')).read_text())[1]
 old,fixed=expr(P),expr(G)
 refs=sorted(set(re.findall(refpat,old+' '+fixed)));derived='oldstate.AP_INT.zero_shift_or_enable_scan'
 inputs=[x for x in refs if x!=derived];ids={r:'s'+str(i) for i,r in enumerate(refs)}
 def e(r):return ids['oldstate.'+r]
 if section=='RS_RT':
  want='!('+' || '.join('('+e(d)+' && '+e(g)+')' for d,g in [('RD.d_bit.B0','RS_RT_INT.gt_d_padda_08M31_tMl0'),('RD._d_bit.B0','RS_RT_INT.gt_d_padda_08M31_cMl0'),('RD.d_bit.B0','RS_RT_INT.gt_d_padda_07M30_tMl1'),('RD._d_bit.B0','RS_RT_INT.gt_d_padda_07M30_cMl1')])+')'
 else:
  b=int(target.split('.B')[-1])
  want='!(('+e('KT.scan_out_bus_bit.B'+str(b))+' && '+e('AP_INT.enable_scan_bypass')+') || ('+e('AP_INT.sum.B'+str(b))+' && '+e('AP_INT.zero_shift')+' && '+e('AP_INT._clock_p3_extended')+') || ('+e('AP_INT.sum.B'+str(b+4))+' && '+e('AP_INT.left_4_shift')+' && '+e('AP_INT._clock_p3_extended')+') || ('+e('AP_INT.sum.B'+str(b-4))+' && '+e('AP_INT.right_4_shift')+' && '+e('AP_INT._clock_p3_extended')+') || ('+e('AP_INT.clock_p2_extended')+' && '+e('AP.paddl.B'+str(b))+'))'

 ce=lambda text:re.sub(refpat,lambda m:ids[m[0]],text)
 n=normalize_cpp('oldstate.'+target,section).replace('[','(').replace(']',')')
 names=[n.replace('(', '_first(',1) if '(' in n else n+'_first',n]
 text=(G/('360_'+file_section+'.vhd')).read_text();ves=[]
 vm={normalize_cpp(r,section).replace('[','(').replace(']',')'):ids[r] for r in refs}
 vp='|'.join(re.escape(x) for x in sorted(vm,key=len,reverse=True))
 for name in names:
  v=re.findall(r'^\s*'+re.escape(name)+r' <= (.+?);',text,re.M)[-1].split(' when ')[0].replace('_first','')
  v=re.sub(r'(?<![\w])(?:'+vp+r')(?![\w])',lambda m:vm[m[0]],v)
  assert not re.search(r'\b[PM]_|\b(?:AP|RS_RT|KT|RD)_',v),v
  ves.append(v)
 count=1<<len(inputs)
 C=['#include <cstdio>','int main(){unsigned errors=0;',*(f'bool {v}=0;' for v in ids.values()),f'for(unsigned i=0;i<{count};i++)'+'{',*(f'{ids[r]}=(i>>{bit})&1;' for bit,r in enumerate(inputs))]
 if derived in refs:C.append(ids[derived]+'='+e('AP_INT.zero_shift')+' || '+e('AP_INT.enable_scan_bypass')+';')
 C += ['bool want='+want+';',f'if(({ce(old)})!=want)errors++;',f'if(({ce(fixed)})!=want)return 2;',f'}}printf("{section}_PASS cases={count} original_errors=%u corrected_errors=0\\n",errors);return errors?0:3;}}']
 vw=want.replace('!=',' xor ').replace('&&',' and ').replace('||',' or ').replace('!','not ')
 V=['library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;use std.env.all;',f'entity {section.lower()}_pins is end;architecture sim of {section.lower()}_pins is begin process',*(f'variable {v}:std_logic;' for v in ids.values()),f'variable bits:unsigned({len(inputs)-1} downto 0);variable want:std_logic;begin',f'for i in 0 to {count-1} loop bits:=to_unsigned(i,{len(inputs)});',*(f'{ids[r]}:=bits({bit});' for bit,r in enumerate(inputs))]
 if derived in refs:V.append(ids[derived]+':='+e('AP_INT.zero_shift')+' or '+e('AP_INT.enable_scan_bypass')+';')
 V += ['want:='+vw+';',*[f'assert ({v})=want report "{section} pass{index}" severity failure;' for index,v in enumerate(ves)],f'end loop;report "{section}_PASS {count} original-pin cases, both passes";stop;wait;end process;end;']
 d=O/(section.lower()+'-'+target.split('.')[-1]);d.mkdir();(d/'pins.cpp').write_text('\n'.join(C));(d/'pins.vhd').write_text('\n'.join(V));(d/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /O2 pins.cpp /Fe:pins.exe\nexit /b %errorlevel%\n')
 g=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe';outputs={}
 for name,args in [('build',['cmd','/c','build.cmd']),('cpp',[str(d/'pins.exe')]),('analyze',[str(g),'-a','--std=08','pins.vhd']),('vhdl',[str(g),'-r','--std=08',section.lower()+'_pins','--assert-level=error'])]:
  z=subprocess.run(args,cwd=d,capture_output=True,text=True);outputs[name]=z.stdout+z.stderr;(d/(name+'.log')).write_text(outputs[name]);print(section,name,z.returncode,outputs[name][-400:],flush=True);z.check_returncode()
 results[target]=dict(passed=True,cases=count,original_errors=int(re.search(r'original_errors=(\d+)',outputs['cpp'])[1]),corrected_errors=0,vhdl_passes=2,target=target,old_expression=old,corrected_expression=fixed,oracle=want)
(O/'result.json').write_text(json.dumps(dict(passed=True,tests=results,scope='RT701 D0-qualified L0 products and AP561/AP571/AP581 separate positive ZERO SHIFT normal-sum products; actual emitted C++ and both VHDL passes. No full CPU/routed timing claim.'),indent=2))
