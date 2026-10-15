# Day/Night Cycle

The 24-hour time-of-day tick, per-frame time-of-day lighting application, and the area intro / day-cycle step tasks that sequence lighting and BGM per step.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002114E0` | `UpdateTimeOfDayCounter` | Increments and wraps the 24-hour time-of-day tick counter at offset +0x14, updating DAT_00335730 if flag 0x1000 is set. | `0x002156D0` | — |
| `0x0026CA48` | `ApplyTimeOfDayLighting` | Sets time-of-day value (+0x14) from DAT_002dcd28[arg], re-applies updateTimeOfDayLight every frame, spawns setup tasks on start. [inferred] | `0x0026CB60`, `0x0026CCD0`, `0x0026D1C0`, `0x0026D2C0` | `0x00204F58`, `0x00210158`, `0x00210430`, `0x0021C6A0`, `0x00204C48`, `0x00225988` |
| `0x0026CB60` | `StartAreaIntroDayCycle` | Area 0xa script: spawn lighting task, BGM 0x0a1c0201; tick 780 fires event, tick 844 restarts step 1 with BGM 0x0a1a0201. [inferred] | `0x0022AEB0`, `0x0026CCD0` | `0x00258F30`, `0x00258FB0`, `0x00258EC8`, `0x00204DE8`, `0x00204C48` |
| `0x0026D1C0` | `AdvanceDayCycleStep` | Waits for clock field (+8) to reach DAT_002dcd50[step], kills/respawns lighting task 0x0026ca48 per step, ends after step 10. [inferred] | `0x0026CB60`, `0x0026D2C0` | `0x00204DE8`, `0x00204C48` |
| `0x0026D2C0` | `StartDayCycleStepTask` | Area 0xb entry: BGM 0x0a1a0201, spawns lighting task 0x0026ca48 at step 1, swaps handler to stepper 0x0026d1c0 when player ready. [inferred] | `0x0022AEB0`, `0x0026CCD0` | `0x00258F30`, `0x00258EC8`, `0x00204C48` |

