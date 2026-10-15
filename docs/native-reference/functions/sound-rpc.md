# Sound RPC & Playback

The SIF RPC client for the IOP sound driver: command wrappers, BGM/SE playback helpers, volume control, and vehicle sound effects (tires, horn/boost loops).

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00217C68` | `UpdateCarTireSound` | Updates tire skid and surface friction sound effects and pitch based on vehicle speed, slip, and surface type. | `0x0021A310` | `0x002586A0`, `0x00258E50`, `0x00259070` |
| `0x00217F10` | `UpdateContinuousSoundEffect` | Manages start, loop, and stop sound playback for continuous audio effects (horn or boost) with countdown timer [inferred]. | `0x0021BA70` | `0x00258E50` |
| `0x0022C630` | `PlayEventSoundStub` | One-line handler that plays sound id 0x8035 via FUN_00258e50. | `0x0022C518` | `0x00258E50` |
| `0x00256AF8` | `InitSoundRpcDriver` | Boot-time sound init: binds RPC to IOP server 0x1234567, sends port/config calls (cmds 0,5,4), sets DGA/DMA2 registers, and registers bgm/SE file names via RPC cmds 6/7/0x32. [inferred] | `0x002003B8` | `0x002575E0`, `0x00279290`, `0x002793D0`, `0x0027E840`, `0x00287CD8`, `0x00287EA8`, `0x0027F1E0` |
| `0x002574A0` | `SoundRpcPlaySequence` | Sends RPC cmd 0x23 with a parameter then cmd 0x22 with a single entry to the sound driver, likely loading/starting a sequence. [inferred] | `0x00218458` | `0x00287EA8` |
| `0x002575E0` | `SoundRpcRegisterFiles` | Registers two file names per call via RPC cmd 0x32 at slot index*2 and *2+1, copying strings with sizes into the driver request buffer. [inferred] | `0x00256AF8` | `0x00287EA8`, `0x0027F1E0` |
| `0x00257760` | `BindAndCallRpcInit` | Binds SIF RPC 0x175e840 with key 0x1234567 and issues initial RPC call. | — | `0x00287CD8`, `0x00287EA8` |
| `0x002577C0` | `CallRpcCmd1` | Issues SIF RPC command 1 on service 0x175e840. | — | `0x00287EA8` |
| `0x00257800` | `SendDialogueWindowRpc4` | Packs dialogue window/cursor addresses into RPC buffer and sends RPC command 4. | — | `0x00287EA8` |
| `0x002578A8` | `SendDialogueWindowRpc5` | Packs dialogue window/cursor addresses into RPC buffer and sends RPC command 5. | — | `0x00287EA8` |
| `0x00257950` | `SendRpcWithStringArg` | Clears 0x40-byte RPC buffer, copies string argument via strcpy, sends RPC with dialogue state. | — | `0x0027E840`, `0x00287EA8`, `0x0027F1E0` |
| `0x00257A28` | `SendMusicPlayRpc` | Same packing as SendRpcWithStringArg; called from gs_music_play_bgm to send BGM play RPC [inferred]. | `0x00258EC8` | `0x0027E840`, `0x00287EA8`, `0x0027F1E0` |
| `0x00257BA8` | `SoundRpcGetStatus` | Sends RPC cmd 8 with arg 1 to the sound driver and returns the 32-bit value returned in DAT_0175f880 (status/result query). [inferred] | `0x00208F50` | `0x00287EA8` |
| `0x00257C68` | `SendRpcCmd9Value` | Packs 12-bit value and pointer into RPC buffer, sends RPC command 9. | `0x00259070` | `0x00287EA8` |
| `0x00257D18` | `SendRpcCmd9Pair` | Packs two 32-bit args into RPC buffer, sends RPC command 9. | — | `0x00287EA8` |
| `0x00257DD0` | `SndRpcCmdA` | Stages 0x40-byte request from args (incl. window/cursor globals) and sends sound-engine RPC cmd 0xA; no reply. [inferred] | — | `0x00287EA8` |
| `0x00257E80` | `SndRpcCmdB` | Stages 0x40-byte request from caller-supplied stack tail and sends sound-engine RPC cmd 0xB; no reply. [inferred] | — | `0x00287EA8` |
| `0x00257F30` | `MusicRpcPlayBgm` | Sends RPC cmd 0xe with two params to the sound driver; shared backend for all BGM-slot wrappers. [inferred] | `0x00258F30`, `0x00258F50`, `0x00258F70`, `0x00258F90`, `0x00258FB0`, `0x00258FD0`, `0x00258EC8` | `0x00287EA8` |
| `0x00257FD8` | `MusicRpcCmd15` | Sends RPC cmd 0xf with one param to the sound driver; lowest-level stop/reset wrapper used by BGM control. [inferred] | `0x00258EB0`, `0x00258FF0`, `0x00258EC8` | `0x00287EA8` |
| `0x00258080` | `SndRpcQueryCmd10` | Fills 0x40 send buffer from arg tail, sends RPC cmd 0x10 with 0x40 readback at 0x175f880; returns first dword. [inferred] | — | `0x00287EA8` |
| `0x00258138` | `SndRpcReadBlock1F` | RPC cmd 0x1F reads 0x40 bytes into 0x175f880 then copies all 64 bytes to caller's out struct. [inferred] | — | `0x00287EA8` |
| `0x00258218` | `SndRpcCmdCArgs` | Builds 0x40 request (dword0=0x3F1 plus 9 args from tail) and sends sound-engine RPC cmd 0xC; no reply. [inferred] | — | `0x00287EA8` |
| `0x00258308` | `SndRpcPlayOrStop` | Arg 0 (& 0x8000FFFF mask) -> RPC cmd 1 (stop all); nonzero arg staged as 0x40 request for RPC cmd 0xD (play). [inferred] | `0x00258E50` | `0x00287EA8` |
| `0x002583F0` | `SetVolumeDgaRegs` | Writes two DGA2 registers (0x981, 0xa81) with the given volume values via FUN_002793d0; used by the options volume screen for BGM and SE. [inferred] | `0x002090C0` | `0x002793D0` |
| `0x00258578` | `SoundInitVolumes` | Stop/reset sound then sets both channels 0 and 1 to value 0x1ccc via FUN_002585f0 (RPC 0x21); called during system init. [inferred] | `0x00205088`, `0x002052D0` | `0x002585B0`, `0x002585F0` |
| `0x002585B0` | `SoundRpcStopAll` | Sends a single RPC cmd 0x20 to the sound server (stop-all/flush voices). [inferred] | `0x00258578` | `0x00287EA8` |
| `0x002585F0` | `SoundRpcSetVolume` | Sends RPC cmd 0x21 with three params (id, value, value) to the sound server, setting a channel/voice volume pair. [inferred] | `0x00258578` | `0x00287EA8` |
| `0x002586A0` | `SndRpcCmd22` | RPC cmd 0x22 with 0x40 request; count arg forced to 1 when zero; called from scene code. [inferred] | `0x00217C68` | `0x00287EA8` |
| `0x00258768` | `SndRpcCmd24` | Stages 0x40 request whose first dword is the arg; sends sound-engine RPC cmd 0x24. [inferred] | — | `0x00287EA8` |
| `0x00258810` | `SoundRpcCmd25` | Sends RPC cmd 0x25 with one param to the sound server; start/open voice command used when clearing sound state. [inferred] | `0x00258FF0` | `0x00287EA8` |
| `0x002588B8` | `SoundRpcCmd26` | Sends RPC cmd 0x26 with one param to the sound server; used in pairs for channels 0 and 1 (likely fadeout/stop per channel). [inferred] | `0x00259050` | `0x00287EA8` |
| `0x00258960` | `SndRpcCmd30` | Fire-and-forget sound-engine RPC cmd 0x30, no payload. [inferred] | — | `0x00287EA8` |
| `0x002589A0` | `SoundRpcCmd31` | Sends RPC cmd 0x31 with two equal params to the sound server; used to push a master volume value derived from global amp. [inferred] | `0x002137B0`, `0x002590B0` | `0x00287EA8` |
| `0x00258A48` | `SoundRpcCmd33PlaySe` | Sends RPC cmd 0x33 with two params to the sound server; plays a sound by index (first param) given a volume/voice param. [inferred] | `0x002137B0`, `0x002590B0` | `0x00287EA8` |
| `0x00258AF0` | `SoundRpcCmd34` | Sends a parameterless RPC cmd 0x34 to the sound server (commit/trigger after Cmd33). [inferred] | `0x002137B0`, `0x002590B0` | `0x00287EA8` |
| `0x00258B30` | `SoundRpcCmd35` | Sends a parameterless RPC cmd 0x35 to the sound server (sound-system reset/stop). [inferred] | `0x002105B8`, `0x002137B0` | `0x00287EA8` |
| `0x00258B70` | `SndRpcCmd36` | Fire-and-forget sound-engine RPC cmd 0x36, no payload. [inferred] | — | `0x00287EA8` |
| `0x00258BB0` | `SndRpcCmd37` | Fire-and-forget sound-engine RPC cmd 0x37, no payload. [inferred] | — | `0x00287EA8` |
| `0x00258BF0` | `SndRpcCmdCStruct` | Copies 56-byte struct (7 qwords incl window/cursor fields) to request buffer and sends sound-engine RPC cmd 0xC. [inferred] | — | `0x00287EA8` |
| `0x00258CA8` | `SndRpcCmdD` | Stages 0x40 request (first dword = arg) and sends RPC cmd 0xD; same opcode as play path of 0x00258308. [inferred] | — | `0x00287EA8` |
| `0x00258D50` | `SndRpcReadStatBlock` | RPC cmd 0xFFF0 reads 0x300 bytes to 0x175f880, copies 96 qwords to caller buffer (8-aligned fast path). [inferred] | — | `0x00287EA8` |
| `0x00258E50` | `PlayUiSound` | Plays a UI sound effect: calls FUN_00258308 with the given id OR 0x40400000 (float-encoded flag); ubiquitous beep/click wrapper with 125+ callers. | `0x00206D28`, `0x002083C0`, `0x00208428`, `0x00208848`, `0x00209520`, `0x00209848`, `0x00209CB0`, `0x0020A2B0`, `0x0020F3B8`, `0x002105B8`, `0x00212AC8`, `0x002135C8`, `0x002137B0`, `0x00213A78`, `0x00214470`, `0x00216E00`, `0x00217830`, `0x00217C68`, `0x00217F10`, `0x00218458`, `0x00219660`, `0x0021A990`, `0x0021B3D0`, `0x0021BA70`, `0x0021C998`, `0x0021CA90`, `0x00227380`, `0x00228708`, `0x00228B10`, `0x00228D10`, `0x002291C8`, `0x00229538`, `0x0022A6B8`, `0x0022ABA0`, `0x0022AEB0`, `0x0022C630`, `0x0022C8A0`, `0x0022CA28`, `0x0022CD38`, `0x0022D390`, `0x0022DAE8`, `0x00233A80`, `0x00236858`, `0x00236D28`, `0x00237080`, `0x00239260`, `0x00239378`, `0x00239508`, `0x00239748`, `0x00239900` | `0x00258308` |
| `0x00258EB0` | `BgmStop1` | Trivial wrapper calling FUN_00257fd8(1) (RPC cmd 0xf), likely stopping/asserting BGM channel 1. [inferred] | `0x00262678` | `0x00257FD8` |
| `0x00258F30` | `PlayBgmSlot1` | Wrapper calling FUN_00257f30(1,0) — plays BGM slot 1 via RPC cmd 0xe (used at scene starts). [inferred] | `0x0020A150`, `0x0022AEB0`, `0x0022CD38`, `0x0022DAE8`, `0x0022ECB8`, `0x00233A80`, `0x00251B60`, `0x00254430`, `0x00260DC0`, `0x002631A0`, `0x0026CB60`, `0x0026D2C0`, `0x002455C0` | `0x00257F30` |
| `0x00258F50` | `PlayBgmSlot2` | Wrapper calling FUN_00257f30(2,0) — plays BGM slot 2. [inferred] | `0x0020A2B0`, `0x0022C418`, `0x00251B60`, `0x00254430` | `0x00257F30` |
| `0x00258F70` | `PlayBgmSlot3` | Wrapper calling FUN_00257f30(3,0) — plays BGM slot 3. [inferred] | `0x0020F3B8`, `0x00239378`, `0x00239F68`, `0x0023A140`, `0x0026CCD0` | `0x00257F30` |
| `0x00258F90` | `PlayBgmSlot4` | Wrapper calling FUN_00257f30(4,0) — plays BGM slot 4. [inferred] | `0x0020F3B8` | `0x00257F30` |
| `0x00258FB0` | `PlayBgmSlot5` | Wrapper calling FUN_00257f30(5,0) — plays BGM slot 5 (used at menu/scene transitions). [inferred] | `0x0020A150`, `0x0022AEB0`, `0x0022CD38`, `0x0022DC68`, `0x0022EB78`, `0x00251B60`, `0x00254430`, `0x00254D20`, `0x0025A120`, `0x0025EB70`, `0x00261AC8`, `0x002631A0`, `0x0026CB60`, `0x002455C0` | `0x00257F30` |
| `0x00258FD0` | `PlayBgmSlot6` | Wrapper calling FUN_00257f30(6,0) — plays BGM slot 6. [inferred] | `0x00239378`, `0x00239F68`, `0x0023A140` | `0x00257F30` |
| `0x00258FF0` | `SoundStopAllChannels` | Stops both voices via FUN_00258810(0/1), sends BGM cmd with param 1, then clears all two sound handle slots in DAT_00335728; standard sound teardown. [inferred] | `0x0020F3B8`, `0x002105B8`, `0x0022DC68`, `0x0024FB38`, `0x0025A120`, `0x0025DBB8`, `0x0025E278`, `0x0025E630`, `0x0025EB70`, `0x0025F6A0`, `0x0025FBF8`, `0x0025FF00`, `0x00260638`, `0x00262678`, `0x002631A0`, `0x00263710`, `0x002638C0`, `0x00263E68`, `0x0026CCD0` | `0x00257FD8`, `0x00258810` |
| `0x00259050` | `SoundFadeOutChannels` | Calls FUN_002588b8 for channels 0 and 1 (RPC cmd 0x26 pair) — fades out/stops all sound channels. [inferred] | `0x0020F3B8`, `0x002105B8`, `0x0024FB38`, `0x00262678` | `0x002588B8` |
| `0x00259070` | `SoundStopVoiceByHandle` | Stops the voice whose handle is stored at (&DAT_00335728)[slot] by calling FUN_00257c68 with it, if nonzero. [inferred] | `0x00217C68` | `0x00257C68` |
| `0x002590B0` | `ApplySoundVolumes` | Computes scaled master volume from global amp DAT_01780a3e and pushes it via FUN_002589a0, then optionally plays a SE (DAT_01780a3d) followed by FUN_00258af0. [inferred] | `0x00210430`, `0x002105B8`, `0x002137B0` | `0x002589A0`, `0x00258A48`, `0x00258AF0` |

