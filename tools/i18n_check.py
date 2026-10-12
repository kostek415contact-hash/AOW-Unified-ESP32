#!/usr/bin/env python3
"""Kontrola tłumaczeń panelu WWW.

Teksty panelu mają NEUTRALNE KLUCZE (np. 'log_in_to_the_panel'), nie polskie ani angielskie zdania:
  JS:    TL('klucz', {param})     liczba mnoga: plN(n, 'klucz')
  HTML:  data-i18n="klucz"  data-i18n-html="klucz"  data-i18n-attr="atrybut:klucz"
Katalog języka to web/lang/lang_<kod>.h z obiektem JSON  "klucz": "tekst". Językiem odniesienia jest angielski (lang_en.h).

Użycie (z katalogu głównego repo):
  python3 tools/i18n_check.py              sprawdza wszystkie języki (kod wyjścia 0 = OK)
  python3 tools/i18n_check.py --keys       wypisuje klucze użyte w panelu
  python3 tools/i18n_check.py --sync de    dopisuje do web/lang/lang_de.h brakujące klucze (z tekstem angielskim do przetłumaczenia)
  python3 tools/i18n_check.py --add-log    dopisuje nowe komunikaty dziennika z firmware do web/lang/logsrc.h i katalogów

DZIENNIK: komunikaty w kodzie firmware są po ANGIELSKU (LogBuffer::printf("[MOTOR] ...")) - to tekst dla programisty i Seriala.
Panel pokazuje je w wybranym języku: web/lang/logsrc.h trzyma szablony z kodu (klucz log_<znacznik>_<nn> -> tekst z %d/%s),
a katalogi KAŻDEGO języka (także pl i en) tłumaczenia z pozycyjnymi {1} {2}... (kolejność argumentów printf).
Ta kontrola pilnuje, żeby każdy komunikat w kodzie miał wpis i tłumaczenie w każdym języku.
"""
import glob, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import log_extract
from panel_src import panel_html

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
UI = os.path.join(ROOT, 'web', 'web_ui.h')
SERVER = os.path.join(ROOT, 'web', 'web_server.h')   # zawiera też stronę /update (OTA) z własnym tłumaczeniem
LANG_DIR = os.path.join(ROOT, 'web', 'lang')
LOGSRC = os.path.join(LANG_DIR, 'logsrc.h')
SPEC = re.compile(r'%%|%[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z|h)?[diuxXcsfeEgG]')
PH = re.compile(r'\{(\w+)\}')
KEY = re.compile(r'^[a-z0-9_]+$')
PLURAL_FORMS = {'pl': 3}          # liczba form mnogich; inne języki: 2 (patrz PLURAL w web/ui/js_core.h)

def read(p):
    with open(p, encoding='utf-8') as f:
        return f.read()

def js_string(s, i):
    q = s[i]; i += 1; out = []
    while i < len(s):
        c = s[i]
        if c == '\\':
            n = s[i + 1]
            if n == 'n': out.append('\n'); i += 2
            elif n == 't': out.append('\t'); i += 2
            elif n == 'u': out.append(chr(int(s[i + 2:i + 6], 16))); i += 6
            elif n == 'x': out.append(chr(int(s[i + 2:i + 4], 16))); i += 4
            else: out.append(n); i += 2
        elif c == q:
            return ''.join(out), i + 1
        else:
            out.append(c); i += 1
    raise ValueError('niezamknięty literał')

def used_keys():
    """Klucze użyte w panelu w kolejności pierwszego wystąpienia: (lista, słownik klucz -> rodzaj, liczba TL bez literału)."""
    page = ''
    page = panel_html() + '\n'          # web/web_ui.h + web/ui/*.h
    for m in re.finditer(r'R"HTML\((.*?)\)HTML"', read(SERVER), re.S): page += m.group(1) + '\n'
    js = '\n'.join(re.findall(r'<script[^>]*>(.*?)</script>', page, re.S))
    kinds, order = {}, []
    def add(k, kind):
        if k not in kinds: kinds[k] = kind; order.append(k)
    nonlit = 0
    for m in re.finditer(r'(?<![\w$.])TL\(', js):
        i = m.end()
        if i < len(js) and js[i] in '\'"': add(js_string(js, i)[0], 'text')
        else: nonlit += 1
    for m in re.finditer(r'(?<![\w$.])plN\(\s*[\w.$]+\s*,\s*(?=[\'"])', js):
        add(js_string(js, m.end())[0], 'plural')
    for m in re.finditer(r'\bdata-i18n(?:-html)?="([^"]+)"', page): add(m.group(1), 'text')
    for m in re.finditer(r'\bdata-i18n-attr="[\w-]+:([^"]+)"', page): add(m.group(1), 'text')
    return order, kinds, nonlit

def catalog(path):
    t = read(path)
    m = re.search(r'window\.AOG_CAT\.(\w+)=', t)
    if not m: raise ValueError('brak window.AOG_CAT.<kod>=')
    a = m.end(); b = t.rindex('};')
    pairs = json.loads(t[a:b + 1], object_pairs_hook=lambda p: p)
    names = [k for k, _ in pairs]
    dup = sorted({k for k in names if names.count(k) > 1})
    return m.group(1), dict(pairs), dup, t


def spec_count(tpl):
    """Liczba argumentów formatu printf w szablonie (bez %%)."""
    return len([m for m in SPEC.findall(tpl) if m != '%%'])

_RX = {'d': r'([-+]?\d+)', 'i': r'([-+]?\d+)', 'u': r'(\d+)', 'x': r'([0-9A-Fa-f]+)', 'X': r'([0-9A-Fa-f]+)', 'c': r'(.)', 's': r'(.*?)',
       'f': r'([-+]?\d+(?:\.\d+)?|-?nan|-?inf)'}
_SAMPLE = {'d': '-12', 'i': '7', 'u': '42', 'x': '3F', 'X': '3F', 'f': '1.5', 's': 'abc', 'c': 'q'}

def log_regex(tpl):
    """To samo, co w panelu (logTr w web_ui.h): szablon printf -> wyrażenie dopasowujące gotową linię."""
    out, last = '', 0
    for m in SPEC.finditer(tpl):
        out += re.escape(tpl[last:m.start()]) + ('%' if m.group(0) == '%%' else _RX[m.group(0)[-1]])
        last = m.end()
    return re.compile('^' + out + re.escape(tpl[last:]) + '$')

def log_sample(tpl):
    return SPEC.sub(lambda m: '%' if m.group(0) == '%%' else _SAMPLE[m.group(0)[-1]], tpl)

def log_shadowed(src):
    """Szablony, które panel dopasowałby do cudzej linii (kolejność: ten sam znacznik, dłuższy szablon pierwszy)."""
    groups = {}
    for k, t in src.items():
        if k.startswith(('log_', 'msg_')):
            groups.setdefault((re.match(r'\[[^\]]+\]', t) or [''])[0] if re.match(r'\[[^\]]+\]', t) else '', []).append((len(t), k, log_regex(t)))
    bad = []
    for g in groups.values():
        g.sort(key=lambda x: -x[0])
        for _, k, _r in g:
            line = log_sample(src[k])
            first = next((kk for _, kk, rr in g if rr.match(line)), None)
            if first != k: bad.append('%s przesłonięty przez %s' % (k, first))
    return bad

def logsrc_load():
    """Szablony dziennika: klucz -> oryginalny tekst z firmware (log_*) albo polski argument (logarg_*)."""
    if not os.path.exists(LOGSRC): return {}
    t = read(LOGSRC)
    a = t.index('window.AOG_LOGSRC=') + len('window.AOG_LOGSRC=')
    return json.loads(t[a:t.rindex('};') + 1])

def logsrc_save(d):
    body = ',\n'.join('  %s: %s' % (json.dumps(k), json.dumps(v, ensure_ascii=False)) for k, v in d.items())
    open(LOGSRC, 'w', encoding='utf-8').write(
        '#pragma once\n// Panel WWW - ORYGINALNE (polskie) szablony komunikatów dziennika z firmware, do dopasowania w przeglądarce.\n'
        '// Generowane/uzupełniane: python3 tools/i18n_check.py --add-log. Tłumaczenia: web/lang/lang_<kod>.h (te same klucze).\n'
        'static const char WEB_LANG_LOGSRC[] PROGMEM = R"LANG(window.AOG_LOGSRC={\n' + body + '\n};)LANG";\n')

FW_KIND = {}   # szablon -> ('log', None) | ('msg', obszar); wypelnia fw_templates()
MSG_AREA = {'web_server.h': 'server', 'settings_validation.h': 'validation', 'panel_auth.h': 'auth', 'current_math.h': 'current', 'ota_status.h': 'ota'}

def fw_templates():
    """Unikalne szablony z kodu firmware w kolejności występowania + lista wywołań z formatem nie-literalnym."""
    seen, nonlit = [], []
    FW_KIND.clear()
    for f, l, t in log_extract.extract(ROOT):
        if t is None: nonlit.append('%s:%d' % (f, l))
        elif t not in seen: seen.append(t); FW_KIND[t] = ('log', None)
    for f, l, t in log_extract.extract_msgs(ROOT):
        if t is None: nonlit.append('%s:%d' % (f, l))
        elif t not in seen: seen.append(t); FW_KIND[t] = ('msg', MSG_AREA.get(os.path.basename(f), re.sub(r'\W+', '_', os.path.splitext(os.path.basename(f))[0]).lower()))
    return seen, nonlit

def log_slug(tpl):
    m = re.match(r'\[([^\]]+)\]', tpl)
    slug = re.sub(r'[^a-z0-9]+', '_', (m.group(1) if m else 'misc').lower()).strip('_') or 'misc'
    return slug

def add_log():
    src = logsrc_load()
    have = set(src.values())
    tpls, _ = fw_templates()
    new = [t for t in tpls if t not in have]
    for t in new:
        kind, area = FW_KIND[t]
        pre, slug = ('log', log_slug(t)) if kind == 'log' else ('msg', area)
        n = 1
        while '%s_%s_%02d' % (pre, slug, n) in src: n += 1
        src['%s_%s_%02d' % (pre, slug, n)] = t
    logsrc_save(src)
    key_of = {v: k for k, v in src.items()}
    for f in sorted(glob.glob(os.path.join(LANG_DIR, 'lang_*.h'))):
        code, cat, _, raw = catalog(f)
        add = {}
        for t in new:
            n = [0]
            def num(m):
                if m.group(0) == '%%': return '%'
                n[0] += 1; return '{%d}' % n[0]
            add[key_of[t]] = SPEC.sub(num, t)
        if not add: continue
        b = raw.rindex('};'); body = raw[:b].rstrip()
        ins = ',\n'.join('  %s: %s' % (json.dumps(k), json.dumps(v, ensure_ascii=False)) for k, v in add.items())
        open(f, 'w', encoding='utf-8').write(body + ',\n' + ins + '\n' + raw[b:])
    print('dopisano %d nowych komunikatów dziennika (tekst z kodu jako placeholder we wszystkich katalogach - przetłumacz je)' % len(new))
    return 0

def check():
    order, kinds, nonlit = used_keys()
    keys = set(order)
    print('panel: %d kluczy tekstów (TL / plN / data-i18n)' % len(keys))
    bad = 0
    src = logsrc_load()
    logk = [k for k in src if k.startswith(('log_', 'msg_'))]
    argk = [k for k in src if k.startswith('logarg_')]
    tpls, nonlit = fw_templates()
    for k in src:
        if not k.startswith(('log_', 'msg_', 'logarg_')) or not KEY.match(k):
            bad += 1; print('BŁĄD [logsrc]: zły klucz %s (log_*, msg_* albo logarg_*, a-z 0-9 _)' % k)
    miss = [t for t in tpls if t not in set(src.values())]
    if miss:
        bad += 1; print('BŁĄD: %d komunikatów firmware (LogBuffer / MSG) z kodu nie ma w web/lang/logsrc.h (uruchom: python3 tools/i18n_check.py --add-log):' % len(miss))
        for t in miss[:6]: print('    ', t[:100])
    gone = [k for k in logk if src[k] not in set(tpls)]
    if gone:
        bad += 1; print('BŁĄD: %d wpisów logsrc.h nie ma już w kodzie (zmieniono tekst komunikatu?) - usuń je albo przenieś tłumaczenie:' % len(gone))
        for k in gone[:6]: print('    ', k, src[k][:80])
    sh = log_shadowed(src)
    if sh:
        bad += 1; print('BŁĄD: szablony dziennika, które panel dopasuje do innej linii (zmień kolejność/treść):'); [print('    ', x) for x in sh[:6]]
    if nonlit:
        print('uwaga: %d wywołań LogBuffer z formatem nie będącym literałem (nie zostaną przetłumaczone): %s' % (len(nonlit), nonlit[:3]))
    print('komunikaty z firmware: %d (dziennik + MSG), %d argumentów słownikowych' % (len(logk), len(argk)))
    badkeys = [k for k in order if not KEY.match(k)]
    if badkeys:
        bad += 1; print('BŁĄD: klucze muszą mieć postać a-z, 0-9, _ : %s' % badkeys[:5])
    files = sorted(glob.glob(os.path.join(LANG_DIR, 'lang_*.h')))
    cats = {}
    for f in files:
        fname = re.search(r'lang_(\w+)\.h', f).group(1)
        try:
            code, cat, dup, raw = catalog(f)
        except Exception as e:
            print('BŁĄD [%s]: katalog nie jest poprawnym JSON: %s' % (fname, e)); bad += 1; continue
        if code != fname: print('BŁĄD [%s]: kod w pliku (%s) różni się od nazwy pliku' % (fname, code)); bad += 1
        cats[code] = (cat, dup)
    if 'en' not in cats:
        print('BŁĄD: brak web/lang/lang_en.h (język odniesienia i zapasowy)'); return 1
    ref = cats['en'][0]
    for code, (cat, dup) in cats.items():
        extra = set(logk) | set(argk)
        need = list(order) + logk + argk
        def rep(name, lst):
            nonlocal bad
            if lst:
                bad += 1; print('BŁĄD [%s] %s: %d' % (code, name, len(lst)))
                for k in lst[:8]: print('    ', k)
        rep('brakujące klucze', [k for k in need if k not in cat])
        rep('nieużywane (osierocone) klucze', [k for k in cat if k not in keys and k not in extra])
        rep('puste teksty', [k for k in cat if not str(cat[k]).strip()])
        rep('zmienne {..} różnią się od angielskiego', [k for k in cat if k in ref and k not in extra and sorted(PH.findall(str(cat[k]))) != sorted(PH.findall(str(ref[k])))])
        rep('dziennik: {n} nie pasuje do liczby argumentów szablonu', [k for k in logk if k in cat and sorted(PH.findall(str(cat[k]))) != sorted(str(i) for i in range(1, spec_count(src[k]) + 1))])
        rep('dziennik: tłumaczenie argumentu nie może mieć {..}', [k for k in argk if k in cat and PH.findall(str(cat[k]))])
        want = PLURAL_FORMS.get(code, 2)
        rep('liczba mnoga: wymagane %d formy rozdzielone znakiem |' % want, [k for k in cat if kinds.get(k) == 'plural' and len(str(cat[k]).split('|')) != want])
        rep('zduplikowane klucze', dup)
        rep('zakazany tekst </script', [k for k in cat if '</script' in str(cat[k]).lower()])
        if not any(1 for k in need if k not in cat):
            print('  ok   język %-4s %d tekstów kompletnych' % (code, len(cat)))
    return 1 if bad else 0

def sync(code):
    f = os.path.join(LANG_DIR, 'lang_%s.h' % code)
    order, _, _n = used_keys()
    _, cat, _, raw = catalog(f)
    _, ref, _, _ = catalog(os.path.join(LANG_DIR, 'lang_en.h'))
    add = [k for k in order + list(logsrc_load()) if k not in cat]
    if not add: print('nic do dopisania'); return 0
    ins = ',\n'.join('  %s: %s' % (json.dumps(k), json.dumps(ref.get(k, ''), ensure_ascii=False)) for k in add)
    b = raw.rindex('};')
    body = raw[:b].rstrip()
    if not body.endswith('{'): body += ','
    open(f, 'w', encoding='utf-8').write(body + '\n' + ins + '\n' + raw[b:])
    print('dopisano %d kluczy z tekstem angielskim - przetłumacz je' % len(add)); return 0

if __name__ == '__main__':
    if '--keys' in sys.argv:
        for k in used_keys()[0]: print(k)
    elif '--add-log' in sys.argv:
        sys.exit(add_log())
    elif '--sync' in sys.argv:
        sys.exit(sync(sys.argv[sys.argv.index('--sync') + 1]))
    else:
        sys.exit(check())
