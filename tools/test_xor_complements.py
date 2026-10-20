"""Production RW/RF/AS parity equations against independent odd-parity oracles.

Held inputs isolate parity conversion and complement wiring; these tests do not
establish complete startup or physical clock equivalence.
"""
import argparse,json,re,subprocess
from pathlib import Path
from test_hercules_write_recording import VCVARS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ghdl',type=Path,required=True)
    a=p.parse_args();gen=a.generated.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    primitives=(Path(__file__).resolve().parents[1]/'hercules/360_struc.h').read_text()
    tdx=re.search(r'#define TDX\(.*?(?=\n#define)',primitives,re.S).group(0)
    (out/'360_struc.h').write_text('#pragma once\n#include "ald.h"\nextern _ALD oldstate,newstate;\ninline char TD(char a){return a;}\n'+tdx+'\n#define TD50NS(oi,no) TDX(oi,no,5)\n')
    source=r'''
#include "360_struc.h"
#include <cstdio>
_ALD oldstate,newstate;
unsigned parity(unsigned x){unsigned p=0;while(x){p^=x&1;x>>=1;}return p;}
int main(){
 unsigned rw_n=0,rw_bad=0,rf_n=0,rf4_bad=0,rf0_bad=0,as_n=0,as_bad=0;
 for(unsigned data=0;data<256;++data)for(unsigned p=0;p<2;++p){
  oldstate=_ALD();newstate=oldstate;oldstate.RW.psw_parity_8M15=p;
  oldstate.RW.psw_bit.B8=(data>>7)&1;
oldstate.RW.psw_bit.B9=(data>>6)&1;
oldstate.RW.psw_bit.B10=(data>>5)&1;
oldstate.RW.psw_bit.B11=(data>>4)&1;
oldstate.RW.psw_bit.B12=(data>>3)&1;
oldstate.RW.psw_bit.B13=(data>>2)&1;
oldstate.RW.psw_bit.B14=(data>>1)&1;
oldstate.RW.psw_bit.B15=(data>>0)&1;
  process_RW();
  unsigned want=p^parity(data&15);rw_n++;rw_bad+=newstate.RW.storage_key_parity!=want;
 }
 for(unsigned f=0;f<256;++f)for(unsigned sal=0;sal<256;++sal)for(unsigned logical=0;logical<2;++logical){
  oldstate=_ALD();newstate=oldstate;
  for(int pass=0;pass<8;++pass){
   oldstate.RF.f_bit.F=f;oldstate.AS.saddl.F=sal;oldstate.RF_INT.temp_log_func=logical;
   oldstate.AS._saddl_even_parity_bits_04M07=parity(sal&15);
   oldstate.AS.saddl_parity=1^parity(sal);
   newstate=oldstate;process_RF();oldstate=newstate;
  }
  unsigned want4=1^parity((f&240)|(sal&15)),want0=1^parity((sal&240)|(f&15));
  rf_n++;rf4_bad+=oldstate.RF_INT.set_p_if_gating_sal_04M07!=want4;
  rf0_bad+=oldstate.RF_INT.set_p_if_gating_sal_00M03!=want0;
 }
 for(unsigned data=0;data<8;++data){
  oldstate=_ALD();newstate=oldstate;
  for(int pass=0;pass<6;++pass){
   oldstate.AS_INT.bit_transmit.B1=(data>>2)&1;oldstate.AS_INT.bit_transmit.B2=(data>>1)&1;oldstate.AS_INT.bit_transmit.B3=data&1;
   newstate=oldstate;process_AS();oldstate=newstate;
  }
  as_n++;as_bad+=(oldstate.AS_INT.oe_transmits_bits_01M03!=parity(data) || oldstate.AS_INT._oe_transmits_bits_01M03!=(1^parity(data)));
 }
 std::printf("XOR_COMPLEMENTS RW=%u bad=%u RF=%u low_bad=%u high_bad=%u AS=%u bad=%u\n",rw_n,rw_bad,rf_n,rf4_bad,rf0_bad,as_n,as_bad);
 return !!(rw_bad||rf4_bad||rf0_bad||as_bad);
}
'''
    (out/'fixture.cpp').write_text(source)
    cmd='@echo off\ncall "'+str(VCVARS)+'" >nul\ncl /nologo /std:c++17 /EHsc /O2 /I"'+str(out)+'" /I"'+str(gen)+'" fixture.cpp '+ ' '.join('"'+str(gen/('360_'+s+'.cpp'))+'"' for s in ['rw','rf','as'])+' /Fe:test.exe\nexit /b %errorlevel%\n'
    (out/'build.cmd').write_text(cmd)
    r=subprocess.run(['cmd','/c',str(out/'build.cmd')],cwd=out,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
    r=subprocess.run([str(out/'test.exe')],cwd=out,capture_output=True,text=True);(out/'cpp.log').write_text(r.stdout+r.stderr);print(r.stdout,end='')
    cpp_ok=r.returncode==0
    # Extract the actual emitted first/second-pass equations, including the
    # RF predictor adjustment signal. Each parity oracle is independent.
    def eq(sec,target):
        t=(gen/('360_'+sec+'.vhd')).read_text()
        return [re.findall(r'\b'+n+r' <= (.+?);',t)[-1].split(' when ')[0] for n in [target+'_first',target]]
    expressions=eq('rw','P_storage_key_parity')+eq('rf','P_set_p_if_gating_sal_04M07')+eq('rf','P_set_p_if_gating_sal_00M03')+eq('as','P_oe_transmits_bits_01M03')
    identifiers=set(re.findall(r'\b(?:[A-Z]+_)?[PM]_[A-Za-z0-9_]+',' '.join(expressions)))
    declarations=[];wiring=[]
    for n in sorted(identifiers):
        base=n.removesuffix('_first')
        if base=='P_psw_bit':typ='std_logic_vector(0 to 39)';value='psw'
        elif base=='P_f_bit':typ='std_logic_vector(0 to 7)';value='f'
        elif base=='P_psw_parity_8M15':typ='std_logic';value='pswp'
        elif base=='P_temp_s4M7_pty':typ='std_logic';value='sal4p'
        elif base=='AS_P_saddl_parity':typ='std_logic';value='salp'
        elif base=='M_oe_transmits_bits_01M03':typ='std_logic';value='negtransmit'
        elif base=='P_oe_transmits_bits_01M03':typ='std_logic';value='postransmit'
        else:raise RuntimeError('Unknown extracted input '+n)
        declarations.append('signal '+n+':'+typ+';');wiring.append(n+' <= '+value+';')
    outputs='\n'.join('result('+str(i)+') <= '+e+';' for i,e in enumerate(expressions))
    tb='''library ieee;use ieee.std_logic_1164.all;use ieee.numeric_std.all;use std.env.all;
entity test_xor is end;architecture sim of test_xor is
signal psw:std_logic_vector(0 to 39):=(others=>'0');signal f:std_logic_vector(0 to 7);
signal pswp,sal4p,salp,negtransmit,postransmit:std_logic;signal result:std_logic_vector(0 to 7);
DECL begin WIRING OUTPUTS
process variable want,p:std_logic;variable ff,ss:std_logic_vector(0 to 7);variable bits:std_logic_vector(0 to 3);begin
for data in 0 to 15 loop for par in 0 to 1 loop
 bits:=std_logic_vector(to_unsigned(data,4));psw(12 to 15)<=bits;pswp<=std_logic(to_unsigned(par,1)(0));
 want:=std_logic(to_unsigned(par,1)(0));for i in 0 to 3 loop want:=want xor bits(i);end loop;wait for 1 ns;
 assert result(0)=want and result(1)=want report "RW parity mismatch" severity failure;
end loop;end loop;
for fv in 0 to 255 loop for sv in 0 to 255 loop
 ff:=std_logic_vector(to_unsigned(fv,8));ss:=std_logic_vector(to_unsigned(sv,8));f<=ff;
 p:='1';for i in 4 to 7 loop p:=p xor ss(i);end loop;sal4p<=p;
 p:='1';for i in 0 to 7 loop p:=p xor ss(i);end loop;salp<=p;
 want:='1';for i in 0 to 3 loop want:=want xor ff(i);end loop;for i in 4 to 7 loop want:=want xor ss(i);end loop;wait for 1 ns;
 assert result(2)=want and result(3)=want report "RF low-nibble parity mismatch" severity failure;
 want:='1';for i in 4 to 7 loop want:=want xor ff(i);end loop;for i in 0 to 3 loop want:=want xor ss(i);end loop;
 assert result(4)=want and result(5)=want report "RF high-nibble parity mismatch" severity failure;
end loop;end loop;
for par in 0 to 1 loop postransmit<=std_logic(to_unsigned(par,1)(0));negtransmit<=not std_logic(to_unsigned(par,1)(0));wait for 1 ns;
 assert result(6)=postransmit and result(7)=postransmit report "AS complement wiring mismatch" severity failure;end loop;
report "XOR_VHDL_PASS RW32 RF65536 AS2 both passes";stop;wait;end process;end;
'''.replace('DECL','\n'.join(declarations)).replace('WIRING','\n'.join(wiring)).replace('OUTPUTS',outputs)
    (out/'test_xor.vhd').write_text(tb)
    for name,flags in [('analyze',['-a','--std=08','test_xor.vhd']),('vhdl',['-r','--std=08','test_xor','--assert-level=error'])]:
        r=subprocess.run([str(a.ghdl.resolve()),*flags],cwd=out,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
    (out/'result.json').write_text(json.dumps({'cpp_passed':cpp_ok,'vhdl_passed':True,'scope':__doc__},indent=2))
    if not cpp_ok:raise SystemExit(1)

if __name__=='__main__':main()
