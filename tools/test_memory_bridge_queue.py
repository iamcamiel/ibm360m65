"""Back-to-back storage selection arriving before previous bridge busy clears."""
from pathlib import Path
import argparse,json,subprocess,shutil
from test_memory_bridge import wire_bench
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--negative-generated',type=Path);a=p.parse_args();R=Path(__file__).resolve().parents[1];O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);G=R/'gen/equivalence-tools/ghdl/bin/ghdl.exe'
C=r"""#include "360_struc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {DATA360 oldstate{},newstate{};FILE*lf=stdout;bool logging=false,mon_cycle=false,mon_ros=false,mon_20=false,mon_stor=false;}
int main(int argc,char**argv){unsigned flags=argc>1?atoi(argv[1]):1;init_WA();newstate.MC._select_se_1_even=1;newstate.MC.store_to_se=flags&1;unsigned accepts=0,advances=0;
auto tick=[&](){oldstate=newstate;process_WA();process_WA_clock();oldstate=newstate;process_WA();accepts+=!newstate.WA._accept_se_1;advances+=newstate.WA.advance_sdbo;};
auto ticks=[&](unsigned n){while(n--)tick();};auto setup=[&](unsigned n){newstate.MA.sab.F=0x300+n*8;newstate.MB.mark.F=255;newstate.MB.sdbi.F=0x1234567800000000ULL+n;newstate.MC._select_se_1_even=0;};ticks(100);setup(0);
for(unsigned n=0;n<12;n++){unsigned seq=((n+1)&3)<<30;unsigned watchdog=0;while((unsigned(newstate.WA.reg_se_cmd.F)&0xc0000000u)!=seq&&watchdog++<150)tick();if(watchdog>=150)return 2;
 if(unsigned(newstate.WA.reg_se_addr.F)!=(0xff000300u+n*8))return 3;
 if((flags&1)&&((static_cast<unsigned long long>(unsigned(newstate.WA.reg_se_wdata_hi.F))<<32)|unsigned(newstate.WA.reg_se_wdata_lo.F))!=0x1234567800000000ULL+n)return 4;
 newstate.MC._select_se_1_even=1;ticks(3);newstate.EXTERNAL_.reg_se_rdata_hi.F=0x87654321;newstate.EXTERNAL_.reg_se_rdata_lo.F=n;newstate.EXTERNAL_.reg_se_resp.F=seq;
 watchdog=0;while(!newstate.WA.advance_sdbo&&watchdog++<150)tick();if(watchdog>=150)return 5;
 if(n<11)setup(n+1);else newstate.MC._select_se_1_even=1;
}
ticks(200);if(accepts!=240||advances!=240||newstate.WA_INT.se_busy)return 6;puts("BRIDGE_QUEUE_NATIVE_PASS requests=12");return 0;}
"""
base=(R/'tools/test_memory_bridge.vhd').read_text();base=base[:base.index('  stimulus : process')]
V=r"""  stimulus : process
    variable accepts,advances : natural := 0;
    variable seq : std_logic_vector(1 downto 0);
    procedure tick is begin wait until rising_edge(clk);wait for 1 ns;if accept_n='0' then accepts:=accepts+1;end if;if advance='1' then advances:=advances+1;end if;end;
    procedure ticks(n:natural) is begin for i in 1 to n loop tick;end loop;end;
    procedure setup(n:natural) is begin sab<=std_logic_vector(to_unsigned(16#300#+n*8,24));marks<=(others=>'1');wdata<=std_logic_vector(unsigned'(x"1234567800000000")+n);select_n<='0';end;
  begin
    ticks(5);rst<='0';mode<=std_logic_vector(to_unsigned(FLAGS,5));ticks(100);setup(0);
    for n in 0 to 11 loop
      seq:=std_logic_vector(to_unsigned((n+1) mod 4,2));
      for watchdog in 0 to 150 loop exit when command(31 downto 30)=seq;tick;end loop;
      assert command(31 downto 30)=seq report "pending selection was lost" severity failure;
      assert address=std_logic_vector(to_unsigned(16#300#+n*8,32) or unsigned'(x"FF000000")) report "queued payload/address changed" severity failure;
      if FLAGS mod 2=1 then assert write_hi & write_lo=std_logic_vector(unsigned'(x"1234567800000000")+n) report "queued store data changed" severity failure;end if;
      select_n<='1';ticks(3);response<=seq & "000000000000000000000000000000";rdata<=x"87654321" & std_logic_vector(to_unsigned(n,32));
      for watchdog in 0 to 150 loop exit when advance='1';tick;end loop;
      assert advance='1' report "missing completion" severity failure;
      if n<11 then setup(n+1);else select_n<='1';end if;
    end loop;
    ticks(200);assert accepts=240 and advances=240 report "completion count/width changed" severity failure;
    report "BRIDGE_QUEUE_VHDL_PASS requests=12";stop;wait;
  end process;
end architecture;
"""
(O/'native.cpp').write_text(C)
(O/'build.cmd').write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"'+str(R/'hercules')+'" /I"'+str(R/'gen/ald')+'" native.cpp "'+str(R/'gen/ald/360_wa.cpp')+'" /Fe:native.exe\nexit /b %errorlevel%\n')
def run(name,cmd,cwd=O,negative=False):
 z=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True);(cwd/(name+'.log')).write_text(z.stdout+z.stderr)
 if negative:assert z.returncode!=0,'Old bridge unexpectedly accepted queued selection'
 else:z.check_returncode()
 return z.returncode
run('build',['cmd','/c','build.cmd']);primitives=[R/'src/vhdl/cpu_primitives'/n for n in ['doubless200ns.vhd','doubletd40ns.vhd','doubletd200ns.vhd','doubletd210ns.vhd','doubletd340ns.vhd']]
(O/'bench.vhd').write_text(wire_bench(base+V,R/'gen/ald'))
run('analyze',[G,'-a','--std=08','-fsynopsys',*primitives,R/'gen/ald/360_wa.vhd',O/'bench.vhd'])
for flags in [0,1]:run('native-'+str(flags),[O/'native.exe',flags]);run('vhdl-'+str(flags),[G,'-r','--std=08','-fsynopsys','test_memory_bridge','-gFLAGS='+str(flags),'--assert-level=error'])
negative=False
if a.negative_generated:
 D=O/'negative';D.mkdir();(D/'bench.vhd').write_text(wire_bench(base+V,a.negative_generated.resolve()));run('analyze',[G,'-a','--std=08','-fsynopsys',*primitives,a.negative_generated.resolve()/'360_wa.vhd',D/'bench.vhd'],D);run('run',[G,'-r','--std=08','-fsynopsys','test_memory_bridge','-gFLAGS=1','--assert-level=error'],D,True);negative=True
(O/'result.json').write_text(json.dumps({'passed':True,'native_cases':2,'vhdl_cases':2,'requests_per_case':12,'sequence_wraps':3,'old_bridge_rejected':negative,'scope':'Actual emitted native/VHDL bridge, new selection queued at previous advance while busy. No CPU or routed timing equivalence claim.'},indent=2));print('BRIDGE_QUEUE_PASS')
