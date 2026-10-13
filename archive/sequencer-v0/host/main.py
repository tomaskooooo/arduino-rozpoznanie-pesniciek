"""Wires the Mega, synth, recorder and recognizer together.

Usage: python main.py COM9
Requires the AUDD_API_TOKEN environment variable.
"""
import sys

from link import Link, ProtocolError
from recognizer import api_key, identify, lcd_message, read_audio
from recorder import Recorder
from synth import Synth

RECORDING_FILE = "recording.json"


def recognize(link: Link):
    """Called right after LISTEN_START: receive the audio, identify it, show the result."""
    try:
        wav = read_audio(link.ser)
        link.send("STATE BUSY")
        result = identify(wav)
    except Exception:
        link.send("STATE ERR")
        raise
    print("Recognized:", result)
    link.send(lcd_message(result))
    link.send("STATE OK")


def main(port: str):
    api_key()  # fail immediately if the token is missing
    synth = Synth()
    link = Link(port)
    recorder = Recorder()
    synth.start()
    print(f"Connected to {port}. Waiting for the Mega (it restarts and runs the matrix test first)...")

    def play(kind: str, value: int, pitch: int):
        if kind == "STEP":
            synth.play_step(value, pitch)
        else:
            synth.play_hit(value, pitch)

    mode = None
    pitch = 512
    try:
        while True:
            try:
                msg = link.poll()
            except ProtocolError as e:
                print("WARNING: damaged line from the Mega, skipped:", e)
                continue
            if msg is None:
                continue
            kind, args = msg
            if kind in ("MODE", "PLAY", "REPLAY", "LISTEN_START"):
                print(kind, *args)

            if kind == "PITCH":
                pitch = args[0]
            elif kind in ("TEMPO", "PLAY"):
                pass  # only the Mega's own step clock needs these
            elif kind == "STEP":
                play("STEP", args[1], pitch)
                if recorder.recording:
                    recorder.add("STEP", args[1], pitch)
            elif kind == "HIT":
                play("HIT", args[0], pitch)
                if recorder.recording:
                    recorder.add("HIT", args[0], pitch)
            elif kind == "MODE":
                recorder.stop_replay()
                if recorder.recording:
                    recorder.stop()
                    recorder.save(RECORDING_FILE)
                mode = args[0]
                if mode == "REC":
                    recorder.start()
            elif kind == "REPLAY":
                if not recorder.events:
                    link.send("LCD NOTHING|RECORDED")
                else:
                    link.send("LCD REPLAY|")
                    link.send("STATE BUSY")
                    recorder.replay(play, lambda: link.send("STATE IDLE"))
            elif kind == "LISTEN_START":
                recognize(link)
    finally:
        recorder.stop_replay()
        synth.stop()
        link.close()


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: python main.py <serial port, e.g. COM9>")
    main(sys.argv[1])
