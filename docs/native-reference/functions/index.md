# Native Function Reference

Per-function documentation for the Road Trip native code, generated from `docs/native-reference/functions.tsv` (731 functions). Each module page lists its functions in address order with what the decompilation says they do, who calls them, and what they call.

Functions whose names are still `UNKNOWN_` placeholders are marked *(unresolved)*; their descriptions are inferred from decompilation notes.

## Modules

| Module | Contents | Functions |
| --- | --- | --- |
| [Boot & System Init](boot-init.md) | Low-level PlayStation 2 bring-up: syscall stubs, GS double-buffer setup, and the `Ps2InitSystem` IOP reboot / module-load sequence. | 10 |
| [Task System](task-system.md) | The core of the game's cooperative task scheduler: 48 fixed task slots, task records with cleanup callbacks, and recursively-linked task trees. | 21 |
| [Menu Flow & Course Selection](menu-core.md) | Top-level menu flow tasks that spawn the selected menu mode, the generic menu panel / selection / navigation task runners, and the procedural grid and list drawing routines shared by most screens. | 28 |
| [Pause & Options Menus](menu-pause-options.md) | The pause menu tree — main menu, submenus, restart/quit confirmation — plus audio settings, controller button remapping, the world map page, and the per-frame options screen loop. | 17 |
| [Controller Input](input-controller.md) | Pad reading, controller input mapping, and the shared transient input state that menu and vehicle code consume each frame. | 10 |
| [UI Layout & Text Drawing](ui-widgets-layout.md) | Low-level 2D drawing and layout helpers: digit counters, dialog text lines, rectangle centering, text measurement/layout, and small conditional widget draw wrappers. | 11 |
| [Math, Vectors & Number Formats](math-transforms.md) | VU0-assisted matrix and vector transforms, fixed-point world-position decomposition and grid-cell helpers, bit counting / bit indexing, and packed-BCD digit formatting. | 29 |
| [Collision & Terrain Queries](collision-terrain.md) | World mesh intersection tests (point, segment, quad), car-vs-car and car-vs-object collision detection with impact response, and arena wall bouncing. | 14 |
| [Vehicle Physics & Steering](car-physics-steering.md) | The per-frame vehicle tick: input acquisition, steering damping, suspension flex, wheel ground contact, motion integration, and state reset — plus replay control-state save/restore and car spawning/spec loading. | 18 |
| [Car AI & Waypoints](car-ai.md) | Waypoint-path navigation for AI cars: target angle computation, progress/lateral error tracking, steering and throttle output, route table setup, and the AI data save file. | 10 |
| [Chase Camera](chase-camera.md) | The camera state machine: smoothed chase-cam basis, terrain collision avoidance, preset cycling and loading, zoom transitions, cutscene wait states, free camera, and view matrix construction. | 18 |
| [Graphics Packet & Texture Helpers](render-texture-helpers.md) | DMA/GIF packet slot builders, VIF uploads and kicks, and the family of animated / counter-driven texture packet submitters used by course effects and HUD animations. | 46 |
| [Car Model Rendering](car-model-render.md) | Showroom and in-world car model drawing: light/env and color-palette uploads, car render register templates, the curated showroom rig, and attachment point selection. | 10 |
| [Car Particle Effects](car-effects-particles.md) | Car-attached particle effects — dust puffs, smoke, sparks and rotational debris — including the effect slot allocator and per-frame emitter stepping. | 9 |
| [Scene Rendering & Lighting](scene-render-lighting.md) | Scene-level rendering setup: terrain tile streaming, the faded arc ribbon, fog and sky colour selection, light matrix upload/scaling, and scene lookat/fog accessors. | 7 |
| [Animation Phases](animation-phases.md) | Motion clip playback and the small phase machines that step object animations, tween spin counters and classify object contexts. | 4 |
| [Memory Card & Save Data](memory-card-save.md) | The memory-card state machines (get info, read, create, format) and their localized chooser/confirm/spinner dialogs, plus progress-RAM defaults and the save-progress writer. | 24 |
| [Town Exploration & Locations](town-exploration.md) | Town mode: block and entity spawning from car/resident tables, resident dialogue tasks, hub arrival with location asset loading, house entry, and the visited-house menu. | 14 |
| [Race Mode](race-mode.md) | Race setup and lifecycle: HUD/viewport and rival spawning, rank and lap/split record tables, countdown timers, course events, zone classification, and race result recording. | 20 |
| [Race HUD & Scoreboard](race-hud-scoreboard.md) | In-race HUD rendering: the per-vehicle HUD dispatch, speedometer / tachometer / boost gauges, gear and race-time formatting, and the score counter readouts. | 18 |
| [Object & HUD State Setup](object-state-setup.md) | The typed object/HUD registry: many small initializers that clear a 0x8cb0-byte state block and configure sprites, road/track data, HUD elements, or a type tag, plus the type dispatch table. | 60 |
| [Race Results & License Screens](race-results-license.md) | The results screens: standings and per-player result drawing, license award screen, records screen, player name slots, best-time/score updates, and award messages. | 17 |
| [Carousel & Results UI](carousel-results.md) | The item/results carousel: slot transforms and GS packets, quad builders for carousel cards, stat text/bars panels, and the results carousel menu tasks (player select, confirm and prompt dialogs, submenus). | 17 |
| [Day/Night Cycle](day-night-cycle.md) | The 24-hour time-of-day tick, per-frame time-of-day lighting application, and the area intro / day-cycle step tasks that sequence lighting and BGM per step. | 5 |
| [Collection, Flags & Slot Queries](collection-flag-queries.md) | Progress, stamp and state bit flags; flagged-slot scanning and scoring; unlock conditions; the shared byte queues; actor slot helpers; and the knock/mail/hint collectible mask gates. | 50 |
| [Script & Dialogue](script-dialogue.md) | The town/shop event script interpreter, message and dialog display tasks, escape-code text rendering, name token substitution, and the number/amount editor widgets. | 23 |
| [Golf Mini-Game](minigame-golf.md) | The 9-hole golf mini-game: ball launch and physics with terrain bounce, cup attraction and sinking, course/pars tables, hole intro banner and scorecard drawing. | 10 |
| [Stunt Cards & Name Entry](minigame-stunts.md) | The stunt-card flow (stunt list with per-stamp awards, select menu, results/HUD screen) and the character-grid name entry with its D-pad navigation and commit tasks. | 9 |
| [Map Props & Pickups](map-props-pickups.md) | Animated misc-model map prop renderers gated by progress flags, with proximity pickup SFX/counter logic, plus map coin collection. | 13 |
| [Mini-Game Event Tasks](minigame-events.md) | The mini-game event state machines: stamp collection, balloon pop, goal-plane and finish-zone polygon tests, tug-of-war, split-camera runs, and the race-course mini-game flow. | 17 |
| [Slot & Betting UI](slot-betting-ui.md) | The slot-machine betting flow: menu cursor resolution, bet-amount selection, yes/no confirmation dialogs, reel result state, and the payout/results screen that credits funds. | 9 |
| [Fades, Intros & Showcases](scene-fades.md) | Screen fade ramps shared by menus and cutscenes, the intro/long fade sequence tasks, and the scene intro and showcase render tasks. | 8 |
| [Sport Score Displays](sport-score-hud.md) | Mini-game sport scoring overlays — ski jump, curling, figure-eight, discipline leaderboards — plus medal/wheel widgets, course progress scorecards and the result-deck callbacks. | 20 |
| [Shop, Tuning & Garage](menu-shop-ui.md) | Shopping and tuning flows: the tuning garage scene, car shop visits, item shop buy/sell dialogs and state machine, item browsing, shop row drawing, and the money-spending helper. | 16 |
| [Paint Editor](paint-editor.md) | The car paint editor: swatch grid sprites, RGB nibble digit drawing and editing, confirm-with-payment flow, and the window slide-in/slide-out animations. | 9 |
| [Street Scenes & NPC Cars](street-scenes-npc.md) | Street scene tasks: door menu entity spawning from the doors table, the street scene dialogue/arrival state machine, and NPC tail-car transform updates near the player. | 3 |
| [Course Effects](course-effects.md) | Course-level scripted effects: proximity-triggered events, traffic hazards, conditional effect task spawning, and the nearby-car proximity visual/audio effect. | 5 |
| [Data Table Lookups](data-table-lookups.md) | Indexed accessors for the game's static tables — label strings, integer fields, part values and car slot entries — used by the shop and carousel UI. | 35 |
| [Debug Text & Adjuster](debug-text.md) | Text-API debug print helpers for doors, cameras, vectors and map coordinates, plus the debug adjuster menu that nudges offsets/scales and dumps its state. | 14 |
| [Sound RPC & Playback](sound-rpc.md) | The SIF RPC client for the IOP sound driver: command wrappers, BGM/SE playback helpers, volume control, and vehicle sound effects (tires, horn/boost loops). | 53 |

**Total: 731 functions across 40 modules.**

