"""Compile and exercise diagnostics, PCIe CDC and the full generated CPU.

No hardware access or generated-output installation. Pass the preserved prior
generated directory as --baseline to verify native/CLOCK equations unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--ghdl', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pcie-example-dir', type=Path, help='Xilinx example_design directory if not in xise/ipcore_dir')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    generated = output / 'generated'; generated.mkdir()
    ghdl, compiler, baseline = args.ghdl.resolve(), args.compiler.resolve(), args.baseline.resolve()
    results = []

    def run(name, command, marker=None, cwd=output):
        x = subprocess.run([str(z) for z in command], cwd=cwd, capture_output=True, text=True, timeout=90)
        log = x.stdout + x.stderr
        (output / (name + '.log')).write_text(log)
        assert x.returncode == 0, (name, log[-3000:])
        if marker:
            assert marker in log, (name, 'completion marker missing', log[-1000:])
        results.append(dict(name=name, exit_code=0))
        return log

    run('generation', [compiler, '-O1', repo / 'src/ald', generated])
    differences = []
    for p in sorted(generated.iterdir()):
        if p.suffix not in ('.cpp', '.h', '.vhd'):
            continue
        old = baseline / p.name
        assert old.exists(), old
        if old.read_bytes() != p.read_bytes():
            differences.append(p.name)
    assert differences == [], differences  # R137 must leave every generated CPU file identical to R136
    # Exports add wires only, never change a registered CLOCK or Boolean body.
    oldkw = (baseline / '360_kw.vhd').read_text()
    newkw = (generated / '360_kw.vhd').read_text()
    assert oldkw.split('  process(clk)',1)[1].split('  end process;',1)[0] == newkw.split('  process(clk)',1)[1].split('  end process;',1)[0]
    for begin, end in [('ALD_NOCLOCK_FIRST_BEGIN', 'ALD_NOCLOCK_FIRST_END')]:
        assert oldkw.split(begin)[1].split(end)[0] == newkw.split(begin)[1].split(end)[0]
    for tool in ['audit_ald_logic.py', 'audit_ald_schedule.py']:
        run(tool, [sys.executable, repo / 'tools' / tool, generated])
    primitives = sorted((repo / 'src/vhdl/cpu_primitives').glob('*.vhd'))
    run('full-analyze', [ghdl, '-a', '--std=93', '-fsynopsys', '-fexplicit', *primitives,
                        repo / 'src/vhdl/ls_memory.vhd', repo / 'src/vhdl/rosmem.vhd',
                        *sorted(p for p in generated.glob('*.vhd') if p.name != 'ald.vhd'), generated / 'ald.vhd'])
    run('full-elaborate', [ghdl, '-e', '--std=93', '-fsynopsys', '-fexplicit', 'ald'])
    files = [repo / f for f in ['src/vhdl/cdc_mailbox.vhd', 'src/vhdl/cpu_diagnostics.vhd',
             'src/vhdl/pcie/fpga_build.vhd', 'src/vhdl/pcie/EP_MEM.vhd', 'tools/test_cpu_diagnostics.vhd', 'tools/test_cpu_diagnostic_priority.vhd']]
    run('diagnostics-analyze', [ghdl, '-a', '--std=93', '-fsynopsys', '-fexplicit', *files])
    phases = [(5000, 8000, 3000), (8000, 5000, 1000), (5000, 7000, 500)]
    for i, (cpu, pci, phase) in enumerate(phases):
        run('diagnostics-' + str(i), [ghdl, '-r', '--std=93', '-fsynopsys', '-fexplicit', 'test_cpu_diagnostics',
            '-gCPU_HALF_PS=' + str(cpu), '-gPCI_HALF_PS=' + str(pci), '-gPCI_PHASE_PS=' + str(phase),
            '--assert-level=error', '--stop-time=1ms'], 'CPU_DIAGNOSTICS_TEST_PASS')
    run('event-priority', [ghdl, '-r', '--std=93', '-fsynopsys', '-fexplicit', 'test_cpu_diagnostic_priority', '--assert-level=error', '--stop-time=20us'], 'CPU_DIAGNOSTIC_PRIORITY_TEST_PASS')
    for test in ['test_pcie_cdc', 'test_pcie_tlp_cdc', 'test_panel_snapshot']:
        prj = (repo / 'tools' / (test + '.prj')).read_text()
        inputs = [(repo / 'tools' / p).resolve() for p in re.findall(r'vhdl work "([^"]+)"', prj)]
        if args.pcie_example_dir:
            inputs = [p if p.exists() else args.pcie_example_dir.resolve() / p.name for p in inputs]
        assert all(p.exists() for p in inputs), inputs
        run(test + '-analyze', [ghdl, '-a', '--std=93', '-fsynopsys', '-fexplicit', *inputs])
        run(test, [ghdl, '-r', '--std=93', '-fsynopsys', '-fexplicit', test, '--assert-level=error', '--stop-time=20ms'],
            test.upper().removeprefix('TEST_') + '_TEST_PASS')
    run('decoder', [sys.executable, '-m', 'unittest', 'discover', '-s', repo / 'tools', '-p', 'test_m65_diagnostic_dump.py'])
    result = dict(passed=True, tests=results, generated_differences=differences,
                  native_and_headers_identical=True, all_48_clock_bodies_unchanged=True,
                  scope='Functional diagnostics and existing PCIe/panel regressions; no routed timing, ISE synthesis or hardware installation.')
    (output / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
