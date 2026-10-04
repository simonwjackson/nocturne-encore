# NocturneRecomp scene expansion: one-room proof

Date: 2026-10-03. Status: research proof. Not a shippable mod.

## Result

One room renders real scenery around the original view, at the same pixel
scale, with no stretch. The proof uses the Richter prologue: the castle keep
stairs, then the corridor to the throne. A code mod, loaded by the unchanged
NocturneRecomp v1.4.5 release binary, does all of the work. No fork or rebuild
of NocturneRecomp was necessary.

| Check | Result | Evidence |
|---|---|---|
| Same game frame, expansion off and on, original 256x207 view in VRAM | 0 of 52,992 pixels differ | `evidence/stairs-same-frame/side.png` |
| Same frame on screen, original rectangle (232,54)-(1048,666) | Interior identical. Only the rightmost edge column (1 px, 323 pixels) differs, from bilinear sampling of the new neighbour pixels | `screen-off.png`, `screen-on.png` |
| Extra area | 128 extra columns (64 per side), 26,496 of 26,496 pixels drawn | same |
| Pixel aspect | 384 source px into 1224 screen px. That is 3.1875 px per source px, the same as 256 into 816 | stretch rectangle (28,54)-(1252,666) |
| Enemy and object activation | 960 frames of scripted play from one state snapshot. 12 spawn or despawn events. Camera, player and every byte of all 256 entity slots match between off and on, frame by frame. An off/off control also matches | `evidence/walk-activation/*.trace` |
| Final frame after 960 independent frames | Original view identical (0 pixels). Entity tables byte-identical | `evidence/walk-activation/final-frame-side.png` |

All checks ran on x86_64 with Mesa llvmpipe under Xvfb. The Odin and the Mini V2 were not used.

## Sources

Addresses and structure offsets come from the recompiled XBLA code
(`rexglue codegen` output) and from live guest memory. Function names such as
`RenderTilemap` follow the sotn-decomp project only as labels for
cross-reference. No sotn-decomp code is in this repository. The margin-tile
code follows the recompiled routine `sub_82258190`.

The mod source is `src/scene_expansion/`. The proof results below came from
the same code before the margin-tile function was rewritten from the
recompiled routine. Its results were then checked again (see "Re-check").

## How the XBLA port draws a frame

These facts come from the recompiled code and live memory dumps. Addresses are
for the vanilla image. A title-update build moves them.

- The PS1 game code survives almost unchanged. `RenderTilemap` is
  `sub_82258190`, `RenderPrimitives` is `sub_8223B798`, `UpdateGame` is
  `sub_822521C0`, and the main-loop iteration is `sub_8224DCE8`.
- A software PS1 GPU object (pointer at `0x83133B00`) rasterizes ordering
  tables into an emulated 1024x512 16-bit VRAM at object offset 4. `DrawOTag`
  is vtable slot `sub_824FF3D0`. It honours the PS1 `DRAWENV` clip and offset.
- The stage uses two 256x240 buffers at VRAM x=0 and x=256
  (`g_GpuBuffers` at `0x8318F660` and `0x831AB158`). Rows 240 to 255 hold CLUTs.
  Texture pages start at x=512. Sprite sheets fill y=256 and below.
- The front end samples the `DISPENV` rectangle from VRAM into the stretch
  rectangle at `0x82882C68`. The NocturneRecomp "gameplay preview" texture is
  this VRAM region.

## How the proof expands the scene

The model is ar-recomp PR #2. The camera never moves. The game draws its
original view, and the mod draws around it.

1. Both stage buffers use one 384-wide buffer at VRAM x=0. Clip width is 384
   and the draw offset is x=64. Every primitive moves 64 px right.
2. The display rectangle is 384 wide. The stretch rectangle grows by 384/256
   around its centre.
3. Before the original `RenderTilemap` runs, the mod adds 16x16 sprites for
   the tile columns outside the original 17. It does this for the foreground
   and all 16 background layers. The sprites come from a private guest pool,
   so the game's 640-sprite budget does not change. Because the original call
   runs after, its texture-page primitives stay first in each ordering-table
   slot.
4. Screen-space primitives (`DRAW_ABSPOS`) that lie wholly outside the
   original view stay hidden. The game parks HUD parts there, for example the
   Richter boss gauge at x=264. This is the fail-closed rule from ar-recomp PR
   #1. The mod sets `DRAW_HIDE` for the call and restores it after.
5. The mod writes no camera, scroll, layout or entity state.

## Known limits

- Single buffering. The game now draws and displays the same VRAM buffer. No
  tearing showed in llvmpipe captures. Real GPUs and the Odin are unverified.
- Existing offscreen entities become visible early. Dracula sits on his throne
  in the left margin before the original view reaches him. Objects that the
  game spawns later can pop in at the margin edge.
- The margin is fixed at 64 px. That gives 16:9 at the PSX Default preset.
  Larger presets, such as 16:10 Huge, can push the rectangle off screen.
- Interaction with a persisted `graphics_settings` preset is untested. That
  code rewrites the stretch rectangle every frame.
- Menus keep native framing. The mod only acts when both buffers hold the
  exact stage layout.
- Partly visible parked HUD parts would show their visible part.
- Only two prologue rooms were tested.

## Reproduce

The tools expect `/tmp/nocturne-expand` with the pinned SDK, a codegen
checkout, and the owner's extracted assets. `shell.nix` provides the tools.

1. Build the mod: `tools/build-mod.sh ../../src/scene_expansion scene_expansion <rundir>`.
2. Start Xvfb on `:77`. Then run `tools/session.sh <rundir> 7200 --enabled_mods=scene_expansion`.
3. Run `tools/to-prologue.sh`. Check that `dump 8314EEC0 8` shows a non-zero player.
4. Run `tools/ab.sh <label>`, then `tools/compare.py ab/<label>`.
5. Run `tools/activation.sh <label> scripts/walk-long.txt`.

The mod reads commands from `$SCENE_PROBE_DIR/cmd`. It also replaces pad 0
with a scripted pad.

## Re-check after the rewrite

The margin-tile function was rewritten from the recompiled routine
`sub_82258190` before the move to this repository. Both checks ran again on
2026-10-03 with the build from `src/scene_expansion/`.

| Check | Result |
|---|---|
| Same frame, stairs, original view in VRAM | 0 of 52,992 pixels differ |
| Same frame on screen, interior (236,104)-(1044,616) | 0 of 413,696 pixels differ |
| Margins | 26,496 of 26,496 pixels drawn |
| 960-frame activation A/B, 12 spawn or despawn events | 0 mismatching frames. Off/off control also 0 |
| Final frame and entity tables | 0 pixels differ in the original view. Tables byte-identical |

Images: `evidence/recheck/`.
