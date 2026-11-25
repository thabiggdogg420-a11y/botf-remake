# Hull quality audit (BOTF remaster, main `7e5cf49`)

Written 10 Nov 2026 by Odo (audit; he built none of this). Evidence: every hero model in `game\LocalAssets\ships\godot-hero-manifest.json` was rendered by `godot\scenes\battle\tools\hull_gallery.gd` from the main working tree, Forward+ 1920x1080, three views each (`3q.png` = the tool's default three-quarter view, `top.png`, `starboard.png`), into `C:\Users\BiggDogg\Documents\ChatGPT\hull-audit\<class>\` (contact sheets in `hull-audit\_sheets\`). Triangle, material and texture facts come from the glb JSON chunks (scripts in the session scratch area, not the repo). Grades use the bar "DS9-era show model in a close fly-by".

## 1. Verdict

"Exe matches dev, hulls are fine" is false. The exe was only proven to load the same files as dev; those files are the problem.

- 151 distinct classes are reachable (98 roster classes + 53 legacy-catalogue classes). **78 resolve to a hero model and 73 have no model and draw the procedural fallback hull** (grade D). `resolved-manifest.json` has no fallback or stand-in links, so there is no shared stand-in: a class either has a hero glb or gets the generic hull.
- Class-level grades (fallbacks counted as D): A 7, B 46, C 7, D 91.
- Per model file (82): A/A- 7, B 49, C 8, D 18. Four heroes serve no class at all (`ship.car.platform`, `ship.fed.ranger`, `ship.kli.bop`, `ship.rom.bop`).
- **The empires a player meets first have the worst coverage.** Every starting hull of the Romulans, Cardassians and Ferengi (scout, colony ship, destroyer, cruiser, troop ship: `Simulation.cpp` ~4050-4070, roles 0,5,1,2,9) is a fallback hull. The Federation colony ship (Edward) and troop ship (London) and the Klingon colony ship (LI'wI'), destroyer (May'Duj) and troop ship (Charghwi') are kitbashes with untextured slabs or floating blobs. Only Fed scout/destroyer/cruiser (Oberth, Miranda, Ambassador) and Klingon scout/cruiser (B'rel, K'T'Inga) are real models.
- Only one hull (Galaxy) is on 4x textures. Everything else is on the 1x Bridge Commander textures (128 to 2048 px, most 1024 or less). Normal maps exist for nearly every model but are generated and almost flat (section 4).

## 2. How a class gets its model (from the code)

`BattleShip.asset_keys_for`: the class `assetKey`, then `ship.<faction>.<slug(class name)>`, then `study.<faction>`. `BattleAssets.instantiate_ship` takes the first key with a hero glb in `godot-hero-manifest.json`, else a `resolved-manifest.json` record (33 entries, all heroes), else `ShipFactory.build` (procedural). No `study.*` records exist, so the last step is always the procedural hull. Legacy classes use `ship.<fac>.<slug>`, so Romulan, Cardassian and Ferengi legacy classes (D'renet, Nerok, Bronta ...) need hero entries under those exact keys. Hero entries for `ship.rom.bop`, `ship.kli.bop`, `ship.car.platform` exist but nothing asks for those keys (the roster uses `ship.rom.bop_ent`, `ship.rom.bop_tos`, ...).

Material path (`hero_look.gd`, `hero_hull.gdshader`): glb base colour x tint, glb normal (only when present), ORM, emissive; images come from the glb itself, mip-chained at load.

## 3. The Nebula: what produces the smear Simon sees

Frame: `hull-audit\fed_nebula\starboard.png` (opened full size). The saucer window rows are drawn as soft elongated vertical dashes and the NCC-67420 registry is a thin mushy band; the hull between them is flat pale blue-white. Findings:

1. **Texel density, not mapping.** The ship is four 1024x1024 atlases (`T_fed_nebula_M00..M03`); the BC source (`H:\StarhelmTools\bc-ships\Nebula\textures`, 8 files) is also 1024 max. In the shippack lane's atlas image each saucer disc takes roughly 400 px for a ~300 m saucer: about 0.75 m per texel, and a window is 2 to 3 texels. Their measurement "mapped 1:1" is true and beside the point: 1:1 of a 3-texel window at close range is a window several screen pixels per texel, which reads as stretched writing.
2. **Windows are lit through the emissive map, which is sampled without anisotropy.** `hero_hull.gdshader` line 11: `emissive_tex ... filter_linear_mipmap`, while `bc_tex`, `normal_tex` and `orm_tex` use `filter_linear_mipmap_anisotropic`. In a side view the saucer is seen at a grazing angle; the emissive part picks a coarse mip along the view axis and smears. `project.godot` sets no anisotropic level, so the engine default applies to the other maps. This is read from the shader and the frame; I did not change the sampler and re-render.
3. **No relief from the normal map.** The Nebula glb normals are generated: R/G standard deviation 4 to 7 of 255 (Galaxy 4 to 5, Sovereign up to ~10). They add almost nothing at close range, so nothing hides the texel blur.
4. **Not LOD.** `HeroLook.min_lod_ratio = 1.0` forces LOD0; the 24k-triangle LOD0 is what is drawn.
5. Top-down (`top.png`) reads fine, as Simon saw; edge-on saucers are the worst case for a 1024 atlas.

Cheapest real fixes, in order: (a) make the emissive sampler anisotropic (one shader line); (b) put the Nebula on 4x textures (`StarhelmBC\Pipeline\materials\upscale_ship_textures.py`, output `H:\StarhelmTools\bc-ships-hd\Nebula\`) and make the glb carry the HD normal; (c) try library `DS9FXNebula` (2048 px native, 29 textures, 10k tris; not viewed). None of these was tested.

## 4. Texture facts across all models

- 1x BC textures everywhere except `ship.fed.galaxy` (4096 px, 4x pipeline for base and emissive; its BOTF normal maps are 3.8 to 5.3 MB each versus 6.7 to 8.1 MB for `bc-ships-hd\Galaxy\*_normal_4x.png`, so they are not the HD normals, and R/G std is only 4 to 5). `ship.kli.brel` hull maps are 2048 (4x of BirdOfPrey 512) but its head/bridge maps stay 512.
- Native 2048: `ambassador` (km variant), `galaxy_x`, `olympic`, `raven`, `rhode_island`, `sovereign` (DS9FXSovereign), `kelvin_type`, `constitution_kelvin`, `k7`, `starbase`.
- 512 px or below dominates: `daedalus`, `danube`, `yclass`, `springfield`, `bop`, `bop_ent`, `kvort`, `mayduj`, `qang`, `d5`, `d7`, `d12`, all `rom.*` (128-256), `kli.starbase` (256), `hideki` (128-512), `vengeance` (256x128).
- No textures at all in the glb: `ship.fed.soyuz`, `ship.fed.peregrine` (flat colour).
- Normal maps are read (`has_normal` is set when the glb carries one; shader line 26). Models with no normal or ORM map: `soyuz`, `peregrine`; `curry`, `ranger`, `vengeance`, `yclass`, `mayduj`, `qang` have base, normal and ORM but no emissive map (see maps column).

## 5. Full table, 82 hero models (worst grade first)

Tris = LOD0 triangles from the manifest. Tex = base-colour map sizes (largest first). Maps = materials carrying each map. Frames: `hull-audit\<class>\3q|top|starboard.png`, class folder = key without `ship.`, dots as underscores.

| key | classes served | model / source | tris | tex (base) | maps | grade | faults and frame |
|---|---|---|---:|---|---|---|---|
| `ship.fed.daedalus` | Daedalus (roster 1001) | BC library `Daedalus` (mods-stargate_pack_3_0-Stargate-Pack-3.0) | 57689 | 512x512, 512x256, 256x256 | base 35/47, normal 35/47, emis 14/47, ORM 35/47 | D | WRONG SHIP: the model is the Stargate-pack Daedalus (BC-304, source mods-stargate_pack_3_0), not the 22nd-century Starfleet Daedalus the roster names; dark grungy 128-512 px atlases (47 materials), 57k tris at 105 m (fed_daedalus/top.png). |
| `ship.fed.edward` | Edward (legacy 22) | BOTF kitbash: kitbash of ship.fed.nebula (colony ship: Nebula saucer, colony cylinders) | 24960 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of the Nebula saucer: Nebula decals duplicated and mis-mapped (huge NCC-67420 repeats, giant red decals), flat white (fed_edward/top.png). |
| `ship.fed.k7` | K-7 (roster 1032) | BOTF kitbash: kitbash of ship.fed.starbase (K-7 station: column, ring, arms) | 7848 | 128x128 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of the Fed starbase column on 128 px: stretched streaks, no K-7 identity (fed_k7/top.png). |
| `ship.fed.london` | London (legacy 39) | BOTF kitbash: kitbash of ship.fed.constellation (long-range cruiser) | 3639 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of Constellation: detached floating ball above the hull, smeared white nacelles (fed_london/3q.png). |
| `ship.fed.peregrine` | Peregrine (roster 1028) | BOTF kitbash: original model: 30 m Peregrine-class fighter (wedge hull, twin nacelles), built in Blender from primitives | 2172 | none | NONE (flat colour) | D | Primitive-built, brick-tile pattern, toy look; not the show Peregrine (fed_peregrine/top.png). |
| `ship.fed.soyuz` | Soyuz (roster 1005) | BOTF kitbash: kitbash of ship.fed.excelsior (frigate, raised nacelles) | 24427 | none | NONE (flat colour) | D | KITBASH of Excelsior with NO textures at all: plain white geometry (fed_soyuz/3q.png). |
| `ship.fed.sydney` | Sydney (roster 1006) | BOTF kitbash: kitbash of ship.fed.oberth (transport with cargo containers) | 8443 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of Oberth: flat grey box containers, no skin (fed_sydney/top.png). |
| `ship.fed.vengeance` | Dreadnought (Vengeance) (roster 1902) | BOTF kitbash: kitbash of ship.fed.sovereign (dreadnought) | 19732 | 256x128 | base 1/9, normal 1/9, ORM 1/9 | D | KITBASH of Sovereign on one 256x128 map: grey striped flat shading, grey slab box (fed_vengeance/top.png). |
| `ship.fed.wells` | Wells (roster 1029) | BOTF kitbash: kitbash of ship.fed.saber (special: ring ship) | 2251 | 1024x256 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of Saber with a giant plain ring; fake ship (fed_wells/3q.png). |
| `ship.fed.yclass` | Y-class (roster 1002) | BOTF kitbash: kitbash of ship.fed.danube (utility transport) | 10998 | 128x256 | base 1/9, normal 1/9, ORM 1/9 | D | KITBASH of Danube: untextured flat grey slab on top (fed_yclass/3q.png). |
| `ship.fer.nagus` | Nagus (roster 1401) | BC library `Marauder` (orion) | 10540 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | D | Same Marauder mesh as D'Kora re-scaled to 520 m: stand-in (fer_nagus/3q.png). |
| `ship.kli.bop_ent` | Bird-of-Prey (22nd c.) (roster 1102) | BC library `BirdofPreyE` (orion) | 39653 | 512x512, 128x128 | base 19/32, normal 19/32, emis 4/32, ORM 19/32 | D | Same mesh as B'rel (39,653 tris; frames identical): stand-in for the 22nd-century BoP (kli_bop_ent vs kli_brel). |
| `ship.kli.charghwi` | CharghwI' (legacy 79) | BOTF kitbash: kitbash of ship.kli.vorcha (heavy cruiser) | 25680 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of Vor'cha with a flat olive slab (kli_charghwi/top.png). |
| `ship.kli.d12` | D12 (roster 1104) | BOTF kitbash: kitbash of ship.kli.d7 (destroyer) | 4053 | 512x512 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of D7 with plain grey cylinders; 512 px (kli_d12/3q.png). |
| `ship.kli.liwi` | LI'wI' (legacy 67) | BOTF kitbash: kitbash of ship.kli.ktinga (cruiser) | 17387 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of K'Tinga with an untextured egg blob (kli_liwi/3q.png). |
| `ship.kli.mayduj` | May'Duj (legacy 73) | BOTF kitbash: kitbash of ship.kli.kvort (bulk carrier) | 41237 | 512x512 | base 1/9, normal 1/9, ORM 1/9 | D | KITBASH of K'Vort with huge flat grey cylinders (kli_mayduj/3q.png). |
| `ship.kli.neghtev` | Negh'Tev (roster 1106) | BOTF kitbash: kitbash of ship.kli.neghvar (battleship) | 24342 | 1024x1024 | base 1/9, normal 1/9, emis 1/9, ORM 1/9 | D | KITBASH of Negh'Var with flat green cylinders and slab (kli_neghtev/top.png). |
| `ship.kli.qang` | Qang (roster 1105) | BOTF kitbash: kitbash of ship.kli.kvort (ram-prow heavy cruiser) | 40803 | 512x512 | base 1/9, normal 1/9, ORM 1/9 | D | KITBASH of K'Vort with giant flat cylinders and a cone (kli_qang/3q.png). |
| `ship.car.hideki` | Hideki (roster 1300) | BC library `C_TNG_HIDEKI` (mods-bc_aftermath_cardassian_union-BC-Af) | 1358 | 256x256, 512x128, 256x128 | base 11/23, normal 11/23, emis 2/23, ORM 11/23 | C | 1.4k tris, 128-512 px atlas: mushy yellow, faceted (car_hideki/starboard.png). |
| `ship.fed.curry` | Curry (roster 1021) | BC library `Curry` (orion) | 6689 | 1024x1024 | base 6/18, normal 6/18, ORM 6/18 | C | Blotchy low-res saucer mottle, heavily faceted forward hull (6.7k tris), registry blur (fed_curry/3q.png). |
| `ship.fed.nebula` | Nebula (legacy 20) | BC library `Nebula` (orion) | 23776 | 1024x1024 | base 4/17, normal 4/17, emis 4/17, ORM 4/17 | C | SIDE-ON SMEAR: saucer window rows stretched into blurred slits and NCC-67420 / USS name soft and smeared on the saucer rim (fed_nebula/starboard.png). Cause in section 3. 1024 px; normal map shallow. |
| `ship.fed.outpost` | Outpost  (legacy 28) | BC library `FedOutpost` (orion) | 35791 | 1024x1024 | base 4/16, normal 4/16, emis 4/16, ORM 4/16 | C | Washed-out lumpy white with soft 1024 px skin, indistinct detail (fed_outpost/3q.png). |
| `ship.fed.ranger` | (unused) | Icewolf132 community art (franger.glb) | 677 | 512x512 | base 1/13, normal 1/13, ORM 1/13 | C | 677 tris: 14-sided faceted saucer, blurry mottled 512 px (fed_ranger/starboard.png). Class is in neither roster nor legacy list: unused. |
| `ship.kli.starbase` | Starbase  (legacy 77) | BC library `KlingonStarbase` (orion) | 1360 | 256x256 | base 4/16, normal 4/16, emis 3/16, ORM 4/16 | C | 1.4k tris, 256 px: stretched garish green, smeared panels (kli_starbase/top.png). |
| `ship.rom.outpost` | Outpost  (legacy 91) | BC library `RomulanOutpost` (km) | 3376 | 256x256, 128x128 | base 18/30, normal 18/30, emis 18/30, ORM 18/30 | C | 4k tris, 128/256 px: floating flat white slabs, no detail, stretched streaks (rom_outpost/starboard.png). |
| `ship.rom.starbase` | Starbase (legacy 97) | BC library `Romulanstarbase` (orion) | 1026 | 256x256 | base 4/16, normal 4/16, emis 4/16, ORM 4/16 | C | 1k tris, 256 px: giant pixelated green slabs close up (rom_starbase/top.png). |
| `ship.car.galor` | Galor (legacy 4) | BC library `Galor` (orion) | 5678 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Low-poly (5.7k); flat tan skin, little detail. |
| `ship.car.keldon` | Keldon (legacy 0) | BC library `Keldon` (orion) | 7422 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Flat tan skin; adequate. |
| `ship.car.outpost` | Outpost  (legacy 6) | BC library `CardOutpost` (orion) | 16532 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Mottled gold skin with dark smudges; shape fine. |
| `ship.car.platform` | (unused) | BC library `OrbitalWeaponsPlatform` (orion) | 1443 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Readable panels, strong yellow emissive; fine. |
| `ship.car.starbase` | Starbase  (legacy 12) | BC library `CardStarbase` (orion) | 51770 | 1024x1024 | base 6/18, normal 6/18, emis 6/18, ORM 6/18 | B | Best Cardassian hull: DS9 station readable at both distances; soft panels. |
| `ship.fed.akira` | Akira (roster 1022) | BC library `Akira` (orion) | 13530 | 1024x1024 | base 3/16, normal 3/16, emis 3/16, ORM 3/16 | B | 1024 px; plating and lettering soft, low contrast. |
| `ship.fed.ambassador` | Ambassador (legacy 43) | BC library `Ambassador__km` (km) | 32930 | 2048x2048 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | 2048 px (km variant) but blocky tile pattern on saucer top; registry fine. |
| `ship.fed.centaur` | Centaur (roster 1016) | BC library `Centaur` (orion) | 20885 | 1024x1024, 512x512, 1024x128 | base 14/26, normal 14/26, emis 9/26, ORM 14/26 | B | Soft hull panels; readable shape. |
| `ship.fed.challenger` | Challenger (roster 1009) | BC library `Challenger` (orion) | 3728 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Blocky pixel tiling on saucer plating (top view); fine at distance. |
| `ship.fed.cheyenne` | Cheyenne (roster 1010) | BC library `Cheyenne` (orion) | 4190 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Same blocky saucer plating as Challenger; nacelle stripes blurry. |
| `ship.fed.constellation` | Constellation (legacy 32) | BC library `Constellation` (orion) | 2767 | 1024x1024, 512x256, 128x64 | base 5/17, normal 5/17, emis 5/17, ORM 5/17 | B | Low-poly (2.8k tris), flat saucer, soft plating; legible. |
| `ship.fed.constitution_refit` | Constitution (refit) (roster 1004) | BC library `ConstitutionR` (orion) | 27360 | 1024x1024, 512x512 | base 5/18, normal 5/18, emis 5/18, ORM 5/18 | B | Clean; flat low-detail texture, tiny windows. |
| `ship.fed.danube` | Danube (roster 1017) | BC library `Danube` (orion) | 10434 | 512x512, 512x256, 256x512 | base 28/40, normal 28/40, emis 14/40, ORM 28/40 | B | 512 px skin soft; shapes and registry OK. |
| `ship.fed.defiant` | Defiant (legacy 34) | BC library `Defiant` (orion) | 54578 | 1024x1024, 512x512, 256x256 | base 16/28, normal 16/28, emis 12/28, ORM 16/28 | B | Top-down plating mushy (1024 px over a 170 m hull), side hull flat grey; decals readable. |
| `ship.fed.excelsior` | Excelsior (roster 1007) | BC library `Excelsior` (orion) | 23843 | 1024x1024, 512x512, 1024x128 | base 16/29, normal 16/29, emis 10/29, ORM 16/29 | B | Smooth; 1024 px, lettering readable, little surface detail. |
| `ship.fed.freedom` | Freedom (roster 1012) | BC library `Freedom` (orion) | 2566 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Low-poly (3.1k), blotchy saucer plating; OK at battle range. |
| `ship.fed.kelvin_type` | Kelvin (roster 1900) | BC library `bckelvin_refit` (mods-Retro-BC-Retro-BC) | 29281 | 2048x2048, 1024x1024 | base 16/28, normal 16/28, emis 15/28, ORM 16/28 | B | 2048 px but a checker/tile pattern visible on the saucer top; lettering fine. |
| `ship.fed.luna` | Luna (roster 1030) | BC library `Luna` (orion) | 14640 | 1024x1024, 512x512 | base 4/16, normal 4/16, emis 4/16, ORM 4/16 | B | Decent 1024 px skin, plates slightly soft. |
| `ship.fed.miranda` | Miranda (legacy 30) | BC library `Miranda` (orion) | 9965 | 1024x1024 | base 5/17, normal 5/17, emis 5/17, ORM 5/17 | B | Flat white hull with thin red lines; legible, little detail. |
| `ship.fed.new_orleans` | New Orleans (roster 1014) | BC library `NewOrleans` (orion) | 4922 | 1024x1024 | base 3/15, normal 3/15, emis 3/15, ORM 3/15 | B | Low-poly; saucer plating pixelated top-down; fine at distance. |
| `ship.fed.niagara` | Niagara (roster 1013) | BC library `Niagara` (orion) | 4812 | 1024x1024 | base 3/15, normal 3/15, emis 3/15, ORM 3/15 | B | Saucer window rows soft; nacelle stripes blurry. |
| `ship.fed.norway` | Norway (roster 1025) | BC library `Norway` (orion) | 7416 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Pale low-contrast panels; windows and greebles faint. |
| `ship.fed.nova` | Nova (roster 1018) | BC library `Nova` (orion) | 18618 | 1024x1024, 512x1024 | base 13/25, normal 13/25, emis 8/25, ORM 13/25 | B | Clean but soft; decals legible. |
| `ship.fed.nx` | NX (roster 1000) | BC library `NX` (orion) | 30858 | 1024x1024 | base 3/16, normal 3/16, emis 3/16, ORM 3/16 | B | Plating mottled and soft at 1024 px; shape good. |
| `ship.fed.oberth` | Oberth (legacy 35) | BC library `Oberth` (orion) | 7947 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Flat white; saucer lettering OK, few details. |
| `ship.fed.olympic` | Olympic (roster 1015) | BC library `Olympic` (orion) | 14798 | 2048x2048 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | 2048 px; sphere window grid good; some blur top-down. |
| `ship.fed.prometheus` | Prometheus (roster 1026) | BC library `Prometheus` (orion) | 7948 | 1024x1024 | base 6/18, normal 6/18, emis 5/18, ORM 6/18 | B | Clean; plating soft, registry readable. |
| `ship.fed.raven` | Raven (roster 1008) | BC library `JLSRaven` (mods-Delta-Quadrant-Pack-1.0-Delta-Quadr) | 15499 | 2048x1024, 1024x1024 | base 6/18, normal 6/18, emis 6/18, ORM 6/18 | B | Boxy; decals and registry legible, mid-res. |
| `ship.fed.rhode_island` | Rhode Island (roster 1027) | BC library `JLSRhodeIsland` (mods-Delta-Quadrant-Pack-1.0-Delta-Quadr) | 16910 | 2048x2048, 1024x1024 | base 5/17, normal 5/17, emis 2/17, ORM 5/17 | B | 2048 px; hull plating soft and grey; fine overall. |
| `ship.fed.saber` | Saber (roster 1024) | BC library `F_TNG_SABER` (mods-bc_aftermath_deep_space_nine-BC-Aft) | 703 | 512x1024, 512x512, 1024x256 | base 10/22, normal 10/22, emis 9/22, ORM 10/22 | B | Only 1.3k tris: faceted saucer rim from the side; skin OK. |
| `ship.fed.spacedock` | Spacedock (roster 1031) | BC library `DryDock` (orion) | 16238 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Grungy framework readable; 1024 px, soft panels. |
| `ship.fed.springfield` | Springfield (roster 1011) | BC library `Springfield` (orion) | 3407 | 512x512, 512x128, 128x128 | base 7/20, normal 7/20, emis 7/20, ORM 7/20 | B | Blotchy low-res saucer (512 px), mottled plating; readable shape. |
| `ship.fed.starbase` | Starbase  (legacy 37) | BC library `FedStarbase` (orion) | 17616 | 2048x2048, 1024x1024, 512x512 | base 11/24, normal 11/24, emis 11/24, ORM 11/24 | B | Soft 2048 px, stretched panel streaks on the lower hub; OK at range. |
| `ship.fed.steamrunner` | Steamrunner (roster 1023) | BC library `Steamrunner` (orion) | 5112 | 1024x1024, 32x32 | base 5/17, normal 5/17, emis 4/17, ORM 5/17 | B | Plain; plating soft, registry fine. |
| `ship.fer.dkora` | D'Kora (legacy 49) | BC library `Marauder` (orion) | 8548 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Marauder: smooth orange hull, window dots only; flat but clean. |
| `ship.kli.bop` | (unused) | BC library `D4_BOP` (mods-Retro-BC-Retro-BC) | 19574 | 512x512, 128x128 | base 23/35, normal 23/35, emis 7/35, ORM 23/35 | B | Green BoP (Retro-BC pack); 512 px, soft detail. |
| `ship.kli.brel` | B'rel (legacy 75) | BC library `BirdOfPrey` (orion) | 39653 | 2048x2048, 512x512 | base 19/32, normal 19/32, emis 4/32, ORM 19/32 | B | 4x (2048 px) hull maps in the glb but 512 px head; reads fine. |
| `ship.kli.d4` | D4 (roster 1903) | BC library `MJD4` (mods-Retro-BC-Retro-BC) | 19260 | 1024x1024 | base 4/16, normal 4/16, emis 3/16, ORM 4/16 | B | Retro D4; grunge reads as concrete; OK. |
| `ship.kli.d5` | D5 (roster 1100) | BC library `KD5` (orion) | 2020 | 512x512 | base 4/16, normal 4/16, emis 4/16, ORM 4/16 | B | 2k tris, 512 px: soft but legible. |
| `ship.kli.d7` | D7 (roster 1103) | BC library `K_TOS_D7` (mods-bc_aftermath_tos_war_pack-BC-Afterm) | 3329 | 512x512 | base 3/15, normal 3/15, emis 3/15, ORM 3/15 | B | Low-poly (3.3k), pale flat skin, lit windows readable. |
| `ship.kli.ktinga` | K'T'Inga (legacy 81) | BC library `KTinga` (orion) | 16503 | 1024x1024 | base 3/15, normal 3/15, emis 3/15, ORM 3/15 | B | Olive hull with dark crease shading; low gloss. |
| `ship.kli.kvort` | K'Vort (legacy 83) | BC library `Kvort` (orion) | 40041 | 512x512, 128x128 | base 15/28, normal 15/28, emis 4/28, ORM 15/28 | B | 512 px skin soft; black engine barrels flat, hull OK. |
| `ship.kli.neghvar` | Negh'Var (legacy 65) | BC library `Neghvar` (orion) | 23606 | 1024x1024 | base 2/15, normal 2/15, emis 2/15, ORM 2/15 | B | Saturated green/red 1024 px; panels soft, lettering unreadable. |
| `ship.kli.outpost` | Outpost  (legacy 71) | BC library `K_TOS_OUTPOST` (mods-bc_aftermath_tos_war_pack-BC-Afterm) | 3492 | 1024x1024 | base 1/13, normal 1/13, emis 1/13, ORM 1/13 | B | Large close-up reads fine; grungy soft texture. |
| `ship.kli.raptor` | Raptor (roster 1101) | BC library `K_TNG_RAPTOR` (mods-aftermath_wolf_359_addon_pack-BC-Af) | 4230 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Detailed and OK; 1024 px. |
| `ship.kli.vorcha` | Vor'cha (legacy 69) | BC library `Vorcha` (orion) | 25020 | 1024x1024 | base 2/14, normal 2/14, emis 2/14, ORM 2/14 | B | Clean; 1024 px panels soft top-down. |
| `ship.rom.bop` | (unused) | BC library `RomBoP` (orion) | 2199 | 256x256, 256x128, 128x128 | base 22/34, normal 22/34, emis 13/34, ORM 22/34 | B | Small 256 px atlas set; fine at range. |
| `ship.rom.dderidex` | D'Deridex (legacy 89) | BC library `Warbird` (orion) | 10035 | 1024x1024, 1024x512, 512x512 | base 7/19, normal 7/19, emis 5/19, ORM 7/19 | B | Warbird: green mottled soft 1024 px skin, excellent shape. |
| `ship.rom.valdore` | Valdore (roster 1205) | BC library `R_TNG_VALDORE` (mods-bc_aftermath_tng_romulan_addition-B) | 4016 | 1024x512, 512x512, 1024x256 | base 11/23, normal 11/23, emis 5/23, ORM 11/23 | B | Best Romulan detail; soft at close range. |
| `ship.fed.constitution` | Constitution (roster 1003) | BC library `Constitution` (orion) | 28542 | 1024x1024, 512x256, 256x256 | base 12/24, normal 12/24, emis 10/24, ORM 12/24 | A- | TOS Enterprise: clean, registry legible, minor flatness. |
| `ship.fed.constitution_kelvin` | Constitution (Kelvin) (roster 1901) | BC library `WCJJEnterprise` (mods-Retro-BC-Retro-BC) | 32419 | 2048x2048, 1024x2048, 2048x1024 | base 25/37, normal 25/37, emis 17/37, ORM 25/37 | A- | 2048 px, readable decals, smooth hull; low surface detail. |
| `ship.fed.galaxy` | Galaxy (legacy 24) | BC library `Galaxy` (orion) | 18505 | 4096x4096 | base 3/15, normal 3/15, emis 3/15, ORM 3/15 | A | Only hull on 4096 px maps (3 materials); windows and NCC-71879 crisp top and side. 100 MB glb. |
| `ship.fed.galaxy_x` | Galaxy-X (legacy 41) | BC library `GalaxyX` (orion) | 31278 | 2048x2048, 1024x1024 | base 5/17, normal 5/17, emis 5/17, ORM 5/17 | A- | 2048 px; crisp top and side; Galaxy skin with a third nacelle. |
| `ship.fed.intrepid` | Intrepid (roster 1019) | BC library `Intrepid` (orion) | 9288 | 1024x1024 | base 4/16, normal 4/16, emis 4/16, ORM 4/16 | A- | Clean 1024 px skin, registry sharp top-down; soft only at extreme close range. |
| `ship.fed.sovereign` | Sovereign (legacy 26) | BC library `DS9FXSovereign` (km) | 19048 | 2048x2048, 256x128 | base 6/18, normal 6/18, emis 4/18, ORM 6/18 | A- | 2048 px; plating readable top-down; slight softness on pylons. |
| `ship.fed.yeager` | Yeager (roster 1020) | BC library `F_DS9_YEAGER` (mods-bc_aftermath_deep_space_nine-BC-Aft) | 5421 | 1024x1024, 512x512 | base 7/19, normal 7/19, emis 7/19, ORM 7/19 | A- | Good hull, tidy 1024 px skin; slightly soft windows. |

## 6. Classes with NO model (procedural fallback hull, grade D): 73

Frames: `hull-audit\car_nerok`, `fer_tomax`, `rom_rtan`, `rom_science`, `borg_sphere`, `dom_attack_ship` (opened: nerok, tomax, rtan, science, dom). The tool treats every non-major faction as Federation-owned, so the Dominion/Borg frame shows the Federation procedural saucer; in play they get their owner's procedural hull. The procedural hulls are brick-tiled shells with glowing blobs (Romulan neon green, Ferengi/Cardassian copper, Federation white saucer with tiles): the generic placeholder look.

| class key | class(es) | library candidate in `H:\StarhelmTools\bc-ships\` (tris, max tex) | verdict |
|---|---|---|---|
| `ship.and.kumari` | Kumari (roster 1800) | none | needs sourcing |
| `ship.baj.assault` | Bajoran Assault Vessel (roster 1803) | none | needs sourcing |
| `ship.baj.lightship` | Lightship (roster 1804) | none | needs sourcing |
| `ship.borg.diamond` | Queen's Diamond (roster 1603) | JLSBorgDiamond (36k, 1024), BorgDiamond (km, 896) | library has the real class |
| `ship.borg.probe` | Probe (roster 1601) | Pyramid / PyramidP (847, 512), BorgCylinder (726, 256) | weak |
| `ship.borg.scout` | Scout Vessel (roster 1604) | BorgFT (76) / Pyramid | weak |
| `ship.borg.sphere` | Sphere (roster 1600) | JLSBorgSphere / Sphere (22.3k, 1024) | library has the real class |
| `ship.borg.tactical_cube` | Tactical Cube (roster 1602) | CubeT / JLSBorgTacticalCube (35k, 1024), Cube (15.8k) | library has the real class |
| `ship.breen.breen_warship` | Breen Warship (roster 1503) | BreenBB (3.6k, 512x256), BreenCA/CL | library has the real class |
| `ship.car.atr4107` | ATR-4107 (roster 1302) | none | needs sourcing |
| `ship.car.groumall` | Groumall (roster 1301) | CardFreighter__km (3.9k, 256) | library has a real-ish hull |
| `ship.car.kraxon` | Kraxon (roster 1304) | ObsidianKeldon (7.4k, 1024), Bakrus (3.4k, 512) | plausible |
| `ship.car.nerok` | Nerok (legacy 10) | C_TNG_KIMAL (842, 256), Barkus | plausible scout |
| `ship.car.netel` | Netel (legacy 8) | Hideki (776, 512), C_TNG_BARKUS (1.2k, 512) | plausible |
| `ship.car.neterok` | Neterok (legacy 16) | DS9FXKeldon (1.0k, 512), Keldon (7.4k, 1024) | plausible |
| `ship.car.nor` | Nor (roster 1303) | CardStation (34k tris, 1024), CardStarbase__km (5k, 512) | library has the station; CardStation best |
| `ship.car.ranol` | Ranol (legacy 2) | CardFreighter__km (3.9k, 256) | plausible colony ship |
| `ship.car.telok` | Telok (legacy 18) | Keldon, ObsidianKeldon (7.4k, 1024) | plausible |
| `ship.car.toran` | Toran (legacy 14) | CardFreighter__km (3.9k, 256) | plausible transport |
| `ship.devore.warship` | Devore Warship (roster 1721) | JLSDevoreWarship (4.4k, 2048) | library has the real class |
| `ship.dom.attack_ship` | Jem'Hadar Attack Ship (roster 1500) | DS9FXDominionFighter / bug__km (608, 512), D_TNG_BUGSHIP (1.5k, 512) | library has the real class |
| `ship.dom.battlecruiser` | Jem'Hadar Battle Cruiser (roster 1501) | DomBC (13.4k, 512), DS9FXDominionBC (13.4k, 512) | library has the real class |
| `ship.dom.battleship` | Jem'Hadar Battleship (roster 1502) | DomBB (9.4k, 512), D_TNG_BATTLESHIP (1.3k, 2048) | library has the real class |
| `ship.fer.bronta` | Bronta (legacy 57) | none | needs sourcing |
| `ship.fer.glantor` | Glantor (legacy 61) | none | needs sourcing |
| `ship.fer.goront` | Goront (legacy 63) | none (Marauder only) | Marauder-based or sourcing |
| `ship.fer.ngort` | Ngort (legacy 53) | none | needs sourcing |
| `ship.fer.ooron` | Ooron (legacy 45) | none (Marauder only) | Marauder-based or sourcing |
| `ship.fer.outpost` | Outpost  (legacy 51) | none | needs sourcing |
| `ship.fer.pod` | Pod Ship (roster 1400) | none | needs sourcing |
| `ship.fer.starbase` | Starbase  (legacy 59) | none | needs sourcing |
| `ship.fer.tokorn` | Tokorn (legacy 55) | none | needs sourcing |
| `ship.fer.tomax` | Tomax (legacy 47) | none (Marauder only) | Marauder-based or sourcing |
| `ship.gorn.cruiser` | Gorn Cruiser (roster 1701) | GornFreighter (394, 256), GornHFT | weak |
| `ship.hiro.hunter` | Hunting Vessel (roster 1703) | JLSHirogenHunter (20k, 2048), HirogenHunter | library has the real class |
| `ship.hiro.warship` | Hirogen Warship (roster 1704) | HirogenVenatic (40.9k, 1024), JLSHirogenHoloship | library has the real class |
| `ship.kazon.carrier` | Predator (roster 1706) | JLSKazonPredator (4.6k, 2048x1024) | library has the real class |
| `ship.kazon.raider` | Kazon Raider (roster 1705) | JLSKazonRaider / KazonRaider (5.1k, 1024) | library has the real class |
| `ship.krenim.timeship` | Temporal Weapon Ship (roster 1715) | JLSKrenimTimeship (25k, 2048) | library has the real class |
| `ship.krenim.warship` | Krenim Warship (roster 1716) | JLSKrenimWarship (13k, 2048) | library has the real class |
| `ship.malon.freighter` | Export Vessel (roster 1717) | MalonFreighter (5.6k, 512) | library has the real class |
| `ship.maquis.raider` | Maquis Raider (roster 1719) | JLSMaquisRaider (6.2k, 2048) | library has the real class |
| `ship.nau.raider` | Nausicaan Raider (roster 1805) | Raider / HeavyRaider (<=448, 256) | weak |
| `ship.orion.interceptor` | Orion Interceptor (roster 1702) | Raider / HeavyRaider (<=448, 256), OrionDD (456, 256) | weak |
| `ship.reman.scimitar` | Scimitar (roster 1720) | Scimitar (16k, 1024) | library has the real class |
| `ship.rom.bop_ent` | Bird-of-Prey (22nd c.) (roster 1200) | hero `ship.rom.bop` (RomBoP) is already in LocalAssets but unused; R_TOS_BOP (1.3k, 512) | alias the existing hero |
| `ship.rom.bop_tos` | Bird-of-Prey (roster 1202) | hero `ship.rom.bop` unused; TOSromulanBOP (Retro, 3.6k, 512), R_TOS_BOP | alias the existing hero |
| `ship.rom.ddredar` | D'dredar (legacy 101) | R_TNG_OSPREY (5.9k, 1024), WarbirdCVB (10k, 1024) | plausible |
| `ship.rom.drenet` | D'renet (legacy 95) | R_TNG_RAXAVIS (1.5k, 1024), RomulanFT (120) | plausible |
| `ship.rom.dretex` | D'retex (legacy 87) | RomulanFreighter (1.1k, 256), SFPRomulanFreighter | plausible (colony ship) |
| `ship.rom.dridren` | D'ridren (legacy 93) | R_TNG_HARRIER (2.2k, 1024) | plausible |
| `ship.rom.drone` | Drone Ship (roster 1201) | none | needs sourcing |
| `ship.rom.mogai` | Mogai (roster 1207) | R_TNG_TAVARA (5.2k, 1024x512), Valdore family | plausible |
| `ship.rom.narada` | Narada (roster 1904) | bcnarada (6.0k, 1024) | library has the real class |
| `ship.rom.norexan` | Norexan (roster 1206) | R_TNG_FALCON (3.9k, 1024), R_TNG_OSPREY (5.9k, 1024) | plausible |
| `ship.rom.rdaran` | R'daran (legacy 99) | RomulanFreighter, SFPRomulanFreighter (256) | plausible |
| `ship.rom.rderex` | R'derex (legacy 103) | R_TNG_FALCON (3.9k, 1024), R_TNG_RAPTOR (4.8k, 1024) | plausible |
| `ship.rom.rtan` | R'tan (legacy 85) | R_TNG_FALCON / R_TNG_HARPY (2.5k, 1024) | plausible |
| `ship.rom.science` | Science Vessel (roster 1204) | R_TNG_HARRIER (2.2k, 1024), R_TNG_TALON (2.3k, 1024) | plausible |
| `ship.rom.scout` | Scout Ship (roster 1203) | R_TNG_RAXAVIS (1.5k tris, 1024), RomulanFT / RomulanHFT (<=212, 256) | library has a plausible hull |
| `ship.s8472.bioship` | Bioship (roster 1714) | JLS8472 / Bioship (21.5k, 1024) | library has the real class |
| `ship.sona.battleship` | Son'a Battleship (roster 1718) | SonaBattleship (18.8k, 1024) | library has the real class |
| `ship.sul.cell_ship` | Cell Ship (roster 1708) | none | needs sourcing |
| `ship.tal.observer` | Observation Craft (roster 1806) | none | needs sourcing |
| `ship.thol.web_spinner` | Web Spinner (roster 1700) | TholianFreighter (592, 256), TholianFT, TholianStarbase | weak (small, low-res) |
| `ship.vid.warship` | Vidiian Warship (roster 1707) | JLSVidiian (1.7k, 2048), VidiianCruiser (1.7k, 1024) | library has the real class |
| `ship.vul.dkyr` | D'Kyr (roster 1801) | none | needs sourcing |
| `ship.vul.suurok` | Suurok (roster 1802) | none | needs sourcing |
| `ship.xindi.aquatic` | Aquatic Cruiser (roster 1711) | none | needs sourcing |
| `ship.xindi.insectoid` | Insectoid Warship (roster 1710) | none | needs sourcing |
| `ship.xindi.primate` | Primate Ship (roster 1712) | none | needs sourcing |
| `ship.xindi.reptilian` | Reptilian Warship (roster 1709) | none | needs sourcing |
| `ship.xindi.weapon` | Xindi Weapon (roster 1713) | none | needs sourcing |

Library candidates are by name from `H:\StarhelmTools\bc-ships\` and `BULK-REPORT.md`; none was rendered. Weapon sockets and hardpoints would have to be built for any added model.

## 7. Ranked fix list, worst first, ordered by what the player sees

Seen-in-play basis: starting fleets (`generateCampaign`: Early = scout + colony ship; Developed adds a destroyer; Expanded a cruiser and troop ship; Advanced/Supreme add a role-4 command ship) and the legacy catalogue (`data/base/ships.json`):

| empire | scout | colony | destroyer | cruiser | troop | command / mid build | starbase / outpost |
|---|---|---|---|---|---|---|---|
| Federation | Oberth (B) | **Edward (D kitbash)** | Miranda (B) | Ambassador (B) | **London (D kitbash)** | Galaxy (A), Sovereign (A-), Defiant (B), Nebula strike cruiser (C) | Starbase (B), **Outpost (C)** |
| Klingon | B'rel (B) | **LI'wI' (D kitbash)** | **May'Duj (D kitbash)** | K'T'Inga (B) | **Charghwi' (D kitbash)** | Vor'cha (B), Negh'Var (B), K'Vort (B) | **Starbase (C)**, Outpost (B) |
| Romulan | **D'renet (none)** | **D'retex (none)** | **D'ridren (none)** | **R'derex (none)** | **R'daran (none)** | D'Deridex (B); **R'tan, D'dredar (none)** | **Starbase (C), Outpost (C)** |
| Cardassian | **Nerok (none)** | **Ranol (none)** | **Netel (none)** | **Neterok (none)** | **Toran (none)** | Galor (B), Keldon (B); **Telok (none)** | Starbase (B), Outpost (B) |
| Ferengi | **Bronta (none)** | **Tomax (none)** | **Ngort (none)** | **Goront (none)** | **Glantor (none)** | D'Kora (B); **Tokorn, Ooron (none)** | **Starbase, Outpost (none)** |

1. **Romulan, Cardassian and Ferengi early game: 21 classes with no model** (7 Romulan, 6 Cardassian, 8 Ferengi in legacy catalogue plus roster extras in section 6). The library has plausible Romulan (R_TNG_* family, RomulanFreighter) and Cardassian (CardFreighter__km, Hideki/Barkus/Keldon variants, CardStation) hulls, and `ship.rom.bop` / `ship.kli.bop` heroes already sit unused in LocalAssets. Cheapest: add hero entries under the exact legacy keys. Ferengi small ships need sourcing.
2. **Fed Nebula** (every Fed player builds strike cruisers): emissive anisotropy plus 4x re-texture (section 3).
3. **Colony and troop ships** (Fed Edward/London, Klingon LI'wI'/Charghwi'/May'Duj): bake real textures onto the kitbashes or replace; on screen from turn 1.
4. **Starbases and outposts** (everyone builds them): Fed outpost, Klingon starbase, Romulan outpost and starbase are C; Ferengi has none.
5. Daedalus (wrong ship), K-7, Soyuz, Peregrine, Vengeance, Wells, Y-class, Sydney: later-era or rare; source or bake.
6. Soft-texture lift for the B hulls seen most: Miranda, Ambassador, Oberth, Defiant, Constellation, K'T'Inga, Vor'cha, Negh'Var, Galor, Keldon, via the HD pipeline.
7. Pipeline gap: Galaxy has 4x base and emissive but not the 4x normal, and no other hull has any 4x texture. Whichever lane re-textures must also change the glb packing so the HD normal is what `hero_look.gd` binds.

## 8. Cheapest real fix per C/D hull that has a model file

| key | grade | cheapest fix |
|---|---|---|
| `ship.fed.daedalus` | D | No 22nd-century Starfleet Daedalus in the library. Needs a new model, or keep as a labelled stand-in; `NXRefit` (11.7k tris, 2048) is NX-class. |
| `ship.fed.edward` | D | Replace the kitbash. Library has no colony ship (nearest `Freighter__base`, `Merchantman`, 256 px). Needs a new model, or a kitbash with its own textures. |
| `ship.fed.k7` | D | No K-7 in library; closest station hulls `DryDock`, `SpaceFacility` (40k tris), `BiranuStation`; none is K-7. |
| `ship.fed.london` | D | No London-class in library. Clean Constellation-family hull (`Constellation`, `Constellation2`) with the floating ball removed, or source. |
| `ship.fed.peregrine` | D | Library `Peregrine` is a re-skinned Nebula (23.8k tris), not the real class. Needs a new model. |
| `ship.fed.soyuz` | D | No Soyuz in library. Bake the parent Excelsior textures onto the kitbash (cheap), or source a model. |
| `ship.fed.sydney` | D | No Sydney in library; bake Oberth textures onto the kitbash; alternate `Oberth__mods-Retro-BC-Retro-BC` (512 px). |
| `ship.fed.vengeance` | D | No Vengeance in library. Bake Sovereign textures onto the kitbash, or source a model. |
| `ship.fed.wells` | D | No Relativity-era ship in library. Needs a new model. |
| `ship.fed.yclass` | D | No Y-class in library; bake textures on the Danube kitbash, or source. |
| `ship.fer.nagus` | D | Only the Marauder exists in the library; needs a new model for a distinct Nagus ship. |
| `ship.kli.bop_ent` | D | Library has `BirdOfPrey__base` (2.9k tris, 256 px), `BirdOfPrey__km` (2.5k, 512), `K_TOS_RAPTOR` (1.7k, 512); a distinct 22nd-century look is not there. Needs a new model; meanwhile alias to hero `ship.kli.bop` (D4_BOP). |
| `ship.kli.charghwi` | D | No Charghwi' in library. Bake textures onto the Vor'cha kitbash; alternates `K_DS9_VORCHA`, `K_TOS_C8`. |
| `ship.kli.d12` | D | No D12 in library; bake textures on the D7 kitbash (`D7TOS` 5.7k tris, `KD7` family). |
| `ship.kli.liwi` | D | No LI'wI' in library; bake textures on the K'Tinga kitbash (library `K_TNG_KTINGA`, 3.4k tris, 512). |
| `ship.kli.mayduj` | D | No May'Duj in library; bake K'Vort kitbash or use `KC7`/`KDN` (kdfmod, 512 px). |
| `ship.kli.neghtev` | D | No Negh'Tev in library; bake textures on the Negh'Var kitbash (library `K_TNG_NEGHVAR`, 1024 px). |
| `ship.kli.qang` | D | No Qang in library; bake textures on the K'Vort kitbash. |
| `ship.car.hideki` | C | Library `Hideki` (776 tris, 512), `DS9FXHideki` (776), `C_TNG_KIMAL`/`C_TNG_BARKUS` (<1.3k tris) are all low-poly. HD pipeline on the current atlas at best. |
| `ship.fed.curry` | C | Re-texture only (library has no other Curry): HD pipeline on `Curry`. |
| `ship.fed.nebula` | C | Keep the model, fix sampling and re-texture: run the HD pipeline on library `Nebula` (1024 px, 8 maps -> 4096) and wire the HD normal and emissive; or try library `DS9FXNebula` (km, 10,115 tris, 29 tex, 2048 px native; not viewed). |
| `ship.fed.outpost` | C | In use: library `FedOutpost` (35.9k tris, 1024). Alternates `SpaceFacility` / `BiranuStation` (40k tris, 1024) not viewed. HD pipeline on the current one is the cheapest. |
| `ship.fed.ranger` | C | Unused by any class; leave or retire. |
| `ship.kli.starbase` | C | Library `KlingonStarbase` is 1.4k tris / 256 px and is the only Klingon starbase there. Needs a new model, or a heavy re-texture. |
| `ship.rom.outpost` | C | Library `RomulanOutpost` (km 3.4k tris / 256 px) is in use. Needs a new model or heavy re-texture. |
| `ship.rom.starbase` | C | Library `Romulanstarbase` 1k tris / 256 px in use; `RomulanTMPStarbase` 560 tris. No better one; needs a new model. |

Library model swaps for B hulls (not viewed; tris and max texture from BULK-REPORT / folders): Defiant `DS9FXDefiant` / `AdminDef` (10.6k tris, 2048 vs current 54.6k / 1024), Excelsior `ExcelsiorII` (40k, 4096) or `WCexcel*` (2048), Constitution `NeoConstitution` (57.7k, 2048), Intrepid `JLSYOHVoyager` (12k, 2048), NX `NXRefit` (11.7k, 2048), Centaur `WCcentaur` (21k, 2048), Miranda `MirandaCVL` (30.6k, 2048), Warbird `DS9FXWarbird` (12.6k, 1024).

## 9. What I could not inspect

- The packed exe was not examined; all frames are the dev tree (`--path godot`, LocalAssets from `game\LocalAssets`). Exe parity is the shippack lane's claim. Before the first render the stale `godot\.godot` class cache (git-ignored) was refreshed with `godot --headless --import`; `git status` and `git diff` were identical before and after.
- Frames were opened as 2x2 contact sheets (3q, top, starboard at 960x540 each) except the Nebula starboard frame (1920x1080). Fine texture detail on other hulls is graded from half-scale frames.
- The gallery tool has no lit fly-by view beyond its default three-quarter, and maps every non-major faction prefix to Federation ownership.
- No model was opened in Blender; triangle counts are manifest values, glb accessor counts differ slightly; library candidates were not rendered.
- The emissive-sampler cause is read from the shader and frame, not tested live.
- Starting-fleet picks use the role sets in `Simulation.cpp` and the legacy catalogue; `eraBaseTechLevel` gating was not evaluated, so some cruiser/troop picks may be a tier later than listed.
- `docs/ASSET-SOURCES.md` lists Sketchfab/Trekmeshes candidates (e.g. Nebula `nestor_d`, Galaxy `CrustyBread`), but the models in use are BC-library models; ASSET-SOURCES.md marks BC Remastered as restricted (author permission needed). Licensing was not assessed.

## 9. Hull look lane (25 Nov 2026, Barclay): the title fleet and battle stations

Simon, 25 Nov: "the main menu ships are always really low res and low poly and look terrible, they have all these little white specs all over them, and the starbases in the battles do too". Frames: `Documents\ChatGPT\rc-captures\hull-look\before` and `after` (side by side in `after\side-by-side`).

Causes, each seen in a frame:
1. **The web title never drew a hero model.** `TitleFleet` was given a `BattleAssets` with no hull pack mounted (the hero packs are lazy: a campaign start fetches them), and nothing ever raised the title's own three packs, so the WebGL and WebGPU title drew the procedural stand-in hulls for the whole visit (`before\web-webgl\webgl-title-e*-40s.png`, `before\web-webgpu\`; the page fetched 0 hull packs). Those hulls are `hull.gdshader`: a window grid finer than a pixel with window energy 9.6 (4 x the battle's WINDOW_GAIN 2.4), which aliased into white speckle and bloomed. The 19 Nov Compatibility fix (WEB-PARITY gap 5) fixed the aliasing on web only and left Forward+ (the PC procedural station, any fallback hull) as it was.
2. **Navigation lights hung in open space.** `BattleShip._show_look` put the red, green and white strobe lights at the corners of the hull's bounding box (radius 0.12-0.45 units, flash 15x), so on a saucer, a nacelle pair or a bird-of-prey they floated beside the ship as red, green and blinding white balls: the white specks beside the PC title ships and every PC station (`before\pc-dev\title-real-e*.png`, `station-e*-*-close.png`).
3. **Window and normal-map sparkle on the hero shader.** `hero_hull.gdshader` sampled the emissive (window) map at full sharpness at grazing angles and let the window emission (strength x 2.4) run far over the glow threshold, with no specular anti-aliasing: each window texel landed on one pixel one frame and none the next. On Compatibility the same emission clipped to white dots and a white dome blob.
4. The fleet filled about a quarter of the frame (hulls about 500 px), so even correct hulls read soft; three escorts were low-detail (Cardassian Hideki 1.5k triangles, Romulan Bird-of-Prey 2.4k with 128 px mips).

Not causes (checked): dust motes (alpha 0.08), MSAA (the title viewport already runs MSAA 2x), texture mipmaps (the glb images are mip-chained on the load thread), VRAM compression (none on desktop; the web packs are WebP).

Changes: `TitleFleet` fetches its empire's hero packs on the web behind the finished backdrop, never draws a stand-in while they download, and fades each ship in (dither dissolve in the hero shaders); camera a fifth closer; Hideki and Bird-of-Prey escorts replaced by Kraxon and Mogai. `hero_hull.gdshader`: emission sampled `emissive_bias` softer and capped (0.8 and warmed on Compatibility), Toksvig plus geometric specular anti-aliasing. `hull.gdshader`: window fade and energy cap in every renderer. Nav lights snap to the nearest hull vertex, a light's size, strobe 5x not 15x.

Check: `tools\verify\hull_look_check.ps1` (PC: triangles, texture size, speck count per million hull pixels, nav-light distance, stand-in hull; web: `check_title_packs.py` for the packs and `web_title_shots.py --assert` for the title actually fetching them). Result before and after in the lane report.
