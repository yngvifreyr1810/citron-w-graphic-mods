# citron-mods-by-yngvifreyr1810 — Beta v0.1

First public beta of a PC-focused Citron community fork. **Graphic Packs is the first feature; more modding and quality-of-life improvements are planned.**

## Included

- Open Graphic Packs from Emulation or Tools, choose a game, then Graphics or Mods.
- Category panels expose available effects; details, compatibility and credits expand on demand.
- Version/Build ID filtering, curated effects, managed installation and backups.
- TOTK NX Optimizer 3.2.0 / UltraCam Calamity 3.0.0 integration, with compact labelled settings and wheel-safe selectors.
- Existing NX settings outside the exposed controls are retained when applying.
- Compact Add-ons summaries; hover for the complete installed list.
- Discord Rich Presence: project name, Beta v0.1, current game or library status. Toggle/configure under Tools > Discord Rich Presence.

## Use

Extract the Windows x64 ZIP into a new folder. Run `citron-mods-by-yngvifreyr1810.exe`. Keep the bundled DLLs and plugin directories beside it. The empty `user` directory selects portable mode. Supply your own game files and required emulator data; none are included.

Stop emulation before applying packs. Select a pack for the exact installed update. Review existing mods for conflicts. Graphic Packs is inspired by Cemu's workflow; Cemu's Wii U graphic-pack files are not compatible with Switch games.

NX components download separately from their pinned upstream commit and retain their own licence. Their files are not bundled in this release.

## Validation and limits

The focused Qt suite passes 19 checks, including category navigation, cache invalidation, preservation of NX settings and scroll behaviour. The Windows app builds with Clang/Qt 6.9.3. Earlier manual TOTK 1.2.1 testing demonstrated the NX FPS setting in gameplay; this is not a broad compatibility or performance certification.

Known issues include inherited Mario Kart 8 Deluxe white rendering on the local setup and limited reviewed catalogue coverage. Not every upstream mod is exposed. Performance depends on game, mods and hardware. This beta does not claim to resolve inherited renderer defects.

## Credits

Citron/yuzu contributors; Cemu for interaction inspiration; MaxLastBreath for NX Optimizer/UltraCam; theboy181, StevensND, yuzu-mirror, cucholix, ChanseyIsTheBest and the original credited mod authors. See README and upstream source links for licensing and attribution.
