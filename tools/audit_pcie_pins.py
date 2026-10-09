"""Check implemented PCIe locations against the XUPV5/ML505 connector.

AF24 is the legacy design's presence-derived reset input, not PCIe PERST#.
Check XDL package pins as well when an existing routed XDL is supplied; this
tool never launches another routed-netlist conversion.
"""
from __future__ import print_function
from optparse import OptionParser
import json
import re
import sys

GT = 'pcie/ep/pcie_ep0/pcie_blk/SIO/.pcie_gt_wrapper_i/GTD[0].GT_i'
EXPECTED_PCF = {
    'sys_clk_p': 'AF4',
    'sys_clk_n': 'AF3',
    'sys_reset_n': 'AF24',
    GT: 'GTP_DUAL_X0Y2',
}
EXPECTED_XDL = {
    'sys_clk_p': 'AF4', 'sys_clk_n': 'AF3',
    'sys_reset_n': 'AF24',
    'pci_exp_txp': 'AD2', 'pci_exp_txn': 'AE2',
    'pci_exp_rxp': 'AE1', 'pci_exp_rxn': 'AF1',
    GT: 'GTP_DUAL_X0Y2',
}

def read(path):
    with open(path) as stream:
        return stream.read()

def check(kind, actual, expected):
    return [dict(source=kind, signal=name, expected=site,
                 actual=actual.get(name), matched=actual.get(name) == site)
            for name, site in sorted(expected.items())]

def main():
    parser = OptionParser(description=__doc__)
    parser.add_option('--pcf')
    parser.add_option('--xdl')
    parser.add_option('--pad')
    parser.add_option('--json')
    args, extra = parser.parse_args()
    if not args.pcf or extra:
        parser.error('--pcf is required; no positional arguments are accepted')
    pcf = dict(re.findall(r'COMP\s+"([^"]+)"\s+LOCATE\s*=\s*SITE\s+"([^"]+)"', read(args.pcf)))
    checks = check('pcf', pcf, EXPECTED_PCF)
    if args.xdl:
        xdl = dict(re.findall(r'inst\s+"([^"]+)"\s+"[^"]+",\s*placed\s+\S+\s+(\S+)\s*,', read(args.xdl)))
        checks += check('routed_xdl', xdl, EXPECTED_XDL)
    if args.pad:
        pads = {}
        for line in read(args.pad).splitlines():
            fields = line.split('|')
            if len(fields) >= 3 and fields[1].strip():
                pads[fields[1].strip()] = fields[0].strip()
        expected = dict((name, site) for name, site in EXPECTED_XDL.items() if name != GT)
        checks += check('routed_pad', pads, expected)
    result = dict(board='XUPV5/ML505 PCIe connector',
                  passed=all(c['matched'] for c in checks), checks=checks,
                  reset_note='AF24 is legacy presence-derived reset; PERST# is available on the board CPLD.',
                  hardware_link_verified=False)
    output = json.dumps(result, indent=2, sort_keys=True)
    if args.json:
        with open(args.json, 'w') as stream:
            stream.write(output + '\n')
    print(output)
    return 0 if result['passed'] else 1

if __name__ == '__main__':
    sys.exit(main())
