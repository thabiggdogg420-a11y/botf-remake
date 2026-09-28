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
