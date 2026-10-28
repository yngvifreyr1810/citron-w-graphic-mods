# Unreleased: complete author packages

IPS/RomFS archives are now installed as a single Add-on across games and sources. The installer preserves each payload file, combines the author's ExeFS/IPS and RomFS components under one mod directory, and exposes one switch for the whole package. Conflicting alternative files in a single archive stop installation instead of silently overwriting one another.

The cucholix/NX-IPS-romfs-compilation adapter now uses complete `release_*` archives with their original descriptions and credits. Development pchtxt files and UE_ini source archives are not presented as additional effects. Compatibility remains game version / Build ID dependent; upstream Switch hardware testing is not a claim of Citron runtime compatibility. Multipart archives are not currently offered.

Description metadata is tied to the archive's Git blob SHA. When it no longer matches a refreshed archive, version compatibility remains unverified rather than guessing from a potentially inaccurate filename. In particular, Odyssey's `release_1.0.3.rar` is described upstream as targeting game v1.3.0.

Properties > Add-ons displays the package's name, description and source when available. All IPS and RomFS components share the existing folder-level enabled state. Older independently installed mods remain independent; no unverified collection of legacy mods is automatically merged.

Validation: 23 focused Qt checks pass, including actual independently downloaded Odyssey and TOTK RAR archives installed into temporary directories. To run the optional upstream archive check, set `CITRON_BUNDLE_FIXTURES` to a directory containing `SMO-release_1.0.3.rar` and `TOTK-release_1.2.1.rar`; 7-Zip is required. This verifies package integrity and installation, not gameplay performance.

Beta v0.1 remains the existing published snapshot. These changes are in the development build.
