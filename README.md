# citron-mods-by-yngvifreyr1810

**Beta v0.1** · A PC-focused Citron community fork by yngvifreyr1810.

Graphic Packs is the first feature of this project. Further modding tools and quality-of-life improvements will follow as the project develops. This is an independent fork, not an official Citron release.

## Features

- Cemu-inspired Graphic Packs: select a game, then Graphics or Mods, and configure the available effects on the right.
- A curated community catalogue with installed-version / Build ID filtering, source attribution, managed downloads and backups.
- Native Tears of the Kingdom settings powered by NX Optimizer / UltraCam, with compact controls and preservation of other saved NX settings.
- Compact library Add-ons summaries with full details in tooltips.
- Discord Rich Presence showing the fork name, Beta v0.1 and the current game or library status.

## Getting started

Download the Windows x64 archive from [Releases](https://github.com/yngvifreyr1810/citron-w-graphic-mods/releases), extract to a fresh folder and run `citron-mods-by-yngvifreyr1810.exe`. Keep the DLLs and plugin folders together. The included empty `user` folder selects portable mode. No games, firmware, keys, saves or personal profiles are included.

Open **Emulation > Graphic packs** (also under Tools). Choose a game, then **Graphics** or **Mods**. Stop the game before applying changes. Expand **Description, compatibility and credits** when you need source details or warnings. Existing unlisted mods remain under the game's **Properties > Add-ons**.

This implements a similar workflow to Cemu, using Switch-specific patches. Cemu graphic-pack files cannot be loaded directly. Not every mod from the source repositories is approved or included.

For TOTK, select **Graphics**, load NX Optimizer settings, choose options and apply. Components download separately from the pinned upstream release. Applying uses 1× emulator scaling and extended memory when required; affected settings are backed up.

## Discord Rich Presence

Rich Presence is enabled with the project's public Application ID. Run Discord desktop, and allow activity sharing in Discord settings. It shows the project name, current game/library and Beta v0.1. Use **Tools > Discord Rich Presence** to disable it or change the public ID. It needs no bot token or account token. The main application name comes from Discord Developer Portal; the version is sent as activity state.

The local settings file is `user/config/citron-mods.ini` in portable mode. Game titles are shared with Discord while enabled. See [Discord RPC documentation](https://github.com/discord/discord-rpc/blob/master/documentation/hard-mode.md).

## Status and development

This is an early beta with a limited reviewed catalogue. Known inherited issues include Mario Kart 8 Deluxe white rendering on the development setup. This release does not claim to fix that renderer issue or guarantee every mod combination.

- [Beta v0.1 notes](docs/releases/Beta-v0.1.md)
- [Roadmap](docs/ROADMAP.md)
- [Build instructions and upstream](docs/README-UPSTREAM.md)
- [Windows build](docs/BUILDING-CITRON-WINDOWS.md)
- [Linux build](docs/BUILDING-CITRON-LINUX.md)

For the focused Qt tests, configure `tools/graphic_packs_tests` with Qt Widgets, Network, Concurrent and Test, build, then run `graphic_packs_tests -platform offscreen`. The beta changes do not require Discord SDK libraries; Rich Presence uses Qt local IPC.

## Credits and licences

Built on [Citron Neo](https://github.com/citron-neo/emulator) and the work of Citron/yuzu contributors. Cemu inspired the Graphic Packs interaction design. Existing copyright notices and the GPL licence remain in place; see [LICENSE](LICENSE) and [LICENSES](LICENSES).

Community sources and original author credits:

- [MaxLastBreath/nx-optimizer](https://github.com/MaxLastBreath/nx-optimizer): NX Optimizer 3.2.0 / UltraCam Calamity 3.0.0; separately downloaded assets retain CC BY-NC 2.0 terms and are not bundled.
- [theboy181/switch-ptchtxt-mods](https://github.com/theboy181/switch-ptchtxt-mods)
- [StevensND/switch-port-mods](https://github.com/StevensND/switch-port-mods)
- [yuzu-mirror/yuzu-mod-archive](https://github.com/yuzu-mirror/yuzu-mod-archive)
- [cucholix/NX-IPS-romfs-compilation](https://github.com/cucholix/NX-IPS-romfs-compilation)
- [ChanseyIsTheBest/NX-60FPS-RES-GFX-Cheats](https://github.com/ChanseyIsTheBest/NX-60FPS-RES-GFX-Cheats)
- HUGE to Codex and Qoder (when Codex is out of quota), they did 99% of the work.

Source inclusion is not a claim of blanket compatibility or permission to redistribute every mod. Downloaded mods retain their upstream licences and credits.
