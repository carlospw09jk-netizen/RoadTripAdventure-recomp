#!/bin/zsh
# Boot a disc image in PCSX2 (PINE enabled in PCSX2.ini beforehand), poll the attract demo's cars
# over PINE into <out>.bin, then quit PCSX2.
# usage: pcsx2_run.sh <disc .bin outside ~/Downloads> <out.bin> [frames]
BIN=$1; OUT=$2; FRAMES=${3:-3000}
HERE=${0:A:h}
rm -f "${TMPDIR}pcsx2.sock"
/Applications/PCSX2.app/Contents/MacOS/PCSX2 -logfile "${OUT%.bin}.pcsx2.log" -batch -fastboot -- "$BIN" > /dev/null 2>&1 &
PC=$!
python3 "$HERE/pine_dump.py" --out "$OUT" --frames "$FRAMES" --timeout 400
kill $PC 2>/dev/null; sleep 2; pkill -x PCSX2 2>/dev/null
rm -f "${TMPDIR}pcsx2.sock"
