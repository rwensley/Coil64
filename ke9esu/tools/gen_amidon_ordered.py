#!/usr/bin/env python3
"""KE9ESU fork of Coil64 (GPL-3.0-or-later).
Convert the standalone designer's amidon_data.json (built from Coil64 amidon.cpp + amidoncorp.com pages by
ke9esu_balun_designer/tools/gen_amidon.py) into ke9esu/data/amidon_ordered.json: the same data, with every object
turned into an ordered [key, value] list in JavaScript Object.keys() order (integer-like keys ascending first,
then insertion order). The C++ port iterates in that order so the Amidon core list, duplicate handling and the
Fair-Rite/Micrometals cross-reference are identical to the web app (QJsonObject would sort keys alphabetically)."""
import json, sys, os
src = sys.argv[1] if len(sys.argv) > 1 else '/workspace/ke9esu_balun_designer/amidon_data.json'
d = json.load(open(src, encoding='utf-8'))
def js_keys(o):
    ints = sorted([k for k in o if k.isdigit() and (k == '0' or not k.startswith('0'))], key=int)
    return ints + [k for k in o if k not in ints]
out = {"source": d.get("source", ""),
       "ferrite": [[k, d["ferrite"][k]] for k in js_keys(d["ferrite"])],
       "iron": [[k, d["iron"][k]] for k in js_keys(d["iron"])],
       "web": [[k, d["web"][k]] for k in js_keys(d["web"])]}
dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'amidon_ordered.json')
json.dump(out, open(dst, 'w', encoding='utf-8'), ensure_ascii=False, separators=(',', ':'))
print('wrote', dst, len(out['ferrite']), 'ferrite mixes,', len(out['iron']), 'iron mixes,', len(out['web']), 'web parts')
