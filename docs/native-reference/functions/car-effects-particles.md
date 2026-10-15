# Car Particle Effects

Car-attached particle effects — dust puffs, smoke, sparks and rotational debris — including the effect slot allocator and per-frame emitter stepping.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00223D58` | `UpdateCarEffectSlots` | Iterates 128 car-effect slots, dispatches per-type effect update via FUN_00223b30, advances timers; called from drawCarEffects. | `0x00225120` | `0x00223B30`, `0x00225390` |
| `0x00224068` | `SpawnCarDustPuffs` | Per frame advances a 0..0x20 cycle, scatters up to 2 dust puffs via FUN_00223b30 (effect 1) at world points, registers slot at DAT_0172ff70 if 2 units away. [inferred] | `0x00225120` | `0x00223B30`, `0x002729D0`, `0x00272728`, `0x00274B10` |
| `0x002243E0` | `SpawnCarSmoke` | Lifecycle 0..0x10; emits smoke puff (effect 2) via FUN_00223b30, scales by speed-FUN_0021d358/300 (heat), with vu0MulMatrix-bias on rig. [inferred] | `0x00225120` | `0x00223B30`, `0x002729D0`, `0x00272728`, `0x00272758`, `0x00274B10` |
| `0x002246C0` | `SpawnCarSparks` | Cycle 0..0x10; emits spark (effect 3) if 1.25 units from registered slot; same rig-bias+heat scaling as sparks generator. [inferred] | `0x00225120` | `0x00223B30`, `0x002729D0`, `0x00272728`, `0x00272758`, `0x00274B10` |
| `0x002249B8` | `SpawnCarDebrisCycle` | Each call rolls 4 random 0..1 offsets to add a new "type-8" emitter (rotational debris) into slot table; count/speed scale with car state. [inferred] | `0x00225120` | `0x0021D358`, `0x0027B540`, `0x002729D0`, `0x00272728`, `0x00272758` |
| `0x00224CD0` | `SpawnCarEmitterEntry` | Allocates the next free slot in DAT_0172ff70 (DAT_0172ff7c==0 enables), seeds it with arg3/arg4 (type 8, subtype 2), marks cls flag. [inferred] | `0x00225120` | `0x0021D358`, `0x00273268`, `0x002729D0`, `0x00272728`, `0x00272758` |
| `0x00224EC0` | `AdvanceCarEmitterStep` | Frame-step a slot's lifecycle: writes ty/tz via vu0ApplyMatrix, may advance type-7 puff param, registers new slot if free. [inferred] | `0x00225120` | `0x00223B30`, `0x002729D0`, `0x00272728`, `0x00272758` |
| `0x00225390` | `AdvanceCarEffectSlot` | Ticks stored effect slot (state byte controls two sub-animations with type-specific falloff); updates DAT_0172ff7c and rtt param. [inferred] | `0x00223D58` | `0x00273268`, `0x00272728` |
| `0x00225588` | `ResetCarEffectSlots` | Zeroes effect slot tables (DAT_0172ff70, DAT_01730770, DAT_017307f0) — render-init clear. [inferred] | `0x00221300` | `0x0027E840` |

