"""Record timestamped events during REC mode and replay them through the synth."""
import json
import threading
import time


class Recorder:
    def __init__(self):
        self.events = []  # (seconds since start, "STEP" or "HIT", mask or velocity, pitch)
        self._start = None
        self._stop_replay = threading.Event()
        self._replay_thread = None

    @property
    def recording(self) -> bool:
        return self._start is not None

    def start(self):
        self.events = []
        self._start = time.monotonic()

    def stop(self):
        self._start = None

    def add(self, kind: str, value: int, pitch: int):
        if self._start is None:
            raise RuntimeError("recorder is not recording")
        self.events.append((time.monotonic() - self._start, kind, value, pitch))

    def save(self, path: str):
        with open(path, "w") as f:
            json.dump(self.events, f)

    def replay(self, play, on_done):
        """Play a snapshot of the events on a thread: play(kind, value, pitch), then on_done()."""
        self.stop_replay()
        self._stop_replay.clear()
        self._replay_thread = threading.Thread(
            target=self._run, args=(list(self.events), play, on_done), daemon=True
        )
        self._replay_thread.start()

    def stop_replay(self):
        if self._replay_thread is not None:
            self._stop_replay.set()
            self._replay_thread.join()
            self._replay_thread = None

    def _run(self, events, play, on_done):
        t0 = time.monotonic()
        for t, kind, value, pitch in events:
            if self._stop_replay.wait(max(t0 + t - time.monotonic(), 0)):
                return  # stopped early: no on_done
            play(kind, value, pitch)
        on_done()
