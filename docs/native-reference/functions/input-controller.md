# Controller Input

Pad reading, controller input mapping, and the shared transient input state that menu and vehicle code consume each frame.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0020A660` | `UpdateMenuInputState` | Updates menu input state and associated option records. | `0x00205088` | — |
| `0x0020A8A8` | `ReadPadState` | Reads controller state and updates the corresponding input data. | `0x0020AB10` | `0x00273BC8`, `0x00273D68`, `0x00273FA8`, `0x00273B48`, `0x00274250`, `0x00274198`, `0x002740E0` |
| `0x0020AB10` | `UpdateControllerInput` | Processes controller input and updates shared input state. | `0x00205088` | `0x0020A8A8` |
| `0x0020CC30` | `CheckInputMapping` | Checks a controller input mapping for the selected input mode. | `0x0020D120` | `0x0020C7B8` |
| `0x0020CD10` | `UpdateInputMapping` | Updates input mapping state according to the active controller mode. | `0x0020D120` | `0x0020C7B8` |
| `0x0020CDC0` | `DispatchInputMapping` | Dispatches input processing according to the active controller mapping mode. | `0x0020D120` | — |
| `0x0020D008` | `HandleInputEvent` | Processes an input event and updates associated state. | — | `0x002006F0`, `0x00270180`, `0x00270A20`, `0x00207B50` |
| `0x0020DF98` | `UpdateInputDeviceState` | Updates input device state using shared controller/device data. | `0x002052D0`, `0x0020D120` | `0x0020D120` |
| `0x0020E018` | `ClearInputTransientState` | Clears transient input state when the shared input mode is inactive. | `0x00205088` | — |
| `0x0020E4E8` | `ToggleInputFlagWrapper` | Thin wrapper calling FUN_0020dcd8 with param XOR 1. | — | `0x0020DCD8` |

