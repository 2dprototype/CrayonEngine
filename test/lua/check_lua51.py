#!/usr/bin/env python3
"""Fails if any prelude/example Lua file uses syntax or libraries that LuaJIT (Lua 5.1) does not have.
The engine embeds LuaJIT, but tests run on a newer Lua; this closes that gap.   usage: check_lua51.py"""
import glob, re, sys, os
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
files = glob.glob(os.path.join(root, 'src/scripting/prelude/*.lua')) + glob.glob(os.path.join(root, 'examples/2[0-9]_*.lua'))
bad = 0
for f in sorted(files):
    for n, line in enumerate(open(f, encoding='utf-8'), 1):
        code = re.sub(r'--.*$', '', line)
        code = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', '""', code)
        hits = []
        if re.search(r'(?<![/])//(?![/])', code): hits.append('integer division //')
        if re.search(r'\bgoto\b|::\w+::', code): hits.append('goto/label')
        if re.search(r'<const>|<close>', code): hits.append('variable attributes')
        if re.search(r'[^<>=~]>>[^=]|<<|(?<![<>=~])&(?!&)|(?<!\|)\|(?!\|)|~(?!=)', code): hits.append('bitwise operator')
        if re.search(r'\butf8\.|\bmath\.tointeger|\btable\.move|\bstring\.pack|\bmath\.type', code): hits.append('Lua 5.3 library')
        for h in hits:
            print('%s:%d: %s: %s' % (os.path.relpath(f, root), n, h, line.strip()[:100])); bad += 1
print('LuaJIT/5.1 compatibility: %s (%d files scanned)' % ('OK' if bad == 0 else 'FAILED', len(files)))
sys.exit(1 if bad else 0)
