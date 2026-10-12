#!/usr/bin/env python3
"""Panel smoke test in a headless browser (Chromium via Playwright).

EN: serves the real panel (web/web_ui.h + web/ui/*.h) and the real language catalogs from a tiny mock of the module, feeds it
    tests/fixtures/status.json as /status and then, in English and in Polish: opens every tab, opens a confirmation
    dialog and closes it with Esc, and fails on any JavaScript error, failed request (other than the expected 404s)
    or untranslated key shown to the user. It also checks that the fixture has every key the firmware puts into /status.
PL: serwuje prawdziwy panel (web/web_ui.h) i katalogi języków z atrapy modułu, podaje tests/fixtures/status.json jako
    /status i w obu językach: otwiera każdą kartę, otwiera okno potwierdzenia i zamyka je Esc; błąd JavaScript, nieudane
    zapytanie albo nieprzetłumaczony klucz na ekranie = niepowodzenie. Sprawdza też, czy fixture ma każdy klucz /status.

Run:  python3 tools/panel_smoke.py     (needs: pip install playwright && playwright install chromium)
Exit: 0 = OK, 1 = problem found, 2 = skipped (Playwright or Chromium missing and --require not given).
"""
import http.server, json, os, re, sys, threading

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(*p):
    with open(os.path.join(ROOT, *p), encoding='utf-8') as f:
        return f.read()


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panel_src import panel_html   # noqa: E402  (the parts of web/web_ui.h joined as the compiler does)


def catalogs():
    out = {}
    for code, fn in (('en', 'lang_en.h'), ('pl', 'lang_pl.h'), ('logsrc', 'logsrc.h')):
        m = re.search(r'R"LANG\((.*?)\)LANG"', read('web', 'lang', fn), re.S)
        out[code] = m.group(1)
    return out


def status_keys_in_firmware():
    s = read('web', 'web_server.h')
    i = s.index('static void handleStatus')
    j = s.index('static void handle', i + 30)
    nested = {'type', 'count', 'n', 'p', 'm', 'ip', 'ign', 'age', 'sel'}
    return {k for k in re.findall(r'\\"(\w+)\\":', s[i:j]) if k not in nested}


class Mock(http.server.BaseHTTPRequestHandler):
    html = ''
    cats = {}
    status = b'{}'
    posts = []

    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype='text/plain; charset=utf-8'):
        if isinstance(body, str):
            body = body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path, _, q = self.path.partition('?')
        if path == '/':
            self._send(200, Mock.html, 'text/html; charset=utf-8')
        elif path == '/lang.js':
            c = Mock.cats.get(q.replace('l=', ''))
            self._send(200, c, 'application/javascript; charset=utf-8') if c else self._send(404, 'no such language')
        elif path == '/status':
            self._send(200, Mock.status, 'application/json')
        elif path == '/log':
            self._send(200, '[SYS] smoke test log line\n[HYDRAULICS] Back to automatic control (AgOpenWeb).\n')
        elif path == '/events':
            self._send(200, '{"nowMs":1000,"events":[],"prev":{"n":0,"sess":0,"list":[]}}', 'application/json')
        elif path == '/gps_sats':
            self._send(200, '{"sats":[]}', 'application/json')
        else:
            self._send(404, 'not found')   # /history and others: the panel must cope with "no history"

    def do_POST(self):
        n = int(self.headers.get('Content-Length') or 0)
        self.rfile.read(n)
        Mock.posts.append(self.path)
        self._send(200, 'OK')


def main():
    require = '--require' in sys.argv
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print('  (panel smoke test skipped: Playwright is not installed)')
        return 1 if require else 2

    problems = []
    fixture = json.load(open(os.path.join(ROOT, 'tests', 'fixtures', 'status.json'), encoding='utf-8'))
    missing = sorted(status_keys_in_firmware() - set(fixture))
    if missing:
        problems.append('tests/fixtures/status.json lacks keys that /status sends: ' + ', '.join(missing))

    Mock.html = panel_html()
    Mock.cats = catalogs()
    Mock.status = json.dumps(fixture).encode('utf-8')
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Mock)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d/' % srv.server_address[1]

    with sync_playwright() as pw:
        try:
            browser = pw.chromium.launch()
        except Exception as e:   # no browser binary
            print('  (panel smoke test skipped: Chromium not available: %s)' % str(e).splitlines()[0])
            return 1 if require else 2
        # scenarios: every diagnostics card open (all debug flags on) and a sparse /status (older firmware or a half-started module)
        full = dict(fixture); full.update({k: True for k in fixture if k.startswith('debug')})
        sparse = {k: fixture[k] for k in ('firmwareVersion', 'firmwareBuild', 'uptimeSec', 'authMode')}
        # steering engaged but the motor not armed after a restart: the readiness list must say so
        unarmed = dict(full); unarmed.update({'cmdEngaged': True, 'cmdFresh': True, 'motorArmed': False, 'sectionExpEn': False})
        # two diagnostics cards switched on: they must sit at the very top (the first row), not between switched-off cards
        two = dict(fixture); two.update({k: False for k in fixture if k.startswith('debug')}); two.update({'debugMotor': True, 'debugImu': True})
        for scen, data, lang in [('full', full, 'en'), ('full', full, 'pl'), ('sparse', sparse, 'en'), ('unarmed', unarmed, 'en'), ('diagorder', two, 'en')]:
            Mock.status = json.dumps(data).encode('utf-8')
            ctx = browser.new_context(viewport={'width': 1100, 'height': 900})
            ctx.add_init_script("try{localStorage.setItem('lang','%s')}catch(e){}" % lang)
            page = ctx.new_page()
            errs = []
            page.on('pageerror', lambda e, errs=errs: errs.append('JS error: %s' % e))
            page.on('console', lambda m, errs=errs: errs.append('console.error: %s' % m.text) if m.type == 'error' and 'Failed to load resource' not in m.text else None)
            page.on('requestfailed', lambda r, errs=errs: errs.append('request failed: %s' % r.url))
            page.goto(base)
            page.wait_for_timeout(1500)
            tabs = page.eval_on_selector_all('nav.tabs button', 'bs=>bs.map(b=>b.dataset.tab)')
            if len(tabs) < 6:
                errs.append('only %d tabs found' % len(tabs))
            for t in tabs:
                page.click('nav.tabs button[data-tab="%s"]' % t)
                page.wait_for_timeout(500)
                txt = page.inner_text('body')
                leaked = sorted(set(re.findall(r'\b[a-z]+(?:_[a-z0-9]+){2,}\b', txt)))
                if leaked:
                    errs.append('tab %s shows untranslated keys: %s' % (t, ', '.join(leaked[:8])))
            # a confirmation dialog opens and Esc closes it (Calibration -> current sensor reset is a danger action)
            page.click('nav.tabs button[data-tab="calibration"]')
            page.wait_for_timeout(300)
            btn = page.query_selector('#curReset')
            if scen == 'sparse':
                pass   # only the absence of JS errors matters with a sparse /status
            elif btn:
                btn.click()
                page.wait_for_timeout(300)
                if not page.query_selector('[role="dialog"]'):
                    errs.append('confirmation dialog did not open')
                else:
                    page.keyboard.press('Escape')
                    page.wait_for_timeout(300)
                    if page.query_selector('[role="dialog"]'):
                        errs.append('Esc did not close the confirmation dialog')
            else:
                errs.append('button #curReset not found on the Calibration tab')
            if scen == 'full':   # a row that throws shows "—" and hides the cause: with a complete /status none may throw
                row_errs = page.evaluate('window.rowErrors||[]')
                if row_errs:
                    errs.append('%d panel rows threw: %s' % (len(row_errs), '; '.join(sorted(set(row_errs))[:5])))
            if scen == 'full' and lang == 'en':   # Settings: titled sections, jump chips, long explanations folded
                page.click('nav.tabs button[data-tab="settings"]')
                page.wait_for_timeout(400)
                nsec = page.eval_on_selector_all('#setGrid h2.sech', 'e => e.length')
                nchips = page.eval_on_selector_all('#setGrid .setnav button', 'e => e.length')
                nfold = page.eval_on_selector_all('#setGrid details.fold', 'e => e.length')
                if nsec < 6 or nchips != nsec or nfold < 1:
                    errs.append('settings layout broken: %d sections, %d chips, %d folds' % (nsec, nchips, nfold))
            if scen == 'full' and lang == 'en':   # Settings on a phone: only the first section starts open, a chip opens its section
                pctx = browser.new_context(viewport={'width': 390, 'height': 844})
                pctx.add_init_script("try{localStorage.setItem('lang','en')}catch(e){}")
                pp = pctx.new_page()
                pp.goto(base)
                pp.wait_for_timeout(1200)
                pp.click('nav.tabs button[data-tab="settings"]')
                pp.wait_for_timeout(400)
                nsec = pp.eval_on_selector_all('#setGrid .secbox', 'e => e.length')
                nshut = pp.eval_on_selector_all('#setGrid .secbox.shut', 'e => e.length')
                if nsec < 6 or nshut != nsec - 1:
                    errs.append('phone settings: %d sections, %d folded (expected all but the first)' % (nsec, nshut))
                pp.click('#setGrid .setnav button:last-child')
                pp.wait_for_timeout(500)
                if pp.eval_on_selector_all('#setGrid .secbox.shut', 'e => e.length') != nsec - 2:
                    errs.append('phone settings: a jump chip did not open its section')
                over = pp.evaluate('document.documentElement.scrollWidth-document.documentElement.clientWidth')
                if over > 0:
                    errs.append('phone settings: horizontal overflow %d px' % over)
                pctx.close()
            if scen == 'full' and lang == 'en':   # cards fold by their title, "collapse all" / "expand all", and the choice survives a reload
                for tab, grid in (('readings', 'readGrid'), ('diag', 'diagGrid'), ('calibration', 'calGrid')):
                    page.click('nav.tabs button[data-tab="%s"]' % tab)
                    page.wait_for_timeout(300)
                    ncards = page.eval_on_selector_all('#%s .card' % grid, 'e => e.length')
                    page.click('#tab-%s .foldbar button:nth-child(1)' % tab)
                    page.wait_for_timeout(200)
                    nshut = page.eval_on_selector_all('#%s .card.shut' % grid, 'e => e.length')
                    if ncards < 3 or nshut != ncards:
                        errs.append('%s: collapse all folded %d of %d cards' % (tab, nshut, ncards))
                    page.click('#%s .card h3.fh' % grid)   # one card opens again
                    if page.eval_on_selector_all('#%s .card.shut' % grid, 'e => e.length') != ncards - 1:
                        errs.append('%s: tapping a card title did not unfold it' % tab)
                    page.reload()
                    page.wait_for_timeout(1200)
                    page.click('nav.tabs button[data-tab="%s"]' % tab)
                    page.wait_for_timeout(300)
                    if page.eval_on_selector_all('#%s .card.shut' % grid, 'e => e.length') != ncards - 1:
                        errs.append('%s: folded cards were not remembered after a reload' % tab)
                    page.click('#tab-%s .foldbar button:nth-child(2)' % tab)
                    if page.eval_on_selector_all('#%s .card.shut' % grid, 'e => e.length') != 0:
                        errs.append('%s: expand all left cards folded' % tab)
            if scen == 'full' and lang == 'en':   # Calibration: the step-by-step instructions are folded, not a wall of text
                page.click('nav.tabs button[data-tab="calibration"]')
                page.wait_for_timeout(400)
                ncf = page.eval_on_selector_all('#calGrid details.fold', 'e => e.length')
                if ncf < 3:
                    errs.append('calibration instructions are not folded (%d folds)' % ncf)
            if scen == 'full' and lang == 'en':   # the diagnostic report downloads and holds no secret
                page.click('nav.tabs button[data-tab="log"]')
                page.wait_for_timeout(300)
                try:
                    with page.expect_download(timeout=4000) as dl:
                        page.click('#logbar button:has-text("Diagnostic report")')
                    path = dl.value.path()
                    rep = open(path, encoding='utf-8').read()
                    if 'AOW-Unified-ESP32 diagnostic report' not in rep or '=== Log ===' not in rep or '=== Ready to drive ===' not in rep:
                        errs.append('diagnostic report is incomplete')
                    if re.search(r'(?i)^\w*(pass|token|secret)\w* = (?!"\(hidden\)")', rep, re.M):
                        errs.append('diagnostic report leaks a secret field')
                except Exception as ex:
                    errs.append('diagnostic report download failed: %s' % str(ex).splitlines()[0])
            if scen == 'diagorder':
                page.click('nav.tabs button[data-tab="diag"]')
                page.wait_for_timeout(900)
                info = page.evaluate('''() => {
                    const g = document.getElementById('diagGrid').getBoundingClientRect().top;
                    const open = [...document.querySelectorAll('#diagGrid article.card')].filter(c => c.querySelector('input').checked);
                    return {n: open.length, tops: open.map(c => Math.round(c.getBoundingClientRect().top - g))};
                }''')
                if info['n'] != 2 or any(t > 4 for t in info['tops']):
                    errs.append('open diagnostics cards are not at the top: %r' % info)
            if scen == 'settings_probe':
                pass
            if scen == 'unarmed':
                page.click('nav.tabs button[data-tab="status"]')
                page.wait_for_timeout(300)
                txt = page.inner_text('#ready')
                if 'NOT armed' not in txt:
                    errs.append('readiness does not warn that the motor is not armed: %r' % txt[:200])
                if 'Section expander' not in txt:
                    errs.append('readiness does not mention the lost section expander')
            for e in errs:
                problems.append('[%s/%s] %s' % (scen, lang, e))
            ctx.close()
        browser.close()
    srv.shutdown()

    if problems:
        print('  panel smoke test FAILED:')
        for p in problems:
            print('    - ' + p)
        return 1
    print('  ok   panel in the browser       tabs + dialog + all cards + sparse status')
    return 0


if __name__ == '__main__':
    sys.exit(main())
