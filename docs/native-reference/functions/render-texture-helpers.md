# Graphics Packet & Texture Helpers

DMA/GIF packet slot builders, VIF uploads and kicks, and the family of animated / counter-driven texture packet submitters used by course effects and HUD animations.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0020E030` | `SendTexturePacketsDMA` | Walks packet list until 'p' marker, adjusts texture field, waits DMA channel 1, then calls sendTexturePackets. | — | `0x002006F0`, `0x00270180`, `0x00270A20`, `0x00207B50` |
| `0x0021D7F8` | `BuildDmaPacketSlot3A` | Writes 3-quad DMA/GIF tag header from constants at DAT_0029cc00 and calls FUN_0021d470 (variant 3,0). | — | `0x0021D470` |
| `0x0021D850` | `BuildDmaPacketSlot4A` | Writes DMA tag header (type 4) from DAT_0029cc00 and calls FUN_0021d470 (variant 4,0). | — | `0x0021D470` |
| `0x0021D8A8` | `BuildDmaPacketSlot3B` | Writes DMA tag header (type 3) from DAT_0029cc10 and calls FUN_0021d470 (variant 3,0). | — | `0x0021D470` |
| `0x0021D900` | `BuildDmaPacketSlot4B` | Writes DMA tag header (type 4) from DAT_0029cc10 and calls FUN_0021d470 (variant 4,0). | — | `0x0021D470` |
| `0x0021D958` | `BuildDmaPacketSlot3C` | Writes DMA tag header (type 3) from DAT_0029cc20 and calls FUN_0021d470 (variant 3,1). | — | `0x0021D470` |
| `0x0021DA08` | `BuildDmaPacketSlot3D` | Writes DMA tag header (type 3) from DAT_0029cc30 and calls FUN_0021d470 (variant 3,1). | — | `0x0021D470` |
| `0x0021DA60` | `BuildDmaPacketSlot4C` | Writes DMA tag header (type 4) from DAT_0029cc30 and calls FUN_0021d470 (variant 4,1). | — | `0x0021D470` |
| `0x002212C8` | `SetVifFillDirect` | Stores replicated 8-bit pattern to _PTR_DAT_0029d130 and issues vifDirect with 6 quadwords. | — | `0x00226F70` |
| `0x002213E0` | `renderInitNoOp` | Empty stub called from renderInit; kept as a seam/no-op initializer. [inferred] | `0x00221300` | — |
| `0x00223810` | `MakeVifCall15xGif` | Builds a 64-bit VIF call: reg 0x1be4 + 10*idx and reg 0x1bec + 10*idx inside an MSCA-like GIF packet. [inferred] | `0x00243C50` | — |
| `0x00226298` | `TransformMatrixAndSubmit` | Multiplies global matrix at 0x177f5e0 by input matrix, then submits via FUN_0021d3b8. | — | `0x0021D3B8`, `0x00272758` |
| `0x00226398` | `KickVifPacket` | Calls vifKick on a fixed packet address 0x28e730. | `0x002001B8` | `0x002271F0` |
| `0x0022EA78` | `UploadVifPacketA` | Stores param_1 into DAT_0029f540 and sends the 5-quad VIF direct packet at 0x29f510 via vifDirect. [inferred] | `0x0025EB70` | `0x00226F70` |
| `0x0022EAA0` | `UploadVifPacketB` | Stores param_1 into DAT_0029f590 and sends the 5-quad VIF direct packet at 0x29f560 via vifDirect. [inferred] | `0x0025EB70` | `0x00226F70` |
| `0x0023E018` | `AdvanceOscillatingValue` | Adjusts a signed 16-bit value through table-defined ranges and toggles direction near its upper bound. [inferred] | `0x0023E9C0`, `0x0023ECA0`, `0x0023F398`, `0x002412E0` | — |
| `0x0023E508` | `SubmitAnimatedTextureLayout` | Builds animated texture packets and transform matrices, then submits 13 packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00272C38`, `0x00207B50`, `0x00272A48` |
| `0x0023E7E0` | `SubmitThreeTexturePackets` | Builds three frame-indexed texture packets and submits 13 packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x0023E8D0` | `SubmitFiveTexturePackets` | Builds five frame-indexed texture packets and submits 21 packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x0023E9C0` | `SubmitOscillatingTextureLayout` | Updates three oscillating transforms, builds texture packets, and submits five packets. [inferred] | `0x00241490` | `0x00207D80`, `0x0023E018`, `0x00272AE8`, `0x00272B90`, `0x00272C38`, `0x00207B50`, `0x00272A48` |
| `0x0023ECA0` | `SubmitEightTexturePackets` | Builds eight texture packet entries, advances one oscillating frame, and submits 33 packets. [inferred] | `0x00241490` | `0x00207D80`, `0x0023E018`, `0x00207B50` |
| `0x0023EE70` | `SubmitConditionalTextureLayout` | Builds three texture packets and conditionally resets a transform and copies a vector. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50`, `0x002729D0`, `0x00272A48` |
| `0x0023EFB0` | `SubmitFourTexturePackets` | Builds four frame-indexed texture packets and submits 17 packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x0023F0A0` | `UpdateRotatingEffect` | Updates effect entities and two opposing rotation transforms based on a periodic input state. [inferred] | `0x00241490` | `0x0023D778`, `0x00272C38`, `0x002729D0`, `0x00272A48` |
| `0x0023F398` | `SubmitDualAnimatedTextureLayout` | Builds two frame-driven texture packet groups, advances an oscillator, and submits nine packets. [inferred] | `0x00241490` | `0x00207D80`, `0x0023E018`, `0x00207B50` |
| `0x0023F540` | `SubmitSingleTexturePacketA` | Builds one frame-indexed texture packet from layout A and submits nine packets. [inferred] | `0x002412E0` | `0x00207D80`, `0x00207B50` |
| `0x0023F630` | `SubmitSingleTexturePacketB` | Builds one frame-indexed texture packet from layout B and submits nine packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023F720` | `SubmitThreeTexturePacketsB` | Builds three frame-indexed texture packets from layout B and submits 13 packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023F810` | `SubmitFourTexturePacketsB` | Builds four frame-indexed texture packets from layout B and submits 17 packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023F900` | `SubmitFiveTexturePacketsB` | Builds five packet entries, updates a float from the animation counter, and submits 21 packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023FAD0` | `SubmitFiveTexturePacketsC` | Builds five frame-indexed texture packets from layout C and submits 21 packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023FBC0` | `SubmitSingleTexturePacketC` | Builds one frame-indexed texture packet from layout C and submits nine packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x0023FCB0` | `SubmitConditionalTexturePackets` | Builds one or two texture packet groups according to a shared state query, then submits nine packets. [inferred] | — | `0x00207D80`, `0x0023D778`, `0x00207B50` |
| `0x0023FF58` | `SubmitSingleTexturePacketD` | Builds one frame-indexed texture packet from layout D and submits nine packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x00240048` | `SubmitSingleTexturePacketE` | Builds one frame-indexed texture packet from layout E and submits nine packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x00240138` | `SubmitSingleTexturePacketF` | Builds one frame-indexed texture packet from layout F and submits nine packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x00241688` | `SubmitAnimatedTexturePacketA` | Builds a frame-indexed texture packet from layout A and submits five packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x00241760` | `SubmitAnimatedTexturePacketB` | Builds a frame-indexed texture packet from layout B and submits five packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x00241838` | `NoOpEffect` | Performs no action. [inferred] | `0x00241490` | — |
| `0x00241840` | `SubmitAnimatedTexturePacketC` | Builds a frame-indexed texture packet from layout C and submits five packets. [inferred] | `0x00241490` | `0x00207D80`, `0x00207B50` |
| `0x00241918` | `SubmitCounterTexturePacketA` | Builds a texture packet indexed by the effect counter and submits five packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x002419F0` | `SubmitCounterTexturePacketB` | Builds a texture packet indexed by the effect counter and submits five packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x00241AC8` | `SubmitCounterTexturePacketC` | Builds a texture packet indexed by the effect counter and submits five packets. [inferred] | — | `0x00207D80`, `0x00207B50` |
| `0x00241BA0` | `NoOpEffectB` | Performs no action. [inferred] | `0x00241490` | — |
| `0x002423B8` | `InitializeEffectPackets` | Initializes four fixed graphics packet words for a course effect. [inferred] | `0x00241490` | — |
| `0x002584B0` | `GsSetupFrameContext` | Emit 3 packet items via FUN_002793D0 (codes 0x8050/0x8130/0x8070, ctx=arg&1); likely GS framebuffer/scissor setup. [inferred] | — | `0x002793D0` |

