# Scene Expansion

Shows real scenery on both sides of the original 256-pixel view, at the same
pixel scale. The gameplay camera does not move. The game draws its original
view, and the mod draws the extra tile columns around it.

Status: early. It passes the one-room proof in
`research/scene_expansion/README.md`. Only the prologue is tested.

When the mod is enabled, expansion is on, the margin is Auto and the HUD is
docked. Disable the mod in the F1 mod manager to turn it off.

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

## Research mode

The test controls are off unless `$SCENE_PROBE_DIR` names a control
directory. `research/scene_expansion/tools/session.sh` sets it.

- The mod adds a scripted pad for controller 0.
- It reads commands from `$SCENE_PROBE_DIR/cmd`.
- Expansion starts off. Send `expand on` to turn it on.
- `margin auto|<n>`, `dock on|off`, and `status` change and report the layout.

## Limits

- Vanilla v1.4.5 only. The guest addresses are hardcoded.
- Single buffer: the game draws and displays the same VRAM buffer.
- The HUD groups are found through the player-HUD record at `0x82E86BF0`.
  Only the Richter HUD is tested. Alucard's HUD is untested.
- `graphics_settings` can rewrite the stretch rectangle every frame. The mod
  then treats each rewrite as a new base. This is untested.
