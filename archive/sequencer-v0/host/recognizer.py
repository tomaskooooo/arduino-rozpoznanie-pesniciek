"""Receive audio from the Mega's sound sensor (A4) over serial and identify it via the AudD API.

Serial capture protocol (Mega -> PC), sent after LISTEN_START:
    AUDIO <n>\\n        header line, n = number of sample bytes that follow
    <n raw bytes>      unsigned 8-bit samples at MEGA_SAMPLE_RATE, no separators
"""
import io
import os
import sys
import wave

import numpy as np
import requests
import serial

AUDD_URL = "https://api.audd.io/"

MEGA_SAMPLE_RATE = 8000
HEADER_TIMEOUT_S = 5
PAYLOAD_TIMEOUT_S = 15
REQUEST_TIMEOUT_S = 30
LCD_MAX_LEN = 60
CAPTURE_FILE = "last_capture.wav"
CLIP_LEVEL = 250  # samples at/above this (or at/below 255 - this) count as clipped
MAX_CLIPPED_FRACTION = 0.05


def api_key() -> str:
    key = os.environ.get("AUDD_API_TOKEN")
    if not key:
        raise RuntimeError("AUDD_API_TOKEN environment variable is not set")
    return key


def read_audio(ser: serial.Serial) -> bytes:
    """Read one AUDIO message from the Mega and return it as WAV bytes.

    Call right after LISTEN_START arrives; the next line must be the AUDIO header.
    """
    ser.timeout = HEADER_TIMEOUT_S
    header = ser.readline().decode("ascii", errors="replace").strip()
    if not header.startswith("AUDIO "):
        raise RuntimeError(f"expected 'AUDIO <n>' header, got {header!r}")
    n = int(header.split()[1])

    ser.timeout = PAYLOAD_TIMEOUT_S
    raw = ser.read(n)
    if len(raw) != n:
        raise RuntimeError(f"audio truncated: got {len(raw)} of {n} bytes")
    return _to_wav(np.frombuffer(raw, dtype=np.uint8))


def _to_wav(u8: np.ndarray) -> bytes:
    """Remove the mic's DC bias, normalize to near full scale, wrap as 16-bit mono WAV."""
    samples = u8.astype(np.float32)
    mean = samples.mean()
    samples -= mean
    peak = np.abs(samples).max()
    if peak < 2:
        raise RuntimeError(f"audio is silent (min {u8.min()}, max {u8.max()}): check the sound sensor wiring (S to A4)")
    pcm = (samples / peak * 0.9 * 32767).astype(np.int16)

    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(MEGA_SAMPLE_RATE)
        w.writeframes(pcm.tobytes())
    wav = buf.getvalue()
    with open(CAPTURE_FILE, "wb") as f:  # so you can listen to what the Mega heard
        f.write(wav)

    clipped = np.mean((u8 >= CLIP_LEVEL) | (u8 <= 255 - CLIP_LEVEL))
    if clipped > MAX_CLIPPED_FRACTION:
        raise RuntimeError(
            f"{clipped:.0%} of samples are clipped (min {u8.min()}, max {u8.max()}, mean {mean:.0f}): "
            f"lower the speaker volume or move the sensor away; listen to {CAPTURE_FILE}"
        )
    return wav


def identify(wav_bytes: bytes) -> tuple[str, str] | None:
    """Return (artist, title), or None if AudD found no match."""
    resp = requests.post(
        AUDD_URL,
        data={"api_token": api_key()},
        files={"file": ("sample.wav", wav_bytes, "audio/wav")},
        timeout=REQUEST_TIMEOUT_S,
    )
    resp.raise_for_status()
    data = resp.json()
    if data.get("status") != "success":
        raise RuntimeError(f"AudD error: {data}")
    result = data.get("result")  # null or [] when nothing matched
    if not result:
        return None
    try:
        return result["artist"], result["title"]
    except (KeyError, TypeError):
        raise RuntimeError(f"Unexpected recognizer response: {data}")


def lcd_message(result: tuple[str, str] | None) -> str:
    """Format a result as the PC -> Mega line: 'LCD <line0>|<line1>'."""
    if result is None:
        return "LCD NO MATCH|"
    artist, title = result
    # The Mega rejects lines longer than LCD_MAX_LEN (see firmware config.h).
    return f"LCD {artist.replace('|', '/')[:LCD_MAX_LEN]}|{title.replace('|', '/')[:LCD_MAX_LEN]}"


if __name__ == "__main__":
    # Usage: python recognizer.py COM9   (then trigger LISTEN on the Mega)
    with serial.Serial(sys.argv[1], 115200) as ser:
        print("Waiting for LISTEN_START from the Mega...")
        while ser.readline().decode("ascii", errors="replace").strip() != "LISTEN_START":
            pass
        print(lcd_message(identify(read_audio(ser))))
