#!/usr/bin/env python3
"""Regenerates the panel pictures in docs/img/ (EN and PL) from the real panel and the smoke-test fixture.

EN: serves the panel like tools/panel_smoke.py does (mock module + tests/fixtures/status.json), with a few realistic values,
    and saves 1280 px wide dark-theme pictures of Readings and Settings. Run it after a visible change of those tabs.
PL: serwuje panel jak tools/panel_smoke.py (atrapa modulu + fixture) i zapisuje obrazy 1280 px (ciemny motyw) zakladek
    Odczyty i Ustawienia. Uruchom po widocznej zmianie tych zakladek.

Run:  python3 tools/doc_screenshots.py     (needs Playwright + Chromium, like the smoke test)
"""
import http.server, json, os, sys, threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import panel_smoke as ps   # noqa: E402

SHOTS = [('readings', 'readings', 760), ('settings', 'settings', 760), ('calibration', 'calibration', 760)]


def main():
    from playwright.sync_api import sync_playwright
    fx = json.load(open(os.path.join(ps.ROOT, 'tests', 'fixtures', 'status.json'), encoding='utf-8'))
    fx.update({'wifiSsid': 'HomeNet', 'apSsid': 'AOG-ESP32-Setup', 'rssi': -58, 'hydLiftEn': True, 'hydLiftActive': False,
               'hydDownActive': False, 'hydLiftManual': False, 'wifiSsid2': '', 'wifiSsid3': '', 'wifiSsid4': ''})
    ps.Mock.html = ps.panel_html()
    ps.Mock.cats = ps.catalogs()
    ps.Mock.status = json.dumps(fx).encode('utf-8')
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), ps.Mock)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d/' % srv.server_address[1]
    out = os.path.join(ps.ROOT, 'docs', 'img')
    with sync_playwright() as pw:
        browser = pw.chromium.launch()
        for lang in ('en', 'pl'):
            ctx = browser.new_context(viewport={'width': 1280, 'height': 760})
            ctx.add_init_script("try{localStorage.setItem('lang','%s');localStorage.setItem('theme','dark')}catch(e){}" % lang)
            page = ctx.new_page()
            page.goto(base)
            page.wait_for_timeout(1500)
            for tab, name, h in SHOTS:
                page.set_viewport_size({'width': 1280, 'height': h})
                page.click('nav.tabs button[data-tab="%s"]' % tab)
                page.wait_for_timeout(2500)   # the masonry layout animates into place
                page.screenshot(path=os.path.join(out, 'panel-%s-%s.png' % (name, lang)))
                print('saved panel-%s-%s.png' % (name, lang))
            ctx.close()
        browser.close()


if __name__ == '__main__':
    main()
