# nocturne-encore

A fork of [birabittoh/NocturneRecomp-Mods](https://github.com/birabittoh/NocturneRecomp-Mods)
with extra NocturneRecomp enhancements. Each enhancement lives in its own
`src/<name>/` folder, in the same layout as upstream. Enhancements that
upstream accepts go there. The others stay here.

This file is the only fork-specific file at the repository root. Upstream
files stay unchanged, so upstream merges stay clean.

## Enhancements

| Folder | Status | Upstream |
|---|---|---|
| `src/scene_expansion/` | Research. One-room proof passed. | Not offered yet |

## Track upstream

The `upstream` remote points at birabittoh/NocturneRecomp-Mods. Its push URL
is disabled.

```sh
git remote add upstream git@github.com:birabittoh/NocturneRecomp-Mods.git
git remote set-url --push upstream DISABLED
git fetch upstream
git switch main
git rebase upstream/main
git push --force-with-lease origin main
```

To offer one enhancement upstream, branch from `upstream/main`, copy only its
`src/<name>/` folder, and open the pull request from that branch.

## Scene expansion: remaining work

1. Licence. The margin-tile code now follows the recompiled routine
   `sub_82258190`, not sotn-decomp (AGPL-3.0). Confirm this before any
   upstream offer.
2. Ask the maintainer whether to send it as a mod or as an in-app feature.
3. Move the test controls (scripted pad, command file, snapshot, freeze) to a
   separate dev mod.
4. Look up addresses through `game_symbols`. Add the title-update build.
5. Add an on/off setting that persists and changes live.
6. Done: Auto margin from the front-end frame and the stretch preset, live.
   Open: a window that is not 16:9 is letterboxed by the SDK.
7. Work with `graphics_settings`, which rewrites the stretch rectangle every
   frame. Its `graphics_settings.preset_applied` event is a likely hook.
8. Double buffering. Two 384-wide buffers do not fit below the texture pages
   at x=512.
9. Full-screen fades, flashes and letterbox bars. Expected to stay 256 wide.
   Not checked.
10. Decide what the margins show: early reveals (Dracula on his throne) and
    pop-in at the margin edge.
11. Rooms with invalid tile data outside the room, and repeating layers.
12. Done for the Richter HUD: left and right groups dock to the picture
    edges. Open: the boss gauge in view, Alucard's HUD, widened fades.
13. Native framing in the pause menu, map, inventory, cutscenes, room
    transitions, game over and title screen.
14. Same-frame and activation checks across many room types, including the
    inverted castle.
15. The built-in native renderer. Only Xenos is tested.
16. Builds for windows-x64, linux-x64 and linux-arm64. Performance on the Odin.
17. Unit tests for the size calculation that run without game files.
18. Release packaging: manifest, icon, README, before and after images.
19. Ship a pinned release in the Korri Nocturne plugin.
