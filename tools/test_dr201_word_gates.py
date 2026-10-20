"""Check the production DR201 6L/6N register word-scan gates against ALD pins."""
import argparse,json,re,subprocess
from pathlib import Path
from test_hercules_write_recording import ROOT,VCVARS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ghdl',type=Path,required=True)
    a=p.parse_args();g=a.generated.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    def run(name,cmd):
        r=subprocess.run([str(x) for x in cmd],cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr);print(name,r.returncode,(r.stdout+r.stderr)[-1000:]);r.check_returncode();return r
    cpp=r'''
#include "360_struc.h"
#include <cstdio>
extern "C" { DATA360 oldstate{},newstate{}; FILE* lf; double runtime=0; }
int main(){
 unsigned cases=0,bad=0;
 for(unsigned gate=0;gate<2;++gate)for(unsigned reg=0;reg<32;++reg)for(unsigned lth=0;lth<32;++lth){
  oldstate={};newstate={};init_DR();
  oldstate.DR_INT.gate_scan_fields_rosr=gate;
  oldstate.RY.ros_reg_bit.F=(unsigned long long)reg<<7;
  oldstate.RY._ros_reg_bit.F=(unsigned long long)(reg^31)<<7;
  oldstate.RY.ros_data_lth_bit.F=(unsigned long long)lth<<7;
  oldstate.RY._ros_data_lth_bit.F=(unsigned long long)(lth^31)<<7;
  // Contradict the obsolete latched one-hot decodes as well as the raw latch.
  oldstate.DR_INT.temp_dr201_sg.B1=!(lth&1);
  oldstate.DR_INT.temp_dr201_sg.B4=!(lth&4);
  process_DR_clock();
  const bool right=gate&&!(reg&8)&&(reg&1);
  const bool left=gate&&!(reg&8)&&!(reg&1)&&(reg&4);
  if(bool(newstate.DR._scan_out_right_word_ros)!=!right||bool(newstate.DR._scan_out_left_word_ros)!=!left)++bad;
  ++cases;
 }
 printf("DR201_WORD_GATES cases=%u bad=%u\n",cases,bad);return bad!=0;
}
'''
    (out/'fixture.cpp').write_text(cpp)
    (out/'build.cmd').write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{ROOT/"hercules"}" /I"{g}" fixture.cpp "{g/"360_dr.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n')
    run('build',['cmd','/c',out/'build.cmd']);run('cpp',[out/'fixture.exe'])
    # Exercise the actual generated entity, entering scan mode through DR211.
    # Both external snapshots receive held inputs; no test-only internal wires.
    ports=re.findall(r'^\s+(\w+)\s*:\s*(in|buffer)\s+(STD_LOGIC(?:_VECTOR\s*\([^;]+\))?)', (g/'360_dr.vhd').read_text(),re.M|re.I)
    maps=[]
    for name,mode,kind in ports:
        if name in ['clk','rst','hlt']: value={'clk':'clk','rst':'rst','hlt':"'0'"}[name]
        elif name=='M_scan_out_left_word_ros':value='left_n'
        elif name=='M_scan_out_right_word_ros':value='right_n'
        elif name.startswith('RY_P_ros_reg_bit') and name in ['RY_P_ros_reg_bit','RY_P_ros_reg_bit_first']:value='reg_bits'
        elif name in ['RY_M_ros_reg_bit','RY_M_ros_reg_bit_first']:value='not reg_bits'
        elif name in ['RY_P_ros_data_lth_bit','RY_P_ros_data_lth_bit_first']:value='lth_bits'
        elif name in ['RY_M_ros_data_lth_bit','RY_M_ros_data_lth_bit_first']:value='not lth_bits'
        elif name in ['KU_P_ton_scan_mode_tgr','KU_P_ton_scan_mode_tgr_first']:value='enable'
        elif mode.lower()=='buffer':value='open'
        else:value="(others => '0')" if 'VECTOR' in kind else "'0'"
        maps.append(name+' => '+value)
    tb='''library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all;
entity test_dr201_word_gates is end;
architecture test of test_dr201_word_gates is
 signal clk,rst,enable : std_logic := '0';
 signal reg_bits : std_logic_vector(0 to 42) := (others=>'0');
 signal lth_bits : std_logic_vector(2 to 42) := (others=>'0');
 signal left_n,right_n : std_logic;
 function sl(b:boolean) return std_logic is begin if b then return '1'; else return '0'; end if; end;
begin
 dut:entity work.DR port map('''+',\n'.join(maps)+''');
 process
  variable cases:natural:=0;
  procedure step is begin clk<='0';wait for 5 ns;clk<='1';wait for 5 ns;end;
 begin
 for gate in 0 to 1 loop for reg in 0 to 31 loop for lth in 0 to 31 loop
  rst<='1';step;rst<='0';enable<=sl(gate=1);
  reg_bits<=(others=>'0');lth_bits<=(others=>'0');
  reg_bits(31 to 35)<=std_logic_vector(to_unsigned(reg,5));
  lth_bits(31 to 35)<=std_logic_vector(to_unsigned(lth,5));
  for i in 1 to 12 loop step;end loop;
  assert right_n=not sl(gate=1 and ((reg/8) mod 2)=0 and reg mod 2=1) report "DR201 right word gate" severity failure;
  assert left_n=not sl(gate=1 and ((reg/8) mod 2)=0 and reg mod 2=0 and ((reg/4) mod 2)=1) report "DR201 left word gate" severity failure;
  cases:=cases+1;
 end loop;end loop;end loop;
 report "DR201_VHDL_PASS cases="&integer'image(cases);wait;
 end process;
end;
'''
    (out/'test_dr201_word_gates.vhd').write_text(tb)
    run('analyze',[a.ghdl.resolve(),'-a','--std=08',g/'360_dr.vhd',out/'test_dr201_word_gates.vhd'])
    r=run('vhdl',[a.ghdl.resolve(),'-r','--std=08','test_dr201_word_gates','--assert-level=error'])
    assert 'DR201_VHDL_PASS cases=2048' in r.stdout
    (out/'result.json').write_text(json.dumps({'passed':True,'cpp_cases':2048,'vhdl_cases':2048,'scope':'DR201 6L/6N actual register pins, enable, ignored bits and independent latch values; not whole logout or timing proof.'},indent=2)+'\n')
if __name__=='__main__':main()
