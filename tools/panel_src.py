"""Panel page source: joins the parts listed in web/web_ui.h (#include "ui/*.h") exactly like the compiler does.
PL: Źródło strony panelu: skleja części wymienione w web/web_ui.h (#include "ui/*.h") tak, jak robi to kompilator."""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def panel_html():
    base = os.path.join(ROOT, 'web')
    with open(os.path.join(base, 'web_ui.h'), encoding='utf-8') as f:
        top = f.read()
    names = re.findall(r'^#include "(ui/[\w.]+\.h)"', top, re.M)
    if not names:
        raise ValueError('web/web_ui.h lists no ui/*.h parts')
    out = ''
    for n in names:
        with open(os.path.join(base, n), encoding='utf-8') as f:
            src = f.read()
        m = re.search(r'R"HTML\((.*)\)HTML"', src, re.S)
        if not m:
            raise ValueError(n + ': no R"HTML(...)HTML" literal')
        out += m.group(1)
    return out
