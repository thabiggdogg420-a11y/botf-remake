# Asset sources (generated from `remaster/assets/manifest.json`)

Research date 2026-09-28. **Do not edit by hand**; edit the manifest and run `python remaster/assets/render_asset_docs.py`.

**Policy.** Every third-party asset here (HD ship models, show audio, mod sounds, original BOTF voice files) is private and local only. The repo holds the manifest, importer scripts and credits, never the binaries. Files are downloaded by hand into a folder, resolved by `remaster/game/Import-Assets.ps1` into the gitignored `remaster/game/LocalAssets/`, and imported into the gitignored `/Game/Local`. A licence on an upload is the uploader's claim. It cannot grant rights in Star Trek designs or show audio. "Permission unclear" is the normal state for these assets.

## Totals

- 90 asset keys: 48 model, 1 music, 25 sfx, 16 voice.
- 154 ranked source candidates: 71 sketchfab, 32 trekcore, 22 botf-install, 9 gamefront/nexus, 7 trekmeshes, 6 in-repo, 3 blendswap, 3 moddb, 1 archive.org.
- Permission flags: 6 clear, 22 clear-personal, 32 restricted, 94 unclear.

Permission flags: **clear** means redistributable with credit (in-repo Icewolf132/project art only). **clear-personal** means the user's own game copy, for private use. **unclear** means an uploader licence or no stated terms; the franchise rights are unresolved. **restricted** means the author's terms forbid reuse or derivatives (NoDerivs, or mod readmes that deny reuse). The importer skips restricted candidates unless `-AllowRestricted` is passed.

## Per-faction ship coverage

| Faction | Keys | With a downloadable, non-restricted candidate | Only restricted / unresolved | Gaps (fallback) |
|---|---:|---|---|---|
| Federation | 19 | fed.galaxy, fed.sovereign, fed.defiant, fed.nebula, fed.miranda, fed.oberth, fed.ambassador, fed.excelsior, fed.akira, fed.intrepid, fed.steamrunner, fed.norway, fed.saber, fed.nova, fed.prometheus, fed.danube, fed.ranger, fed.starbase | fed.constellation | none |
| Klingon | 5 | kli.neghvar, kli.bop, kli.ktinga | kli.vorcha | kli.starbase (-> fed.starbase) |
| Romulan | 5 | rom.dderidex, rom.valdore, rom.bop | rom.scout | rom.starbase (-> fed.starbase) |
| Cardassian | 6 | car.galor, car.groumall, car.platform | car.keldon, car.hideki, car.scout | none |
| Ferengi | 4 | fer.shuttle | fer.dkora, fer.scout, fer.starbase | none |
| Dominion | 1 | none | dom.attackship | none |
| Borg | 2 | bor.cube, bor.sphere | none | none |
| Breen | 1 | brn.cruiser | none | none |

## Gaps

- **Klingon Vor'cha.** No downloadable Sketchfab upload was found; phoenixbondi's is view-only. The remaining routes are Trekmeshes (entry unresolved), BC Remastered, which is restricted, and Armada 3, also restricted. Until one is chosen it falls back to the Negh'Var.
- **Cardassian Keldon and Hideki.** These exist only in mod packs whose terms deny reuse. They fall back to the Galor, which has a CC-BY octave767 upload and a Trekmeshes Mike Wright entry.
- **Ferengi D'Kora.** No free Sketchfab upload exists. Trekmeshes (Raul Mamoru OBJ, permission unclear) is the main route.
- **Starbases** for Klingon and Romulan have no candidates and use the Federation starbase. Outposts share the starbase key.
- **Weapon audio for Cardassian, Ferengi, pulse phaser and plasma torpedo** has no dedicated TrekCore clip. Stand-in clips are listed and flagged as such in the display name.
- **HD versus game-ready.** ZEOCMF uploads are the most detailed (1–5M triangles) but carry CC-BY-NC-ND. Converting and decimating them is arguably a derivative, so they are marked restricted. CrustyBread, wholock, octave767 and rossrobotics uploads are CC-BY and 10k–260k triangles.
- **Uploads ripped from games** (e.g. "Star Trek Online | ..." by LoganRolphh) are listed only as a last resort. The uploader cannot licence them.
- **Original BOTF voice** is available only from the user's own install. See AUDIO-PLAN.md.
- **Minor races and the Guardian/special ships** are not covered, except Borg, Breen and the Jem'Hadar fighter.

## Mod and library sources consulted

| Source | Where | Author/team | Stated terms | Use here |
|---|---|---|---|---|
| Star Trek: Armada 3 (Sins of a Solar Empire: Rebellion mod) | [ModDB](https://www.moddb.com/mods/star-trek-armada-3/downloads) | Armada 3 team + credited modellers | Readme: Aad Moerman models need special permission (not granted), and STAR remaster models may not be reused in other mods | restricted, reference only |
| Bridge Commander Remastered 1.2 | [GameFront](https://www.gamefront.com/games/bridge-commander/file/bridge-commander-remastered), [Nexus](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | BCR team + ship authors | Original author permission required to reuse | restricted. NIF plus hardpoint .py files are the best reference for weapon socket positions |
| Bridge Commander Kobayashi Maru | [ModDB](https://www.moddb.com/mods/bridge-commander-kobayashi-maru/downloads), [bckobayashimaru.de](http://bckobayashimaru.de/ws/index.php?id=downloads) | KM team | Per-ship readmes | not listed per class; use for hardpoint reference |
| BOTF mods: Ultimate Universe, Galaxies MOD (v0.4.0), Vanilla Upgrade/AIO | [Armada Fleet Command BOTF forum](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=1536) | mod teams | Forum downloads; no reuse grant | 2D ship art and sounds only, private reference; not in manifest |
| TrekCore audio archive | [trekcore.com/audio](https://www.trekcore.com/audio/) | show audio | No terms stated | sfx candidates (unclear) |
| Star Trek Sound Effects LP (1988) | [archive.org](https://archive.org/details/star-trek-tos-sound-effects) | Paramount | none | bulk TOS library (unclear) |
| Trekmeshes | [trekmeshes.eu](https://trekmeshes.eu/download/all) | per uploader | no reuse grant on pages; site closing | candidates (unclear) |
| BlendSwap | [blendswap.com](https://blendswap.com) | per uploader | uploader-declared CC | candidates (unclear) |
| Sketchfab | [sketchfab.com](https://sketchfab.com/tags/star-trek) | per uploader | uploader-declared CC, verified through the public API | main HD/game-ready candidate pool |

## All candidates

### `study.fed` Federation procedural fleet study (original project art)

Kind model, faction Federation. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | In-repo FleetStudies (`remaster/game/ArtSource/FleetStudies/Federation.glb`) | Project (original) | Project original work | clear | glb |  |

### `study.kli` Klingon procedural fleet study (original project art)

Kind model, faction Klingon. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | In-repo FleetStudies (`remaster/game/ArtSource/FleetStudies/Klingon.glb`) | Project (original) | Project original work | clear | glb |  |

### `study.rom` Romulan procedural fleet study (original project art)

Kind model, faction Romulan. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | In-repo FleetStudies (`remaster/game/ArtSource/FleetStudies/Romulan.glb`) | Project (original) | Project original work | clear | glb |  |

### `study.car` Cardassian procedural fleet study (original project art)

Kind model, faction Cardassian. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | In-repo FleetStudies (`remaster/game/ArtSource/FleetStudies/Cardassian.glb`) | Project (original) | Project original work | clear | glb |  |

### `study.fer` Ferengi procedural fleet study (original project art)

Kind model, faction Ferengi. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | In-repo FleetStudies (`remaster/game/ArtSource/FleetStudies/Ferengi.glb`) | Project (original) | Project original work | clear | glb |  |

### `fed.galaxy` Galaxy class

Kind model, faction Federation. Fallback: `study.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek TNG Galaxy Class Model](https://sketchfab.com/models/6029fefcfbd44908b2dfa9938cafecf5) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 48,373 |
| 2 | [Sketchfab: Star Trek - Galaxy class](https://sketchfab.com/models/08b80314e55647bea46906514374b241) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 15,574 |
| 3 | [Sketchfab: USS Enterprise-D - Galaxy-class [Star Trek] ZEO](https://sketchfab.com/models/4953e4b8a7854e8da3d97f9639da8bd7) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 2,214,516 |
| 4 | [Trekmeshes: U.S.S. Enterprise-D](https://trekmeshes.eu/download/dl196/USSEnterpriseD) | David Metlesits (Blender conversion Fabio A. Correa) | No licence stated on page | unclear | blend |  |
| 5 | [Bridge Commander Remastered 1.2 (Galaxy)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `fed.sovereign` Sovereign class

Kind model, faction Federation. Fallback: `fed.galaxy`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek Sovereign Class with Shuttlebay](https://sketchfab.com/models/f57fa908f23d498daf3d43d17d1b5f22) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 83,715 |
| 2 | [Sketchfab: Star Trek - Sovereign Class](https://sketchfab.com/models/e54ca13f5a3747c584dd304e4bb81826) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 29,900 |
| 3 | [Sketchfab: USS Enterprise - Sovereign class [Star Trek]](https://sketchfab.com/models/da33c14e49424015ba8c22d43c288c74) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 1,579,654 |
| 4 | [BlendSwap: Star Trek Sovereign Class by woki](https://blendswap.com/blend/10430) | woki | CC-BY (uploader-declared); unfinished model | unclear | blend |  |
| 5 | [Bridge Commander Remastered 1.2 (Sovereign)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `fed.defiant` Defiant class

Kind model, faction Federation. Fallback: `fed.akira`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: U.S.S. Defiant](https://sketchfab.com/models/8e875133c49240699d3b19de324826cf) | Feltman_Langer | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 118,402 |
| 2 | [Sketchfab: Defiant - Star Trek DS9](https://sketchfab.com/models/f1ea79fde6684a41b4c6e26b89cfb53a) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 119,752 |
| 3 | [Sketchfab: Star Trek Defiant Class](https://sketchfab.com/models/4c6b37f8f6ec4ce3932374f06f4ab8c3) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 34,114 |
| 4 | [Sketchfab: USS Defiant [Star Trek] ZEO](https://sketchfab.com/models/56e09b38c122468a8b0553ba7766025e) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 1,793,364 |
| 5 | [BlendSwap: U.S.S. Defiant by FatManInABox](https://blendswap.com/blend/22476) | FatManInABox | CC-BY badge; description restricts to non-commercial with credit | unclear | blend |  |

### `fed.nebula` Nebula class

Kind model, faction Federation. Fallback: `fed.galaxy`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Nebula-class cruiser](https://sketchfab.com/models/4fa2e93de1ba48ad9425c1e4228d7abf) | nestor_d | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 22,942 |
| 2 | [Sketchfab: USS Farragut - Nebula-class [Star Trek] ZEO](https://sketchfab.com/models/4ea8c66c395e4d9eb81bbe152e1afc03) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 3,162,710 |
| 3 | [Bridge Commander Remastered 1.2 (Nebula)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `fed.miranda` Miranda class

Kind model, faction Federation. Fallback: `fed.excelsior`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek Miranda Class](https://sketchfab.com/models/08b835fb9b8743cc8d22aec03ea05756) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 27,581 |
| 2 | [Sketchfab: Star Trek - Miranda class](https://sketchfab.com/models/1df8856c0eb5423f9840bd103f8d7257) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 12,028 |
| 3 | [Sketchfab: USS Reliant - Miranda-class [Star Trek] ZEO](https://sketchfab.com/models/d7f0cc6445ff43d08d2e456c3c21b167) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 1,004,496 |
| 4 | [Sketchfab: Miranda Class (Generic TNG/DS-9 Variant)](https://sketchfab.com/models/27e4aa654c03451eb6ce256d7ee713eb) | MajorStress | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 25,119 |

### `fed.constellation` Constellation class

Kind model, faction Federation. Fallback: `fed.excelsior`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: USS Stargazer Constellation-class [Star Trek]ZEO](https://sketchfab.com/models/a572c77178e14bbebe1f3c3f1db78b6c) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 879,414 |

### `fed.oberth` Oberth class

Kind model, faction Federation. Fallback: `study.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Oberth Class](https://sketchfab.com/models/c1a3bd9d678f4189a9d0ef65ce0591b5) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,742 |
| 2 | [Sketchfab: Star Trek - Oberth class](https://sketchfab.com/models/995df2bf95264253a52ee2e0f32fe469) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 6,395 |
| 3 | [Sketchfab: Star Trek Oberth Transport](https://sketchfab.com/models/e0f8180b687c4cb2bedee084cf6368e9) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,349 |

### `fed.ambassador` Ambassador class

Kind model, faction Federation. Fallback: `fed.galaxy`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Ambassador class](https://sketchfab.com/models/25ed1463b2d14ad98dae8985a223f663) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 10,130 |
| 2 | [Sketchfab: Ambassador Class U.S.S. Enterprise NCC-1701-C](https://sketchfab.com/models/200cc04ceb2e48438b7a21cd4605f960) | MajorStress | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 15,044 |

### `fed.excelsior` Excelsior class

Kind model, faction Federation. Fallback: `study.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: U.S.S. Excelsior](https://sketchfab.com/models/e55ae4071bf840b085ae430cd0149198) | ejlabarbera | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 36,518 |
| 2 | [Sketchfab: Star Trek - Excelsior class](https://sketchfab.com/models/ea99fe0c7afe47668c31245cf4906b41) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 11,568 |
| 3 | [Sketchfab: USS Excelsior NCC-2000 [Star Trek] ZEO](https://sketchfab.com/models/bc26564dd77b48feb11e4cb94d6c39d1) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 3,989,405 |
| 4 | [Sketchfab: Excelsior](https://sketchfab.com/models/46745c1df3ea4e268467052e5e8c6723) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 348,528 |

### `fed.akira` Akira class

Kind model, faction Federation. Fallback: `fed.excelsior`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Akira Class](https://sketchfab.com/models/84b459f462ec437b9e4a165ab98cee51) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,954 |
| 2 | [Sketchfab: Akira](https://sketchfab.com/models/ad318bdd1d44429e88ca7245006fe2e9) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 158,529 |
| 3 | [Bridge Commander Remastered 1.2 (Akira)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `fed.intrepid` Intrepid class

Kind model, faction Federation. Fallback: `fed.nebula`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Intrepid Class Starship](https://sketchfab.com/models/da7580cbd73a4a9b832c23366b5ceb65) | Feltman_Langer | CC-BY-NC-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 550,856 |
| 2 | [Sketchfab: Voyager - Intrepid-class [Star Trek] ZEO](https://sketchfab.com/models/35ea3d6d867a44a7981deca6c94a2d6b) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 2,441,607 |
| 3 | [Sketchfab: Star Trek - Intrepid Type](https://sketchfab.com/models/554e931031264afca1b4c62ecbff2978) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 22,796 |

### `fed.steamrunner` Steamrunner class

Kind model, faction Federation. Fallback: `fed.akira`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Steamrunner class](https://sketchfab.com/models/6baf9770699f4677a9112b3b44248720) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 7,940 |

### `fed.norway` Norway class

Kind model, faction Federation. Fallback: `fed.akira`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Norway class](https://sketchfab.com/models/9e3196504fb047219cd01e33c7f2a949) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,018 |

### `fed.saber` Saber class

Kind model, faction Federation. Fallback: `fed.defiant`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Saber class](https://sketchfab.com/models/b0a899f2a9cc422f8d0d49182953611e) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 6,237 |
| 2 | [Sketchfab: USS Argo - Saber class [Star Trek] ZEO](https://sketchfab.com/models/81d57d781bae49928494b560560dcf10) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 934,172 |
| 3 | [Sketchfab: Saber Class Yeager NCC 61947](https://sketchfab.com/models/b0081529aada4e0a8e50873469d5922b) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 573,285 |

### `fed.nova` Nova class

Kind model, faction Federation. Fallback: `fed.oberth`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: USS Equinox - Nova Class [Star Trek] ZEO](https://sketchfab.com/models/b38e1b246312499da9b03d07c288c31e) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 2,231,156 |
| 2 | [BlendSwap: Star Trek Nova Class by ask13freak](https://blendswap.com/blend/9497) | ask13freak | CC-BY-SA (uploader-declared) | unclear | blend |  |

### `fed.prometheus` Prometheus class

Kind model, faction Federation. Fallback: `fed.sovereign`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Prometheus Class Starship (Star Trek)](https://sketchfab.com/models/1a597521c37649e0af1aa9f9a5fb3255) | giacar25 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 134,338 |

### `fed.danube` Danube class runabout

Kind model, faction Federation. Fallback: `fed.oberth`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Danube Class](https://sketchfab.com/models/efc194b3d5f14a2e8718fe5bb7baee7a) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 11,882 |

### `fed.ranger` Ranger (Icewolf132, in-repo)

Kind model, faction Federation. Fallback: `study.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Icewolf132 Mesh Pack 1 (in repo) (`remaster/game/ArtSource/Community/Icewolf132/franger.glb`) | Icewolf132 | README permits edit/convert/use in other games with credit | clear | glb |  |

### `fed.starbase` Federation starbase

Kind model, faction Federation. Fallback: `study.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Starbase 375](https://sketchfab.com/models/a1f79e463bbe44948f8594ebf6c8fe96) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,574 |
| 2 | [Sketchfab: Star Trek Earth Spacedock](https://sketchfab.com/models/1e37520c846c448a81ca70f1c82cd09e) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 57,057 |

### `kli.vorcha` Vor'cha class

Kind model, faction Klingon. Fallback: `kli.neghvar`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Trekmeshes: Vor'cha battlecruiser (resolve individual entry)](https://trekmeshes.eu/download/all) | unresolved (catalogue index) | No licence stated on page | unclear | max |  |
| 2 | [Bridge Commander Remastered 1.2 (Vorcha)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |
| 3 | [Star Trek: Armada 3 (Vor'cha)](https://www.moddb.com/mods/star-trek-armada-3/downloads) | Armada 3 team (Dominus_Noctis et al.) + credited modellers | Per-model credits list; no reuse grant | restricted | mesh |  |
| 4 | [Sketchfab: Klingon Vor'cha Attack Cruiser (view only)](https://sketchfab.com/3d-models/klingon-vorcha-attack-cruiser-d24b7c9877d94fa38593b5021690b223) | phoenixbondi | Not downloadable | restricted | gltf |  |

### `kli.neghvar` Negh'Var class

Kind model, faction Klingon. Fallback: `kli.ktinga`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Negh Var - Enterprise](https://sketchfab.com/models/26ec3eaa4f754c08bbc4e203b90d4816) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 215,322 |
| 2 | [Sketchfab: Negh Var Class](https://sketchfab.com/models/a74716758bd04624b849e947f3f4e420) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 100,135 |
| 3 | [Trekmeshes: Negh'Var Flagship](https://trekmeshes.eu/download/all) | David 'Knightrider' Metlesits (per site news) | No licence stated on page | unclear | max |  |

### `kli.bop` B'rel / K'Vort Bird-of-Prey

Kind model, faction Klingon. Fallback: `study.kli`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek B'rel Class Klingon Bird of Prey](https://sketchfab.com/models/5bfb6bb66fc3479c9d626af57f66897c) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 57,014 |
| 2 | [Sketchfab: Klingon Bird Of Prey - Star Trek](https://sketchfab.com/models/0b20e0b3708c46e48ac59eb9bca97d66) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 259,300 |
| 3 | [Sketchfab: Star Trek - Bird Of Prey](https://sketchfab.com/models/eaeea42a53bf4c248b5589fe14f6b7cb) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 29,868 |
| 4 | [Sketchfab: Klingon Bird Of Prey Textured and Animated](https://sketchfab.com/models/34e884290820400aa72b9bba0ee9f173) | gstarr (Trekkiestarr) | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 61,158 |

### `kli.ktinga` K't'inga / D7 class

Kind model, faction Klingon. Fallback: `kli.bop`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: klingon D7](https://sketchfab.com/models/20d4a380c9814ea297aef8d523446dde) | CaptainJamesKirk | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 74,465 |
| 2 | [Sketchfab: Klingon D-7c Battlecruiser (IKS G'Roth Variant)](https://sketchfab.com/models/035e804e56be4c5a9e79a44950094e5e) | MajorStress | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 10,130 |
| 3 | [Sketchfab: Ktinga Shift Class](https://sketchfab.com/models/44128c836514440cbbc5d2b77fb58943) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 258,418 |
| 4 | [Sketchfab: Star Trek Klingon D7](https://sketchfab.com/models/3dcf838479f04a09acd9e0382a18625d) | xiaorobear | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 74,465 |

### `kli.starbase` Klingon starbase

Kind model, faction Klingon. Fallback: `fed.starbase`.

_No candidate found._

### `rom.dderidex` D'deridex Warbird

Kind model, faction Romulan. Fallback: `study.rom`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - D'Deridex Class Warbird](https://sketchfab.com/models/4cd1e185eaa645b2ad3eeae31d0c5d0d) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 29,674 |
| 2 | [Trekmeshes: Romulan Warbird (multiple entries)](https://trekmeshes.eu/download/all) | unresolved (several uploaders) | No licence stated on page | unclear | 3ds |  |
| 3 | [Bridge Commander Remastered 1.2 (Warbird)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `rom.valdore` Valdore type (late)

Kind model, faction Romulan. Fallback: `rom.dderidex`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek Online \| Romulan Valdore](https://sketchfab.com/models/b895da5d0f14435cb867407a21cb86df) | LoganRolphh | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,757 |

### `rom.scout` Romulan scout

Kind model, faction Romulan. Fallback: `rom.bop`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Romulan Scout Ship [Star Trek] ZEO](https://sketchfab.com/models/02ebeb97bcd3479d957bea5dec6018fb) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 505,988 |

### `rom.bop` Romulan light warbird / Bird-of-Prey

Kind model, faction Romulan. Fallback: `rom.dderidex`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Picard Romulan variant of Bird Of Prey](https://sketchfab.com/models/34fe048bc7554b738b34ba1c2f415d1a) | morenostefanuto | Sketchfab Free Standard licence (uploader-declared) | unclear | gltf | 144,257 |
| 2 | [Sketchfab: Romulan Gallant Wing Class Cruiser](https://sketchfab.com/models/93b476d1d0c945dd865656d8f4021f55) | resurrected | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 32,744 |
| 3 | [Trekmeshes: Romulan Bird of Prey](https://trekmeshes.eu/download/all) | unresolved | No licence stated on page | unclear | 3ds |  |

### `rom.starbase` Romulan starbase

Kind model, faction Romulan. Fallback: `fed.starbase`.

_No candidate found._

### `car.galor` Galor class

Kind model, faction Cardassian. Fallback: `study.car`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Cardassian Galor](https://sketchfab.com/models/d9305d314e154639a5769747fd94b85f) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,942 |
| 2 | [Sketchfab: Cardassian Galor class Cruiser [Star Trek]](https://sketchfab.com/models/e81131eaccef44f0a65f963e6bb62281) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 436,990 |
| 3 | [Trekmeshes: Galor class Starship](https://trekmeshes.eu/download/dl080/GalorclassStarship) | Mike Wright | No licence stated on page | unclear | 3ds |  |
| 4 | [Bridge Commander Remastered 1.2 (Galor)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `car.keldon` Keldon class

Kind model, faction Cardassian. Fallback: `car.galor`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Bridge Commander Remastered 1.2 (Keldon)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |
| 2 | [Star Trek: Armada 3 (Keldon)](https://www.moddb.com/mods/star-trek-armada-3/downloads) | Armada 3 team (Dominus_Noctis et al.) + credited modellers | Per-model credits list; no reuse grant | restricted | mesh |  |

### `car.hideki` Hideki class

Kind model, faction Cardassian. Fallback: `car.galor`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Star Trek: Armada 3 (Hideki)](https://www.moddb.com/mods/star-trek-armada-3/downloads) | Armada 3 team (Dominus_Noctis et al.) + credited modellers | Per-model credits list; no reuse grant | restricted | mesh |  |

### `car.groumall` Groumall freighter

Kind model, faction Cardassian. Fallback: `car.galor`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Cardassian freighter Groumall](https://sketchfab.com/models/133e314850a945a3ad7e03f9aff99213) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 9,850 |

### `car.platform` Cardassian weapon platform

Kind model, faction Cardassian. Fallback: `car.galor`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Cardassian Weapon Platform](https://sketchfab.com/models/fbf1ab33d9fd4b6e876b9e91dcab9e93) | wholock | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 4,956 |

### `car.scout` Cardassian scout

Kind model, faction Cardassian. Fallback: `car.galor`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Cardassian Scout Ship [Star Trek] ZEO](https://sketchfab.com/models/c4c039ae1aa04e149da490a9d91b50ac) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 562,577 |

### `fer.dkora` D'Kora Marauder

Kind model, faction Ferengi. Fallback: `study.fer`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Trekmeshes: Ferengi Marauder](https://trekmeshes.eu/download/dl215/FerengiMarauder) | Raul Mamoru | No licence stated on page | unclear | obj |  |
| 2 | [Bridge Commander Remastered 1.2 (Marauder)](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | Bridge Commander Remastered team + credited ship authors | Per-ship readmes; reuse requires original author permission | restricted | nif |  |

### `fer.shuttle` Ferengi shuttle

Kind model, faction Ferengi. Fallback: `fer.dkora`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Ferengi shuttle](https://sketchfab.com/models/361d3431d7374559bc288efe505f1172) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 4,162 |

### `fer.scout` Ferengi scout

Kind model, faction Ferengi. Fallback: `fer.dkora`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Ferengi Scout Ship [Star Trek] ZEO](https://sketchfab.com/models/af7a26947de940b3aca2d54cb477732f) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 590,733 |

### `fer.starbase` Ferengi starbase

Kind model, faction Ferengi. Fallback: `fed.starbase`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Ferengi Starbase [Star Trek] ZEO](https://sketchfab.com/models/a066fcbdba604ad4bea0ee34adfe90c4) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 5,619,224 |

### `dom.attackship` Jem'Hadar attack ship

Kind model, faction Dominion. Fallback: `fer.scout`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Jem'Hadar Fighter [Star Trek] ZEO](https://sketchfab.com/models/4f4427dc57084ca8915a97c0bfc99cd8) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 230,396 |

### `bor.cube` Borg cube

Kind model, faction Borg. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Borg Cube - Star Trek TNG](https://sketchfab.com/models/22cdad6bb86c48cca74f1721b535bce9) | rossrobotics | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 593,987 |
| 2 | [Sketchfab: Borg Cube [Star Trek] ZEO](https://sketchfab.com/models/3d6031ce2cd745deba9c477231fda18c) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 3,037,324 |

### `bor.sphere` Borg sphere

Kind model, faction Borg. Fallback: `bor.cube`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Borg Sphere Variant 1](https://sketchfab.com/models/4c087e15af4b463aa515ae7feff4332f) | CrustyBread | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 14,186 |
| 2 | [Sketchfab: Borg Sphere [Star Trek] ZEO](https://sketchfab.com/models/1a9d4d4c16914112b4a5fe5c9ef3ff59) | ZEOCMF | CC-BY-NC-ND-4.0 (uploader-declared on Sketchfab); conversion/rescale is arguably a derivative | restricted | gltf | 1,464,986 |

### `brn.cruiser` Breen cruiser

Kind model, faction Breen. Fallback: `study.car`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Sketchfab: Star Trek - Breen cruiser](https://sketchfab.com/models/feb010c966a94285948ab56d23a1cbaf) | octave767 | CC-BY-4.0 (uploader-declared on Sketchfab) | unclear | gltf | 11,180 |

### `wpn.phaser.fed` Federation phaser array

Kind sfx. Fallback: `wpn.beam.generic`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Phaser 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TNG Phaser 2](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 3 | [TrekCore: TNG Phaser 5](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 4 | Original BOTF sfx.snd: phaser (index TBD) (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `wpn.disruptor.kli` Klingon disruptor

Kind sfx. Fallback: `wpn.beam.generic`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Disruptor](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TOS Disruptor](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.disruptor.rom` Romulan disruptor

Kind sfx. Fallback: `wpn.disruptor.kli`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Disruptor](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.spiral.car` Cardassian spiral-wave disruptor

Kind sfx. Fallback: `wpn.phaser.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Phaser 9 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.plasma.fer` Ferengi plasma energy whip

Kind sfx. Fallback: `wpn.phaser.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Weapons 1 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.pulse.defiant` Pulse phaser cannon

Kind sfx. Fallback: `wpn.phaser.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Weapons 2 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.torpedo.photon` Photon torpedo

Kind sfx. Fallback: `wpn.torpedo.generic`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Torpedo 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TNG Torpedo 2](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 3 | [TrekCore: TOS Photon Torpedo 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.torpedo.quantum` Quantum torpedo

Kind sfx. Fallback: `wpn.torpedo.photon`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Quantum Torpedoes 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: Quantum Torpedoes 2](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.torpedo.plasma` Romulan plasma torpedo

Kind sfx. Fallback: `wpn.torpedo.photon`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Torpedo 3 (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.cutting.borg` Borg cutting beam

Kind sfx. Fallback: `wpn.phaser.fed`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Deflector Enterprise B (stand-in)](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `wpn.beam.generic` Generic beam (BOTF original)

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF sfx.snd: beam weapon (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `wpn.torpedo.generic` Generic torpedo (BOTF original)

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF sfx.snd: torpedo (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `sfx.explosion.large` Large ship explosion

Kind sfx. Fallback: `sfx.explosion.botf`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Large Explosion 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: Large Explosion 3](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.explosion.small` Small explosion / hull hit

Kind sfx. Fallback: `sfx.explosion.botf`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Small Explosion 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TOS Hull Hit 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.explosion.botf` Explosion (BOTF original)

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF sfx.snd: explosion (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `sfx.shield.hit` Shield impact

Kind sfx. Fallback: `sfx.explosion.small`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Shield Impact Sizzle](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TNG Phaser Strike](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.cloak.kli` Klingon cloak

Kind sfx. Fallback: `sfx.cloak.rom`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Cloak - Klingon](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.decloak.kli` Klingon decloak

Kind sfx. Fallback: `sfx.decloak.rom`.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Decloak - Klingon](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.cloak.rom` Romulan cloak

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Cloak - Romulan](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.decloak.rom` Romulan decloak

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Decloak - Romulan](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.warp.in` Warp arrival

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Warp Exit](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.warp.out` Warp departure

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Warp Out 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: TNG Warp Flash](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.alert.red` Red alert (combat start)

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: TNG Red Alert 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | [TrekCore: DS9 Red Alert 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |

### `sfx.ui.beep` LCARS confirm beep

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [TrekCore: Computer Alert 1](https://www.trekcore.com/audio/) | Paramount/CBS (ripped by TrekCore) | No terms stated; show audio | unclear | mp3 |  |
| 2 | Original BOTF sfx.snd: UI click (`local:<BOTF>/sfx.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `sfx.ext.archive_tos` Star Trek TOS sound effects LP (1988) bulk archive

Kind sfx. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | [Internet Archive: Star Trek Sound Effects OST 1988](https://archive.org/details/star-trek-tos-sound-effects) | Paramount (1988 LP), uploaded by archive.org user | No licence; commercial LP rip | unclear | mp3 |  |

### `vo.combat.ack.fed` FED combat order acknowledgement

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FED combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.advisor.fed` FED advisor / event announcements

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FED advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.diplomacy.fed` FED diplomatic greeting/response

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FED diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.combat.ack.kli` KLI combat order acknowledgement

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: KLI combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.advisor.kli` KLI advisor / event announcements

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: KLI advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.diplomacy.kli` KLI diplomatic greeting/response

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: KLI diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.combat.ack.rom` ROM combat order acknowledgement

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: ROM combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.advisor.rom` ROM advisor / event announcements

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: ROM advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.diplomacy.rom` ROM diplomatic greeting/response

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: ROM diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.combat.ack.car` CAR combat order acknowledgement

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: CAR combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.advisor.car` CAR advisor / event announcements

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: CAR advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.diplomacy.car` CAR diplomatic greeting/response

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: CAR diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.combat.ack.fer` FER combat order acknowledgement

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FER combat order acknowledgement (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.advisor.fer` FER advisor / event announcements

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FER advisor / event announcements (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.diplomacy.fer` FER diplomatic greeting/response

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: FER diplomatic greeting/response (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `vo.minor.generic` Minor race diplomacy

Kind voice. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF english.snd: Minor race diplomacy (`local:<BOTF>/english.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

### `mus.botf.all` BOTF soundtrack (music.snd)

Kind music. Fallback: none.

| # | Source | Author | Stated terms | Permission | Format | Tris |
|---:|---|---|---|---|---|---:|
| 1 | Original BOTF music.snd: BOTF soundtrack (music.snd) (`local:<BOTF>/music.snd`) | MicroProse (1999) | Owned game copy; personal use | clear-personal | wav |  |

