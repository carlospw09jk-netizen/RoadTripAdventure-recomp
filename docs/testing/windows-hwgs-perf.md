# Windows test brief: hardware GS against paraLLEl-GS (issue #19)

For a **Claude Code session on the Windows PC**. Your job is to measure, not to change code.
Write the results to `build/scratch/perf/hwgs_windows.md` (format below), commit nothing, and
report the table back on https://github.com/silentsudin/RoadTripAdventure-recomp/issues/19.

## Why
On Windows the GS thread was busy enough with frame generation (120 Hz) that frames dipped. The app
has two GPU GS backends:

- **hardware GS** (`RT_GS_BACKEND=hw`; Options → Graphics → Renderer **Hardware**, now the default
  everywhere): Vulkan's own rasteriser drawing what Road Trip draws; shadow frames are recorded on
  the presenter thread outside the device lock.
- **paraLLEl-GS** (`RT_GS_BACKEND=pgs`; Renderer **Accurate (paraLLEl-GS)**): emulates the GS in
  compute shaders. With frame generation it copies every GIF packet into shadow instances while
  holding the device lock; its "GS %" includes waiting on that lock.

On the M3 Max the hardware GS took about half the GPU time and less CPU at the same frame rates
(`build/scratch/perf/hwgs_desktop.md` on the Mac). The question here: is it lighter and smoother on
this PC too, with every upscaler still working?

## Rules
- Branch of the PR for issue #19. Never commit to `main`. Never commit game data, disc images, or
  anything under `build/` or `generated/`.
- **Don't touch the installed copy.** Everything runs from separate folders under the checkout's
  `build\` (the scripts set `RT_DATA_DIR` per run). If the installed app is the only source of the game
  files or saves, you may *copy* from `%LOCALAPPDATA%\RoadTripRecomp`, never write to it.
- One run at a time (the GPU is shared). Close other GPU-heavy programs. The runs are windowed at
  the display's own refresh rate, not `RT_HEADLESS`. Check with Task Manager that no `RoadTrip.exe`
  is left running at the end.

## Setup
```powershell
git fetch origin; git checkout <PR branch>
git submodule update --init third_party/PS2Recomp      # NOT --recursive (path length limit)
python scripts/pipeline.py bootstrap build             # preset windows-release -> build\windows-release\
python -m pip install numpy pillow                     # for the town scene (rtharness)
```
Base data (the extracted disc and the game built for this build), once:
```powershell
$env:RT_DATA_DIR = "$PWD\build\regression\base"
$env:RT_ROM = "<path to the Road Trip (USA).cue>"
$env:RT_EXIT_AT_VBLANK = "120"
build\windows-release\RoadTrip.exe        # extracts the disc and builds the game, then exits
Remove-Item Env:RT_ROM, Env:RT_DATA_DIR, Env:RT_EXIT_AT_VBLANK
```
Check there is no game code in the app: `llvm-nm build\windows-release\RoadTrip.exe | findstr /c:sub_00`
prints nothing.

## Scenes (`scripts/gs_perf.py`)
Both run windowed in real time with supersampling 4 (render scale 2x on the hardware GS),
re-rendered frame generation at 120, and log `[perf]` (RT_PERF_LOG), `[pacing]` (RT_PRESENT_DEBUG)
and `[gputime]` (RT_GPU_TIMES). Each result is the median of the last 25 s of the run.

1. **race**: the scripted Quick Race of `tests/regression/test_perf.py` (`RT_INPUT_SCRIPT`, exits at
   vblank 5700). No setup needed.
2. **town**: Peach Town, gas held, steering gently among the buildings and traffic. It loads a save
   state made once by the regression harness (lockstep, the same recipe as `test_save_states.py`'s
   town: Continue an Adventure save, Q's Factory → Drive around town):
   ```powershell
   # an Adventure save on memory card 1: the suite's checkpoint, or a COPY of your own card
   robocopy "$env:LOCALAPPDATA\RoadTripRecomp\saves\mc0" build\scratch\perf\mc0 /E
   python scripts/gs_perf.py town-state --checkpoint build\scratch\perf\mc0
   ```
   The harness talks to the app over `RT_TEST_SOCKET=tcp:<port>` on Windows; this path is new and
   untested there. If `town-state` fails, report why and measure the race only.

## Matrix
```powershell
python scripts/gs_perf.py matrix --upscalers fsr1,fsr3,dlss     # or xess on Intel/AMD; ~1.5 min a run
```
Three rounds of: scene x 60/120 Hz x upscaler x GS (paraLLEl-GS and the hardware GS alternate).
One configuration: `python scripts/gs_perf.py run race hw 120 fsr3 r1`. Results append to
`build\scratch\perf\gs_perf\results.jsonl`; logs are in `build\scratch\perf\gs_perf\runs\`.
`python scripts/gs_perf.py table` prints the medians:

| column | meaning |
|---|---|
| shown, game | pictures on screen / game frames per second |
| worst | longest frame in a half-second window (ms) |
| under57, shown_low | windows with the game under 57 fps / pictures under 95% of the rate (frame generation paused) |
| skipped | frames frame skip dropped |
| gs, vu1, cpu | GS and VU1 thread busy %, the app's CPU % |
| gpu_util | GPU utilisation less the idle level (`nvidia-smi`; NVIDIA only, else blank) |
| gpu_ms | `[gputime]` ms per frame (presenter and hardware-GS passes; paraLLEl-GS's compute isn't in it) |
| lock_wait_ms | presenter's wait for the device lock per present |

On AMD or Intel note the GPU load from Task Manager (GPU 3D %) by hand for one race at 120 Hz per GS.

## Checks
- On `hw`, Options (Esc) → Graphics → Upscaling lists FSR 3 and (with the add-on beside the exe, on a
  GPU that runs them) DLSS and XeSS, and they draw correctly (no ghosting trails on cars, no black picture).
- Options → Graphics → **Renderer** shows "Hardware" by default; set it to "Accurate (paraLLEl-GS)",
  restart: the log says paraLLEl-GS (`[gs]` lines, no `[hwgs] hardware GS on ...`); set it back.
  The hint says the change takes effect at the next start. The Mipmaps row shows only on Accurate.
- First launch with an empty data folder on `hw`: no long freeze when the first race starts
  (`[hwgs] pipelines: ... compiled ahead` in the log, `worst` under ~50 ms in the race windows).
- Optional: `$env:RT_GS_BACKEND="hw"; python scripts/regress.py -k smoke` (the harness pins
  paraLLEl-GS unless `RT_GS_BACKEND` is set).

## Output
`build/scratch/perf/hwgs_windows.md`: the `table` output, then GPU model and driver, CPU, display
refresh rate, Windows build, and in a few lines: which GS is lighter on CPU and GPU, whether frame
generation held (shown ≈ 2x game for the whole race), any visual difference, anything that failed.
