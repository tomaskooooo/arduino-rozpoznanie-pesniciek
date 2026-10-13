"""Diagnostic: wait for LISTEN_START, then report exactly what the Mega sends. Not part of the app.

Usage: python host\\debug_capture.py COM9   (get to MODE LISTEN, then touch the touch sensor)
"""
import sys
import time

import numpy as np
import serial

ser = serial.Serial(sys.argv[1], 115200, timeout=0.1)
ser.reset_input_buffer()
print("Waiting for LISTEN_START (MODE button to LISTEN, then touch)...")
while ser.readline().strip() != b"LISTEN_START":
    pass

header = ser.readline()
print("header:", header)
n = int(header.split()[1])

data = bytearray()
t0 = time.time()
last_byte_t = t0
while time.time() - t0 < 10:
    chunk = ser.read(1024)
    if chunk:
        data += chunk
        last_byte_t = time.time()
        if len(data) >= n:
            break

print(f"received {len(data)} of {n} bytes")
print(f"first-to-last byte time: {last_byte_t - t0:.2f} s  (expected about 5.0 s)")
if len(data) > 1:
    u8 = np.frombuffer(bytes(data[:n]), dtype=np.uint8)
    print(f"min {u8.min()}  max {u8.max()}  mean {u8.mean():.0f}  clipped {np.mean((u8 >= 250) | (u8 <= 5)):.0%}")
    with open("debug_capture.bin", "wb") as f:
        f.write(data)
    print("raw bytes saved to debug_capture.bin")
