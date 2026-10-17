"""Check the AP581 bit-58 input from RT845 AV4 in emitted C++ and VHDL."""
import argparse, json, re, subprocess
from pathlib import Path
from test_hercules_write_recording import ROOT, VCVARS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ghdl',type=Path,required=True)
    a=p.parse_args();g=a.generated.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    def run(name,cmd):
        r=subprocess.run(list(map(str,cmd)),cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr)
        print(name,r.returncode,(r.stdout+r.stderr)[-1000:]);r.check_returncode();return r
    cpp=r'''
#include "360_struc.h"
#include <cstdio>
extern "C" { DATA360 oldstate{},newstate{}; FILE* lf; double runtime=0; }
int main(){unsigned cases=0,bad=0;
 for(unsigned byte=0;byte<256;++byte)for(unsigned hot=0;hot<2;++hot)for(unsigned common=0;common<2;++common){
  oldstate={};newstate={};init_AP();
  oldstate.RS_RT._st_d_padda_bus.F=~byte;
  oldstate.RS_RT._one_to_padda_58=!hot;
  oldstate.RS_RT._one_to_padda_62=1;
  oldstate.AP_INT._ones_to_padda_53O56M59=!common;
  oldstate.AP_INT._ones_to_padda_60=1;
  oldstate.AP_INT._ones_to_padda_61M63=1;
  process_AP();
  // AP581 uses the independent RT845 input. Neighboring bits keep the common format input.
  unsigned expected=byte|(hot?0x20:0)|(common?0xd0:0);
  if((newstate.AP_INT.temp_padda.F&255)!=expected)++bad;
  ++cases;
 }
 printf("AP581_HOT_ONE cases=%u bad=%u\n",cases,bad);return bad!=0;
}
'''
    (out/'fixture.cpp').write_text(cpp)
    (out/'build.cmd').write_text(f'@echo off\ncall "{VCVARS}" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"{ROOT/"hercules"}" /I"{g}" fixture.cpp "{g/"360_ap.cpp"}" /Fe:fixture.exe\nexit /b %errorlevel%\n')
    run('build',['cmd','/c',out/'build.cmd']);run('cpp',[out/'fixture.exe'])
    rtl=(g/'360_ap.vhd').read_text()
    ports=re.findall(r'^\s+(\w+)\s*:\s*(in|buffer)\s+(STD_LOGIC(?:_VECTOR\s*\([^;]+\))?)',rtl,re.M|re.I)
    # Add observation ports only; all generated logic, clocks and inputs stay intact.
    rtl,n=re.subn(r'\bport\s*\(', 'port (\n    trace_first,trace_second : out STD_LOGIC;',rtl,count=1,flags=re.I)
    assert n==1
    rtl=rtl.replace('\nbegin\n','\nbegin\n  trace_first <= P_temp_padda_first(58);\n  trace_second <= P_temp_padda(58);\n',1)
    assert 'trace_first <= P_temp_padda_first(58)' in rtl
    (out/'observed_ap.vhd').write_text(rtl)
    maps=['trace_first => first_bit','trace_second => second_bit']
    for name,mode,kind in ports:
        if name in ['clk','rst','hlt']:value={'clk':'clk','rst':'rst','hlt':"'0'"}[name]
        elif name in ['RS_RT_M_one_to_padda_58','RS_RT_M_one_to_padda_58_first']:value='not hot'
        elif name in ['RS_RT_M_st_d_padda_bus','RS_RT_M_st_d_padda_bus_first']:value='bus_n'
        elif mode.lower()=='buffer':value='open'
        else:value="(others => '0')" if 'VECTOR' in kind else "'0'"
        maps.append(name+' => '+value)
    tb='''library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;
entity test_ap581_hot_one is end;
architecture sim of test_ap581_hot_one is
 signal clk,rst,hot,first_bit,second_bit:std_logic:='0';
 signal bus_n:std_logic_vector(40 to 63):=(others=>'1');
 function sl(b:boolean)return std_logic is begin if b then return '1';else return '0';end if;end;
begin
 dut:entity work.AP port map('''+',\n'.join(maps)+''');
 process
  procedure step is begin clk<='0';wait for 5 ns;clk<='1';wait for 5 ns;end;
  variable cases:natural:=0;
 begin
  for byte in 0 to 255 loop for force_one in 0 to 1 loop
   rst<='1';step;rst<='0';hot<=sl(force_one=1);
   bus_n<=(others=>'1');bus_n(56 to 63)<=not std_logic_vector(to_unsigned(byte,8));
   for i in 1 to 12 loop step;end loop;
   assert first_bit=sl(((byte/32) mod 2)=1 or force_one=1) and second_bit=first_bit
    report "AP581 bit-58 data/hot-one connection" severity failure;
   cases:=cases+1;
  end loop;end loop;
  report "AP581_VHDL_PASS cases="&integer'image(cases);wait;
 end process;
end;
'''
    (out/'test_ap581_hot_one.vhd').write_text(tb)
    run('analyze',[a.ghdl.resolve(),'-a','--std=08','observed_ap.vhd','test_ap581_hot_one.vhd'])
    r=run('vhdl',[a.ghdl.resolve(),'-r','--std=08','test_ap581_hot_one','--assert-level=error'])
    assert 'AP581_VHDL_PASS cases=512' in r.stdout
    (out/'result.json').write_text(json.dumps({'passed':True,'cpp_cases':1024,'vhdl_cases':512,'scope':'Actual AP581 bit-58 gate, all byte data, independent RT845 hot-one input, common-format independence and neighboring C++ gates; both emitted VHDL passes with observation ports only. Not whole logout or routed timing proof.'},indent=2)+'\n')
if __name__=='__main__':main()
