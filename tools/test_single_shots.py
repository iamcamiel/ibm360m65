"""Validate every single-shot, preserving nominal widths and explicit test scope.

The scaled mode changes only private copies of counter limits above 1000 ticks.
nominal-fast tests actual production widths through 5.5 ms. nominal-long checks
one full held-input pulse at the actual 30 ms, 1 s and 2.5 s widths.
"""
from pathlib import Path
import argparse, hashlib, json, re, subprocess

ROOT = Path(__file__).resolve().parents[1]
SPECS = {
    'doubless200ns': (20, 1, 1), 'ss300ns': (30, 1, 1),
    'nssn525ns': (53, 0, 0), 'nssn1200ns': (120, 0, 0),
    'nssn2800ns': (280, 0, 0), 'nssn3us': (300, 0, 0),
    'nssn5500us': (550000, 0, 0), 'ssn5500us': (550000, 1, 0),
    'nssn30ms': (3000000, 0, 0), 'nssn1000ms': (100000000, 0, 0),
    'nssn2500ms': (250000000, 0, 0),
}

def prepare(source_root, output, mode):
    output.mkdir(parents=True, exist_ok=False)
    sources, entries, instances = [], {}, []
    for name, (ticks, inp, active) in SPECS.items():
        if mode == 'nominal-fast' and ticks > 550000: continue
        if mode == 'nominal-long' and ticks <= 550000: continue
        source = source_root / 'src/vhdl/cpu_primitives' / (name + '.vhd')
        text = source.read_text()
        assert f'0 to {ticks - 1}' in text, name
        tested_ticks = 37 if mode == 'scaled' and ticks > 1000 else ticks
        if tested_ticks != ticks:
            # Only the counter bound and reload constant change privately.
            assert text.count(str(ticks - 1)) == 2, name
            text = text.replace(str(ticks - 1), str(tested_ticks - 1))
        target = output / (name + '.vhd'); target.write_text(text)
        sources.append(target)
        entries[name] = dict(nominal_ticks=ticks, tested_ticks=tested_ticks,
                            input_active=inp, output_active=active,
                            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        ix = len(instances)
        instances.append(f'''u{ix}: entity work.{name} port map(clk,rst({ix}),hlt({ix}),i({ix}),o({ix}));
c{ix}: entity work.single_shot_check generic map("{name}",{tested_ticks},'{inp}','{active}',{str(mode != 'nominal-long').lower()})
  port map(clk,rst({ix}),hlt({ix}),i({ix}),o({ix}),done({ix}),errors({ix}));''')
    n = len(instances)
    wrapper = f'''library ieee;use ieee.std_logic_1164.all;
entity test_single_shots is end;
architecture test of test_single_shots is
signal clk:std_logic:='0';signal rst,hlt,i,o,done:std_logic_vector(0 to {n-1});
type counts is array(0 to {n-1}) of natural;signal errors:counts;
begin
process begin while done /= (done'range=>'1') loop clk<='0';wait for 5 ns;clk<='1';wait for 5 ns;end loop;wait;end process;
{chr(10).join(instances)}
process variable total:natural:=0;begin wait until done=(done'range=>'1');wait for 1 ns;
for j in errors'range loop total:=total+errors(j);end loop;
assert total=0 report "SINGLE_SHOTS_FAIL errors="&integer'image(total) severity failure;
report "SINGLE_SHOTS_PASS";wait;end process;
end;
'''
    bench = output / 'test_single_shots.vhd'; bench.write_text(wrapper)
    checker = ROOT / 'tools/test_single_shots.vhd'
    sources += [checker, bench]
    (output / 'manifest.json').write_text(json.dumps(dict(mode=mode, primitives=entries), indent=2))
    return sources, entries

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=['scaled','nominal-fast','nominal-long'], default='scaled')
    args = parser.parse_args()
    output = args.output.resolve()
    sources, entries = prepare(args.source_root.resolve(), output, args.mode)
    ghdl = ROOT / 'gen/equivalence-tools/ghdl/bin/ghdl.exe'
    for label, options in [('analyze', ['-a','--std=08','-fsynopsys', *map(str,sources)]),
                           ('simulation', ['-r','--std=08','-fsynopsys','test_single_shots','--assert-level=error'])]:
        with (output / (label+'.log')).open('w') as stream:
            result = subprocess.run([str(ghdl),*options],cwd=output,stdout=stream,stderr=subprocess.STDOUT)
        (output / (label+'.exit')).write_text(str(result.returncode))
        if label == 'analyze': assert result.returncode == 0
    log = (output/'simulation.log').read_text()
    cases = {name:int(errors) for name,errors in re.findall(r'SINGLE_SHOT_RESULT (\w+) errors=(\d+)',log)}
    report = dict(mode=args.mode, exit=result.returncode, results=cases,
                  complete=set(cases)==set(entries), passed='SINGLE_SHOTS_PASS' in log,
                  long_counters_scaled=args.mode=='scaled')
    (output/'result.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report),flush=True)
    raise SystemExit(0 if report['passed'] else 1)

if __name__ == '__main__': main()
