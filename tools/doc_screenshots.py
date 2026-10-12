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

# (tab, file name, desktop height). Each tab is saved for three devices: panel-<name>-<lang>.png (computer, 1280 px),
# panel-<name>-tablet-<lang>.png (820 px) and panel-<name>-phone-<lang>.png (390 px).
SHOTS = [('status', 'status', 760), ('readings', 'readings', 760), ('charts', 'charts', 760), ('calibration', 'calibration', 760),
         ('settings', 'settings', 760)]
DEVICES = [('', 1280, None, 1), ('-tablet', 820, 1100, 1), ('-phone', 390, 844, 2)]   # suffix, width, height (None = per tab), pixel ratio


def main():
    from playwright.sync_api import sync_playwright
    fx = json.load(open(os.path.join(ps.ROOT, 'tests', 'fixtures', 'status.json'), encoding='utf-8'))
    fx.update({'wifiSsid': 'HomeNet', 'apSsid': 'AOG-ESP32-Setup', 'rssi': -58, 'hydLiftEn': True, 'hydLiftActive': False,
               'hydDownActive': False, 'hydLiftManual': False, 'wifiSsid2': '', 'wifiSsid3': '', 'wifiSsid4': ''})
    ps.Mock.html = ps.panel_html()
    ps.Mock.cats = ps.catalogs()
    ps.Mock.status = json.dumps(fx).encode('utf-8')

    import math, time
    t0 = time.time()

    def wander():   # slowly changing values, so the charts in the pictures show lines instead of a single dot
        while True:
            t = time.time() - t0
            v = dict(fx)
            v.update({'hAcc': round(0.02 + 0.006 * math.sin(t / 4) + 0.003 * math.sin(t / 1.3), 4), 'satellites': 24 + int(round(math.sin(t / 7) * 2)),
                      'gpsSpeedKmh': round(7.0 + 1.2 * math.sin(t / 5), 1), 'rssi': int(-58 + 4 * math.sin(t / 6)),
                      'steerAngleDeg': round(2.5 * math.sin(t / 3), 1), 'ageDgps': round(1.0 + 0.4 * math.sin(t / 2.2), 1),
                      'imuRoll': round(1.2 + 0.8 * math.sin(t / 4.4), 1), 'loopLoadPct': int(10 + 3 * math.sin(t / 3.1))})
            ps.Mock.status = json.dumps(v).encode('utf-8')
            time.sleep(0.5)
    threading.Thread(target=wander, daemon=True).start()
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), ps.Mock)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d/' % srv.server_address[1]
    out = os.path.join(ps.ROOT, 'docs', 'img')
    with sync_playwright() as pw:
        browser = pw.chromium.launch()
        for lang in ('en', 'pl'):
            for suffix, w, hh, dpr in DEVICES:
                ctx = browser.new_context(viewport={'width': w, 'height': hh or 760}, device_scale_factor=dpr, has_touch=bool(suffix),
                                          is_mobile=(suffix == '-phone'))
                ctx.add_init_script("try{localStorage.setItem('lang','%s');localStorage.setItem('theme','dark');localStorage.removeItem('aog.shut');"
                                    "localStorage.removeItem('aog.tab');localStorage.removeItem('aog.field')}catch(e){}" % lang)
                page = ctx.new_page()
                page.goto(base)
                page.wait_for_timeout(30000)   # the charts fill up while the panel polls
                for tab, name, h in SHOTS:
                    page.set_viewport_size({'width': w, 'height': hh or h})
                    page.click('nav.tabs button[data-tab="%s"]' % tab)
                    page.wait_for_timeout(2500)   # the masonry layout animates into place
                    fn = 'panel-%s%s-%s.png' % (name, suffix, lang)
                    page.screenshot(path=os.path.join(out, fn))
                    print('saved ' + fn)
                ctx.close()
        browser.close()


if __name__ == '__main__':
    main()
