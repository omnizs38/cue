#!/usr/bin/env python3
# Reads out/rows.tsv from tools/audit_menus.cpp and measures, per row of every menu, where its
# parts land (screen px and art px): the icon's column, the words' column, the right-hand end
# (value, chevron, switch...), the vertical centre of words and icons in their row, and the air
# around the rules and at the panel's top and bottom.  Prints the inconsistencies.
#   python3 tools/audit_menus.py out [--detail]
import sys, collections

out = sys.argv[1] if len(sys.argv) > 1 else "out"
panels = collections.OrderedDict()
for line in open(out + "/rows.tsv", encoding="utf-8"):
    f = line.rstrip("\n").split("\t")
    if f[0] == "P":
        key = (f[1], f[2])
        panels[key] = dict(s=float(f[3]), w=int(f[4]), h=int(f[5]), x=int(f[6]), y=int(f[7]), W=int(f[8]), H=int(f[9]), rows=[], ink=[])
    elif f[0] == "R":
        key = (f[1], f[2])
        panels[key]["rows"].append(dict(i=int(f[3]), k=f[4], id=int(f[5]), x=int(f[6]), y=int(f[7]), w=int(f[8]), h=int(f[9]), label=f[10], right=f[11], ink=[]))
    elif f[0] == "I":
        key = (f[1], f[2])
        panels[key]["ink"].append(dict(kind=f[3], what=f[4], x=int(f[5]), y=int(f[6]), w=int(f[7]), h=int(f[8]), ax=int(f[9]), ay=int(f[10])))

def owner(p, e):
    cy = e["y"] + e["h"] / 2
    for r in p["rows"]:
        if r["y"] <= cy < r["y"] + r["h"] and r["x"] - 2 <= e["x"] + e["w"] / 2 <= r["x"] + r["w"] + 2:
            return r
    return None

for p in panels.values():
    for e in p["ink"]:
        if e["kind"] == "box":
            continue
        r = owner(p, e)
        if r is not None:
            r["ink"].append(e)

findings = collections.defaultdict(list)  # (what) -> [(tag, scene, detail)]
def note(cat, key, msg):
    findings[cat].append((key[0], key[1], msg))

ICONISH = ("icon", "bitmap", "cells")
for key, p in panels.items():
    s = p["s"]
    tag, scene = key
    rows = p["rows"]
    # right-hand ends per element kind, and the rows' vertical centring
    for r in rows:
        k = r["k"]
        if k in ("Separator", "Preview", "Sample", "Keys", "Brand", "Profile", "Credit", "VSlider", "Fonts"):
            continue
        rc = r["y"] + r["h"] / 2
        texts = [e for e in r["ink"] if e["kind"] in ("text", "mask")]
        icons = [e for e in r["ink"] if e["kind"] in ICONISH]
        # words centred: the capitals' middle (box top + 2 + 3.5 art px) on the row's middle
        for e in texts:
            if e["kind"] != "text":
                continue
            cap_mid = e["ay"] + 5.5 * s
            d = cap_mid - rc
            if k == "Hint":
                continue  # section names sit on their control, by design
            if abs(d) > 0.5 * s + 0.01:
                note("words off centre in their row", key, f"{k} '{e['what'][:24]}' {d:+.1f}px ({d/s:+.2f} art) row h {r['h']}")
        for e in icons:
            if k in ("Tools", "Segmented", "Chips", "SpeedBar", "Picker", "Stepper", "Links"):
                continue
            d = (e["y"] + e["h"] / 2) - rc
            if abs(d) > 0.5 * s + 0.01:
                note("icon off centre in its row", key, f"{k} {e['what']} {d:+.1f}px ({d/s:+.2f} art)")
        # icon and words of one row: the same middle
        if icons and texts and k in ("Option", "Action", "Submenu", "Toggle", "Header", "Stepper", "Hint", "Contacts"):
            ic = min(icons, key=lambda e: e["x"])
            t0 = min((e for e in texts if e["kind"] == "text"), key=lambda e: e["x"], default=None)
            if t0 and ic["x"] < t0["x"]:
                d = (ic["y"] + ic["h"] / 2) - (t0["ay"] + 5.5 * s)
                if abs(d) > 0.5 * s + 0.01:
                    note("icon and words not on one middle", key, f"{k} {ic['what']} vs '{t0['what'][:18]}' {d:+.1f}px ({d/s:+.2f} art)")

def rights(p):
    out = []
    for r in p["rows"]:
        ink = [e for e in r["ink"] if e["kind"] != "box"]
        if not ink or r["k"] in ("Separator", "Preview", "Sample", "Keys", "Brand", "Profile", "Credit"):
            continue
        e = max(ink, key=lambda e: e["x"] + e["w"])
        right = p["w"] - (e["x"] + e["w"])
        # the left-most thing, and where the words start
        left = min(ink, key=lambda e: e["x"])
        words = [e for e in ink if e["kind"] == "text"]
        out.append(dict(r=r, end=e, right=right, left=left["x"], leftkind=left["kind"],
                        words=min((w["ax"] for w in words), default=None)))
    return out

def fmt(v, s):
    return f"{v}px/{v/s:.2f}"

detail = "--detail" in sys.argv
# per panel: the right-hand ends of the rows (what stands at the right edge, and how far in)
for key, p in panels.items():
    s = p["s"]
    rs = rights(p)
    ends = [x for x in rs if x["end"]["x"] > p["w"] / 2]  # things at the right
    vals = collections.Counter(x["right"] for x in ends)
    if len(vals) > 1:
        desc = ", ".join(f"{x['r']['k']}:{x['end']['kind']}:{(x['end']['what'] or '')[:10]}={x['right']}" for x in ends)
        note("right edge differs between rows", key, desc)
    lefts = collections.Counter(x["left"] for x in rs if x["leftkind"] in ICONISH)
    if len(lefts) > 1:
        note("icon column differs between rows", key, ", ".join(f"{x['r']['k']}:{x['left']}" for x in rs if x['leftkind'] in ICONISH))
    words = collections.Counter(x["words"] for x in rs if x["words"] is not None and x["words"] < p["w"] / 3 and x["r"]["k"] not in ("Segmented", "Chips", "Picker", "Fonts", "Info", "SpeedBar", "Links"))
    if len(words) > 1:
        note("words column differs between rows", key, ", ".join(f"{x['r']['k']}:'{x['r']['label'][:12]}'={x['words']}" for x in rs if x["words"] is not None and x["words"] < p["w"] / 3 and x["r"]["k"] not in ("Segmented", "Chips", "Picker", "Fonts", "Info", "SpeedBar", "Links")))
    # the rules: air above and below (ink of the row before and after)
    rows = p["rows"]
    for i, r in enumerate(rows):
        if r["k"] != "Separator":
            continue
        rule = [e for e in p["ink"] if e["kind"] == "fill" and r["y"] <= e["y"] < r["y"] + r["h"] and e["h"] <= s + 1]
        if not rule or i == 0 or i + 1 >= len(rows):
            continue
        ry = rule[0]["y"]
        above = [e for e in rows[i - 1]["ink"] if e["kind"] in ("text",) + ICONISH + ("switch",)]
        below = [e for e in rows[i + 1]["ink"] if e["kind"] in ("text",) + ICONISH + ("switch",)]
        if not above or not below:
            continue
        # by the capitals of the words (a row's real line), not by descenders
        def cap_bottom(e): return e["ay"] + 9 * s if e["kind"] == "text" else e["y"] + e["h"]
        def cap_top(e): return e["ay"] + 2 * s if e["kind"] == "text" else e["y"]
        a = ry - max(cap_bottom(e) for e in above)
        b = min(cap_top(e) for e in below) - (ry + rule[0]["h"])
        if abs(a - b) > s + 0.01:
            note("air around a rule uneven", key, f"rule {i}: above {fmt(a, s)} below {fmt(b, s)} ({rows[i-1]['k']} / {rows[i+1]['k']})")
    # top and bottom: from the panel's edge to the first capitals / last ink
    ink_rows = [r for r in rows if r["ink"] and r["k"] not in ("Separator",)]
    if ink_rows:
        first, last = ink_rows[0], ink_rows[-1]
        def captop(r):
            t = [e["ay"] + 2 * s for e in r["ink"] if e["kind"] == "text"]
            return min(t) if t else min(e["y"] for e in r["ink"])
        def capbot(r):
            t = [e["ay"] + 9 * s for e in r["ink"] if e["kind"] == "text"]
            o = [e["y"] + e["h"] for e in r["ink"] if e["kind"] not in ("text",)]
            return max(t + o)
        top = captop(first); bot = p["h"] - capbot(last)
        # the side: from the panel's edge to the first ink of the rows
        side = min((e["x"] for r in rows for e in r["ink"]), default=0)
        p["frame"] = (top, bot, side)
        if abs(top - bot) > 2 * s + 0.01:
            note("air at top and bottom of the panel uneven", key, f"top {fmt(top, s)} bottom {fmt(bot, s)} side {fmt(side, s)} (first {first['k']}, last {last['k']})")

cats = sorted(findings, key=lambda c: -len(findings[c]))
for c in cats:
    items = findings[c]
    scenes = collections.Counter(x[1] for x in items)
    print(f"\n## {c}: {len(items)} (menus: {len(scenes)})")
    shown = set()
    for tag, scene, msg in items:
        k = (scene, msg.split("(")[0] if c != 'right edge differs between rows' else msg)
        if k in shown and not detail:
            continue
        shown.add(k)
        if not detail and len(shown) > 40:
            break
        print(f"  {tag:18} {scene:22} {msg}")
