"""Truth-test both emitted AP793 passes against the original ODD detector."""
import argparse,re,subprocess
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated',type=Path,required=True)
    p.add_argument('--ghdl',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    text=(a.generated/'360_ap.vhd').read_text()
    equations=[re.findall(r'\b'+target+r' <= (.+?);',text)[-1].split(" when ")[0]
               for target in ['P_half_sum_error_first','P_half_sum_error']]
    inputs=sorted(set(re.findall(r'\bM_half_sum_error_\w+', ' '.join(equations))))
    groups=['04M07','08M15','16M23','24M31','32M39','40M47','48M55','56M67']
    declarations='\n'.join(f'signal {n}:std_logic;' for n in inputs)
    wiring='\n'.join(f'{n} <= bits({groups.index(n.removeprefix("M_half_sum_error_").removesuffix("_first"))});' for n in inputs)
    tb='''library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;
entity test_ap793 is end;
architecture sim of test_ap793 is
signal bits:std_logic_vector(7 downto 0);
signal first_pass,second_pass:std_logic;
DECLARATIONS
begin
WIRING
first_pass <= FIRST_EXPRESSION;
second_pass <= SECOND_EXPRESSION;
process
variable want:std_logic;
begin
for mask in 0 to 255 loop
 bits<=std_logic_vector(to_unsigned(mask,8));wait for 10 ns;
 want:='0';for i in 0 to 7 loop if (mask/(2**i)) mod 2=1 then want:=not want;end if;end loop;
 assert first_pass=want and second_pass=want report "AP793 ODD detector mismatch at mask "&integer'image(mask) severity failure;
end loop;
report "AP793_VHDL_PASS 256 cases, both Boolean passes";
stop;wait;
end process;
end;
'''.replace('DECLARATIONS',declarations).replace('WIRING',wiring).replace('FIRST_EXPRESSION',equations[0]).replace('SECOND_EXPRESSION',equations[1])
    (out/'test_ap793.vhd').write_text(tb)
    for name,flags in [('analyze',['-a','--std=08','test_ap793.vhd']),('run',['-r','--std=08','test_ap793','--assert-level=error'])]:
        r=subprocess.run([str(a.ghdl.resolve()),*flags],cwd=out,capture_output=True,text=True)
        (out/(name+'.log')).write_text(r.stdout+r.stderr)
        print(r.stdout+r.stderr,end='');r.check_returncode()

if __name__=='__main__':main()
