# Chase Camera

The camera state machine: smoothed chase-cam basis, terrain collision avoidance, preset cycling and loading, zoom transitions, cutscene wait states, free camera, and view matrix construction.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0021DC18` | `ComputeChaseCameraBasis` | Smoothed chase-cam offset from car yaw and cam pitch with per-slot spring limits; outputs 2 normalized basis vectors [inferred] | `0x0021E690`, `0x0021EC58` | `0x00208008`, `0x00208080`, `0x002727E8`, `0x00274938`, `0x00274A10`, `0x002729D0` |
| `0x0021E070` | `ResolveCameraTerrainCollision` | When camera points (+0x100/+0x110) clip terrain per height query, steps camera pitch +/-128 and rebuilds camera until both clear ground | `0x0021E690` | `0x0021F698` |
| `0x0021E328` | `CycleCameraPresetState` | Cycles camera state +0x25 mod 3, picks matching preset from 24-entry table and relaunches task 0x238b18 with its index [inferred] | `0x0021E4D8`, `0x0021EE98`, `0x0021EF98` | `0x00204D20` |
| `0x0021E4D8` | `InitCameraTaskState` | Camera boot handler: spawns cam task 0x238b18 or loads preset type 0x10/0x20 from table, then chains next handler fn (+0xc) [inferred] | `0x0021E690` | `0x00204D20`, `0x00204C48` |
| `0x0021E690` | `RunCarCameraTask` | Per-frame car camera task: selects target and preset, eases FOV, computes chase basis, builds camera, runs terrain collision [inferred] | `0x0020EBE8`, `0x0021EC58`, `0x0021ED90` | `0x00204D20`, `0x0021C7F0`, `0x0021C880`, `0x0021DC18`, `0x0021E070`, `0x0021E4D8`, `0x0021EC58`, `0x0021F198`, `0x0021F910`, `0x0021FAE8`, `0x0021FB20`, `0x00274938`, `0x00274A10`, `0x0021F698`, `0x002729D0` |
| `0x0021EC58` | `RunCameraZoomTransition` | Zoom transition: ramps zoom short +0x177ea06 by 512/frame until counter +0x8 hits 0x80, then resumes main cam handler [inferred] | `0x0021E690` | `0x0021C7F0`, `0x0021DC18`, `0x0021F698` |
| `0x0021ED90` | `CameraWaitOrCutsceneState` | Wait state: resumes main update when player flag clears, else builds fixed scene cam (types 0x17/0x18) from stored matrices [inferred] | `0x0021E690`, `0x00238B18` | `0x00204D20`, `0x0021E690`, `0x0021F698`, `0x002729D0`, `0x00272A48` |
| `0x0021EE98` | `SwitchCameraPresetType10` | After cam timer +0x8 hits 0xc0, loads preset type 0x10 from table for this player and chains handler to 0x21ef98 [inferred] | `0x0021E4D8`, `0x0021EF98` | `0x00204D20` |
| `0x0021EF98` | `SwitchCameraPresetType20` | After cam timer +0x8 hits 0xc0, loads preset type 0x20 from table for this player and chains handler to 0x21ee98 [inferred] | `0x0021E4D8`, `0x0021EE98` | `0x00204D20` |
| `0x0021F098` | `BuildMenuSceneCamera` | For scene 0x16 eases spin angle DAT_0029ccd0 and zoom from state then builds fixed menu cam; else uses stored matrix ptrs [inferred] | `0x0020EBE8` | `0x0021F698`, `0x002729D0`, `0x00272A48` |
| `0x0021F198` | `ApplyFreeCameraInput` | Moves free-cam by tuning deltas per button bits (0x1/4/0x1000-0x8000), scales zoom, adjusts pitch/yaw when mode is 7 [inferred] | `0x0021E690` | `0x0026D880` |
| `0x0021F340` | `ComposeCameraMatrix` | Builds 4x4 cam matrix from euler angles at src+0x14, premultiplies optional matrices, writes result plus 4 vectors [inferred] | `0x0021F698` | `0x00207EC8`, `0x00272B90`, `0x00272C38`, `0x00272728`, `0x002729D0`, `0x00272758`, `0x00272A48` |
| `0x0021F450` | `BuildCameraViewMatrices` | Builds camera basis from forward/up vectors and emits frustum/projection matrices with X axis mirrored into output matrices [inferred] | `0x0021F698` | `0x002727A0`, `0x002727E8`, `0x00272868`, `0x00272728`, `0x002729D0`, `0x00272A48` |
| `0x0021F910` | `SetViewDistanceGlobals` | Writes two float view-distance globals (DAT_003356e0/e4) set from camera modes and the options draw-distance page [inferred] | `0x00208848`, `0x0021E690`, `0x0022AEB0`, `0x0022BFD0`, `0x0022C280`, `0x0022CD38`, `0x00244988`, `0x00246250`, `0x00216738` | — |
| `0x0021FAE8` | `LoadCameraPreset` | Copies the 32-byte camera preset record at index param_2 (stride 32) from table 0x29ce00 into the camera state slot [inferred] | `0x0021E690` | — |
| `0x00238B18` | `SelectAndBuildCamera` | Picks nearest camera block for the current car and builds the camera with follow/zoom flags; type -1 delegates to FUN_0021ed90. [inferred] | `0x0021E328`, `0x0021E4D8`, `0x0021E690`, `0x0021EE98`, `0x0021EF98` | `0x0021ED90`, `0x00274B10`, `0x0021F698`, `0x00272A48` |
| `0x00240228` | `UpdateCameraTransition` | Initializes and advances a multi-state camera transition with bounded horizontal and vertical motion. [inferred] | — | `0x002406E8` |
| `0x0024FB80` | `InitCameraTransforms` | Resets camera/transform state, clearing angle field and initializing two 4x4 matrices to identity [inferred] | `0x00250F00`, `0x00254430`, `0x00254D20`, `0x0025A120`, `0x0025EB70`, `0x00260DC0`, `0x00262678` | `0x0021C998`, `0x00272A48` |

