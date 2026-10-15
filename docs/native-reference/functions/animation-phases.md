# Animation Phases

Motion clip playback and the small phase machines that step object animations, tween spin counters and classify object contexts.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0022A408` | `ApplyMotionClipFrame` | Steps animation clip param_3 by a time delta, accumulating rotation deltas and rebuilding the transform matrix; 0 ends the clip. | `0x0022A6B8` | `0x00207F38`, `0x00272940`, `0x00272C38`, `0x00272728`, `0x002729D0`, `0x00272A48` |
| `0x0022A6B8` | `UpdateAnimationPhaseMachine` | Drives clip playback via FUN_0022a408, tweens spin counters, plays SE 0x35/0x8035 and advances the phase byte at +0x27. [inferred] | `0x0022AEB0` | `0x0022A408`, `0x00258E50`, `0x00272C38`, `0x00273268`, `0x002729D0`, `0x00272A48` |
| `0x0022ABA0` | `AdvanceAnimationPhaseState` | Transitions on mode word +2 and phase +0x27, resets phases, rebuilds matrices and plays SE 0x34/0x35. [inferred] | `0x0022AEB0` | `0x00258E50`, `0x00272C38`, `0x00273268`, `0x00272A48` |
| `0x0022AD70` | `ClassifyObjectContext` | Classifies an object from bytes at +0x21/+0x23 and a field at +0x20, returning codes 0-5. [inferred] | `0x0022AEB0`, `0x00246250`, `0x00269E88` | — |

