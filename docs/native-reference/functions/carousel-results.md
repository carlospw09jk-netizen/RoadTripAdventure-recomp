# Carousel & Results UI

The item/results carousel: slot transforms and GS packets, quad builders for carousel cards, stat text/bars panels, and the results carousel menu tasks (player select, confirm and prompt dialogs, submenus).

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00243B38` | `SetupCarouselSlotTransforms` | Loops 16 slots: sets draw state via FUN_0021d788, copies VU0 vectors and writes 0xff0 scale words | `0x002455C0` | `0x0021D788`, `0x0021DAB8`, `vu0CopyVector@002729d0` |
| `0x00243C50` | `BuildCarouselSlotPackets` | Builds per-slot GS draw packets for 16 carousel entries from item tables; uses GetPartValueU16 and FUN_00223810 [inferred] | `0x00246250` | `0x0021D788`, `0x0021DAB8`, `0x00223810`, `0x00243A00`, `vu0CopyVector@002729d0` |
| `0x00244078` | `LayoutRaceRowQuads` | Fills 16 rows of 4-vertex quad data (positions, colors) from race tables, table chosen by flag [inferred] | `0x002455C0` | `0x0021D788`, `0x0021DAB8` |
| `0x002443D0` | `BuildCarouselQuadA` | Writes quad vertex data for a carousel card using cos-based scale and per-slot offsets [inferred] | `0x00246250`, `0x002455C0` | `0x0000000Cos@00274810` |
| `0x00244538` | `BuildCarouselQuadB` | Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred] | `0x002455C0` | `0x0000000Cos@00274810` |
| `0x002446A8` | `BuildCarouselQuadC` | Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred] | `0x00246250` | `0x0000000Cos@00274810` |
| `0x00244800` | `BuildCarouselQuadD` | Variant of BuildCarouselQuadA with different offsets; writes 4 vertices for a carousel card [inferred] | `0x002455C0` | `0x0000000Cos@00274810` |
| `0x00244988` | `BuildCarouselDrawPacket` | Builds packed transform/GS register words for a carousel item, then sets up camera and light direction [inferred] | `0x002455C0` | `0x0000000FUN_0021db48`, `0x0000000FUN_0021f910`, `0x0000000BuildCamera@0021f698`, `updateLightDirection@002261a0`, `vu0CopyMatrix@002729e0` |
| `0x002477A8` | `DrawCarouselStatText` | Draws stat labels and numeric values for a carousel item via text-draw helpers [inferred] | `0x00248180` | `0x0000000FUN_00201e48`, `0x0000000FUN_00203fc8`, `0x0000000FUN_00204310`, `0x0000000FUN_00204568`, `0x0000000FUN_002046c0` |
| `0x00247AC8` | `DrawCarouselStatBars` | Draws stat values for one item: sums 20-byte table slices and draws two text readouts [inferred] | `0x00247BF0`, `0x00247E38` | `0x0000000FUN_00201e48`, `0x0000000FUN_00204310` |
| `0x00247BF0` | `DrawCarouselSlotPanel` | Draws a carousel slot panel (header, icons, stat rows) by calling DrawCarouselStatBars [inferred] | `0x00248180` | `0x0000000FUN_00203b78`, `0x0000000FUN_00204310`, `0x0000000FUN_00204568`, `0x0000000FUN_0023db00`, `0x0000000DrawCarouselStatBars@00247ac8` |
| `0x00247E38` | `DrawTwoPlayerResults` | Draws 2-player race results table UI, printing scores and calculating total sums for both players [inferred] | `0x00248180` | `0x00201E48`, `0x00203B78`, `0x00204310`, `0x00204568`, `0x0023DB00`, `0x00247AC8` |
| `0x00248180` | `UpdateRaceResultsMenu` | Task update function for quick race and 2-player race results screen, handling inputs and screen transitions [inferred] | `0x002455C0` | `0x00200DC0`, `0x00203990`, `0x002477A8`, `0x00247BF0`, `0x00247E38`, `0x00258E50`, `0x00204DE8` |
| `0x00248398` | `UpdateCarouselConfirmDialog` | Task update function displaying a two-option confirmation dialog in the carousel menu and handling confirmation [inferred] | `0x002455C0` | `0x00203990`, `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x0022EDB8`, `0x00258E50`, `0x00204DE8`, `0x00204C48` |
| `0x00248580` | `UpdateCarouselPlayerSelect` | Task update function for carousel player/entry selection menu, managing 1P/2P choices and spawning sub-tasks [inferred] | `0x002455C0` | `0x00203990`, `0x002058A8`, `0x00205C08`, `0x00205C20`, `0x0020AD18`, `0x0022C280`, `0x0023DB00`, `0x00258E50`, `0x00204DE8`, `0x00204C48` |
| `0x00248A70` | `UpdateCarouselPromptDialog` | Task update function that spawns a prompt dialog task (0x239150) and handles return result or cancellation [inferred] | `0x002455C0` | `0x00203990`, `0x00258E50`, `0x00204DE8`, `0x00204C48` |
| `0x00248BF0` | `UpdateCarouselSubMenu` | Task update function for a 3-item carousel sub-menu, handling item navigation, sound effects, and task spawning [inferred] | `0x002455C0` | `0x00203990`, `0x002058A8`, `0x00205AD0`, `0x00205C08`, `0x00205C20`, `0x00258E50`, `0x00204DE8`, `0x00204C48` |

