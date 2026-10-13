"""Hľadač piesní (PC strana): čaká na stlačenie tlačidla na Arduine Mega, rozpozná nahrávku cez AudD a ukáže ju na LCD.

Použitie: python recognize.py COM9 [jas%]   (stlač tlačidlo na Arduine; Ctrl+C ukončí program)

API kľúč sa hľadá v tomto poradí:
  1. súbor host/audd_token.txt (jeden riadok, súbor je v .gitignore, nikdy sa nenahrá na GitHub)
  2. premenná prostredia AUDD_API_TOKEN
"""
import io
import msvcrt  # len Windows: číta klávesy + a -
import os
import pathlib
import sys
import time
import unicodedata
import wave

import numpy as np
import requests
import serial

BAUD = 250000
SAMPLE_RATE = 8000
LCD_MAX = 120  # longest text the Mega accepts (TEXT_MAX in the sketch)
SENS_DEFAULT = 6  # bar sensitivity at startup, same scale as SENS_DEFAULT in the sketch


def load_token() -> str:
    path = pathlib.Path(__file__).with_name("audd_token.txt")
    token = path.read_text(encoding="utf8").strip() if path.exists() else os.environ.get("AUDD_API_TOKEN", "").strip()
    if not token or token == "SEM_VLOZ_SVOJ_TOKEN":
        sys.exit("Chýba AudD kľúč. Vlož ho do súboru host/audd_token.txt (pozri audd_token.example.txt).")
    return token


AUDD_TOKEN = load_token()

PUNCTUATION = {"–": "-", "—": "-", "‘": "'", "’": "'", "“": '"', "”": '"', "…": "..."}
EXTRA_LETTERS = {"ł": "l", "Ł": "L", "đ": "d", "Đ": "D", "ø": "o", "Ø": "O", "ß": "ss", "æ": "ae", "Æ": "AE", "œ": "oe", "Œ": "OE", "ı": "i"}


def lcd_safe(text: str) -> str:
    """Plain ASCII for the LCD, whose font has no accented letters: é -> e, š -> s, ť -> t, ď -> d, and so on."""
    text = unicodedata.normalize("NFKD", text)  # splits "š" into "s" + a combining caron
    text = "".join(c for c in text if not unicodedata.combining(c))
    return text.translate(str.maketrans({**PUNCTUATION, **EXTRA_LETTERS}))


def lcd(ser: serial.Serial, text: str) -> None:
    """Show text on the top LCD row (the bottom row is the Mega's live spectrum analyzer).

    The Mega scrolls text longer than 16 characters. Non-ASCII becomes '?'. The Mega's serial buffer is
    64 bytes and it is busy while it redraws the LCD, so the message goes out in small chunks with a
    short pause between them.
    """
    # The leading newline ends any half-received line the Mega might still be holding, so it can't swallow ours.
    data = f"\nL{lcd_safe(text).replace(chr(10), ' ')[:LCD_MAX]}\n".encode("ascii", errors="replace")
    for i in range(0, len(data), 24):
        ser.write(data[i:i + 24])
        ser.flush()
        time.sleep(0.05)


def to_wav(u8: np.ndarray) -> bytes:
    s = u8.astype(np.float32)
    print(f"min {u8.min()}  max {u8.max()}  mean {s.mean():.0f}  clipped {np.mean((u8 >= 250) | (u8 <= 5)):.0%}")
    s -= s.mean()
    peak = np.abs(s).max()
    if peak < 2:
        raise RuntimeError("audio is silent: check the sensor wiring (S to A4)")
    pcm = (s / peak * 30000).astype(np.int16)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(pcm.tobytes())
    return buf.getvalue()


def identify(ser: serial.Serial, n: int) -> None:
    """Read the n audio bytes that follow an AUDIO header, look the song up, and show it on the LCD."""
    # Never change ser.timeout here: reconfiguring a Windows port while samples stream in can drop a byte.
    raw = bytearray()
    deadline = time.time() + 15
    while len(raw) < n and time.time() < deadline:
        raw += ser.read(n - len(raw))
    if len(raw) < n * 0.98:
        raise RuntimeError(f"got {len(raw)} of {n} bytes")
    if len(raw) != n:
        print(f"note: {n - len(raw)} sample(s) missing, carrying on")
    wav = to_wav(np.frombuffer(bytes(raw), dtype=np.uint8))
    with open("last_capture.wav", "wb") as f:
        f.write(wav)
    lcd(ser, "Searching...")
    r = requests.post(
        "https://api.audd.io/",
        data={"api_token": AUDD_TOKEN},
        files={"file": ("capture.wav", wav, "audio/wav")},
        timeout=30,
    )
    result = r.json()
    song = result.get("result")
    if song:
        print(f"=> {song['artist']} - {song['title']}")
        lcd(ser, f"{song['title']} - {song['artist']}")
    else:
        print(f"=> no match ({result.get('error', {}).get('error_message', 'unknown song')})")
        lcd(ser, "No match, try again")


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit("Použitie: python recognize.py COM9 [jas%]")
    ser = serial.Serial()
    ser.port = sys.argv[1]
    ser.baudrate = BAUD
    ser.timeout = 1
    ser.dtr = False  # keep DTR low so opening/closing the port does not reset the Mega (and wipe the LCD)
    ser.rts = False
    ser.open()
    time.sleep(0.3)
    ser.reset_input_buffer()
    if len(sys.argv) > 2:  # optional backlight brightness in percent, e.g. "python recognize.py COM9 40"
        ser.write(f"\nB{round(int(sys.argv[2]) * 255 / 100)}\n".encode("ascii"))
    sens = SENS_DEFAULT
    ser.write(f"\nS{sens}\n".encode("ascii"))
    lcd(ser, "Press button to listen")
    print("Ready. Press the button on the Mega, then play the song near the mic. Ctrl+C to quit.")
    print("Click this window, then press + or - to change how sensitive the bars are (1-9).")
    ser.timeout = 0.2  # short, so key presses are noticed quickly (never changed again while audio streams)
    try:
        while True:
            while msvcrt.kbhit():
                key = msvcrt.getwch()
                if key in "+=" and sens < 9 or key in "-_" and sens > 1:
                    sens += 1 if key in "+=" else -1
                    ser.write(f"\nS{sens}\n".encode("ascii"))
                    print(f"bar sensitivity {sens}/9")
            header = ser.readline().decode("ascii", errors="replace").strip()
            if not header.startswith("AUDIO "):
                continue
            print("Listening...")
            try:
                identify(ser, int(header.split()[1]))
            except Exception as e:
                print(f"error: {e}")
                lcd(ser, f"Error: {e}")
            time.sleep(0.2)
            ser.reset_input_buffer()  # drop anything sent while we were busy
            print("Ready for the next press.")
    except KeyboardInterrupt:
        pass
    finally:
        time.sleep(0.3)  # let the last LCD line reach the Mega before the port closes
        ser.close()


if __name__ == "__main__":
    main()
