#!/usr/bin/env python3
"""Poll a running PCSX2 over PINE and dump the attract demo's race state per demo frame, in the
format of rt_attract_dump.py (u32 vblank-or-0, u32 total_time, u32 countdown, 24 car objects).

PCSX2 must run Road Trip with EnablePINE = true (socket $TMPDIR/pcsx2.sock). Memory is read live:
a sample is kept only when the demo clock (0x177A138) is the same before and after the read and
the same block was read twice in a row (the EE updates the cars in a burst after the vblank and
idles the rest of the frame), and the last such sample of each clock value is the one written.

usage: pine_dump.py --out <file> [--frames N] [--timeout S]
"""
import argparse
import os
import socket
import struct
import sys
import time

MODE, TOTAL, COUNT = 0x33590A, 0x177A138, 0x177A13C  # demo task frames run, its function (0x20A150)
CARS, STRIDE, N = 0x177ACE0, 0x270, 24
CARS_LEN = STRIDE * N


class Pine:
    def __init__(self, path):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(path)

    def _recv(self, n):
        b = b""
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c:
                raise ConnectionError("PINE closed")
            b += c
        return b

    def batch(self, cmds):
        """cmds: list of (opcode, addr). Returns the concatenated result payload."""
        body = b"".join(struct.pack("<BI", op, a) for op, a in cmds)
        self.s.sendall(struct.pack("<I", 4 + len(body)) + body)
        size = struct.unpack("<I", self._recv(4))[0]
        data = self._recv(size - 4)
        if data[0] != 0:
            raise RuntimeError("PINE error")
        return data[1:]

    def read(self, addr, n):
        """n bytes (multiple of 8) at addr."""
        return self.batch([(3, addr + i) for i in range(0, n, 8)])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--frames", type=int, default=3000)
    ap.add_argument("--timeout", type=float, default=240)
    ap.add_argument("--sock", default=os.path.join(os.environ.get("TMPDIR", "/tmp"), "pcsx2.sock"))
    a = ap.parse_args()
    t0 = time.time()
    p = None
    while p is None:
        try:
            p = Pine(a.sock)
        except OSError:
            if time.time() - t0 > a.timeout:
                sys.exit("no PINE socket")
            time.sleep(0.5)
    print("connected", flush=True)
    # one batch: mode, total (before), count, cars, total (after)
    cmds = [(3, MODE & ~7), (2, TOTAL), (2, COUNT)] + [(3, CARS + i) for i in range(0, CARS_LEN, 8)] + [(2, TOTAL)]
    samples = {}   # total -> (count, block)
    prev = None
    reads = 0
    start = None
    while time.time() - t0 < a.timeout:
        try:
            r = p.batch(cmds)
        except (OSError, RuntimeError, ConnectionError):
            time.sleep(0.2)
            continue
        reads += 1
        mode = r[MODE & 7]
        tb = struct.unpack_from("<I", r, 8)[0]
        count = struct.unpack_from("<I", r, 12)[0]
        block = r[16:16 + CARS_LEN]
        ta = struct.unpack_from("<I", r, 16 + CARS_LEN)[0]
        if mode != 8 or tb != ta or count != 0x20A150 or block[:12] == bytes(12):
            prev = None
            continue
        if start is None:
            start = time.time()
            print(f"demo race seen, total_time {tb}", flush=True)
        if prev and prev[0] == tb and prev[1] == block:
            samples[tb] = (count, block)
        prev = (tb, block)
        if len(samples) >= a.frames:
            break
        if start and time.time() - start > 120 + a.frames / 60:
            break
    with open(a.out, "wb") as f:
        for t in sorted(samples):
            c, b = samples[t]
            f.write(struct.pack("<III", 0, t, c) + b)
    print(f"wrote {len(samples)} frames, {reads} reads", flush=True)


if __name__ == "__main__":
    main()
