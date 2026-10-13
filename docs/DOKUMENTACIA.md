# Hľadač piesní – Arduino Mega, LCD a zvukový senzor

Projekt rozpozná pieseň, ktorá práve hrá, a zobrazí jej názov na LCD displeji. Dolný riadok displeja medzitým funguje ako živý analyzátor spektra (equalizer), ktorý poskakuje podľa hudby.

```
┌────────────────┐
│Zivot ide dalej │  ← horný riadok: „názov - interpret“ (dlhý text sa posúva)
│▂▅█▇▄▃▂▁ ▁▂▃▂▁  │  ← dolný riadok: 16 stĺpcov = 16 frekvenčných pásiem
└────────────────┘
```

Stav: funkčný prototyp (október 2026). Pôvodný, väčší projekt (16-krokový sekvencér s LED maticou, joystickom a ďalším) ostal nedotknutý v priečinku `archive/sequencer-v0/`. Toto je jeho jednoduchšia verzia, postavená odznova po jednej časti.

## Obsah

1. [Ako to funguje](#1-ako-to-funguje)
2. [Potrebné súčiastky](#2-potrebné-súčiastky)
3. [Zapojenie](#3-zapojenie)
4. [Inštalácia softvéru](#4-inštalácia-softvéru)
5. [Nahratie programu do Mega](#5-nahratie-programu-do-mega)
6. [Používanie](#6-používanie)
7. [Nastavenia v kóde](#7-nastavenia-v-kóde)
8. [Analyzátor spektra](#8-analyzátor-spektra)
9. [Rozpoznávanie piesne](#9-rozpoznávanie-piesne)
10. [Sériový protokol](#10-sériový-protokol)
11. [Súbory v projekte](#11-súbory-v-projekte)
12. [Riešenie problémov](#12-riešenie-problémov)
13. [Obmedzenia a nápady na rozšírenie](#13-obmedzenia-a-nápady-na-rozšírenie)

---

## 1. Ako to funguje

Arduino Mega nemá internet, preto sa o prácu delia dve zariadenia:

| Zariadenie | Čo robí |
|---|---|
| **Arduino Mega** | číta tlačidlo, vzorkuje zvukový senzor, kreslí na LCD, počíta analyzátor spektra |
| **PC** (`recognize.py`) | prijme nahrávku z Mega, pošle ju službe AudD a výsledok pošle späť na LCD |

Priebeh jedného rozpoznania:

1. Stlačíš tlačidlo (pin 2). Mega okamžite zobrazí „Listening...“.
2. Mega 5 sekúnd číta zvukový senzor (8 000 vzoriek za sekundu, 8 bitov, spolu 40 000 bajtov) a posiela ich cez USB do PC. Najprv pošle hlavičku `AUDIO 40000`.
3. PC vzorky prijme, odstráni jednosmernú zložku, zosilní signál na plnú hlasitosť a uloží ich ako `last_capture.wav`.
4. PC nahrávku odošle službe [AudD](https://audd.io) (HTTPS). Na LCD je medzitým „Searching...“.
5. AudD odpovie názvom a interpretom. PC ich pošle na Mega a ten ich zobrazí v hornom riadku.
6. Dolný riadok LCD celý čas ukazuje analyzátor spektra z toho istého senzora.

Ak AudD pieseň nepozná, na LCD sa zobrazí „No match, try again“.

---

## 2. Potrebné súčiastky

| Počet | Súčiastka | Poznámka |
|---|---|---|
| 1 | Arduino Mega 2560 | pripojené USB káblom k PC |
| 1 | LCD displej 16×2 (HD44780, 16 pinov) | bez I2C modulu, paralelné pripojenie |
| 1 | Zvukový senzor KS0035 (analógový) | piny G, V, S |
| 1 | Tlačidlo so 4 nožičkami | obyčajné tlačidlo na nepájivé pole |
| 1 | Potenciometer 10 kΩ | nastavenie kontrastu LCD (alebo 2 rezistory, viď nižšie) |
| 1 | Rezistor 220 Ω | podsvietenie LCD |
| – | Nepájivé pole a prepojovacie vodiče | |

---

## 3. Zapojenie

### 3.1 LCD displej 16×2

Displej používa 4-bitový režim. Piny 7 až 10 (D0 až D3) ostávajú **nezapojené**.

| Pin LCD | Označenie | Kam sa zapojí |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | 5V |
| 3 | V0 | stredný vývod potenciometra (kontrast, viď 3.5) |
| 4 | RS | Mega pin 22 |
| 5 | RW | GND (musí byť na GND) |
| 6 | E | Mega pin 23 |
| 7–10 | D0–D3 | nezapojené |
| 11 | D4 | Mega pin 24 |
| 12 | D5 | Mega pin 25 |
| 13 | D6 | Mega pin 26 |
| 14 | D7 | Mega pin 27 |
| 15 | A (podsvietenie +) | cez rezistor 220 Ω na Mega pin 9 |
| 16 | K (podsvietenie −) | GND |

### 3.2 Zvukový senzor KS0035

| Pin senzora | Kam sa zapojí |
|---|---|
| G | GND |
| V | 5V |
| S | Mega pin A4 |

Na senzore je malý trimer (otočný nastavovací odpor), ktorým sa mení zosilnenie. Otáčaj ním po malých krokoch a pri hlasnej piesni sleduj hodnotu `max` vo výpise programu (pozri 6.3).

### 3.3 Tlačidlo

Štvornohé tlačidlo má nožičky spojené po dvojiciach. Zapoj ho **uhlopriečne**: jednu nožičku na Mega pin 2 a protiľahlú (diagonálnu) nožičku na GND. Rezistor netreba, program zapína vnútorný pull-up (`INPUT_PULLUP`).

```
 nožička 1 ●─────────● nožička 2
           │ tlačidlo│
 nožička 3 ●─────────● nožička 4

 nožička 1 → Mega pin 2
 nožička 4 → GND
```

### 3.4 Jas podsvietenia

Rezistor 220 Ω medzi vývodom A displeja a **pinom 9** (PWM). Program mení striedu signálu na pine 9, a tým jas. Podsvietenie odoberá okolo 8 mA, čo pin Mega zvládne bez tranzistora. Ak zapojíš rezistor namiesto toho na 5V, displej svieti naplno a jas sa nedá meniť.

### 3.5 Kontrast displeja

Kontrast určuje napätie na V0 (pin 3 LCD):

- **Potenciometer 10 kΩ (odporúčané):** jeden krajný vývod na 5V, druhý na GND, stredný na V0. Točením nájdeš kontrast, pri ktorom sú vidieť len písmená.
- **Bez potenciometra:** rezistor 4,7 kΩ z 5V na V0 a rezistor 1 kΩ z V0 na GND (delič, na V0 vychádza asi 0,9 V). Ak sú stále vidieť štvorčeky, daj hore väčší rezistor (10 kΩ); ak text bledne, menší (2,2 kΩ).
- **V0 priamo na GND** je maximálny kontrast. Písmená sa strácajú v tmavých štvorčekoch, preto to nepoužívaj.

### 3.6 Prehľad použitých pinov Mega

| Pin | Použitie |
|---|---|
| 0, 1 | USB sériová linka (nepoužívať) |
| 2 | tlačidlo (INPUT_PULLUP) |
| 9 | podsvietenie LCD (PWM) |
| 22 | LCD RS |
| 23 | LCD E |
| 24, 25, 26, 27 | LCD D4, D5, D6, D7 |
| A4 | zvukový senzor |
| 5V, GND | napájanie senzora, LCD, kontrastu |

---

## 4. Inštalácia softvéru

### 4.1 Arduino IDE

Nainštaluj [Arduino IDE](https://www.arduino.cc/en/software). Knižnica `LiquidCrystal` je jeho súčasťou, nič ďalšie sťahovať netreba. Rozšírenie pre dosky Arduino AVR (obsahuje Mega 2560) sa nainštaluje pri prvom použití.

### 4.2 Python

Potrebuješ Python 3 (vyskúšané na 3.12) a tri balíky:

```bash
pip install pyserial numpy requests
```

Vyskúšané verzie: pyserial 3.5, numpy 2.5.3, requests 2.34.2. Program používa modul `msvcrt`, takže ovládanie citlivosti klávesmi funguje len vo **Windows**.

### 4.3 Kľúč k službe AudD

Rozpoznávanie používa službu AudD. Kľúč (token) získaš po registrácii na [dashboard.audd.io](https://dashboard.audd.io). Skúšobná verzia (trial) platí do **26. 10. 2026**; potom treba zakúpiť plán alebo požiadať o ďalšie dotazy.

Kľúč sa do repozitára nikdy nenahráva. Program ho hľadá v tomto poradí:

1. súbor `host/audd_token.txt` s jedným riadkom (súbor je v `.gitignore`; vzor je `host/audd_token.example.txt`),
2. premenná prostredia `AUDD_API_TOKEN`.

Kľúč **nezdieľaj ani nenahrávaj na internet**. Kto ho vidí, môže míňať tvoje dotazy.

### 4.4 COM port

Nájdi číslo portu Mega v Správcovi zariadení (Porty (COM a LPT) → „Arduino Mega 2560“). Na tomto PC je to `COM9`. Ak máš iný, všade nižšie ho nahraď.

---

## 5. Nahratie programu do Mega

**V Arduino IDE:** otvor `firmware/song_finder/song_finder.ino`, vyber dosku „Arduino Mega or Mega 2560“, správny port a stlač Nahrať.

**Z príkazového riadka** (PowerShell, v priečinku s projektom; `arduino-cli` je súčasťou Arduino IDE):

```powershell
$cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$lib = "$env:LOCALAPPDATA\Arduino15\libraries\LiquidCrystal"
& $cli compile --fqbn arduino:avr:mega --library $lib firmware\song_finder
& $cli upload -p COM9 --fqbn arduino:avr:mega firmware\song_finder
```

Pred nahrávaním zatvor `recognize.py` aj Serial Monitor. COM port môže mať otvorený naraz len jeden program, inak nahrávanie zlyhá.

Po nahraní sa displej na chvíľu rozsvieti s textom „Song finder“ a dolný riadok začne reagovať na zvuk.

---

## 6. Používanie

### 6.1 Spustenie

```bash
python host\recognize.py COM9
```

S nastavením jasu podsvietenia v percentách (0 = zhasnuté, 100 = naplno):

```bash
python host\recognize.py COM9 50
```

Program vypíše `Ready...` a čaká. Ukončíš ho stlačením Ctrl+C.

### 6.2 Rozpoznanie piesne

1. Pusti hlasnú pieseň a daj reproduktor **tesne k senzoru** (pár centimetrov).
2. Stlač tlačidlo na Mega.
3. Po 5 sekundách sa objaví výsledok. Ak nepasuje, skús znova (pozri riešenie problémov).

Čo ukazuje displej:

| Horný riadok | Význam |
|---|---|
| `Song finder` | úvodný stav po štarte Mega |
| `Press button to listen` | PC program beží a čaká na tlačidlo |
| `Listening...` | Mega práve nahráva 5 sekúnd |
| `Searching...` | PC čaká na odpoveď AudD |
| `názov - interpret` | výsledok (dlhý text sa posúva) |
| `No match, try again` | pieseň sa nepodarilo rozpoznať |
| `Error: ...` | chyba (napr. príliš ticho, nedostatok dát) |

Počas nahrávania (5 s) je dolný riadok prázdny, analyzátor sa po nahrávaní sám obnoví.

### 6.3 Výpis v konzole

```
Listening...
min 0  max 76  mean 5  clipped 73%
=> Calvin Harris - One Kiss (Extended Mix)
```

- `max` je najvyššia nameraná hodnota (0 až 255). Pri hlasnej piesni by mala byť aspoň okolo 50. Nižšia hodnota znamená, že je senzor ticho alebo ďaleko od reproduktora.
- `clipped` je podiel vzoriek blízko nuly. Tento senzor je v pokoji na nule a kmitá len smerom nahor, preto je hodnota 50 až 90 % **normálna**.

### 6.4 Citlivosť analyzátora (klávesy `+` a `-`)

Klikni do okna s programom a stláčaj `+` (stĺpce poskakujú ľahšie) alebo `-` (stĺpce ostávajú nižšie). Stupnica je 1 až 9, štartuje na 6. Zmena platí hneď. Po novom spustení programu sa vráti na predvolenú hodnotu.

---

## 7. Nastavenia v kóde

### `firmware/song_finder/song_finder.ino`

| Konštanta | Predvolene | Význam |
|---|---|---|
| `BACKLIGHT_DEFAULT` | 255 | jas podsvietenia po štarte (0 až 255) |
| `SECONDS` | 5 | dĺžka nahrávky na rozpoznanie |
| `SCROLL_MS` | 350 | čas medzi posunmi textu v ms |
| `HOLD_TICKS` | 4 | pauza na začiatku textu (počet posunov) |
| `SCROLL_GAP` | 4 | medzera medzi koncom a opakovaním textu |
| `REDRAW_MS` | 10000 | ako často sa displej celý znova inicializuje |
| `BAND_LOW_HZ` / `BAND_HIGH_HZ` | 110 / 3500 | krajné frekvencie analyzátora |
| `GATE_AMP` | 12 | pásmo slabšie ako toto sa nezobrazí (potlačí šum) |
| `MIN_REF` | 48 | amplitúda, pri ktorej je stĺpec plný, keď je ticho |
| `SENS_DEFAULT` | 6 | predvolená citlivosť (1 až 9) |
| `REF_DECAY` | 0,97 | ako rýchlo sa automatické zosilnenie uvoľní po hlasnom momente |
| `BAR_PIXELS` | `0b01110` | šírka stĺpca v bunke (`0b11111` = plná šírka) |

Po zmene program nahraj znova (kapitola 5).

### `host/recognize.py`

| Konštanta | Význam |
|---|---|
| `audd_token.txt` | súbor s kľúčom k službe AudD (nie je v repozitári) |
| `SENS_DEFAULT` | citlivosť, ktorú PC pošle na Mega pri štarte; drž ju rovnakú ako v `.ino` |
| `LCD_MAX` | najdlhší text, ktorý Mega prijme (120 znakov) |

---

## 8. Analyzátor spektra

Dolný riadok LCD ukazuje, koľko energie je v 16 frekvenčných pásmach, zľava (basy) doprava (výšky).

**Postup, jeden obrázok (asi každých 48 ms, teda približne 20 snímok za sekundu):**

1. Mega odmeria 128 vzoriek senzora pri 8 kHz (trvá 16 ms) a odpočíta priemer (jednosmernú zložku).
2. Pre každé z 16 pásiem spustí **Goertzelov filter**. Je to zjednodušená FFT, ktorá počíta len jednu frekvenciu, a preto sa zmestí do malej a pomalej Mega (celé celočíselne, bez knižníc). Pásma sú rozložené logaritmicky od 110 Hz do 3 500 Hz.
3. Z výsledku sa vypočíta amplitúda každého pásma.
4. **Automatické zosilnenie:** najhlasnejšie pásmo posledných ~sekundy určuje, čo je „plný stĺpec“. Preto stĺpce poskakujú nezávisle od hlasitosti, na akú máš reproduktor nastavený.
5. **Šumová brána:** pásmo pod `GATE_AMP` sa nezobrazí, takže v tichu displej nepoblikáva.
6. **Citlivosť:** výška stĺpca je 8 × (amplitúda / referencia)<sup>γ</sup>. Stupeň 9 je γ = 0,5 (stĺpce sú vysoké aj pri slabých pásmach), stupeň 1 je γ ≈ 2 (vysoký je len silný vrchol), stupeň 6 je približne lineárne.
7. Stĺpec rastie okamžite a klesá o jeden stupeň za snímku, vďaka čomu pôsobí plynulo.

**Kreslenie:** každý stĺpec je jeden znak so 8 možnými výškami. Displej má miesto na 8 vlastných znakov, a tie sú celé obsadené týmito stĺpcami. Vlastné znaky sa po každej inicializácii displeja vytvárajú znova.

**Prečo `REDRAW_MS`:** čip displeja (HD44780) môže po rušení (výpadok napätia, uvoľnený vodič) stratiť synchronizáciu a ostať zablokovaný, až kým sa nezopakuje inicializácia. Program preto displej celý inicializuje každých 10 sekúnd. Môžeš si všimnúť veľmi krátke bliknutie.

---

## 9. Rozpoznávanie piesne

- Vzorkovanie 8 kHz, 8 bitov (z 10-bitového ADC sa berie horných 8 bitov), mono, 5 sekúnd.
- PC odpočíta priemer, normalizuje na veľkú hlasitosť a vytvorí 16-bitový WAV.
- WAV sa posiela na `https://api.audd.io/`. Odpoveď obsahuje `title` a `artist`.
- Posledná nahrávka sa uloží ako `last_capture.wav` v priečinku, odkiaľ si program spustil. Keď rozpoznanie nefunguje, prehraj si ju a počuješ, čo senzor naozaj zachytil.
- **Diakritika:** font LCD nemá znaky ako é, š, ť, á a vlastné znaky sú zabrané stĺpcami. Preto PC pred odoslaním diakritiku odstráni („Život ide ďalej“ → „Zivot ide dalej“). Písma ako japončina či azbuka sa zobrazia ako `?`.

Tipy pre lepšie rozpoznanie:

- Hlasná pieseň s výraznou melódiou, reproduktor 2–5 cm od senzora.
- Nerozprávaj ani neklopkaj počas nahrávania.
- Ak nepasuje, skús znova, často pomôže iné miesto v piesni.
- Ak je `max` vo výpise nízky, doladí sa trimerom na senzore.

---

## 10. Sériový protokol

Komunikácia Mega ⇄ PC beží cez USB na rýchlosti **250 000 baud**.

**PC → Mega** (riadky ukončené `\n`):

| Príkaz | Význam |
|---|---|
| `L<text>` | zobrazí `<text>` v hornom riadku LCD (max. 120 znakov) |
| `B<0–255>` | jas podsvietenia |
| `S<1–9>` | citlivosť analyzátora |
| `D1` / `D0` | ladenie zapnúť/vypnúť (Mega vypisuje riadky `V ...`) |
| `g` | začni nahrávať (rovnaké ako tlačidlo) |

**Mega → PC:**

| Správa | Význam |
|---|---|
| `BOOT` | Mega sa práve spustila |
| `AUDIO 40000` + 40 000 bajtov | nahrávka (hlavička, potom surové vzorky) |
| `V a0 … a15 ref ms` | ladiaci výpis: amplitúdy 16 pásiem, referencia a čas snímky |

**Pravidlá, ktoré predišli chybám:**

- PC pred každým príkazom pošle prázdny riadok (`\n`). Ukončí tým prípadný nedokončený riadok v pamäti Mega, ktorý by inak „zožral“ nasledujúci príkaz.
- Mega zahodí nedokončený riadok po 500 ms ticha.
- Dlhé správy posiela PC po 24 bajtoch s 50 ms pauzou, lebo prijímací buffer Mega má len 64 bajtov a počas prekresľovania LCD ho neodčítava.
- PC otvára port s `DTR = nízka`, vďaka čomu sa Mega pri pripojení **nerestartuje**. Pri predvolenom nastavení by sa reštartovala (a objavil by sa `BOOT`).
- Počas príjmu nahrávky sa nesmie meniť nastavenie portu (`ser.timeout`), lebo vo Windows tým mohol vypadnúť bajt.

Ladenie analyzátora (PC musí mať port voľný):

```python
import serial

s = serial.Serial()
s.port = "COM9"
s.baudrate = 250000
s.timeout = 0.3
s.dtr = False  # nastav pred otvorením portu, aby sa Mega nerestartovala
s.rts = False
s.open()
s.write(b"\nD1\n")
for _ in range(20):
    print(s.readline().decode().strip())
s.write(b"\nD0\n")
s.close()
```

---

## 11. Súbory v projekte

| Cesta | Čo to je |
|---|---|
| `firmware/song_finder/song_finder.ino` | program pre Arduino Mega (tlačidlo, LCD, analyzátor, nahrávanie) |
| `host/recognize.py` | program pre PC (komunikácia s Mega, AudD, text na LCD) |
| `docs/DOKUMENTACIA.md` | tento dokument |
| `tools/lcd_test/lcd_test.ino` | diagnostika zapojenia dátových vodičov LCD (pozri 12) |
| `last_capture.wav` | posledná nahrávka (vzniká pri spustení `recognize.py`) |
| `web/` | webová stránka projektu (nasadená cez Vercel) |
| `archive/sequencer-v0/` | pôvodný, väčší projekt (sekvencér), nedotknutý |

---

## 12. Riešenie problémov

| Príznak | Pravdepodobná príčina | Čo spraviť |
|---|---|---|
| Svieti podsvietenie, ale text nie je a v hornom riadku sú len štvorčeky | LCD sa neinicializoval (nedostáva príkazy) | skontroluj E (pin 6 → Mega 23), RS (pin 4 → Mega 22) a RW (pin 5 → GND); výmenou za nové vodiče sa často vyrieši; stlač reset na Mega |
| Obrazovka je prázdna | príliš nízky kontrast | pripoj V0 podľa 3.5; skontroluj VSS na GND a VDD na 5V |
| Hore štvorčeky aj pri správnom zapojení, text sa v nich stráca | kontrast je príliš vysoký (V0 na GND) | pridaj potenciometer alebo delič (3.5) |
| Zhluk náhodných znakov | dátové vodiče D4–D7 sú v zlom poradí alebo uvoľnené | nahraj `tools/lcd_test/lcd_test.ino`. Skúša všetkých 24 poradí, čitateľný text „Hello / Try n“ ukáže správne poradie |
| Jas sa nedá meniť | rezistor 220 Ω je na 5V, nie na pine 9 | presuň ho na pin 9 (3.4) |
| Na displeji ostane „Listening...“ | PC program `recognize.py` nebeží, a teda nikto neodpovedá | spusti ho (6.1) |
| Nahrávanie do Mega zlyhá, alebo port je obsadený | iný program drží COM port | zavri `recognize.py`, Serial Monitor a iné programy (5) |
| `no match` | tichá pieseň, senzor ďaleko, slabá nahrávka | hlasnejšie, bližšie, zopakuj; skontroluj `max` vo výpise a `last_capture.wav` |
| `api_token is incorrect, invalid, or inactive` | zlý, neaktívny alebo expirovaný kľúč, alebo `audd_token.txt` obsahuje starý kľúč | skontroluj `host/audd_token.txt` a stav účtu na dashboard.audd.io |
| `got 39999 of 40000 bytes` (alebo podobne) | stratil sa bajt pri prenose | už opravené (skript toleruje do 2 % chýbajúcich vzoriek); ak sa opakuje vo veľkom, vymeň USB kábel/port |
| Analyzátor v tichu poskakuje | šum senzora alebo okolia | stlač `-`, alebo zvýš `GATE_AMP` v `.ino` |
| Stĺpce sú stále nízke | senzor je ticho alebo je zlý trimer | stlač `+`, priblíž senzor, skontroluj S → A4 a trimer |
| Jeden stĺpec stále svieti pri zníženom jase | PWM podsvietenia (pin 9) ruší senzor (okolo 490 Hz) | nastav jas na 100 %, alebo prenes senzor ďalej od vodičov podsvietenia (možná príčina, neoverené) |
| Klávesy `+`/`-` nič nerobia | okno programu nemá fokus | klikni doň; funguje len vo Windows |
| Príkazy z PC sa občas „stratia“ | nedokončený riadok v pamäti Mega | ošetrené (vodiaci `\n` a časový limit); ak pretrváva, reštartuj Mega |

---

## 13. Obmedzenia a nápady na rozšírenie

**Obmedzenia**

- Potrebuješ zapnutý PC s internetom, Mega rozpoznávať sama nevie.
- Senzor KS0035 je jednoduchý: v pokoji je na nule a kmitá len jedným smerom, takže zvuk je skreslený a rozpoznávanie nie je vždy úspešné.
- Zvuk má len 8 kHz a 8 bitov (telefónna kvalita).
- LCD zobrazuje len ASCII, diakritika sa odstraňuje.
- Ovládanie citlivosti klávesmi funguje len vo Windows.
- Kým Mega nahráva (5 s), nekreslí analyzátor ani neposúva text.
- Skúšobný kľúč AudD vyprší 26. 10. 2026.

**Nápady**

- Lepší mikrofón (modul MAX4466 alebo MAX9814, ktorý je v pokoji v strede rozsahu a kmitá oboma smermi). Zapojenie je rovnaké (VCC, GND, OUT na A4), kód sa nemení.
- Zobraziť rytmus na LED matici 8×8 (z pôvodného projektu) alebo pípnuť bzučiakom po úspešnom rozpoznaní.
- Druhé tlačidlo na zmenu jasu alebo opätovné zobrazenie poslednej piesne.
- Nahradiť PC doskou s Wi-Fi (napr. ESP32), ktorá by nahrávku poslala AudD sama.
- Skutočná diakritika na LCD: stĺpce analyzátora by sa museli zjednodušiť zo 8 na 4 výšky, aby sa uvoľnili miesta pre vlastné znaky s diakritikou.

