"""KS121 original reset pins and actual emitted C++/both VHDL passes."""
from pathlib import Path
import argparse,re,json,subprocess
P=argparse.ArgumentParser();P.add_argument('--output',type=Path,required=True);a=P.parse_args();R=Path(__file__).resolve().parents[1];O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
c=(R/'gen/ald/360_ks.cpp').read_text();v=(R/'gen/ald/360_ks.vhd').read_text();before,clock=c.split('void process_KS_clock()',1)
assert 'newstate.KS_INT._reset_staa =' in before and 'newstate.KS_INT._reset_staa =' not in clock.split('void init_KS()',1)[0]
expr=re.search(r'newstate.KS_INT._reset_staa = (.+?);',before,re.S)[1]
cp=['KS_INT.saszPsajo','DR.temp_dr131_g_l.B2','DR.temp_dr081_f_l.B4','KZ.edit_reset_staa','DR.set_staa_if_sal00M07_not_eq_0','KS_INT._temp_staa_lth','KS_INT.stat_clock_p0']
vp=['P_saszPsajo','DR_P_temp_dr131_g_l(2)','DR_P_temp_dr081_f_l(4)','KZ_P_edit_reset_staa','DR_P_set_staa_if_sal00M07_not_eq_0','M_temp_staa_lth','P_stat_clock_p0']
for i,n in enumerate(cp):expr=expr.replace('oldstate.'+n,'b'+str(i))
assert 'oldstate.' not in expr
want='!(b6 && (b0 || b1 || b2 || b3 || (b4 && b5)))'
C=['#include <cstdio>','int main(){for(unsigned n=0;n<128;n++){',*[f'bool b{i}=(n>>{i})&1;' for i in range(7)],'bool got='+expr+';','if(got!=('+want+'))return 2;','}puts("STAA_RESET_CPP_PASS 128 original-pin cases");}'];(O/'pins.cpp').write_text('\n'.join(C))
ves=[]
for target in ['M_reset_staa_first','M_reset_staa']:
 matches=re.findall(r'\b'+target+r'\s*<=\s*(.+?);',v,re.S);e=matches[-1].split(' when ')[0]
 for i,n in enumerate(vp):
  base=n.split('(')[0];suffix=n[len(base):];e=e.replace(base+'_first'+suffix,'b'+str(i));e=e.replace(n,'b'+str(i))
 assert not re.search(r'\b[PM]_|\b(?:DR|KZ)_',e),e
 ves.append(e)
V=['library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;use std.env.all;','entity staa_reset_pins is end;architecture test of staa_reset_pins is begin process',*[f'variable b{i}:std_logic;' for i in range(7)],'variable bits:unsigned(6 downto 0);variable want:std_logic;begin for n in 0 to 127 loop bits:=to_unsigned(n,7);',*[f'b{i}:=bits({i});' for i in range(7)],'want:=not(b6 and (b0 or b1 or b2 or b3 or (b4 and b5)));',*[f'assert ({e})=want report "STAA reset pass {i}" severity failure;' for i,e in enumerate(ves)],'end loop;report "STAA_RESET_VHDL_PASS 128 original-pin cases, both emitted passes";stop;wait;end process;end;'];(O/'pins.vhd').write_text('\n'.join(V))
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /EHsc /O2 pins.cpp /Fe:pins.exe\nexit /b %errorlevel%\n')
G=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
for name,cmd in [('build',['cmd','/c','build.cmd']),('cpp',[str(O/'pins.exe')]),('analyze',[str(G),'-a','--std=08','pins.vhd']),('vhdl',[str(G),'-r','--std=08','staa_reset_pins','--assert-level=error'])]:
 p=subprocess.run(cmd,cwd=O,capture_output=True,text=True);(O/(name+'.log')).write_text(p.stdout+p.stderr);p.check_returncode()
(O/'result.json').write_text(json.dumps({'passed':True,'original_pin_cases':128,'vhdl_passes':2,'reset_is_noclock':True,'scope':'Original KS121 five AND products and negative-output OR; no whole-CPU or routed timing claim'},indent=2));print('STAA_RESET_PINS_PASS')
