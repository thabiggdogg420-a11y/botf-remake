# Asset credits and presentation status

This personal project uses original prototype artwork, locally extracted BOTF reference data, and the community resources identified below. Ship presentation is provisional; it is not a complete modern five-faction model set.

## Icewolf132 Mesh Pack 1

Meshes and textures: Icewolf132. Original pack compiled 3 March 2009 for the Armada community. The included author terms permit editing, conversion and use in other games with attribution. Retain `ArtSource/Community/Icewolf132/README-original.txt` with any package containing these assets.

Additional design credit: Taldren for the Ulysses class and Lyran Tiger cruiser. The pack's README also credits Taldren for the Yamato design; no converted Yamato is currently included here.

Converted assets: Ranger, Ulysses, Lyran Tiger and three Reaver variants. Source mesh and texture hashes, helper-joint metadata, coordinate conversion and material approximations are recorded in `ArtSource/Community/Icewolf132/conversion-manifest.json`. Conversion script: `../setup/convert-community-models.py`.

The current combat renderer uses Ranger as an interim Federation illustration. It does not accurately depict every Federation class. The other converted models being present on disk does not mean they are used in combat.

## Original project studies

`ArtSource/FleetStudies` contains five procedural faction silhouette studies and their Blender source. These are temporary integration assets, not downloaded community ships or final artwork. The renderer uses these for the other four factions and as fallbacks.

The current 3D scene displays at most eight ships per side. The new tactical radar and ship list represent the entire encounter. Procedural stars, shield shells and effects remain provisional.

## Original BOTF reference material

Research tables derive from the locally available AIO 1.0.1 Vanilla overlay. Archive hashes and extracted table provenance are recorded under `../research/reference-local`; interpretation and uncertainty are documented in `../research/BETA-FIDELITY.md` and `../research/BOTF-RESEARCH.md`. Extracted names, descriptions and stats are reference data; their presence does not establish implementation of every original rule.

Star Trek and original BOTF material belong to their respective rights holders. Community-author asset terms do not transfer franchise ownership. No original soundtrack or voice library is claimed as newly licensed by this project.

## Resources not integrated

Bridge Commander Remastered, Trekmeshes models and the BlendSwap Defiant are research candidates only. They are not included or credited as acquired content; their individual reuse requirements remain unresolved.

## How credits are maintained (local-only asset pipeline)

The sections above list the only third-party assets committed to the repository. Every other asset (HD ship models, show audio, mod sounds and original BOTF voice files) is private and local only:

- `../assets/manifest.json` is the single source of truth. It lists candidate sources per asset key with the author, stated terms, a permission flag and import settings.
- `python ../assets/render_asset_docs.py` regenerates `../docs/ASSET-SOURCES.md` and the generated block below. Do not hand-edit that block.
- `Import-Assets.ps1` writes `LocalAssets/ASSET-CREDITS.local.md` (gitignored) for what is actually installed on a given PC. Keep that file with any private build that contains local assets.

<!-- GENERATED:manifest-credits BEGIN (render_asset_docs.py) -->

## Local-only asset candidates (generated from `../assets/manifest.json`)

The assets below are **not in this repository**. They are candidates the user may download for private, local use. `Import-Assets.ps1` writes what is actually installed on a PC, with the author and stated terms, to `LocalAssets/ASSET-CREDITS.local.md`. Full detail: `../docs/ASSET-SOURCES.md`.

| Key | First non-restricted candidate | Author | Stated terms |
|---|---|---|---|
| fed.galaxy | [Sketchfab: Star Trek TNG Galaxy Class Model](https://sketchfab.com/models/6029fefcfbd44908b2dfa9938cafecf5) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.sovereign | [Sketchfab: Star Trek Sovereign Class with Shuttlebay](https://sketchfab.com/models/f57fa908f23d498daf3d43d17d1b5f22) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.defiant | [Sketchfab: U.S.S. Defiant](https://sketchfab.com/models/8e875133c49240699d3b19de324826cf) | Feltman_Langer | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.nebula | [Sketchfab: Nebula-class cruiser](https://sketchfab.com/models/4fa2e93de1ba48ad9425c1e4228d7abf) | nestor_d | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.miranda | [Sketchfab: Star Trek Miranda Class](https://sketchfab.com/models/08b835fb9b8743cc8d22aec03ea05756) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.oberth | [Sketchfab: Star Trek - Oberth Class](https://sketchfab.com/models/c1a3bd9d678f4189a9d0ef65ce0591b5) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.ambassador | [Sketchfab: Star Trek - Ambassador class](https://sketchfab.com/models/25ed1463b2d14ad98dae8985a223f663) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.excelsior | [Sketchfab: U.S.S. Excelsior](https://sketchfab.com/models/e55ae4071bf840b085ae430cd0149198) | ejlabarbera | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.akira | [Sketchfab: Star Trek - Akira Class](https://sketchfab.com/models/84b459f462ec437b9e4a165ab98cee51) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.intrepid | [Sketchfab: Intrepid Class Starship](https://sketchfab.com/models/da7580cbd73a4a9b832c23366b5ceb65) | Feltman_Langer | CC-BY-NC-4.0 (uploader-declared on Sketchfab) |
| fed.steamrunner | [Sketchfab: Star Trek - Steamrunner class](https://sketchfab.com/models/6baf9770699f4677a9112b3b44248720) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.norway | [Sketchfab: Star Trek - Norway class](https://sketchfab.com/models/9e3196504fb047219cd01e33c7f2a949) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.saber | [Sketchfab: Star Trek - Saber class](https://sketchfab.com/models/b0a899f2a9cc422f8d0d49182953611e) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.nova | [BlendSwap: Star Trek Nova Class by ask13freak](https://blendswap.com/blend/9497) | ask13freak | CC-BY-SA (uploader-declared) |
| fed.prometheus | [Sketchfab: Prometheus Class Starship (Star Trek)](https://sketchfab.com/models/1a597521c37649e0af1aa9f9a5fb3255) | giacar25 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.danube | [Sketchfab: Star Trek - Danube Class](https://sketchfab.com/models/efc194b3d5f14a2e8718fe5bb7baee7a) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fed.starbase | [Sketchfab: Star Trek - Starbase 375](https://sketchfab.com/models/a1f79e463bbe44948f8594ebf6c8fe96) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| kli.vorcha | [Trekmeshes: Vor'cha battlecruiser (resolve individual entry)](https://trekmeshes.eu/download/all) | unresolved (catalogue index) | No licence stated on page |
| kli.neghvar | [Sketchfab: Negh Var - Enterprise](https://sketchfab.com/models/26ec3eaa4f754c08bbc4e203b90d4816) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) |
| kli.bop | [Sketchfab: Star Trek B'rel Class Klingon Bird of Prey](https://sketchfab.com/models/5bfb6bb66fc3479c9d626af57f66897c) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) |
| kli.ktinga | [Sketchfab: klingon D7](https://sketchfab.com/models/20d4a380c9814ea297aef8d523446dde) | CaptainJamesKirk | CC-BY-4.0 (uploader-declared on Sketchfab) |
| rom.dderidex | [Sketchfab: Star Trek - D'Deridex Class Warbird](https://sketchfab.com/models/4cd1e185eaa645b2ad3eeae31d0c5d0d) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) |
| rom.valdore | [Sketchfab: Star Trek Online \| Romulan Valdore](https://sketchfab.com/models/b895da5d0f14435cb867407a21cb86df) | LoganRolphh | CC-BY-4.0 (uploader-declared on Sketchfab) |
| rom.bop | [Sketchfab: Picard Romulan variant of Bird Of Prey](https://sketchfab.com/models/34fe048bc7554b738b34ba1c2f415d1a) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) |
| car.galor | [Sketchfab: Star Trek - Cardassian Galor](https://sketchfab.com/models/d9305d314e154639a5769747fd94b85f) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| car.groumall | [Sketchfab: Star Trek - Cardassian freighter Groumall](https://sketchfab.com/models/133e314850a945a3ad7e03f9aff99213) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| car.platform | [Sketchfab: Star Trek - Cardassian Weapon Platform](https://sketchfab.com/models/fbf1ab33d9fd4b6e876b9e91dcab9e93) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) |
| fer.dkora | [Trekmeshes: Ferengi Marauder](https://trekmeshes.eu/download/dl215/FerengiMarauder) | Raul Mamoru | No licence stated on page |
| fer.shuttle | [Sketchfab: Star Trek - Ferengi shuttle](https://sketchfab.com/models/361d3431d7374559bc288efe505f1172) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| bor.cube | [Sketchfab: Borg Cube - Star Trek TNG](https://sketchfab.com/models/22cdad6bb86c48cca74f1721b535bce9) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) |
| bor.sphere | [Sketchfab: Borg Sphere Variant 1](https://sketchfab.com/models/4c087e15af4b463aa515ae7feff4332f) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) |
| brn.cruiser | [Sketchfab: Star Trek - Breen cruiser](https://sketchfab.com/models/feb010c966a94285948ab56d23a1cbaf) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) |
| wpn.phaser.fed | [TrekCore: TNG Phaser 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.disruptor.kli | [TrekCore: TNG Disruptor](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.disruptor.rom | [TrekCore: TNG Disruptor](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.spiral.car | [TrekCore: TNG Phaser 9 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.plasma.fer | [TrekCore: TNG Weapons 1 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.pulse.defiant | [TrekCore: TNG Weapons 2 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.torpedo.photon | [TrekCore: TNG Torpedo 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.torpedo.quantum | [TrekCore: Quantum Torpedoes 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.torpedo.plasma | [TrekCore: TNG Torpedo 3 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.cutting.borg | [TrekCore: Deflector Enterprise B (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| wpn.beam.generic | Original BOTF sfx.snd: beam weapon (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use |
| wpn.torpedo.generic | Original BOTF sfx.snd: torpedo (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use |
| sfx.explosion.large | [TrekCore: Large Explosion 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.explosion.small | [TrekCore: Small Explosion 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.explosion.botf | Original BOTF sfx.snd: explosion (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use |
| sfx.shield.hit | [TrekCore: Shield Impact Sizzle](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.cloak.kli | [TrekCore: Cloak - Klingon](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.decloak.kli | [TrekCore: Decloak - Klingon](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.cloak.rom | [TrekCore: Cloak - Romulan](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.decloak.rom | [TrekCore: Decloak - Romulan](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.warp.in | [TrekCore: TNG Warp Exit](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.warp.out | [TrekCore: TNG Warp Out 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.alert.red | [TrekCore: TNG Red Alert 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.ui.beep | [TrekCore: Computer Alert 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio |
| sfx.ext.archive_tos | [Internet Archive: Star Trek Sound Effects OST 1988](https://archive.org/details/star-trek-tos-sound-effects) | Paramount (1988 LP), uploaded by archive.org user | No licence; commercial LP rip |
| vo.combat.ack.fed | Original BOTF english.snd: FED combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.advisor.fed | Original BOTF english.snd: FED advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.diplomacy.fed | Original BOTF english.snd: FED diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.combat.ack.kli | Original BOTF english.snd: KLI combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.advisor.kli | Original BOTF english.snd: KLI advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.diplomacy.kli | Original BOTF english.snd: KLI diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.combat.ack.rom | Original BOTF english.snd: ROM combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.advisor.rom | Original BOTF english.snd: ROM advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.diplomacy.rom | Original BOTF english.snd: ROM diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.combat.ack.car | Original BOTF english.snd: CAR combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.advisor.car | Original BOTF english.snd: CAR advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.diplomacy.car | Original BOTF english.snd: CAR diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.combat.ack.fer | Original BOTF english.snd: FER combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.advisor.fer | Original BOTF english.snd: FER advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.diplomacy.fer | Original BOTF english.snd: FER diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| vo.minor.generic | Original BOTF english.snd: Minor race diplomacy (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use |
| mus.botf.all | Original BOTF music.snd: BOTF soundtrack (music.snd) (`local:<BOTF>/music.snd`) | MicroProse (1999) | Owned game copy; personal use |

<!-- GENERATED:manifest-credits END -->
