"""Check every generated Boolean second pass against the C++ snapshot equations.

This checks finite NOCLOCK composition, including foreign-section first-state
references and sampled external inputs. SPECIAL behavior and physical timing
require separate simulation and routed analysis.
"""
import argparse,json,re
from pathlib import Path
from audit_ald_logic import parse,normalize_cpp,normalize_vhdl

def audit(directory):
    records={}
    for cpp in sorted(directory.glob('*.cpp')):
        source=cpp.read_text()
        section=re.search(r'void process_(\w+)\(\)',source)[1]
        noclock=source.split(f'void process_{section}()',1)[1].split(f'void process_{section}_clock()',1)[0]
        equations={normalize_cpp(t,section).lower():parse(normalize_cpp(e,section))
            for t,e in re.findall(r'\s+(newstate\.[\w.]+) = ([^;]+);',noclock)}
        vhdl=cpp.with_suffix('.vhd').read_text()
        second=vhdl.split('-- ALD_NOCLOCK_SECOND_BEGIN',1)[1].split('-- ALD_NOCLOCK_SECOND_END',1)[0]
        second=re.sub(r'--[^\n]*','',second)
        actual={normalize_vhdl(t).lower():(parse(normalize_vhdl(e),vhdl=True),initial)
            for t,e,initial in re.findall(r"\s+(\w+(?:\(\d+\))?)\s+<=\s*(.*?) when ald_settle_active = '1' else '([01])';",second)}
        sampled=set(re.findall(r'\b(\w+)_sampled <=',vhdl))
        records[section]=(equations,actual,{n.lower() for n in sampled})
    differences=[]; checked=0
    if not records:
        differences.append({'reason':'No generated C++ sections found'})
    def split_signal(name):
        parts=name.split('[',1)
        return parts[0], ('['+parts[1] if len(parts)>1 else '')
    for section,(equations,actual,sampled) in records.items():
        local_first={split_signal(n)[0] for n in equations}
        foreign_first=set()
        for owner,(other,_,__) in records.items():
            if owner != section:
                foreign_first.update(owner.lower()+'_'+split_signal(n)[0] for n in other)
        def transform(node):
            if isinstance(node,tuple): return tuple([node[0],*[transform(c) for c in node[1:]]])
            base,index=split_signal(node)
            if base in local_first or base in foreign_first: return base+'_first'+index
            if base in sampled: return base+'_sampled'+index
            return node
        # Reparse transformed AST canonicalization since identifier replacement
        # can change commutative operand sorting in the original canonical tree.
        from audit_ald_logic import canonical
        def canonicalize(node):
            return canonical(node[0],*[canonicalize(c) for c in node[1:]]) if isinstance(node,tuple) else node
        for target,expression in equations.items():
            found=actual.get(target)
            expected=canonicalize(transform(expression))
            if found is None or found[0]!=expected or found[1]!=('1' if target.startswith('m_') else '0'):
                differences.append({'section':section,'target':target,'expected':expected,'actual':found})
            else: checked+=1
        for target in actual.keys()-equations.keys():
            differences.append({'section':section,'target':target,'reason':'extra second-pass assignment'})
    if records and not checked and not differences:
        differences.append({'reason':'No Boolean second-pass assignments checked'})
    return {'sections':len(records),'checked_second_pass_assignments':checked,'differences':differences,
            'scope':'Boolean equations and snapshot wiring; SPECIAL timing and whole-CPU hardware equivalence excluded'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('directory',type=Path)
    result=audit(p.parse_args().directory); print(json.dumps(result,indent=2))
    raise SystemExit(bool(result['differences']))
