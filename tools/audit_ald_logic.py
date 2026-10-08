"""Compare the Boolean next-state expressions in generated C++ and VHDL.

This checks the emitters, not clocking, reset, SPECIAL primitives or FPGA timing.
Run after compiling ALDs, e.g. python tools/audit_ald_logic.py gen/ald.
"""
import argparse
import json
import re
from pathlib import Path


def signal_name(match, section):
    owner, field, bit = match.groups()
    owner = owner.removesuffix('_INT')
    prefix = '' if owner in (section, 'EXTERNAL_') else owner + '_'
    name = ('M_' + field[1:]) if field.startswith('_') else ('P_' + field)
    return prefix + name + (f'[{bit}]' if bit is not None else '')


def normalize_cpp(expression, section):
    return re.sub(r'(?:oldstate|newstate)\.([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)(?:\.B(\d+))?',
                  lambda m: signal_name(m, section), expression)


def normalize_vhdl(expression):
    return re.sub(r'([A-Za-z][A-Za-z0-9_]*)\((\d+)\)', r'\1[\2]', expression)


def canonical(op, *children):
    if op == 'not':
        child = children[0]
        return child[1] if isinstance(child, tuple) and child[0] == 'not' else (op, child)
    flattened = []
    for child in children:
        if isinstance(child, tuple) and child[0] == op:
            flattened.extend(child[1:])
        else:
            flattened.append(child)
    return (op, *sorted(flattened, key=repr))


def parse(expression, vhdl=False):
    token_pattern = r"\s*(\&\&|\|\||!=|!|\(|\)|'0'|'1'|[A-Za-z_][A-Za-z0-9_]*(?:\[\d+\])?)"
    tokens, position = [], 0
    while position < len(expression):
        match = re.match(token_pattern, expression[position:])
        if not match:
            raise ValueError(f'Unsupported expression: {expression[position:]}')
        tokens.append(match[1])
        position += match.end()
    index = 0
    ops = {'&&': 'and', '||': 'or', '!=': 'xor', 'and': 'and', 'or': 'or', 'xor': 'xor'}
    precedence = {'and': 1, 'or': 1, 'xor': 1} if vhdl else {'or': 1, 'and': 2, 'xor': 3}

    def atom():
        nonlocal index
        token = tokens[index]
        index += 1
        if token in ('!', 'not'):
            return canonical('not', atom())
        if token == '(':
            result = expression_at(0)
            if tokens[index] != ')':
                raise ValueError('Missing closing parenthesis')
            index += 1
            return result
        if token in ('INT', 'TD', 'TD10NS'):
            return atom()
        return {'true': '1', 'false': '0', "'1'": '1', "'0'": '0'}.get(token, token.lower())

    def expression_at(minimum):
        nonlocal index
        left = atom()
        while index < len(tokens) and tokens[index] in ops:
            op = ops[tokens[index]]
            if precedence[op] < minimum:
                break
            index += 1
            left = canonical(op, left, expression_at(precedence[op] + 1))
        return left

    result = expression_at(0)
    if index != len(tokens):
        raise ValueError('Unconsumed tokens')
    return result


def audit(directory):
    report = {'sections': 0, 'matched_boolean_assignments': 0, 'differences': [], 'unsupported': []}
    for cpp in sorted(directory.glob('*.cpp')):
        source = cpp.read_text()
        section_match = re.search(r'void process_(\w+)\(\)', source)
        if not section_match:
            continue
        section = section_match[1]
        vhdl = re.sub(r'--[^\n]*', '', cpp.with_suffix('.vhd').read_text())
        body = vhdl.split("elsif (hlt='0') then", 1)[1].split('end process;', 1)[0]
        v_clock_body = body.split("if (hclk = '1') then", 1)[1]
        v_clock_targets = {normalize_vhdl(k) for k in re.findall(r'\s+(\w+(?:\(\d+\))?)\s+<=', v_clock_body)}
        v_assignments = dict(re.findall(r'\s+(\w+(?:\(\d+\))?)\s+<=\s+([^;]+);', body))
        v_assignments = {normalize_vhdl(k): v for k, v in v_assignments.items()}
        report['sections'] += 1
        c_body = source.split('void init_', 1)[0]
        c_clock_body = c_body.split(f'void process_{section}_clock()', 1)[1]
        c_clock_targets = {normalize_cpp(k, section) for k in re.findall(r'\s+(newstate\.[\w.]+) =', c_clock_body)}
        for target, expression in re.findall(r'\s+(newstate\.[\w.]+) = ([^;]+);', c_body):
            target = normalize_cpp(target, section)
            other = v_assignments.pop(target, None)
            if other is None:
                report['differences'].append({'section': section, 'target': target, 'reason': 'missing VHDL assignment'})
                continue
            if (target in c_clock_targets) != (target in v_clock_targets):
                report['differences'].append({'section': section, 'target': target, 'reason': 'CLOCK/NOCLOCK phase differs'})
            try:
                c_tree = parse(normalize_cpp(expression, section))
                v_tree = parse(normalize_vhdl(other), vhdl=True)
            except (ValueError, IndexError) as error:
                report['unsupported'].append({'section': section, 'target': target, 'reason': str(error)})
                continue
            if c_tree == v_tree:
                report['matched_boolean_assignments'] += 1
            else:
                report['differences'].append({'section': section, 'target': target, 'cpp': expression, 'vhdl': other})
        for target in v_assignments:
            report['differences'].append({'section': section, 'target': target, 'reason': 'missing C++ assignment'})
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    result = audit(args.directory)
    print(json.dumps(result, indent=2))
    raise SystemExit(bool(result['differences'] or result['unsupported']))
