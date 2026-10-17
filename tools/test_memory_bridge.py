"""Exercise the actual generated WA bridge in VHDL and native C++.

Twelve selections per case cover sequence wraparound, payload retention,
stale and delayed acknowledgements, data/key parity, held select and halt.
VHDL additionally resets a pending request (native SPECIAL statics have no
runtime reset API). No live CPU, compiler emitter or primitive is modified.
"""
import argparse
import hashlib
import json
import pathlib
import re
import subprocess


def wire_bench(template, generated):
    header = (generated / '360_wa.vhd').read_text().split('end WA;', 1)[0]
    ports = re.findall(r'^\s+(\w+)\s*:\s*(in|out|buffer)\s+(STD_LOGIC(?:_VECTOR\s*\([^\n;]+\))?)', header, re.M | re.I)
    mapping = {'clk': 'clk', 'rst': 'rst', 'hlt': 'hlt', 'P_reg_se_resp': 'response',
               'P_reg_se_rdata_hi': 'rdata(0 to 31)', 'P_reg_se_rdata_lo': 'rdata(32 to 63)',
               'P_reg_se_cmd': 'command', 'P_reg_se_addr': 'address',
               'P_reg_se_wdata_hi': 'write_hi', 'P_reg_se_wdata_lo': 'write_lo',
               'P_sdbo': 'sdbo', 'P_sdbo_parity': 'parity', 'M_accept_se_1': 'accept_n',
               'P_advance_sdbo': 'advance', 'P_key_advance': 'key_advance',
               'P_out_key': 'out_key', 'P_out_key_parity': 'key_parity',
               'MA_P_sab': 'sab', 'MB_P_in_key': 'in_key', 'MB_P_mark': 'marks', 'MB_P_sdbi': 'wdata',
               'MC_P_cancel': 'mode(3)', 'MC_P_insert_key': 'mode(2)', 'MC_P_set_key': 'mode(1)',
               'MC_P_store_to_se': 'mode(0)', 'MC_P_test_and_set': 'mode(4)',
               'MC_M_select_se_1_even': 'select_n'}
    bindings = []
    for name, direction, kind in ports:
        root = name.removesuffix('_first')
        if name in mapping:
            value = mapping[name]
        elif direction.lower() == 'in' and root in mapping:
            value = mapping[root]
        elif direction.lower() != 'in':
            value = 'open'
        else:
            value = "(others=>'0')" if 'VECTOR' in kind else "'0'"
        bindings.append(name + '=>' + value)
    assert len(ports) > 90
    return template.replace('  PORT_MAP', '  dut:entity work.WA port map(' + ',\n'.join(bindings) + ');')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--ghdl', type=pathlib.Path, required=True)
    parser.add_argument('--repo', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument('--negative-generated', type=pathlib.Path)
    args = parser.parse_args()
    repo, generated, output, ghdl = (p.resolve() for p in (args.repo, args.generated, args.output, args.ghdl))
    output.mkdir(parents=True, exist_ok=False)
    here = pathlib.Path(__file__).resolve().parent
    template_path = here / 'test_memory_bridge.vhd'
    template = template_path.read_text()
    template = template.replace('std_logic_vector(to_unsigned(((16#A5# + transaction) mod 256) * 16#1000000# + 16#123458# + transaction * 8, 32))',
                                'std_logic_vector(to_unsigned((16#A5# + transaction) mod 256, 8)) & std_logic_vector(to_unsigned(16#123458# + transaction * 8, 24))')
    (output / 'bench.vhd').write_text(wire_bench(template, generated))
    primitives = [repo / 'src/vhdl/cpu_primitives' / name for name in
                  ['doubless200ns.vhd', 'doubletd40ns.vhd', 'doubletd200ns.vhd', 'doubletd210ns.vhd', 'doubletd340ns.vhd']]

    def run(name, command, cwd=output, expected=0):
        with (cwd / (name + '.log')).open('w') as log:
            process = subprocess.run([str(p) for p in command], cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
        (cwd / (name + '.exit')).write_text(str(process.returncode))
        assert (process.returncode == 0) == (expected == 0), (name, (cwd / (name + '.log')).read_text()[-2200:])
        return process.returncode

    run('analyze', [ghdl, '-a', '--std=08', '-fsynopsys', *primitives, generated / '360_wa.vhd', output / 'bench.vhd'])
    native_source = here / 'test_memory_bridge_native.cpp'
    (output / 'native.cpp').write_bytes(native_source.read_bytes())
    (output / '360_wa.cpp').write_bytes((generated / '360_wa.cpp').read_bytes())
    build = ('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\n'
             'cl /nologo /std:c++17 /EHsc /O2 /DSTANDALONE /I"' + str(repo / 'hercules') + '" /I"' + str(generated) +
             '" native.cpp 360_wa.cpp /Fe:native.exe\nexit /b %errorlevel%\n')
    (output / 'build-native.cmd').write_text(build)
    run('build-native', ['cmd', '/c', output / 'build-native.cmd'])
    configs = [dict(flags=f, latency=l, held=False, halt=False, reset=False) for f in range(32) for l in (0, 1, 50, 500)]
    configs += [dict(flags=f, latency=50, held=True, halt=False, reset=False) for f in range(32)]
    configs += [dict(flags=f, latency=1, held=False, halt=True, reset=False) for f in range(32)]
    configs += [dict(flags=f, latency=50, held=False, halt=False, reset=True) for f in (0, 1, 2, 3, 4, 8, 9, 16, 17)]
    configs += [dict(flags=f, latency=5000, held=False, halt=False, reset=False) for f in (0, 1, 4, 17)]
    records = []
    for index, case in enumerate(configs):
        options = ['-gFLAGS=' + str(case['flags']), '-gLATENCY=' + str(case['latency']),
                   '-gHELD_SELECT=' + str(case['held']).lower(), '-gHALT_REPLY=' + str(case['halt']).lower(),
                   '-gRESET_PENDING=' + str(case['reset']).lower()]
        run('vhdl-%03d' % index, [ghdl, '-r', '--std=08', '-fsynopsys', 'test_memory_bridge', *options, '--assert-level=error'])
        native = None
        if not case['reset']:
            native = run('native-%03d' % index, [output / 'native.exe', case['flags'], case['latency'], int(case['held']), int(case['halt'])])
        records.append(dict(case, vhdl_exit=0, native_exit=native))
    negative = None
    if args.negative_generated:
        negative_dir = output / 'negative-baseline'
        negative_dir.mkdir()
        (negative_dir / 'bench.vhd').write_text(wire_bench(template, args.negative_generated.resolve()))
        run('analyze', [ghdl, '-a', '--std=08', '-fsynopsys', *primitives,
                        args.negative_generated.resolve() / '360_wa.vhd', negative_dir / 'bench.vhd'], cwd=negative_dir)
        negative = run('simulation', [ghdl, '-r', '--std=08', '-fsynopsys', 'test_memory_bridge', '--assert-level=error'], cwd=negative_dir, expected=1)
    sources = [template_path, native_source, pathlib.Path(__file__).resolve(), generated / '360_wa.vhd', generated / '360_wa.cpp',
               generated / '360_wa.h', repo / 'hercules/360_struc.h', *primitives]
    manifest = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    result = dict(passed=True, vhdl_configurations=len(records), native_configurations=sum(c['native_exit'] is not None for c in records),
                  selections_per_configuration=12, sequence_wraps_per_unreset_case=3,
                  baseline_rejected=negative is not None and negative != 0, cases=records, source_hashes=manifest,
                  scope='Actual generated WA C++/VHDL and production SPECIAL implementations; interface behavior, not exact half-pass timing or whole CPU equivalence.')
    (output / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({k: v for k, v in result.items() if k not in ('cases', 'source_hashes')}))


if __name__ == '__main__':
    main()
