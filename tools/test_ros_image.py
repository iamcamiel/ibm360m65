"""Validate ROS layout, production loading and VHDL address mapping.

Optional --qz checks every word against the independently transcribed QZ JSON.
The C++ fixture runs the production init_ros loading/parity code and dumps its
actual bit arrays and decoded controls. --ghdl also reads every physical ROS
address through ROSMEM, including halt and reset checks.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path
from test_hercules_write_recording import ROOT, VCVARS

FIELDS = ['ND', 'P1', 'W', 'A', 'B', 'C', 'D', 'P2', 'E', 'F', 'G',
          'HW', 'H', 'L', 'NEXT_HI', 'NEXT_MID', 'NEXT_LO', 'K', 'J', 'M',
          'N', 'P', 'EW', 'Q', 'P3', 'R', 'T', 'P4', 'U', 'UA', 'V']
WIDTHS = [1, 1, 4, 4, 2, 5, 3, 1, 4, 6, 5, 2, 5, 4, 4, 4, 2,
          5, 7, 5, 4, 3, 1, 3, 1, 1, 4, 1, 4, 1, 3]
CONTROLS = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'l', 'na', 'k',
            'j', 'm', 'n', 'p', 'q', 'r', 't', 'u', 'v', 'w', 'valid']


def read_word(line):
    tokens = line.split()
    assert len(tokens) == 32, f'Wrong number of field groups: {line}'
    assert len(tokens[0]) == 3, f'Address must occupy three columns: {line}'
    for name, width, bits in zip(FIELDS, WIDTHS, tokens[1:]):
        assert len(bits) == width and set(bits) <= {'0', '1'}, (tokens[0], name, bits)
    # Production loader treats each space as a group boundary, not arbitrary whitespace.
    assert line.rstrip() == tokens[0] + ' ' + ' '.join(tokens[1:]), tokens[0]
    return int(tokens[0], 16), dict(zip(FIELDS, tokens[1:])), ''.join(tokens[1:])


def parity(bits):
    groups = [range(2, 43), list(range(43, 69)) + [85],
              list(range(69, 85)) + list(range(86, 100))]
    return [sum(int(bits[i]) for i in group) % 2 for group in groups]


def expected_controls(fields):
    values = {f.lower(): int(fields[f], 2) for f in FIELDS if f.lower() in CONTROLS}
    values.update(e=int(fields['EW'] + fields['E'], 2),
                  h=int(fields['HW'] + fields['H'], 2),
                  u=int(fields['UA'] + fields['U'], 2),
                  na=int(fields['NEXT_HI'] + fields['NEXT_MID'] + fields['NEXT_LO'], 2),
                  valid=int(fields['P1'] == '0'))
    return [values[f] for f in CONTROLS]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--qz', type=Path)
    parser.add_argument('--ghdl', type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    ros_path = ROOT / 'src/ros.txt'
    vhdl_path = ROOT / 'src/vhdl/rosmem.vhd'
    words = {}
    for line in ros_path.read_text().splitlines():
        if not line.strip():
            continue
        address, fields, bits = read_word(line)
        assert address not in words, address
        assert bits[1:] == '1' + '0' * 98 if fields['P1'] == '1' else parity(bits) == [1, 1, 1], address
        words[address] = (fields, bits)
    expected_addresses = {plane * 256 + low for plane in range(16)
                          for low in list(range(0x58)) + list(range(0x80, 0xd8))}
    assert set(words) == expected_addresses and len(words) == 2816
    vhdl_words = re.findall(r'"([01]{100})"\s*,?\s*-- ROS word ([0-9a-fA-F]{3})', vhdl_path.read_text())
    assert len(vhdl_words) == 2816
    for index, (bits, label) in enumerate(vhdl_words):
        address = int(label, 16)
        physical = ((address & 0x07c) << 5) | ((address & 0xf80) >> 5) | (address & 3)
        assert physical == index and words[address][1] == bits, label
    # Semantic regression cases: correct bits alone did not catch the old C boundary.
    assert int(words[0x31c][0]['NEXT_HI'] + words[0x31c][0]['NEXT_MID'] + words[0x31c][0]['NEXT_LO'], 2) << 2 == 0x4a8
    assert words[0x89a][0]['C'] == '01110'
    assert words[0xd97][0]['T'] == '1011' and words[0xd97][0]['R'] == '0'
    assert words[0xe3f][0]['P1'] == '0'
    if args.qz:
        rows = json.loads(args.qz.read_text(encoding='utf-8'))['records']
        assert {int(row['address'], 16) for row in rows} == expected_addresses
        for row in rows:
            assert words[int(row['address'], 16)][1] == row['ND'] + row['ros_bits'], row['address']

    def run(name, command):
        result = subprocess.run([str(v) for v in command], cwd=out, capture_output=True, text=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        print(name, result.returncode, (result.stdout + result.stderr)[-1000:])
        result.check_returncode()
        return result

    # Extract current production definitions, excluding the later CPU-state reset.
    header = (ROOT / 'hercules/360_struc.h').read_text()
    struct = re.search(r'typedef struct __ROS\s*\{.*?\}\s*_ROS;', header, re.S)[0]
    cpp = (ROOT / 'hercules/360_ros.cpp').read_text()
    start = cpp.index('extern "C" void init_ros()')
    loader = cpp[start:cpp.index('\n        /*', start)] + '\n}\n'
    dump = ' '.join(f'fprintf(dump, "%d ", int(rw.{field}));' for field in CONTROLS)
    source = '''
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#define D_fprintf fprintf
STRUCT
_ROS ros[4096] = {};
FILE *lf;
LOADER
int main() {
 lf=fopen("loader.log","w"); init_ros(); fclose(lf);
 FILE *dump=fopen("loaded_words.txt","w");
 for(int a=0;a<4096;++a) {
  if ((a&255)>0x57 && ((a&255)<0x80 || (a&255)>0xd7)) continue;
  _ROS &rw=ros[a]; fprintf(dump,"%03X ",a);
  for(int b=0;b<100;++b) fprintf(dump,"%d",int(rw.bits[b]));
  fprintf(dump," "); DUMP fprintf(dump,"\\n");
 }
 fclose(dump); return 0;
}
'''.replace('STRUCT', struct).replace('LOADER', loader).replace('DUMP', dump)
    (out / 'fixture.cpp').write_text(source)
    shutil.copyfile(ros_path, out / 'ros.txt')
    (out / 'build.cmd').write_text(
        f'@echo off\ncall "{VCVARS}" >nul\n'
        'cl /nologo /std:c++17 /EHsc /W4 /O2 fixture.cpp /Fe:ros_loader.exe\n'
        'if errorlevel 1 exit /b %errorlevel%\n'
        f'cl /nologo /TC /W4 "{ROOT / "tools/test_fpga_version.c"}" /Fe:fpga_compatibility.exe\n'
        'exit /b %errorlevel%\n')
    run('build', ['cmd', '/c', out / 'build.cmd'])
    run('loader', [out / 'ros_loader.exe'])
    for line in (out / 'loaded_words.txt').read_text().splitlines():
        address, bits, *values = line.split()
        fields, wanted = words[int(address, 16)]
        assert bits == wanted and list(map(int, values)) == expected_controls(fields), address
    assert len((out / 'loaded_words.txt').read_text().splitlines()) == 2816
    run('compatibility', [out / 'fpga_compatibility.exe'])
    metadata = (ROOT / 'src/vhdl/pcie/fpga_build.vhd').read_text()
    required = (ROOT / 'hercules/m65_fpga_version.h').read_text()
    fpga_version = int(re.search(r'M65_FPGA_VERSION.*?x"([0-9A-F]+)"', metadata)[1], 16)
    assert fpga_version == int(re.search(r'M65_FPGA_REQUIRED_VERSION\s+0x([0-9A-F]+)U', required)[1], 16)

    if args.ghdl:
        (out / 'expected_ros.txt').write_text(''.join(f'{a:012b} {word[1]}\n' for a, word in sorted(words.items())))
        bench = '''
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_textio.all;
use ieee.numeric_std.all;
use std.textio.all;
use std.env.all;
entity test_ros_image is end;
architecture test of test_ros_image is
 signal clk : std_logic := '0';
 signal rst : std_logic := '1';
 signal hlt : std_logic := '0';
 signal addr : std_logic_vector(0 to 11) := (others=>'0');
 signal data : std_logic_vector(0 to 99);
begin
 clk <= not clk after 5 ns;
 dut: entity work.ROSMEM port map(clk,rst,hlt,addr,data);
 process
  file vectors : text open read_mode is "expected_ros.txt";
  variable row : line;
  variable a : std_logic_vector(0 to 11);
  variable expected,held : std_logic_vector(0 to 99);
  variable count : natural := 0;
 begin
  wait until rising_edge(clk); wait for 1 ns;
  assert data=(data'range=>'0') report "reset" severity failure;
  rst<='0';
  while not endfile(vectors) loop
   readline(vectors,row); read(row,a); read(row,expected); addr<=a;
   wait until rising_edge(clk); wait for 1 ns;
   assert data=expected report "ROS address " & integer'image(to_integer(unsigned(a))) severity failure;
   count:=count+1;
  end loop;
  held:=data; hlt<='1'; addr<=(others=>'0');
  wait until rising_edge(clk); wait for 1 ns;
  assert data=held report "halt did not hold ROS output" severity failure;
  rst<='1';
  wait until rising_edge(clk); wait for 1 ns;
  assert data=(data'range=>'0') report "reset must override halt" severity failure;
  assert count=2816 severity failure;
  report "ROSMEM: 2816 addresses, halt and reset passed";
  stop; wait;
 end process;
end;
'''
        (out / 'test_ros_image.vhd').write_text(bench)
        run('ghdl-analyze', [args.ghdl.resolve(), '-a', '--std=08', vhdl_path, out / 'test_ros_image.vhd'])
        run('ghdl-run', [args.ghdl.resolve(), '-r', '--std=08', 'test_ros_image', '--assert-level=error'])
    result = {'passed': True, 'addresses': 2816, 'canonical_groups': True,
              'production_loader_bits_and_controls_match': True, 'QZ_matches': bool(args.qz),
              'VHDL_bits_and_address_permutation_match': True,
              'VHDL_all_address_simulation_halt_reset': bool(args.ghdl),
              'FPGA_revision': f'{fpga_version >> 16}.{fpga_version & 65535}',
              'source_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                                [ros_path, vhdl_path, ROOT / 'hercules/360_ros.cpp']}}
    (out / 'result.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
