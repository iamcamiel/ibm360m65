"""Inventory direct CPU storage writes in this repository's stripped S/370 core.

This is a source audit, not a proof of runtime behavior. The native regression
fixture checks actual write records; shared channel/DMA and startup stores are
deliberately outside CPU instruction comparison. New unclassified direct writers
cause this audit to fail instead of silently disappearing from the inventory.
"""
from pathlib import Path
import json
import re
from test_hercules_write_recording import function_source

ROOT = Path(__file__).resolve().parents[1]
FILES = ('general1.c', 'general2.c', 'control.c', 'decimal.c', 'float.c', 'io.c')
HELPERS = {
    'vstore.h': ('vstorec', 'vstoreb', 'vstore2', 'vstore4', 'vstore8', 'vstore8_full', 'move_chars'),
    'cpu.c': ('store_psw', 'perform_io_interrupt', 'perform_mck_interrupt'),
    'external.c': ('external_interrupt', 'perform_external_interrupt', 'store_status'),
    'machchk.c': ('sync_mck_interrupt',),
    'decimal.c': ('store_decimal',),
}
EXCLUSIONS = {
    ('control.c', 'load_program_status_word'): 'STORE_DW writes a local PSW buffer, not main storage.',
    ('general1.c', 'execute'): 'memcpy writes the local executed-instruction buffer.',
    ('io.c', 'start_io'): 'Stores initialize a local channel operation request block.',
    ('cpu.c', 'perform_io_interrupt'): 'CSW comes from the shared Hercules channel; CPU old PSW is separately recorded.',
}


def clean(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)


def main():
    rows = []
    missing = []
    for filename in dict.fromkeys((*FILES, *HELPERS)):
        text = (ROOT / 'hercules' / filename).read_text()
        names = [(name, f'DEF_INST({name})') for name in re.findall(r'\bDEF_INST\((\w+)\)', clean(text))]
        names += [(name, f'ARCH_DEP({name})') for name in HELPERS.get(filename, ())]
        for name, marker in names:
            body = clean(function_source(filename, marker))
            direct = bool(re.search(r'MADDR\s*\([^;]*ACCTYPE_WRITE|(?:STORE_(?:HW|FW|DW|W)|store_(?:hw|fw|dw)|memcpy|memset)\s*\(', body))
            hooks = len(re.findall(r'\b(?:record_herc_write(?:_char)?|RECORD_HERC_ABSOLUTE_FIELD)\s*\(', body))
            delegates = sorted(set(re.findall(r'ARCH_DEP\((vstore\w+|store_decimal|move_chars|store_psw|store_status)\)', body)))
            delegates = [callee for callee in delegates if callee != name]
            if not (direct or delegates):
                continue
            exclusion = EXCLUSIONS.get((filename, name))
            if direct and not hooks and not delegates and not exclusion:
                missing.append(f'{filename}:{name}')
            rows.append({'file': filename, 'function': name, 'direct_write_candidate': direct,
                         'recording_sites': hooks, 'delegates': delegates,
                         'shared_or_local_reason': exclusion})
    result = {'scope': 'Current stripped S/370 CPU instruction and interruption paths; shared channel/DMA, IPL/reset and operator alterations are separate.',
              'unclassified_direct_writers': missing, 'paths': rows}
    output = ROOT / 'gen' / 'hercules-write-audit.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(rows)} write paths inventoried; {len(missing)} unclassified direct writers. {output}')
    if missing:
        raise SystemExit('\n'.join(missing))


if __name__ == '__main__':
    main()
