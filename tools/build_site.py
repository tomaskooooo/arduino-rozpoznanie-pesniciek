"""Poskladá web/index.html zo šablóny a skutočných zdrojových súborov (kód na stránke je vždy aktuálny).

Použitie z koreňa repozitára:  python tools/build_site.py
"""
import html
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB = ROOT / "web"

template = (WEB / "index.template.html").read_text(encoding="utf8")
ino = (ROOT / "firmware" / "song_finder" / "song_finder.ino").read_text(encoding="utf8")
py = (ROOT / "host" / "recognize.py").read_text(encoding="utf8")

# poistka: do stránky sa nesmie dostať nič, čo vyzerá ako AudD kľúč (32 hex znakov)
assert not re.search(r"\b[0-9a-f]{32}\b", ino + py), "V zdrojovom kóde je niečo ako API kľúč"

top_text = "One Kiss (Extend"
bar_levels = [7, 8, 6, 5, 6, 4, 3, 4, 2, 3, 2, 2, 1, 1, 0, 0]
lcd_top = "".join("<span></span>" if c == " " else f"<span>{html.escape(c)}</span>" for c in top_text)
lcd_bars = "".join(f'<i style="--h:{h}"></i>' for h in bar_levels)

page = (
    template.replace("{{LCD_TOP}}", lcd_top)
    .replace("{{LCD_BARS}}", lcd_bars)
    .replace("{{ARDUINO}}", html.escape(ino.rstrip("\n"), quote=False))
    .replace("{{PYTHON}}", html.escape(py.rstrip("\n"), quote=False))
)
assert "{{" not in page, "nenahradený zástupný text"
assert not re.search(r"\b[0-9a-f]{32}\b", page), "V stránke je niečo ako API kľúč"

(WEB / "index.html").write_text(page, encoding="utf8")
print(f"web/index.html hotový ({len(page) // 1024} KB)")
