# Scene Expansion (research build)

Shows real scenery on both sides of the original 256-pixel view, at the same
pixel scale. The gameplay camera does not move. The game draws its original
view, and the mod draws the extra tile columns around it.

Status: research. It passes the one-room proof in
`research/scene_expansion/README.md`. It is not ready for normal play.

## What it changes

- The stage draw and display areas, from 256 PS1 pixels to 256 + 2 x margin.
- The front-end stretch rectangle, by the same factor, so pixels keep their shape.
- Extra 16x16 tiles in the margins, from a private sprite pool.
- The player HUD splits at the view centre. The left group docks to the left
  edge of the picture and the right group to the right edge, as in the
  ActRaiser action HUD.
- Untextured fades and flashes that cover the original width are widened.
- Other screen-space parts outside the original view stay hidden.

## Responsive margin

The margin is Auto by default. Every frame the mod reads the front-end frame
and the stretch rectangle that the game or player chose. It then picks the
widest picture, at the same pixel scale, whose sides stay inside the frame.
The maximum is 128 pixels per side, the VRAM limit.

| Stretch preset | Auto margin per side |
|---|---|
| PSX Default | 72 |
| PSX Big | 50 |
| 16:10 Huge | 12 |
| 16:10 Extreme | 0 (no expansion) |

The front-end frame is the guest video mode, 1280x720 by default. The SDK
letterboxes that frame into the window, so a window that is not 16:9 does not
get more scenery.

It never writes camera, scroll, layout or entity state.

## Research controls

This build also contains test controls. They must move to a separate dev mod
before any release.

- It replaces controller 0 with a scripted pad.
- It reads commands from `$SCENE_PROBE_DIR/cmd`. The default directory is
  `/tmp/nocturne-expand/ctl`.
- Expansion starts off. Send `expand on` to turn it on.
- `margin auto|<n>`, `dock on|off`, and `status` change and report the layout.

## Limits

- Vanilla v1.4.5 only. The guest addresses are hardcoded.
- Single buffer: the game draws and displays the same VRAM buffer.
- The HUD groups are found through the player-HUD record at `0x82E86BF0`.
  Only the Richter HUD is tested. Alucard's HUD is untested.
- `graphics_settings` can rewrite the stretch rectangle every frame. The mod
  then treats each rewrite as a new base. This is untested.
