#!/usr/bin/env python3
"""Sample a frozen PSP guest's PCs and N64 counters via PPSSPP's debugger.

These are samples in PPSSPP's emulated clock, not hardware CPU/GPU timings.
"""
import bisect
from collections import Counter
import hashlib
import math
from pathlib import Path
import re
import subprocess


def load_symbols(elf, nm):
    listing = subprocess.check_output([str(nm), '-S', '-n', '-C', str(elf)], text=True)
    symbols = []
    addresses = {}
    for line in listing.splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]+)\s+(?:([0-9a-fA-F]+)\s+)?([A-Za-z])\s+(.+)', line)
        if match:
            start, size, kind, name = match.groups()
            addresses[name] = int(start, 16)
            if kind in 'tT': symbols.append((int(start, 16), int(size or '0', 16), name))
    symbols.sort()
    # Handwritten assembly has labels but no ELF .size. Bound those labels by
    # the next text symbol instead of incorrectly reporting them as JIT code.
    symbols = [(start, size or (symbols[i+1][0]-start if i+1<len(symbols) else 0), name)
               for i, (start, size, name) in enumerate(symbols)]
    return symbols, addresses


def sample(debugger, elf, nm, seconds, interval):
    symbols, addresses = load_symbols(elf, nm)
    starts = [s[0] for s in symbols]
    modules = debugger.request('hle.module.list')['modules']
    module = next(m for m in modules if m['name'].startswith('FrogN64'))
    base = module['address']

    def symbol(pc):
        relative = pc - base
        pos = bisect.bisect_right(starts, relative) - 1
        if pos >= 0 and relative < symbols[pos][0] + symbols[pos][1]: return symbols[pos][2]
        return 'dynamic code / external module'

    def word(name, offset=0):
        reply = debugger.request('memory.read_u32', address=base + addresses[name] + offset)
        return reply['value']

    initial = debugger.request('cpu.status')
    frames = word('gRDPFrame'); vis = word('gVerticalInterrupts')
    rows = []
    # Avoid sampling at the same phase of every 60 Hz frame.
    elapsed = 0.0
    for index in range(math.ceil(seconds / interval)):
        delta = min(interval * (1 + .15 * math.sin(index * 2.399963)), seconds - elapsed)
        if delta <= 0: break
        status = debugger.run(delta); elapsed += delta
        name = symbol(status['pc'])
        row = {'us':status['us'], 'psp_pc':status['pc'], 'symbol':name, 'n64_pc':word('gCPUState', 0x280)}
        if name in ('memmove', 'memcpy'):
            ra = debugger.request('cpu.getReg', name='ra')['uintValue']
            row.update(caller_pc=ra-8, caller_symbol=symbol(ra-8))
        rows.append(row)
    final = debugger.request('cpu.status')
    duration = (final['us'] - initial['us']) / 1e6
    lists = (word('gRDPFrame') - frames) & 0xffffffff
    interrupts = (word('gVerticalInterrupts') - vis) & 0xffffffff
    return {'elf_sha256':hashlib.sha256(Path(elf).read_bytes()).hexdigest(), 'module':module,
        'seconds':duration, 'display_lists':lists, 'display_lists_per_emulated_second':lists/duration,
        'vi_interrupts':interrupts, 'samples':len(rows),
        'top_symbols':Counter(x['symbol'] for x in rows).most_common(30),
        'copy_callers':Counter(x['caller_symbol'] for x in rows if 'caller_symbol' in x).most_common(20),
        'top_n64_pcs':Counter(hex(x['n64_pc']) for x in rows).most_common(30), 'rows':rows}
