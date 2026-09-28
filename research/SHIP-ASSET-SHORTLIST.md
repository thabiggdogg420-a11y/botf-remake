# Candidate ship models for combat presentation

Research snapshot: 27 September 2026. These are candidates only. A listing, preview, or stated licence is not proof that a model has been downloaded, inspected, or integrated. Star Trek designs and marks remain the property of their respective rights holders; a model author's terms cannot grant franchise rights they do not hold.

## Local baseline and scope

The local AIO 1.0.1 Vanilla overlay is not proven byte-identical to retail 1.0.2. Its validated catalogue contains 125 ship records. Numeric races 0–4 each have 20, 25, 20, 20, and 20 entries respectively; entries include two tech tiers for many names, outposts and starbases, and special vessels. Race 36 has ten unresolved Guardian records. Do not treat this visual shortlist as a complete or playable ship roster.

| Reference race ID | Canonical faction | Representative BOTF combat hull families in local catalogue | Candidate visual starting points |
|---:|---|---|---|
| 0 | Cardassian | Keldon, Galor, Netel, Nerok, Neterok, Telok, Toran | Galor class, Cardassian Battleship, Hedeki |
| 1 | Federation | Nebula, Galaxy, Sovereign, Miranda, Constellation, Defiant, Oberth, Ambassador, London | Enterprise-D/Galaxy, Sovereign, Defiant, Nova |
| 2 | Ferengi | Ooron, D'Kora, Ngort, Tokorn, Bronta, Glantor, Goront | Ferengi Marauder |
| 3 | Klingon | Negh'Var, Vor'cha, May'Duj, B'rel, CharghwI', K'T'Inga, K'Vort, LI'wI' | Negh'Var, Vor'cha, Bird of Prey, D7 |
| 4 | Romulan | R'tan, D'retex, D'Deridex, D'ridren, D'renet, R'daran, D'dredar, R'derex | D'Deridex Warbird, Romulan Bird of Prey |

Canonical race mapping follows the first five major-race records in the locally extracted ship table and the source descriptions. Role/name variation means a candidate is a silhouette/production reference, not an asserted one-to-one replacement. The table omits race 5+ special/minor vessels and the unresolved race 36 set.

## Ranked candidates

### 1. Trekmeshes: use as a discovery catalogue, then clear each model with its author

The catalogue reports 270 meshes and has filters for Federation, Klingon, Romulan, Cardassian, formats, and starships; formats include Blender, OBJ, 3DS, MAX, LWO, and others. Its current notice says the site will not be renewed after the current term, so downloads and metadata should be captured while available. The website's catalogue does not by itself provide a general game-redistribution licence. I found no author reuse grant on the individual pages opened; classify permissions as **unresolved; do not integrate or redistribute until the uploader's terms/permission are verified**.

| Candidate | BOTF visual fit | Verified listing facts | Terms and likely work |
|---|---|---|---|
| [U.S.S. Enterprise-D / Galaxy](https://trekmeshes.eu/download/dl196/USSEnterpriseD) | Federation Galaxy/Command Cruiser; useful mainline Federation silhouette | David Metlesits; MAX 27.82 MB and Blender conversion by Fabio A. Correa 46.94 MB are listed. | Permission unresolved. Blender file likely shortest import path; inspect geometry/materials/textures, reduce detail/LODs if needed, normalize pivot, scale and orientation, then export/import to Unreal. |
| [Ferengi Marauder](https://trekmeshes.eu/download/dl215/FerengiMarauder) | Direct family resemblance for D'Kora Marauder; best obvious Ferengi candidate found | Raul Mamoru; MAX 11.58 MB, Poser 10.2 MB, OBJ 12.54 MB, low-poly OBJ 3.41 MB. Page warns some objects can have inverted faces. | Permission unresolved. Prefer the low-poly OBJ for initial inspection; check supplied companion textures/material setup, normals/winding, scale, pivot, silhouette and Unreal import. |
| [Galor class Starship](https://trekmeshes.eu/download/dl080/GalorclassStarship) | Cardassian Galor/Battleship visual anchor | Mike Wright; 3DS 532.5 KB or MAX 548.39 KB, plus shared textures 449.72 KB. | Permission unresolved. 3DS/MAX are legacy inputs; likely conversion and texture relinking. Verify mesh complexity and orientation before use. |
| [Vor'cha battlecruiser](https://trekmeshes.eu/download/all) | Klingon attack cruiser / battlecruiser family | Search catalogue lists MAX and legacy 3DS options. The page is an index; resolve its individual author page before selecting. | Permission and exact author/file quality unresolved. Locate direct entry, capture uploader terms; convert legacy format if needed. |
| [Bird of Prey](https://trekmeshes.eu/download/all) | Klingon B'rel Scout and related light combat silhouettes | Catalogue lists 3DS and LightWave LWO models. | Permission and exact variant unresolved; several distinct Trek designs share this name. Inspect individual entries and choose one whose scale/silhouette matches the intended BOTF family. LWO/3DS may need Blender conversion. |
| [Negh'Var Flagship](https://trekmeshes.eu/download/all) | Klingon Negh'Var Strike Cruiser | Catalogue lists MAX and 3DS/MAX/LWO options; a site news item credits David “Knightrider” Metlesits for a high-quality Negh'var release. | Permission unresolved. Resolve individual file/author, avoid assuming every format is the same model; legacy conversion and material rework likely. |
| [Romulan Warbird](https://trekmeshes.eu/download/all) | Romulan D'Deridex Warbird | Catalogue lists multiple Warbird entries and formats including 3DS/MAX and COB. | Permission unresolved; duplicate listings may be different authors/designs. Need author-specific review and conversion, likely texture relinking. |
| [Romulan Bird of Prey](https://trekmeshes.eu/download/all) | D'renet scout/light ship silhouette | Catalogue lists 3DS. | Permission unresolved; exact class/version unclear. Lower priority than the Warbird until there is a direct entry and confirmed author terms. |

The catalogue index can change and has multiple entries with the same class label. Follow each result to its actual record, author, and archive before making an acquisition decision. Do not credit Trekmeshes itself as the mesh author.

### 2. BlendSwap: clearer declared licences for select Federation hulls

BlendSwap exposes a model file format and uploader-declared licence on each listing. Its current developer documentation explicitly says those licences are not verified by the platform, so retain the page and licence terms and check the asset itself. These pages are useful candidates for a personal project, subject to verifying uploader rights and applicable franchise restrictions.

| Candidate | BOTF fit | Verified listing facts | Terms and likely work |
|---|---|---|---|
| [U.S.S. Defiant by FatManInABox](https://blendswap.com/blend/22476) | Federation Heavy Escort (Defiant) | Blender 2.79, 13.1 MB, procedural textures; page declares CC-BY. | Description separately says usable in a non-commercial project with credit. Preserve author credit and the exact licence/page. Older Blender file: open and inspect node setup, simplify materials/geometry as needed, normalize pivot/scale, export to Unreal. This is the strongest specific Defiant candidate found, but licence declaration remains uploader-provided and franchise rights are outside it. |
| [Star Trek Sovereign Class by woki](https://blendswap.com/blend/10430) | Federation Sovereign / Dreadnought | Blender 2.6x, 1.87 MB; page declares CC-BY. Author says the model is unfinished and shader work remains. | Credit author under declared CC-BY; uploader's underlying rights not independently established. Expect finishing/reworking materials, inspecting missing details, and engine conversion. Good test candidate, not a finished ship. |
| [Star Trek Nova Class by ask13freak](https://blendswap.com/blend/9497) | Federation scout/science silhouette reference, not a direct BOTF class | Blender 2.6x, 4.4 MB; page declares CC-BY-SA and says credit creator. | Attribution plus share-alike obligations apply to the model/adaptation under the declared licence. Review compatibility with the game's intended distribution before adoption. Old Blender Internal materials need conversion; prioritize below Defiant/Sovereign. |
| [Tough Little Ship / Defiant by Chris Kuhn](https://blendswap.com/blend/13098) | Federation Heavy Escort | Blender 2.7x, 6.88 MB; page declares CC-BY-NC. Description says non-commercial only and describes a roughly 100K-polygon Blender model with packed textures. | Non-commercial restriction and high polygon count make it a weaker game candidate; unpack/relink textures, inspect compositor setup, reduce mesh complexity. Candidate only. |

BlendSwap's modern API documentation warns that licences are uploader-declared and unverified; it lists CC-BY, CC-BY-SA and CC-BY-NC as separate terms. This is a useful machine-readable catalogue, not a rights clearance service.

### 3. Existing Icewolf132 pack: cleared conversion pipeline, incomplete faction coverage

The local [original README](../game/ArtSource/Community/Icewolf132/README-original.txt) says models may be edited, kitbashed, converted, and used in other games with credit to Icewolf132. It also credits Taldren for Ulysses, Yamato, and Lyran Tiger designs. The local [conversion manifest](../game/ArtSource/Community/Icewolf132/conversion-manifest.json) records provenance and conversion details; [asset credits](../game/ASSET-CREDITS.md) says six models are converted and only Ranger currently illustrates combat. This source is the most actionable cleared community pack already in the project, but its listed designs cover Federation-style ships and Lyran/Tiger plus Reavers, not the complete canonical five major factions. Keep any new use within the README's attribution condition and preserve the original terms file.

## Practical acquisition and conversion order

1. Inspect the available direct Trekmeshes individual records for **one** strong Klingon (Vor'cha or Negh'Var) and one strong Romulan (D'Deridex). Obtain the author-specific reuse terms before importing. The catalogue has obvious faction coverage but no general permission located in this research.
2. Inspect the low-poly Marauder OBJ and Galor entry next to broaden Ferengi/Cardassian coverage. Capture archive, author, exact terms, and source hashes before conversion.
3. Evaluate the BlendSwap Defiant and Sovereign pages as Federation supplemental candidates. Keep author attribution and licence evidence next to acquired source files. The Declared CC licence does not resolve rights in the Star Trek design.
4. Convert approved candidates in Blender to the project pipeline (prefer GLB/glTF or FBX where supported by the existing Unreal import path). Check front/up axis, origin, consistent scene scale, normals, UVs, texture dependencies, material channels, polygon counts, screen-size readability, and LODs. Make a contact sheet and import into Unreal before calling a model suitable.
5. Build out variants by visual family and size tier, with a common style for hull materials, faction lights, weapons, shields, damage, and silhouette. Avoid assigning one exact class model to both tiers just because the roster has a tier-II record; tier variants may share a mesh only after gameplay/presentation owners choose that mapping.

## Coverage gaps

The shortest strong-model path still lacks verified-permission model coverage for Klingon and Romulan; the two unresolved communities are precisely the ones with rich catalogue listings. Cardassian Galor and Ferengi Marauder listings exist, but also need author-specific reuse clearance. Federation has several direct BlendSwap entries and the already converted pack, but those models are old Blender versions or unfinished, and no inspection has confirmed they meet the requested modern combat quality.

No candidate here covers all 85 mobile-ship records plus 20 station/outpost records for races 0–4, their tier distinctions, outposts/starbases, or the Guardian/special/minor ships. Small fleets can reuse a curated set of distinctive hulls during early combat work, but class-specific final art remains an explicit gap. If permission cannot be confirmed, the safe fallback is new original faction-inspired silhouettes built for this project, subject to its separate design policy.

## Verification sources

- Local source of truth for candidate mapping: `research/reference-local/validated-catalog/reference.json`; schema/provenance and caveats in `research/BOTF-RESEARCH.md`.
- Current project status and existing community pack: `game/ASSET-CREDITS.md`, `game/ArtSource/Community/Icewolf132/README-original.txt`, and `game/ArtSource/Community/Icewolf132/conversion-manifest.json`.
- Trekmeshes [catalogue](https://trekmeshes.eu/download/all), [Enterprise-D entry](https://trekmeshes.eu/download/dl196/USSEnterpriseD), [Marauder entry](https://trekmeshes.eu/download/dl215/FerengiMarauder), [Galor entry](https://trekmeshes.eu/download/dl080/GalorclassStarship), [catalogue](https://trekmeshes.eu/download), and [site notice/collection overview](https://trekmeshes.eu/about). Individual pages identify uploaders and formats but do not state an asset reuse grant in the page content inspected; permissions remain unresolved.
- BlendSwap [Defiant](https://blendswap.com/blend/22476), [Sovereign](https://blendswap.com/blend/10430), [Nova](https://blendswap.com/blend/9497), [Chris Kuhn Defiant](https://blendswap.com/blend/13098), and [licence/API documentation](https://blendswap.com/3d-mcp-api/docs).
- [Bridge Commander Remastered 1.2](https://www.gamefront.com/games/bridge-commander/file/bridge-commander-remastered) reports 57 new ships and third-party content; GameFront explicitly requires original-author permission to reuse or reshare parts. Treat it as a research reference only.

Independent review clarification: the FatManInABox Defiant listing shows a CC-BY badge but its creator text restricts use to non-commercial projects with credit. Preserve that narrower stated restriction in acquisition decisions. Direct Galor archive access was not verified by the independent reviewer. These remain candidates.

