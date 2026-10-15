# Course Effects

Course-level scripted effects: proximity-triggered events, traffic hazards, conditional effect task spawning, and the nearby-car proximity visual/audio effect.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002406F8` | `UpdateProximityEvent` | Runs a proximity-triggered multi-state event, moving its vector state and spawning or killing related tasks. [inferred] | — | `0x002080D8`, `0x0021D358`, `0x0023D2C0`, `0x00258E50`, `0x002727C0`, `0x00272958`, `0x00274B10`, `0x00240BBC`, `0x00204DE8`, `0x0027EA30`, `0x002729D0` |
| `0x00240CD8` | `UpdateTrafficHazard` | Checks a trigger region and nearby vehicles, then changes a timed hazard state and vehicle properties. [inferred] | — | `0x0021C998`, `0x0023D628`, `0x0023D778`, `0x00240BE0`, `0x00204DE8`, `0x0027EA30` |
| `0x002412E0` | `UpdateConditionalEffects` | Spawns or removes three conditional tasks and invokes enabled effect handlers each frame. [inferred] | `0x00241490` | `0x00221258`, `0x0023E018`, `0x0023F540`, `0x00204C48`, `0x0027EA30` |
| `0x00241490` | `DispatchCourseEffect` | Selects and initializes a course effect handler from the current course/event ID. [inferred] | `0x0020EBE8` | `0x00223410`, `0x00241670`, `0x002423B8`, `0x00204C48` |
| `0x00241F68` | `UpdateNearbyCarEffect` | Tracks nearby cars against a selected position and updates a proximity-driven visual/audio effect. [inferred] | `0x00241490` | `0x0021D358`, `0x00258E50`, `0x00272A48` |

