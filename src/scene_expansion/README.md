# Scene Expansion (research build)

Shows real scenery on both sides of the original 256-pixel view, at the same
pixel scale. The gameplay camera does not move. The game draws its original
view, and the mod draws the extra tile columns around it.

Status: research. It passes the one-room proof in
`research/scene_expansion/README.md`. It is not ready for normal play.

## What it changes

- The stage draw and display areas, from 256 to 384 PS1 pixels wide.
- The front-end stretch rectangle, by the same factor, so pixels keep their shape.
- Extra 16x16 tiles in the margins, from a private sprite pool.
- HUD parts that the game parks outside the original view stay hidden.

It never writes camera, scroll, layout or entity state.

## Research controls

This build also contains test controls. They must move to a separate dev mod
before any release.

- It replaces controller 0 with a scripted pad.
- It reads commands from `$SCENE_PROBE_DIR/cmd`. The default directory is
  `/tmp/nocturne-expand/ctl`.
- Expansion starts off. Send `expand on` to turn it on.

## Limits

- Vanilla v1.4.5 only. The guest addresses are hardcoded.
- Single buffer: the game draws and displays the same VRAM buffer.
- The margin is fixed at 64 pixels per side.
