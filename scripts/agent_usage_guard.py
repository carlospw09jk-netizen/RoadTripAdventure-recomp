#!/usr/bin/env python3
"""Pause agents near the 5-hour Claude limit.

Polls `claude -p /usage` (what ClaudeBar's CLI mode does). At >= THRESHOLD % it creates
FLAG; when the session's reset time has passed it removes FLAG. Agents check FLAG between steps.
"""
import datetime as dt, os, re, subprocess, sys, time, json
from zoneinfo import ZoneInfo

ROOT = "/Users/holzr/github/RoadTripAdventure-decomp/build/scratch"
FLAG = f"{ROOT}/agent-pause.flag"
STATE = f"{ROOT}/usage.json"
THRESHOLD = int(os.environ.get("GUARD_THRESHOLD", "85"))
PROBE = "/tmp/probe_usage"
WEEK = 0
WEEK_THRESHOLD = int(os.environ.get("GUARD_WEEK_THRESHOLD", "92"))

def read_usage():
    os.makedirs(PROBE, exist_ok=True)
    out = subprocess.run(["claude", "-p", "/usage"], cwd=PROBE, capture_output=True, text=True, timeout=90).stdout
    global WEEK
    w = re.search(r"Current week \(all models\):\s*(\d+)% used", out)
    WEEK = int(w[1]) if w else 0
    m = re.search(r"Current session:\s*(\d+)% used(?:\s*·\s*resets\s+(\w+ \d+)\s+at\s+(\d+(?::\d+)?)(am|pm)\s*\(([^)]+)\))?", out)
    if not m:
        return None
    pct = int(m[1])
    if not m[2]:  # no reset time is shown while the window is empty
        return pct, dt.datetime.now(ZoneInfo("Europe/London"))
    day, hm, ap, tz = m[2], m[3], m[4], ZoneInfo(m[5])
    h, mi = (hm.split(":") + ["0"])[:2]
    h = int(h) % 12 + (12 if ap == "pm" else 0)
    now = dt.datetime.now(tz)
    t = dt.datetime.strptime(f"{day} {now.year}", "%b %d %Y").replace(hour=h, minute=int(mi), tzinfo=tz)
    if t < now - dt.timedelta(days=30): t = t.replace(year=now.year + 1)
    return pct, t

while True:
    try:
        r = read_usage()
        if r:
            pct, reset = r
            now = dt.datetime.now(reset.tzinfo)
            json.dump({"percent": pct, "resets_at": reset.isoformat(), "checked": now.isoformat()}, open(STATE, "w"))
            if WEEK >= WEEK_THRESHOLD:
                pct = max(pct, 100)  # weekly limit nearly gone: hold until the user lifts it
            if pct >= THRESHOLD and not os.path.exists(FLAG):
                open(FLAG, "w").write(f"paused at {pct}%; resumes after {reset.isoformat()}\n")
                print(f"PAUSE {pct}% resets {reset}", flush=True)
            elif os.path.exists(FLAG) and (now >= reset or pct < THRESHOLD - 5):
                os.remove(FLAG); print(f"RESUME {pct}%", flush=True)
            else:
                print(f"ok {pct}% reset {reset:%H:%M}", flush=True)
        else:
            print("could not read usage", flush=True)
    except Exception as e:
        print("error", e, flush=True)
    paused = os.path.exists(FLAG)
    time.sleep(60)
