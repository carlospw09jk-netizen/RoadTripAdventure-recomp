# Object & HUD State Setup

The typed object/HUD registry: many small initializers that clear a 0x8cb0-byte state block and configure sprites, road/track data, HUD elements, or a type tag, plus the type dispatch table.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x002329A0` | `SetVehicleMode` | Resets a state block and assigns mode-specific sprite widths and colors [inferred] | `0x0022DAE8` | `0x0027E840` |
| `0x00232D68` | `SetWheelSprite` | Init a wheel/vehicle sprite entry with width, pointer, and animation data [inferred] | `0x00254430`, `0x002595F0`, `0x0025EB70`, `0x0025FF00`, `0x00260638`, `0x00260DC0`, `0x002638C0` | `0x0027E840` |
| `0x00232F08` | `SetWheelSprite2` | Initializes an alternate wheel sprite entry with mode-dependent size and asset pointers [inferred] | `0x00254430`, `0x002595F0`, `0x0025EB70`, `0x0025FF00`, `0x00260638`, `0x00260DC0`, `0x002638C0` | `0x0027E840` |
| `0x00233158` | `SetObjectState` | Resets a per-object state table and fills it with a sprite/timer config [inferred] | `0x0025A120`, `0x0025FF00` | `0x0027E840` |
| `0x00233338` | `SetShadowConfig` | Builds a shadow/mesh config from a frame index and stores asset and lifetime data [inferred] | `0x00251B60` | — |
| `0x00233EF0` | `SetBaseValue` | Stores a base value in both object fields so later state updates inherit it [inferred] | `0x00251B60`, `0x00254430`, `0x00254D20`, `0x00260DC0` | — |
| `0x00233F10` | `InitHudConfig` | Initializes a HUD config block with size, timing, and callback data [inferred] | `0x0022DDD0` | — |
| `0x00233FC8` | `InitHudConfig2` | Initializes a second HUD config block with different widths and callback data [inferred] | `0x0025A120`, `0x0025E630`, `0x0025EB70`, `0x0025F6A0`, `0x0025FBF8`, `0x0025FF00`, `0x00260638` | — |
| `0x00234080` | `SetSlotConfig` | Writes slot-local config values for a specific item and its active state [inferred] | `0x0022DDD0` | — |
| `0x002340C0` | `SetStageConfig` | Stores stage-local state values and per-slot transform metadata [inferred] | `0x0022D8C8` | — |
| `0x00234128` | `SetBumperConfig` | Resets a bumper state struct and sets width, alpha, and sprite data [inferred] | `0x0022DDD0`, `0x0025A120`, `0x0025DBB8`, `0x0025EB70`, `0x00263E68` | `0x0027E840` |
| `0x002341F8` | `SetTailConfig` | Resets a tail state struct and fills lifetime, alpha, and sprite values [inferred] | `0x0022DDD0` | `0x0027E840` |
| `0x00234378` | `SetDashConfig` | Resets a dash state block and writes animation, alpha, and sprite data [inferred] | `0x0022DDD0` | `0x0027E840` |
| `0x00234450` | `SetEngineConfig` | Resets an engine state block and writes animation, alpha, and sprite data [inferred] | `0x0022DDD0` | `0x0027E840` |
| `0x00234528` | `ResetStateBlock` | Zeroes a large state struct and initializes a base object config [inferred] | `0x0022ECB8` | `0x0027E840` |
| `0x00234618` | `SetBannerConfig` | Resets a banner/object state and sets a width, sprite pointer, and frame count [inferred] | `0x002631A0` | `0x0027E840` |
| `0x002346F0` | `SetTrackData` | Stores two track-related values into the object metadata fields [inferred] | `0x00262678` | — |
| `0x00234708` | `SetRoadData` | Resets a road state block and fills the pointer, size, and animation data [inferred] | `0x00254D20`, `0x0025E278`, `0x0025E630`, `0x0025F6A0`, `0x00262678` | `0x0027E840` |
| `0x002347F0` | `SetRoadData2` | Resets a variant road state block with a different asset pointer and mode [inferred] | `0x002631A0`, `0x00263E68` | `0x0027E840` |
| `0x002348D8` | `SetRoadData3` | Resets a road state block and sets a larger mode-specific animation config [inferred] | `0x0025FBF8` | `0x0027E840` |
| `0x002349C8` | `SetRoadData4` | Resets a road state block and sets the animation mode/value for another variant [inferred] | `0x00254D20`, `0x002595F0`, `0x0025E278`, `0x0025E630`, `0x00262678` | `0x0027E840` |
| `0x00234AB0` | `SetRoadData5` | Resets a road state block and stores a sprite pointer and mode-specific values [inferred] | `0x00260638` | `0x0027E840` |
| `0x00234B98` | `SetRoadData6` | Resets a road state block and sets the asset pointer and frame mode [inferred] | `0x00262678` | `0x0027E840` |
| `0x00234C80` | `SetFlagConfig` | Resets a flag state entry and writes a sprite pointer and duration value [inferred] | `0x0025A120` | `0x0027E840` |
| `0x00234D10` | `ResetMode12` | Clears a fresh mode-12 object block and seeds its base state fields [inferred] | `0x0025F6A0` | `0x0027E840` |
| `0x00234D58` | `InitMode13` | Clears a mode-13 struct and initializes it with a type ID and start state [inferred] | `0x002595F0` | `0x0027E840` |
| `0x00234DB0` | `InitMode14` | Clears a mode-14 struct and initializes it with a type ID and start state [inferred] | `0x0025FF00` | `0x0027E840` |
| `0x00234E08` | `ResetMode15` | Clears a mode-15 object block and seeds its base state fields [inferred] | `0x0025FBF8` | `0x0027E840` |
| `0x00234E50` | `ResetMode16` | Clears a mode-16 object block and seeds its base state fields [inferred] | `0x00260638` | `0x0027E840` |
| `0x00234E98` | `InitMode17` | Clears a mode-17 object and stores a track pointer and mode metadata [inferred] | `0x00251B60` | `0x0027E840` |
| `0x00234F20` | `SetTrackMeta` | Stores a track metadata value in the object at offset 0x8414 [inferred] | `0x00251B60` | — |
| `0x00234F38` | `SetTrackMeta2` | Stores a second track metadata value into the object fields [inferred] | `0x00251B60` | — |
| `0x00234F50` | `SetTrackMeta3` | Stores a third track metadata value into the object fields [inferred] | `0x00251B60` | — |
| `0x00234F68` | `SetTrackMeta4` | Resets a track animation block and writes sprite pointer and duration data [inferred] | `0x00251B60` | `0x0027E840` |
| `0x00235008` | `ResetMode18` | Clears a mode-18 object block and seeds its type/state values [inferred] | `0x00260DC0` | `0x0027E840` |
| `0x00235050` | `ResetMode19` | Clears a mode-19 object block and seeds its type/state values [inferred] | `0x0025A120` | `0x0027E840` |
| `0x00235098` | `ResetMode1A` | Clears a mode-1A object block and seeds its type/state values [inferred] | `0x00254430` | `0x0027E840` |
| `0x002350F0` | `SetHudState` | Resets a HUD state struct and chooses between compact and tall layouts [inferred] | `0x00254430` | `0x0027E840` |
| `0x002351A0` | `SetHudState2` | Resets a second HUD state block with a different sprite pointer and size [inferred] | `0x00254430` | `0x0027E840` |
| `0x00235238` | `SetHudState3` | Resets a HUD state block and writes sprite/duration/alpha fields [inferred] | `0x00254430` | `0x0027E840` |
| `0x002352D8` | `ResetMode1B` | Clears a mode-1B object block and seeds its base ID/state values [inferred] | `0x00254D20` | `0x0027E840` |
| `0x00235320` | `InitMode1C` | Clears a mode-1C object block and seeds its type/state values [inferred] | `0x002642B8` | `0x0027E840` |
| `0x00235378` | `InitMode1D` | Clears a mode-1D object block and seeds its type/state values [inferred] | `0x0025EB70` | `0x0027E840` |
| `0x002353D0` | `ResetMode1E` | Clears a mode-1E object block and seeds its base state fields [inferred] | `0x00262678` | `0x0027E840` |
| `0x00235418` | `InitObjectType1F` | Clears a 0x8cb0-byte game object and sets its type tag at +0x8008 to 0x1f, returning memset's result. [inferred] | `0x0025E630` | `0x0027E840` |
| `0x00235460` | `InitObjectType20` | Clears a 0x8cb0-byte object, sets type tag +0x8008=0x20 and initialises four +0x8460 slots to -1. [inferred] | `0x002631A0` | `0x0027E840` |
| `0x002354D8` | `SetObjectField844C` | Stores arg2 into the object field at offset +0x844c. [inferred] | `0x002631A0` | — |
| `0x002354F0` | `SetIndexedValueAndSum` | Writes arg3 at index arg2 of the +0x8460 array, records the index at +0x8458, returns the sum of the three positive slots. [inferred] | `0x002631A0` | — |
| `0x00235558` | `SetObjectField845C` | Stores arg2 into the object field at offset +0x845c. [inferred] | `0x002631A0` | — |
| `0x00235570` | `UpdateMaxCounter846C` | Raises global DAT_01780a2c to the object's +0x846c value when larger; returns 1 on a new maximum. [inferred] | `0x002631A0` | — |
| `0x002355A8` | `InitObjectType21` | Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x21. [inferred] | `0x0025E278` | `0x0027E840` |
| `0x002355F0` | `InitObjectType22` | Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x22. [inferred] | `0x00263710` | `0x0027E840` |
| `0x00235638` | `InitObjectType23` | Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x23. [inferred] | `0x00263E68` | `0x0027E840` |
| `0x00235680` | `SetObjectFields847C8470` | Stores arg2 at +0x847c and arg3 at +0x8470 of the object. [inferred] | `0x00263E68` | — |
| `0x00235698` | `UpdateMaxCounter847C` | Raises global DAT_01780a28 to the object's +0x847c value when larger; returns 1 on a new maximum. [inferred] | `0x00263E68` | — |
| `0x002356D0` | `InitObjectType24` | Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x24. [inferred] | `0x0025DBB8` | `0x0027E840` |
| `0x00235718` | `SetObjectFields848C8480` | Stores arg2 at +0x848c and arg3 at +0x8480 of the object. [inferred] | `0x0025DBB8` | — |
| `0x00235730` | `InitObjectType25` | Clears a 0x8cb0-byte object and sets its type tag at +0x8008 to 0x25. [inferred] | `0x002638C0` | `0x0027E840` |
| `0x00235778` | `InitGameStateBlock` | Zeroes 0x8cb0-byte state block, sets fields at +0x8004 and +0x8008=0x26. | — | `0x0027E840` |
| `0x002357C0` | `DispatchTypeSetup` | Switches on object type byte +0x22 and calls FUN_00233a80 with a category/mode; unknown types fall back to FUN_0022dae8. [inferred] | `0x00254D20`, `0x002595F0`, `0x0025A120`, `0x0025DBB8`, `0x0025E278`, `0x0025E630`, `0x0025EB70`, `0x0025F6A0`, `0x0025FBF8`, `0x0025FF00`, `0x00260638`, `0x00262678`, `0x00263710`, `0x002638C0`, `0x00263E68`, `0x002642B8` | — |

