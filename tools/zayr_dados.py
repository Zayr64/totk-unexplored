"""Patch de dados do TotK Unexplored (zayr).

Uso: python zayr_dados.py <map_data.json> <static.json do objmap-totk> <map_pct.json do radar-totk>

- static.json: https://github.com/zeldamods/objmap-totk  public/game_files/map_summary/MainField/static.json
- map_pct.json: https://github.com/zeldamods/radar-totk   map_pct.json

Regrava o map_data.json no mesmo formato (indent 4, UTF-8, LF).
"""
import json
import math
import sys


def mm3(s, seed=0):
    data = s.encode()
    h = seed
    c1, c2 = 0xcc9e2d51, 0x1b873593
    n = len(data)
    for i in range(0, n // 4 * 4, 4):
        k = int.from_bytes(data[i:i + 4], 'little')
        k = (k * c1) & 0xffffffff
        k = ((k << 15) | (k >> 17)) & 0xffffffff
        k = (k * c2) & 0xffffffff
        h ^= k
        h = ((h << 13) | (h >> 19)) & 0xffffffff
        h = (h * 5 + 0xe6546b64) & 0xffffffff
    tail = data[n // 4 * 4:]
    k = 0
    for j, b in enumerate(tail):
        k |= b << (8 * j)
    if tail:
        k = (k * c1) & 0xffffffff
        k = ((k << 15) | (k >> 17)) & 0xffffffff
        k = (k * c2) & 0xffffffff
        h ^= k
    h ^= n
    h ^= h >> 16
    h = (h * 0x85ebca6b) & 0xffffffff
    h ^= h >> 13
    h = (h * 0xc2b2ae35) & 0xffffffff
    h ^= h >> 16
    return h


def pos(t):
    return {"x": round(t[0], 2), "y": round(t[1], 2), "z": round(t[2], 2)}


def main(map_path, static_path, pct_path):
    data = json.load(open(map_path, encoding='utf-8'))
    markers = json.load(open(static_path, encoding='utf-8'))['markers']
    pct = json.load(open(pct_path, encoding='utf-8'))
    pct_flags = {e['flag'] for v in pct.values() for e in v}

    # 1. Cavernas, poços e abismos: cada entrada conta com a flag da caverna E a da própria entrada.
    cave_markers = markers['Cave'] + markers['Chasm']
    used = set()
    for cat in ('caves', 'wells', 'chasms'):
        for e in data[cat]:
            name = e['internal_name']
            cands = [m for m in cave_markers if m['MessageID'] == name and m['hash_id'] not in used]
            p = (e['position']['x'], e['position']['y'], e['position']['z'])
            m = min(cands, key=lambda m: math.dist(p, (m['Translate']['X'], m['Translate']['Y'], m['Translate']['Z'])))
            d = math.dist(p, (m['Translate']['X'], m['Translate']['Y'], m['Translate']['Z']))
            assert d < 3.0, (name, d)  # o abismo de Rito Village fica a 2,5 m
            assert e['completion_hash'] == mm3('IsVisitLocation.' + name), name
            used.add(m['hash_id'])
            entrance = str(int(m['hash_id'], 16))
            flag = 'IsVisitLocation.%s && IsVisitLocationArea_CaveEntrance.%s' % (name, entrance)
            assert flag in pct_flags, flag
            e.pop('entrance_id', None)
            e.pop('completion_hash2', None)
            e['entrance_id'] = entrance
            # Ancient Zora Waterworks: a flag da entrada não liga nem no save 100% (romfs/100.sav).
            if name != 'ZoraZonauTerminal':
                e['completion_hash2'] = mm3('IsVisitLocationArea_CaveEntrance.' + entrance)

    # 2. Locais que faltavam: torres (visita + lançamento), tabuletas antigas e templos.
    have = {e['completion_hash'] for e in data['locations']}
    added = []
    for e in sorted(pct['Towers'], key=lambda e: e['name']):
        tower = e['name']
        cannon = 'IsOpenCannon.' + tower
        assert cannon in pct_flags, cannon
        added.append({"completion_hash": mm3(e['flag']), "completion_hash2": mm3(cannon),
                      "display_name": e['ui_name'], "position": pos(e['Trans'][0])})
    for e in sorted(pct['Zonau Reliefs'], key=lambda e: e['name']):
        num = e['name'].rsplit('_', 1)[1]
        added.append({"completion_hash": mm3(e['flag']),
                      "display_name": "Ancient Tablet %s (show to Wortsworth)" % num, "position": pos(e['Trans'][0]),
                      "layer": "Sky"})  # todas em ilhas do céu (Sky__SkyIsland_Monument_*), duas abaixo de 750 m
    for e in sorted(pct['Large Dungeons'], key=lambda e: e['name']):
        added.append({"completion_hash": mm3(e['flag']), "display_name": e['ui_name'], "position": pos(e['Trans'][0])})
    for a in added:
        if a['completion_hash'] not in have:
            data['locations'].append(a)

    # 3. Koroks na camada errada pela regra da altura (céu >= 750): camada do marcador do jogo.
    korok_layer = {m['hash_id']: m['map_name'] for m in markers['Korok']}
    for cat in ('hidden_koroks', 'carry_koroks'):
        for e in data[cat]:
            e.pop('layer', None)
            y = e['position']['y']
            by_height = 'Sky' if y >= 750 else ('Depths' if y < -50 else 'Surface')
            real = korok_layer[e['internalHash']]
            if real != by_height:
                e['layer'] = real

    with open(map_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(data, indent=4, ensure_ascii=False))


if __name__ == '__main__':
    main(*sys.argv[1:4])
