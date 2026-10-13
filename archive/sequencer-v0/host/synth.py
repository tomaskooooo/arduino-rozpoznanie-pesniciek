"""Turn sequencer events into sound: 8 voices on a pentatonic scale, plus knock hits."""
import threading
import time

import numpy as np
import sounddevice as sd

SAMPLE_RATE = 44100
BLOCK = 512
NOTE_SECONDS = 0.35
GAIN = 0.15

# Voice 0..7 (matrix rows) -> C major pentatonic, C4 up to E5.
VOICE_FREQS = [261.63, 293.66, 329.63, 392.00, 440.00, 523.25, 587.33, 659.25]
HIT_FREQ = 130.81  # C3, for knock hits


def _pitch_ratio(pitch: int) -> float:
    """Pitch pot 0..1023 -> -12..+12 semitones."""
    if not 0 <= pitch <= 1023:
        raise ValueError(f"pitch out of range: {pitch}")
    return 2 ** (((pitch / 1023) * 24 - 12) / 12)


class Synth:
    def __init__(self):
        self._lock = threading.Lock()
        self._voices = []  # each: [freq, phase, samples_left, velocity]
        self._stream = sd.OutputStream(
            samplerate=SAMPLE_RATE, blocksize=BLOCK, channels=1, dtype="float32", callback=self._callback
        )

    def start(self):
        self._stream.start()

    def stop(self):
        self._stream.stop()
        self._stream.close()

    def play_step(self, mask: int, pitch: int):
        """Trigger every voice whose bit is set in the 8-bit mask."""
        if not 0 <= mask <= 255:
            raise ValueError(f"mask out of range: {mask}")
        ratio = _pitch_ratio(pitch)
        for v in range(8):
            if mask & (1 << v):
                self._add(VOICE_FREQS[v] * ratio, 1.0)

    def play_hit(self, velocity: int, pitch: int):
        self._add(HIT_FREQ * _pitch_ratio(pitch), min(max(velocity, 1), 255) / 255)

    def _add(self, freq: float, velocity: float):
        with self._lock:
            self._voices.append([freq, 0.0, int(NOTE_SECONDS * SAMPLE_RATE), velocity])

    def _callback(self, outdata, frames, time_info, status):
        if status:
            raise RuntimeError(f"audio stream error: {status}")
        out = np.zeros(frames, dtype=np.float32)
        with self._lock:
            alive = []
            for voice in self._voices:
                freq, phase, left, vel = voice
                n = min(frames, left)
                t = np.arange(n)
                env = (left - t) / (NOTE_SECONDS * SAMPLE_RATE)  # linear decay
                out[:n] += (np.sin(phase + 2 * np.pi * freq * t / SAMPLE_RATE) * env * vel).astype(np.float32)
                voice[1] = phase + 2 * np.pi * freq * n / SAMPLE_RATE
                voice[2] = left - n
                if voice[2] > 0:
                    alive.append(voice)
            self._voices = alive
        outdata[:, 0] = np.clip(out * GAIN, -1.0, 1.0)


if __name__ == "__main__":
    s = Synth()
    s.start()
    for step in range(16):  # demo: walk up the scale, then a knock hit
        s.play_step(1 << (step % 8), 512)
        time.sleep(0.25)
    s.play_hit(200, 512)
    time.sleep(0.6)
    s.stop()
