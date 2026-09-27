# -*- coding: utf-8 -*-
"""Pull the protocol facts straight out of the firmware so the document cannot drift.

Reads the parameter id list from sb_const.h, then walks the write dispatch
(switch at 6516) and the read dispatch (switch at 7529) recording which ids each one
handles and, for reads, the expression that produces the value - which is where the
scaling lives.  Tracks the #if DEVICE_MODE guards so each id can be attributed to the
build it belongs to.
"""
import io, os, re, json

ROOT = r"D:\TechnicalData\Yagneshbhai\PRODUCT_CODE\xmegabased\ReleaseCode\SEGLED_MM32F0140_EMS_3DP"


def rd(f):
    return io.open(os.path.join(ROOT, f), encoding='utf-8',
                   errors='surrogateescape', newline='').read()


NL = '\r\n'
const = rd('sb_const.h')
main = rd('main.c')

# ---------------------------------------------------------------- id list
ids = {}
for m in re.finditer(r'^#define\s+([A-Z0-9_]*_ID)\s+(0x[0-9A-Fa-f]+)', const, re.M):
    name, val = m.group(1), int(m.group(2), 16)
    ids.setdefault(val, name)

print("parameter ids declared: %d" % len(ids))

lines = main.split(NL)


def walk(start_line, stop_at_depth_zero=True):
    """Collect `case X_ID:` occurrences inside a switch, with the mode guard in force
    and, where present, the expression assigned to tempshort."""
    out = []
    guard = []          # stack of 'DP3' / 'TEMPRH' / None
    depth = 0
    started = False
    pending = []        # case labels seen since the last statement

    for i in range(start_line, len(lines)):
        l = lines[i]
        s = l.strip()

        if s.startswith('#if'):
            if 'DEVICE_MODE==DP1_DP2_DP3_MODE' in s:
                guard.append('DP3')
            elif 'DEVICE_MODE==DP1_TEMP_RH_MODE' in s:
                guard.append('TEMPRH')
            else:
                guard.append(None)
            continue
        if s.startswith('#else'):
            if guard:
                top = guard[-1]
                guard[-1] = 'TEMPRH' if top == 'DP3' else ('DP3' if top == 'TEMPRH' else None)
            continue
        if s.startswith('#endif'):
            if guard:
                guard.pop()
            continue

        depth += l.count('{') - l.count('}')
        if not started and depth > 0:
            started = True
        if started and depth <= 0:
            break

        mc = re.match(r'case\s+([A-Z0-9_]*_ID)\s*:', s)
        if mc:
            g = next((x for x in reversed(guard) if x), None)
            pending.append((mc.group(1), g))
            rest = s[mc.end():].strip()
            if rest:
                em = re.search(r'tempshort\s*=\s*([^;]+);', rest)
                expr = em.group(1).strip() if em else None
                for nm, gg in pending:
                    out.append((nm, gg, expr))
                pending = []
            continue

        if pending:
            em = re.search(r'tempshort\s*=\s*([^;]+);', s)
            if em:
                for nm, gg in pending:
                    out.append((nm, gg, em.group(1).strip()))
                pending = []
            elif s and not s.startswith('//') and not s.startswith('#'):
                for nm, gg in pending:
                    out.append((nm, gg, None))
                pending = []
    for nm, gg in pending:
        out.append((nm, gg, None))
    return out


def find_switch(branch_head, nth=1):
    """Locate the nth `switch(RxBuffer[3])` after a command branch head.

    Hard-coded line numbers were wrong the moment main.c grew: the extractor
    silently reported zero readable parameters and produced a document to match.

    The branch head has to be the `else if (...)` that opens the command's block -
    PARA_WRITE_CMD also appears in the COM-disable gate well before it. And the write
    branch holds two of these switches: the first parses the value out of the frame,
    the second is the dispatch, which is the one that says what is writable.
    """
    anchor = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith('else if') and branch_head in s:
            anchor = i
            break
    assert anchor is not None, "could not find the branch head for %r" % branch_head

    seen = 0
    for j in range(anchor, len(lines)):
        if 'switch(RxBuffer[3])' in lines[j]:
            seen += 1
            if seen == nth:
                return j
    raise AssertionError("fewer than %d switches after %r" % (nth, branch_head))


w_at = find_switch('RxBuffer[2]==PARA_WRITE_CMD', nth=2)
r_at = find_switch('RxBuffer[2]==PARA_READ_CMD', nth=1)
print("write dispatch switch at line %d, read dispatch switch at line %d"
      % (w_at + 1, r_at + 1))

writes = walk(w_at)
reads = walk(r_at)

wmap = {}
for nm, g, _ in writes:
    wmap.setdefault(nm, g)
rmap = {}
for nm, g, e in reads:
    if nm not in rmap or rmap[nm][1] is None:
        rmap[nm] = (g, e)

print("write dispatch handles %d ids" % len(wmap))
print("read  dispatch handles %d ids" % len(rmap))

rows = []
for val in sorted(ids):
    nm = ids[val]
    r = rmap.get(nm)
    w = nm in wmap
    guard = (r[0] if r else None) or wmap.get(nm)
    rows.append({
        'hex': '0x%02X' % val,
        'name': nm,
        'read': bool(r),
        'write': w,
        'expr': (r[1] if r else None),
        'mode': guard,
    })

# A dispatch that handles almost nothing means the walk missed its switch. Fail
# loudly rather than writing a snapshot that would quietly gut the document.
assert len(rmap) > 50, "read dispatch yielded only %d ids - the walk went wrong" % len(rmap)
assert len(wmap) > 40, "write dispatch yielded only %d ids - the walk went wrong" % len(wmap)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ids.json')
io.open(out, 'w', encoding='utf-8').write(json.dumps(rows, indent=1))
print("wrote %s" % out)
print()
print("%-6s %-26s %-5s %-5s %-8s %s" % ("id", "name", "read", "write", "mode", "value expression"))
for r in rows:
    print("%-6s %-26s %-5s %-5s %-8s %s" % (
        r['hex'], r['name'], 'yes' if r['read'] else '-', 'yes' if r['write'] else '-',
        r['mode'] or 'both', r['expr'] or ''))
