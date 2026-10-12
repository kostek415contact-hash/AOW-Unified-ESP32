#!/usr/bin/env python3
"""Kontrola dokumentacji (EN/PL): linki, pary jezykowe i zgodnosc z config.h.

Sprawdza:
  1. linki wzgledne w kazdym *.md (plik istnieje, a kotwica #naglowek istnieje w pliku docelowym; zewnetrzne http(s) pomija),
  2. pary jezykowe: docs/en/X.md <-> docs/pl/X.md oraz README.md <-> README.pl.md istnieja, linkuja do siebie
     i maja ten sam szkielet (liczba naglowkow kazdego poziomu, wierszy tabel, blokow kodu, obrazkow),
  3. zgodnosc z config/config.h: piny i porty z tabel w docs/<lang>/pinout.md.

Uzycie (z katalogu glownego repo):  python3 tools/docs_check.py      (kod wyjscia 0 = OK)
"""
import os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SKIP_DIRS = {'.git', 'build', 'node_modules', '__pycache__'}
errors = []

def err(msg): errors.append(msg)
def read(p):
    with open(p, encoding='utf-8') as f: return f.read()
def rel(p): return os.path.relpath(p, ROOT).replace(os.sep, '/')

def md_files():
    out = []
    for d, dirs, files in os.walk(ROOT):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        out += [os.path.join(d, f) for f in files if f.endswith('.md')]
    return sorted(out)

def strip_code(text):
    """Usuwa bloki ```...``` (linki i naglowki w kodzie nie sa dokumentacja) - zostawia puste linie, by numeracja sie zgadzala."""
    out, fenced = [], False
    for line in text.split('\n'):
        if line.lstrip().startswith('```'):
            fenced = not fenced; out.append(''); continue
        out.append('' if fenced else line)
    return '\n'.join(out)

def slug(heading):
    """Kotwica w stylu GitHuba: male litery, bez znakow specjalnych, spacje -> '-'."""
    h = re.sub(r'`|\*|_(?=\S)|\[|\]', '', heading.strip().lower())
    h = re.sub(r'[^\w\- ]', '', h, flags=re.UNICODE)
    return h.replace(' ', '-')

_anchor_cache = {}
def anchors(path):
    if path not in _anchor_cache:
        seen, res = {}, set()
        for line in strip_code(read(path)).split('\n'):
            m = re.match(r'^(#{1,6})\s+(.*?)\s*#*\s*$', line)
            if m:
                s = slug(m.group(2)); n = seen.get(s, 0); seen[s] = n + 1
                res.add(s if n == 0 else '%s-%d' % (s, n))
        for m in re.finditer(r'<a\s+(?:name|id)="([^"]+)"', read(path)): res.add(m.group(1))
        _anchor_cache[path] = res
    return _anchor_cache[path]

LINK = re.compile(r'!?\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)')
def check_links(files):
    n = 0
    for f in files:
        for ln, line in enumerate(strip_code(read(f)).split('\n'), 1):
            for m in LINK.finditer(line):
                t = m.group(1)
                if re.match(r'^[a-z][a-z0-9+.-]*:', t, re.I): continue   # http(s), mailto...
                n += 1
                path, _, frag = t.partition('#')
                target = f if path == '' else os.path.normpath(os.path.join(os.path.dirname(f), path))
                if not os.path.exists(target):
                    err('%s:%d: link do nieistniejacego pliku: %s' % (rel(f), ln, t)); continue
                if frag and target.endswith('.md') and frag.lower() not in anchors(target):
                    err('%s:%d: brak kotwicy #%s w %s' % (rel(f), ln, frag, rel(target)))
    return n

def skeleton(path):
    raw = read(path); body = strip_code(raw)
    heads = {}
    for line in body.split('\n'):
        m = re.match(r'^(#{1,6})\s', line)
        if m: heads[len(m.group(1))] = heads.get(len(m.group(1)), 0) + 1
    return {'naglowki': tuple(sorted(heads.items())),
            'wiersze tabel': sum(1 for l in body.split('\n') if l.lstrip().startswith('|')),
            'bloki kodu': raw.count('```') // 2,
            'obrazki': len(re.findall(r'!\[', body))}

def pairs():
    res = [(os.path.join(ROOT, 'README.md'), os.path.join(ROOT, 'README.pl.md'))]
    en_dir, pl_dir = os.path.join(ROOT, 'docs', 'en'), os.path.join(ROOT, 'docs', 'pl')
    names = set(os.listdir(en_dir)) | set(os.listdir(pl_dir))
    for n in sorted(x for x in names if x.endswith('.md')):
        res.append((os.path.join(en_dir, n), os.path.join(pl_dir, n)))
    return res

def check_pairs():
    for en, pl in pairs():
        if not os.path.exists(en): err('brak pary EN dla %s' % rel(pl)); continue
        if not os.path.exists(pl): err('brak pary PL dla %s' % rel(en)); continue
        for a, b in ((en, pl), (pl, en)):
            if os.path.basename(b) not in read(a): err('%s nie linkuje do swojej pary jezykowej %s' % (rel(a), rel(b)))
        se, sp = skeleton(en), skeleton(pl)
        for k in se:
            if se[k] != sp[k]: err('rozjazd EN/PL (%s): %s=%s vs %s=%s' % (k, rel(en), se[k], rel(pl), sp[k]))
    return len(pairs())

def config_values():
    cfg = read(os.path.join(ROOT, 'config', 'config.h'))
    vals = {}
    for m in re.finditer(r'static\s+(?:const\s+)?(?:uint8_t|uint16_t|int)\s+([A-Z0-9_]+(?:_PIN|_PORT))\s*=\s*(\d+)\s*;', cfg):
        vals[m.group(1)] = int(m.group(2))
    return vals

# stala z config.h -> slowo kluczowe wiersza tabeli (wspolne dla EN i PL)
PIN_ROWS = {'GPS_RX_PIN': 'LG290P', 'GPS_TX_PIN': 'LG290P', 'IMU_UART_RX_PIN': 'BNO085', 'IMU_RESET_PIN': 'BNO085',
            'I2C_SDA_PIN': 'ADS1115', 'I2C_SCL_PIN': 'ADS1115',
            'MOTOR_PWM_PIN': 'MD13S', 'MOTOR_DIR_PIN': 'MD13S', 'ETH_CS_PIN': 'W5500', 'ETH_MOSI_PIN': 'W5500',
            'ETH_SCK_PIN': 'W5500', 'ETH_MISO_PIN': 'W5500', 'STEER_BUTTON_PIN': 'GPIO15', 'WORK_SWITCH_PIN': 'GPIO18',
            'STATUS_LED_PIN': 'WS2812'}
PORT_CONSTS = {'AGIO_PORT': None, 'MODULE_LISTEN_PORT': None, 'AUX_PORT': 'AUX_PORT', 'RTCM_LISTEN_PORT': 'RTCM_LISTEN_PORT'}

def check_config():
    vals = config_values(); n = 0
    imu_rst = re.search(r'static\s+int\s+IMU_RESET_PIN\s*=\s*(\d+)', read(os.path.join(ROOT, 'config', 'config.h')))
    if imu_rst: vals['IMU_RESET_PIN'] = int(imu_rst.group(1))
    for lang in ('en', 'pl'):
        p = os.path.join(ROOT, 'docs', lang, 'pinout.md')
        if not os.path.exists(p): continue
        rows = [l for l in read(p).split('\n') if l.lstrip().startswith('|')]
        for const, key in PIN_ROWS.items():
            if const not in vals: err('config.h: nie znaleziono %s (zaktualizuj tools/docs_check.py)' % const); continue
            num = vals[const]; n += 1
            # the number must be written in bold (**11**, **GPIO4**, **0-14**) or as GPIOn in the row of that module
            cand = [r for r in rows if re.search(key, r)]
            if not any(re.search(r'GPIO\s?%d\b|\*\*[^*]*\b%d\b[^*]*\*\*' % (num, num), r) for r in cand):
                err('%s: %s = %d z config.h nie zgadza sie z wierszem "%s"' % (rel(p), const, num, key))
        text = read(p)
        for const, name in PORT_CONSTS.items():
            if const not in vals: err('config.h: nie znaleziono %s' % const); continue
            num = vals[const]; n += 1
            if name:
                row = [r for r in rows if '`%s`' % name in r]
                if not row or not re.search(r'\b%d\b' % num, row[0]): err('%s: port %s = %d z config.h nie zgadza sie z tabela portow' % (rel(p), const, num))
            elif not re.search(r'^\|\s*%d\s*\|' % num, text, re.M): err('%s: port %s = %d z config.h nie wystepuje w tabeli portow' % (rel(p), const, num))
    return n

def main():
    files = md_files()
    nl = check_links(files); npair = check_pairs(); ncfg = check_config()
    if errors:
        print('BLEDY dokumentacji (%d):' % len(errors))
        for e in errors: print('  - ' + e)
        return 1
    print('docs: %d plikow, %d linkow, %d par EN/PL, %d wartosci z config.h - OK' % (len(files), nl, npair, ncfg))
    return 0

if __name__ == '__main__':
    sys.exit(main())
