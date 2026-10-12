#!/usr/bin/env python3
"""Wyciaga szablony komunikatow dziennika z kodu firmware (LogBuffer::print/println/printf/printfIf/printlnIf).

Uzywane przez i18n_check.py oraz do (re)generowania web/lang/lang_logsrc.h. Szablon = tekst formatu tak, jak
firmware go wysyla (z %d, %s itd.), bez poczatkowych/koncowych znakow nowej linii.
"""
import os, re

CALL = re.compile(r'LogBuffer::(printfIf|printlnIf|printf|println|print)\s*\(')
SKIP_DIRS = {'build', 'tests', 'tools', '.git', 'web/lang'}

def _unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            out.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'"}.get(n, '\\' + n))
            i += 2
        else:
            out.append(c); i += 1
    return ''.join(out)

def _skip_ws_comments(src, i):
    while i < len(src):
        if src[i].isspace(): i += 1
        elif src.startswith('//', i):
            j = src.find('\n', i); i = len(src) if j < 0 else j + 1
        elif src.startswith('/*', i):
            j = src.find('*/', i); i = len(src) if j < 0 else j + 2
        else: break
    return i

def _literals(src, i):
    """Czyta ciag sasiadujacych literalow "..." od pozycji i. Zwraca (tekst, pozycja_po) albo (None, i)."""
    parts = []
    i = _skip_ws_comments(src, i)
    while i < len(src) and src[i] == '"':
        j = i + 1
        while j < len(src) and src[j] != '"':
            j += 2 if src[j] == '\\' else 1
        parts.append(_unescape(src[i + 1:j]))
        i = _skip_ws_comments(src, j + 1)
    return (''.join(parts) if parts else None), i

def extract(root):
    """Zwraca liste (sciezka_wzgledna, nr_linii, szablon|None). None = format nie jest literalem."""
    return [(f, l, t) for f, l, t, _s, _e, _a, _b in extract_full(root)]

def extract_full(root):
    """Jak extract, plus polozenie literalu w pliku (start, koniec) i znaki nowej linii obciete z poczatku/konca."""
    res = []
    for dp, dn, fn in os.walk(root):
        rel = os.path.relpath(dp, root)
        dn[:] = [d for d in dn if os.path.join(rel, d).lstrip('./') not in SKIP_DIRS and d not in ('build', 'tests', 'tools', '.git')]
        for f in sorted(fn):
            if not f.endswith(('.h', '.ino', '.cpp')): continue
            if f == 'log_buffer.h': continue
            p = os.path.join(dp, f)
            src = open(p, encoding='utf-8').read()
            for m in CALL.finditer(src):
                ls = src.rfind('\n', 0, m.start()) + 1
                if src[ls:m.start()].lstrip().startswith('//'): continue
                i = m.end()
                kind = m.group(1)
                if kind.endswith('If'):
                    # pomin pierwszy argument (warunek) - do przecinka na poziomie 0
                    depth = 0
                    while i < len(src):
                        c = src[i]
                        if c == '(': depth += 1
                        elif c == ')': depth -= 1
                        elif c == ',' and depth == 0: i += 1; break
                        i += 1
                i0 = _skip_ws_comments(src, i)
                lit, i1 = _literals(src, i)
                line = src.count('\n', 0, m.start()) + 1
                lead = trail = ''
                if lit is not None:
                    lead = lit[:len(lit) - len(lit.lstrip('\n'))]
                    trail = lit[len(lit.rstrip('\n')):]
                    lit = lit.strip('\n')
                res.append((os.path.relpath(p, root).replace(os.sep, '/'), line, lit, i0, i1, lead, trail))
    return sorted(res, key=lambda r: (r[0], r[1], r[3]))

if __name__ == '__main__':
    import sys
    for f, l, t in extract(sys.argv[1] if len(sys.argv) > 1 else '.'):
        print(f'{f}:{l}\t{t!r}')


MSGCALL = re.compile(r'(?<![\w])MSG\s*\(')

def extract_msgs(root):
    """Komunikaty oznaczone MSG("...") (odpowiedzi/bledy odsylane do panelu): lista (plik, nr_linii, szablon)."""
    res = []
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in ('build', 'tests', 'tools', '.git', 'lang')]
        for f in sorted(fn):
            if not f.endswith(('.h', '.ino', '.cpp')) or f == 'msg.h': continue
            p = os.path.join(dp, f)
            src = open(p, encoding='utf-8').read()
            for m in MSGCALL.finditer(src):
                ls = src.rfind('\n', 0, m.start()) + 1
                if src[ls:m.start()].lstrip().startswith('//') or src[ls:m.start()].lstrip().startswith('#define'): continue
                lit, _ = _literals(src, m.end())
                res.append((os.path.relpath(p, root).replace(os.sep, '/'), src.count('\n', 0, m.start()) + 1, lit))
    return sorted(res)
