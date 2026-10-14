#!/usr/bin/env python3
"""Which recompiled function writes a guest word? Run the recomp headless to vblank N, attach lldb,
read the rdram base from a function every frame calls, put a hardware write watchpoint on the guest
address and print the native backtrace (the game's functions are symbols: sub_<addr>_0x<addr>) at
the first writes.

usage: who_writes.py <app binary> <data dir> <vblank> <guest address> [hits=3] [--env K=V ...]
"""
import os
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests/regression"))
from rtharness import Game  # noqa: E402

app, data, vblank, addr = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4], 0)
hits = int(sys.argv[5]) if len(sys.argv) > 5 and not sys.argv[5].startswith("--") else 3
env = {}
work = Path(tempfile.mkdtemp(prefix="who_"))
with Game(Path(data), work, app=Path(app), env=env, speed="max") as g:
    g.run(vblank)
    pid = g.proc.pid
    cmds = work / "lldb.cmds"
    lines = [
        "settings set auto-confirm true",
        f"process attach -p {pid}",
        # ptrace makes lldb the game's parent; the test harness exits when its parent changes
        f'breakpoint set -n getppid -C "thread return {os.getpid()}" -G true',
        "breakpoint set -n sub_00204F98_0x204f98",
        "continue",
        "register read x0",
        "script import lldb; f = lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0); "
        "print('RDRAM=%x' % f.FindRegister('x0').GetValueAsUnsigned())",
        "breakpoint delete 2",
        f"script f = lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0); "
        f"b = f.FindRegister('x0').GetValueAsUnsigned(); "
        f"lldb.debugger.HandleCommand('watchpoint set expression -w write -s 4 -- 0x%x' % (b + {addr}))",
    ]
    for _ in range(hits):
        lines += ["continue", "bt 8", "script f = lldb.debugger.GetSelectedTarget().GetProcess().GetSelectedThread().GetFrameAtIndex(0); print('PC=', f.GetFunctionName())"]
    lines += ["detach", "quit"]
    cmds.write_text("\n".join(lines) + "\n")
    out = open(work / "lldb.out", "wb")
    lldb = subprocess.Popen(["lldb", "-b", "-s", str(cmds)], stdout=out, stderr=subprocess.STDOUT)
    import time
    time.sleep(10)
    t = threading.Thread(target=lambda: g.run(600), daemon=True)
    t.start()
    try:
        lldb.wait(timeout=240)
    except subprocess.TimeoutExpired:
        lldb.kill()
    print((work / "lldb.out").read_text())
