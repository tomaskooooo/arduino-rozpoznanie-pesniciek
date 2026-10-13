"""Serial line protocol with the Mega (ASCII, one message per line, 115200 baud).

Mega -> PC: STEP <n> <mask> | HIT <vel> | PITCH <0-1023> | TEMPO <bpm>
            | MODE <SEQ|REC|LISTEN> | PLAY <0|1> | LISTEN_START | REPLAY
PC -> Mega: LCD <line0>|<line1> | STATE <IDLE|BUSY|OK|ERR>
"""
import threading

import serial

BAUD = 115200
MODES = {"SEQ", "REC", "LISTEN"}
ARG_COUNT = {"STEP": 2, "HIT": 1, "PITCH": 1, "TEMPO": 1, "PLAY": 1, "MODE": 1, "LISTEN_START": 0, "REPLAY": 0}


class ProtocolError(Exception):
    pass


def parse(raw: bytes) -> tuple[str, list]:
    """Turn one received line into (kind, args); raise ProtocolError if it is malformed."""
    try:
        text = raw.decode("ascii").strip()
    except UnicodeDecodeError:
        raise ProtocolError(f"non-ASCII line from Mega: {raw!r}")
    parts = text.split()
    if not parts or parts[0] not in ARG_COUNT or len(parts) - 1 != ARG_COUNT[parts[0]]:
        raise ProtocolError(f"unknown or malformed line from Mega: {text!r}")

    kind, args = parts[0], parts[1:]
    if kind == "MODE":
        if args[0] not in MODES:
            raise ProtocolError(f"unknown mode: {text!r}")
        return kind, args
    try:
        values = [int(a) for a in args]
    except ValueError:
        raise ProtocolError(f"non-numeric argument: {text!r}")
    if kind == "PLAY" and values[0] not in (0, 1):
        raise ProtocolError(f"PLAY must be 0 or 1: {text!r}")
    return kind, values


class Link:
    def __init__(self, port: str):
        self.ser = serial.Serial(port, BAUD, timeout=0.1)  # opening the port resets the Mega
        self.ser.reset_input_buffer()  # drop stale bytes sent while no program was listening
        self._partial = b""
        self._lock = threading.Lock()
        self._synced = False  # until the first good line, junk bytes from the Mega's reset are dropped

    def close(self):
        self.ser.close()

    def send(self, line: str):
        with self._lock:  # the replay thread and the main loop both send
            self.ser.write((line + "\n").encode("ascii"))

    def poll(self) -> tuple[str, list] | None:
        """Return the next message, or None if no complete line has arrived yet."""
        self._partial += self.ser.readline()
        if not self._partial.endswith(b"\n"):
            return None
        raw, self._partial = self._partial, b""
        if not self._synced:
            # Opening the port resets the Mega and the serial line glitches, so the first line can
            # start with non-ASCII noise. Strip that once; anything else stays a hard error.
            raw = raw.lstrip(bytes(range(128, 256)))
        msg = parse(raw)
        self._synced = True
        return msg
