# Per-call native records

Branch `re/native-records` (fork `native-records`). `RT_NATIVE_RECORDS=1`, hardware GS, off by default.
Goal: stop doing per-strip CPU work on the VIF1 worker (a NativeDraw, a marker, a side-effects step and a
VU1 state read per strip) by taking one game call (`vifCallModel`, 0x227048) as ONE record.

## What a record is

`NativeCallRecord` (`gs_native_draw.h`; the shared structs of `gs_native_records.h` are used, none
invented: `NativeObjectRecord` carries the call's c[24]):

- `strips`: the batches of the model chains the call's DMA tags refer to. A chain is parsed once
  (`NativeChain`, cached by guest address and compared byte for byte, so `drawArcRibbon`'s 42 changing
  vertices just fail the compare and re-parse, then are barred after 8 misses) and shared, never copied.
- the mirror: VU1 `vf[32]` and VU memory qwords 0..31 at the call, copied once. Everything a strip reads
  that is not in the chain is in these (matrices from the setup run, lights, fog sets, tints at qw 24-26,
  the axes at 5, 6, 9).
- `header[2]`: material blocks the EE left at TOP + 402 for the first strips without one of their own
  (vifBatchRegs for cars and props), with the strip each goes before.
- `object`: c[24] of the first strip (`nativeCallObject`).

The VIF1 worker offers each recognised model chain to the recorder (`PS2Memory::setNativeCallRecorder`,
ops begin / add / end). Consecutive chains form a group (VIF NOP, FLUSHE, FLUSH, FLUSHA between them do
not end it; any other command does). At the end: object record, headers, ONE motion marker, publish, ONE
marker packet, side effects of the last strip. No VU1 run, no per-strip draw, no per-strip marker. A
record that cannot be published (ring full) falls back to the per-strip loop with the registers the
group started with. The GS thread (`GS::processNativeCallMarker`) expands a record into the per-strip
draws on the snapshot (`nativeCallStrip` = `buildNativeDraw` on the snapshot) and feeds each to the
existing `processNativeDraw`: this expansion is the interim consumer; the run-based GS side
(SubmitNativeRun, indirect draws, GPU culling) replaces it and reads `strips`, `object` and the
snapshot directly.

## What the game really sends (measured)

The spec said one `vifCallModel` is one chain of 11-2353 strips. In the VIF stream that holds only after
two corrections to the chain parser:

1. A big chain does not start at a payload segment: the DMA tag carries its first two VIF codes,
   STCYCL 1,1 and an UNPACK (V4-32 of the first strip's tag, or V3-32 of the material block at
   TOP + 402) whose data open the payload. Terrain cells are 524 KB segments, car bodies ~87 KB. They
   are now found (STCYCL word 8 bytes before the segment, `processVIF1Data`). Before, only
   one-strip chains were taken (hence the old "a model chain is one strip" note).
2. Chains mix programs (3/4/5 in one cell, 2/3/4 in cars). `RT_NATIVE_RECORDS_MIXED=0` keeps one program
   per group; the verify below shows mixing is safe.

A call still ends at a non-model VIF command (UNPACK, MSCAL of a setup, MSKPATH3...). Small props
(one 240-byte chain at the same address with its own matrix setup between instances) stay one record
per instance. Chains the parser takes whole but that hold a program other than 2-5 (the sky, 6) are
not recorded (per-strip).

## Oracle: RT_NATIVE_RECORDS_VERIFY=1

At the first chain of a call the snapshot is taken; the strips then run on the per-strip native path as
before, and for every strip the constants (c[24], tag, count, entry) rebuilt from the snapshot are
compared bit for bit with the ones built from the live state; at the end of the call the registers the
last strip would leave (vf 8, 12, 17, 18) are compared with what the per-strip path left. Mismatches
are counted by caller (the renderer's return address, `[native-records]` lines under
`RT_NATIVE_DRAW_LOG=1`). `test_native_records.py`.

Results (Mac, lockstep, headless-equivalent harness): town 5.0M strips, race 4.7M, course_0 3.3M+:
**0 constant mismatches, 0 side-effect mismatches**. Taking REF sources at kick did not arise: the
chains' content is read from the DMA data, not VU memory, and the only live inputs (vifUploadCamera
and friends) are already in the setup run's registers when the chain is called.

Pictures: records vs per-strip native, quick race / town / course_0: 0.000% pixels differ.

## Counts (RT_NATIVE_DRAW_LOG=1, lockstep run, per game frame)

| scene | records/frame | strips/frame | strips/record |
| --- | --- | --- | --- |
| town (Peach Town drive) | 24.9 | 2644 | 106 |
| quick race | 86.9 | 6853 | 79 |
| course_0 race | 119.8 | 8657 | 72 |

(The frame counter counts GS frame starts in a lockstep run, so these are the ratios to trust.)
The course-select carousel was not measured.

## CPU (real time, 60 Hz, hardware GS, FSR 1, `scripts/gs_perf.py`, [perf] medians; A per-strip native,
B records, C the old VU1 path (RT_NATIVE_DRAW=0))

| scene | path | VU1 worker % | GS thread % | process CPU % |
| --- | --- | --- | --- | --- |
| town | A per-strip native | 23 / 23 | 34.5 / 38 | 74 / 77.5 |
| town | B records | 3 / 3 | 30 / 32.5 | 51.5 / 55 |
| town | C old VU1 path | 18 / 18 | 15 / 15 | 47 / 46 |
| race | A | 12 / 11.5 | 5 / 5 | 41.5 / 38.5 |
| race | B | 2 / 2 | 10 / 10 | 32.5 / 32.5 |
| race | C | 10 / 10 | 5 / 5 | 30.5 / 31.5 |

(two rounds each, 60.0 fps in all.) Records cut the per-strip path's CPU by 22 points in town and 7 in
race; the old VU1 path is still 4-5 / 1-2 points lower because the GS thread expands every strip.

## Not done / next

- The GS side is still per strip: its cost moves from the worker to the GS thread and stays
  (`processNativeCallMarker` ~ a third of the GS thread's busy time in a sample: `buildNativeDraw`
  classification for the clipped programs, `SubmitNative`, a device mutex it waits on).
  Worker C's SubmitNativeRun should read `NativeCallRecord::strips`, `object` and the snapshot.
- Per-strip variation inside c: tint (entry 2, tag word 3), fog set (tag bit 33), origin (entry 3, top1);
  `nativeCallStrip` derives them.
- The payload compare per call (memcmp of up to 524 KB) and the chain-buffer copy in the DMA walk are
  still per call; a RDRAM dirty bit would remove them.
- Side effects of a clipped last strip are taken from the "inside" formula (no classification on the
  worker); verify shows no difference in the scenes above.
