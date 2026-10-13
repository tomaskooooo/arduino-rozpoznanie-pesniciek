# Hľadač piesní

Arduino Mega, zvukový senzor a LCD displej, ktoré rozpoznajú skladbu, čo práve hrá. Stlačíš tlačidlo, zariadenie päť sekúnd počúva, počítač pošle nahrávku službe [AudD](https://audd.io) a na displeji sa zobrazí názov a interpret. Dolný riadok displeja je zároveň živý analyzátor spektra so 16 stĺpcami.

```
┌────────────────┐
│One Kiss (Extend│  ← názov a interpret, dlhý text sa posúva
│▇█▆▅▆▄▃▄▂▃▂▂▁▁  │  ← spektrum zvuku
└────────────────┘
```

Vyrobili **Tomáš Dobos** a **Boris Kalmár**, študenti CSOŠE Frasattiho III. GDMPS.

## Čo je v repozitári

| Cesta | Obsah |
|---|---|
| `firmware/song_finder/` | program pre Arduino Mega (tlačidlo, LCD, analyzátor spektra, nahrávanie) |
| `host/` | program pre počítač (komunikácia s Arduinom, AudD, text na LCD) |
| `tools/lcd_test/` | diagnostika zapojenia dátových vodičov LCD |
| `tools/build_site.py` | poskladá webovú stránku z aktuálneho kódu |
| `web/` | webová stránka projektu (statická, nasadená cez Vercel) |
| `docs/DOKUMENTACIA.md` | podrobná dokumentácia v slovenčine |
| `archive/sequencer-v0/` | pôvodný, väčší projekt (16-krokový sekvencér) |

## Rýchly štart

1. Zapoj súčiastky podľa [dokumentácie](docs/DOKUMENTACIA.md#3-zapojenie) a nahraj `firmware/song_finder/song_finder.ino` do Arduina Mega.
2. Nainštaluj závislosti: `pip install -r host/requirements.txt`.
3. Zaregistruj sa na [dashboard.audd.io](https://dashboard.audd.io) a svoj kľúč ulož do súboru `host/audd_token.txt` (vzor je `host/audd_token.example.txt`).
4. Spusti `python host/recognize.py COM9` (číslo portu nájdeš v Správcovi zariadení) a stlač tlačidlo.

## API kľúč

Kľúč sa do repozitára nikdy nenahráva. Súbor `host/audd_token.txt` je v `.gitignore`. Kľúč nikomu nedávaj a nevkladaj ho do kódu ani na webovú stránku.

## Webová stránka

Priečinok `web/` je statická stránka bez build kroku. Vo Verceli stačí importovať tento repozitár. `vercel.json` už nastavuje výstupný priečinok na `web`. Po zmene kódu v `firmware/` alebo `host/` spusti `python tools/build_site.py`, aby sa kód na stránke obnovil.
