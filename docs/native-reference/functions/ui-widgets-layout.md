# UI Layout & Text Drawing

Low-level 2D drawing and layout helpers: digit counters, dialog text lines, rectangle centering, text measurement/layout, and small conditional widget draw wrappers.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00202020` | `GsDrawDigitsCounter` | Writes GS prim packets into DMA buffer to render number param_1 as 200 (digit%10<4) or 220 (else) sprites at x-2,y via FUN_002063C8. [inferred] | `0x002207D0`, `0x00264498` | `0x002063C8` |
| `0x00202D10` | `DrawDialogTextLine` | Draws text line in dialog packet chain: 3.5 sprite slabs via FUN_002063C8/C0/C8, shadow via FUN_00201FD0, then glyph loop FUN_002017B0 over chars incl wide-lead bytes. [inferred] | `0x0022E780`, `0x0022E7C0`, `0x0022E7F0`, `0x0022E8C0`, `0x0022EDB8`, `0x00268E40` | `0x002017B0`, `0x00201FD0`, `0x002063C8` |
| `0x00204A00` | `UiDestroyRemovedNodes` | Walks linked list from param_1+0x10: for nodes with flag bit1 calls destructor fn-ptr (+0x2C), frees child object fields 0x10/0x14 via FUN_00205018 and unlinks node. [inferred] | `0x002105B8` | `0x00205018<arg_value>` |
| `0x00205AD0` | `CenterRectangle` | Positions a rectangle within screen bounds based on alignment flags and its width/height. | `0x00209520`, `0x00209848`, `0x00209CB0`, `0x00228D10`, `0x00229538`, `0x0022BB88`, `0x0022C8A0`, `0x0022CA28`, `0x0022CD38`, `0x00239260`, `0x00239748`, `0x00239900`, `0x00239A80`, `0x0023BCD0`, `0x00248BF0`, `0x00268E40`, `0x00269568`, `0x00208F50`, `0x002090C0` | — |
| `0x00205D00` | `AdjustWidgetCentering` | Increments frame byte at +0x11, recenters/resizes widget rect around a width from FUN_00203f40 [inferred]. | — | `0x00203F40` |
| `0x00205DA0` | `LayoutTextItems` | Measures and lays out a bounded sequence of text items, updating positions and item state. | `0x002161E0`, `0x0022BB88`, `0x0022CA28`, `0x00264498`, `0x00268E40`, `0x00269568`, `0x00269F48`, `0x0026BE50`, `0x0026C460` | `0x00203F40`, `0x002058A8` |
| `0x00205EE8` | `DrawWidgetIfFlag16` | If flag bit 0x10 set, draws a 2-unit item at computed centered position via FUN_00203e70. | — | `0x00203E70` |
| `0x00205F48` | `DrawWidgetIfFlag16Alt` | If flag bit 0x10 set, draws a 3-unit item at computed position via FUN_00203e70. | — | `0x00203E70` |
| `0x00205FB0` | `MeasureTextWidth` | Recursively sums glyph widths for a text sequence, skipping marked entries. | `0x002058A8`, `0x00205FB0` | `0x00203F40`, `0x00205FB0` |
| `0x0024F530` | `DrawTextHalfY` | Wrapper that halves the vertical coordinate before delegating to text drawing routine FUN_00203fc8 [inferred] | `0x00211F68`, `0x002161E0`, `0x00216E00`, `0x00252880`, `0x00252CB0`, `0x0025AB28`, `0x0025B0C0`, `0x002652B8`, `0x00265568`, `0x00216738` | `0x00203FC8` |
| `0x0024FAD0` | `DrawOptionSlider` | Draws an options menu slider or selector bar at adjusted coordinates with optional cursor highlight [inferred] | `0x00216738` | `0x00203098`, `0x00203CD0` |

