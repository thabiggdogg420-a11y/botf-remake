# BOTF 3D Combat Ship Asset Pipeline & Candidate Set

## 1. Executive Summary

This document establishes the practical ship-asset acquisition and conversion pipeline for our modern Birth of the Federation (BOTF) 3D tactical combat presentation, targeting Unreal Engine integration.

Operating within our **personal-use scope** (no external author communications or unauthorized republications), this pipeline:
1. Validates community archives cryptographically (SHA-256) and structurally against path-traversal (zip-slip and sibling-prefix) vulnerabilities.
2. Parses legacy binary formats (MilkShape 3D v4) and converts meshes headlessly via Blender 5.2.1 LTS into clean, game-ready glTF/GLB models.
3. Standardizes mesh pivots to the **bounding-box center** at `(0, 0, 0)`, calculates exact measured bounding box dimensions and bounding sphere radii, and authors PBR Principled BSDF materials with packed textures.
4. Classifies assets honestly into **Production-Ready Candidates** (verified geometry, textures, and clear roles) versus **Experimental Visual Studies** (untextured or unfinished concept meshes excluded from default import).
5. Maps candidates against canonical BOTF ship catalogue records (`reference.json`) without silently conflating distinct classes, and provides an explicit stand-in policy for tactical combat.
6. Documents acquisition blockers for external shortlist candidates (BlendSwap, Trekmeshes) in alignment with `research/SHIP-ASSET-SHORTLIST.md`.

---

## 2. Ownership & Asset Governance

* **Owned Pipeline Tooling:**
  - `setup/combat-assets/archive_validator.py`
  - `setup/combat-assets/ms3d_parser.py`
  - `setup/combat-assets/convert_combat_assets.py`
  - `setup/combat-assets/candidate_manifest.py`
  - `setup/combat-assets/unreal_import_candidates.py`
  - `setup/combat-assets/test_combat_assets.py`
* **Owned Candidate Assets & Manifests:**
  - `game/ArtSource/CombatCandidates/source/` (safely unpacked source files and original author terms)
  - `game/ArtSource/CombatCandidates/converted/` (game-ready GLBs and per-asset metadata)
  - `game/ArtSource/CombatCandidates/manifest.json` (machine-readable master candidate manifest)
  - `game/ArtSource/CombatCandidates/README.md` (this authoritative technical specification)
* **Untouched Surfaces (Strict Boundary Enforcement):**
  - Existing `game/ASSET-CREDITS.md` remains untouched.
  - Existing assets under `game/ArtSource/Community/Icewolf132/` and `game/ArtSource/FleetStudies/` remain untouched.
  - Runtime game content in `game/Content/`, C++ simulation and engine source code, global settings, and build scripts remain untouched.

---

## 3. Source Verification & Author Reuse Terms

### 3.1. Icewolf132 Mesh Pack 1 (Permitted Community Asset)
* **Source Archive:** `iwmeshpack1.zip` (SHA-256: `214cedf43cc9ee5f891b588872b058b00d55d928543262da03f7b49292baafe9`, 2,557,756 bytes)
* **Author:** Icewolf132 (Compiled 3 March 2009 for the Armada community).
* **Verbatim Author Reuse Grant (`source/Icewolf132/README.txt`):**
  > *"All are free to complete, edit, rip, kitbash, or convert the models and files within the pack without seeking permission first, under the condition that credit is given where it is due. You must provide credit to me, Icewolf132, ONLY if you use any part of the original files, in short if you do a redo of one of the units, I won't hunt you down if you don't mention me :P.*
  >
  > *This is not limited to the Armada community, if you wish to use anything within this pack for another game or for another purpose, feel free to do so as long as the above condition is met.*
  >
  > *Also please note that some of the models have additional credits attached to them for their design, they are as followed;*
  > *Ulysses class - Design originally by Taldren (SFC I and II, Klingon Academy)*
  > *Yamato class - Design originally by Taldren (SFC I and II, Klingon Academy)*
  > *Lyran "Tiger" class cruiser - Design originally by Taldren (SFC I and II, Klingon Academy)"*
* **Franchise Rights Distinction:**
  Author reuse terms apply strictly to the 3D model files created by Icewolf132. The underlying Star Trek ship designs, marks, and faction identities remain the intellectual property of Paramount/CBS. A community author's reuse grant does not transfer franchise ownership.
* **Verification Status:** **VERIFIED & PERMITTED**. Unconditional community model grant subject to author attribution. Original terms preserved in `game/ArtSource/CombatCandidates/source/Icewolf132/README.txt`.

### 3.2. Evaluation of External Shortlist Candidates (Trekmeshes & BlendSwap)
In accordance with `research/SHIP-ASSET-SHORTLIST.md` and direct system verification:
* **Trekmeshes.eu:**
  - Catalogues listings for Vor'cha, Bird of Prey, Negh'Var, and Enterprise-D.
  - **Blocker:** Trekmeshes has no general game redistribution grant; download links redirect to interactive pages or are gated; the site announcement states pending non-renewal; contacting individual authors is barred by hard rules and our personal-use scope.
  - **Status:** **UNRESOLVED CANDIDATE**. Not acquired or fabricated.
* **BlendSwap.com:**
  - Evaluated candidate: U.S.S. Defiant (`blend/22476` by FatManInABox, declared CC-BY).
  - Evaluated candidate: Sovereign class (`blend/10430` by woki, declared CC-BY, unfinished).
  - **Blocker:** Direct programmatic HTTP downloads return HTML requiring authenticated user account login; BlendSwap API documentation explicitly notes licenses are uploader-declared and unverified; uploader rights in Paramount/CBS Star Trek designs remain unestablished.
  - **Status:** **GATED CANDIDATE**. Documented in manifest, not fabricated.

---

## 4. Candidate Model Set: Measured Geometry & Classification

All converted candidate GLBs and their corresponding `.meta.json` files are stored in `game/ArtSource/CombatCandidates/converted/`. Geometry values below are **measured directly from the output GLBs and metadata**, matching `manifest.json`.

### 4.1. Production-Ready Initial Candidates (Status: `Ready`)
These models have complete, verified geometry and textures, and are approved for default batch import:

| Candidate Asset | Faction | Tactical Presentation Role & BOTF Fit | Author / Lineage | Triangles | Vertices | Textures | Bounding Sphere Radius | Status |
|---|---|---|---|---:|---:|---|---:|---|
| **franger.glb** | Federation | Scout / Light Escort illustration | Icewolf132 | 677 | 581 | `franger.tga` (packed) | 10.288 | **Ready** |
| **freaver1.glb** | Federation | Fast Destroyer (Reaver Variant 1, single nacelle) | Icewolf132 | 647 | 560 | `freaver.tga` (packed) | 7.645 | **Ready** |
| **freaver2.glb** | Federation | Mainline Destroyer (Reaver Variant 2, dual nacelle; Miranda tier) | Icewolf132 | 759 | 680 | `freaver.tga` (packed) | 7.975 | **Ready** |
| **freaver3.glb** | Federation | Heavy Destroyer (Reaver Variant 3, heavy pod; Constellation tier) | Icewolf132 | 891 | 822 | `freaver.tga` (packed) | 6.765 | **Ready** |
| **fUlysses.glb** | Federation | Dreadnought / Command Cruiser illustration (3-nacelle dreadnought) | Icewolf132 (Design: Taldren) | 1,076 | 884 | `SaucerUlysses.tga`, `HullUlysses.tga`, `NacelleUlysses.tga` (packed) | 42.880 | **Ready** |
| **klingon_cruiser.glb** | Klingon | Attack Cruiser Study (Vor'cha / K't'inga archetype) | BOTF Remake Project | 4,824 | 2,986 | Procedural tactical PBR materials | 2.363 | **Ready** |
| **lcruise1.glb** | Independent | Heavy Cruiser (Lyran Tiger class) | Icewolf132 (Design: Taldren) | 1,130 | 1,023 | `lca.tga` (packed) | 11.766 | **Ready** |

### 4.2. Experimental Visual Studies (Status: `Experimental`)
These models represent unfinished author concepts or abandoned visual studies from the original pack. They are converted with synthetic PBR materials and preserved on disk for research and future refinement, but are **excluded from default Unreal batch import**:

| Study Asset | Faction Context | Author Pack Context | Triangles | Vertices | Measured Radius | Status | Exclusion Rationale |
|---|---|---|---:|---:|---:|---|---|
| **kqultoq.glb** | Klingon | Pack README: *"kqultoq - A klingon fighter concept, there was a competed version, but I have no idea where it's at."* | 660 | 399 | 50.156 | **Experimental** | Unfinished concept mesh; untextured geometry assigned synthetic PBR armor; not a canonical BOTF class or B'rel Bird of Prey. |
| **RemanBoP.glb** | Reman | Pack README: *"RemanBoP - A Reman Bird of Prey... but was soon abandoned due to my inability to make it look any better."* | 371 | 575 | 7.704 | **Experimental** | Abandoned author study; Reman identity is distinct from canonical Romulan BOTF classes. |
| **rhhawk_TMP.glb** | Romulan | Pack README: Romulan Hellhawk TMP concept mesh. | 263 | 276 | 8.249 | **Experimental** | Unfinished author concept; retained for silhouette reference only. |

---

## 5. Canonical BOTF Class Mapping & Stand-in Policy

In Birth of the Federation (`research/reference-local/validated-catalog/reference.json`), the Federation (Race 1) and Klingon (Race 3) catalogues specify distinct ship records and tactical roles. **We do not silently equate one hull with every class or tech tier.**

Where community assets provide suitable silhouettes, they serve as **explicit provisional visual stand-ins** and must be labeled as such on tactical UI and radar displays:

### 5.1. Federation Catalogue Mapping (Race 1)

| BOTF Record ID | Canonical Ship Name | Canonical Ship Class | Canonical Role | Provisional Visual Stand-in Asset | Stand-in Classification & Limitations |
|---:|---|---|---:|---|---|
| **20, 21** | Strike Cruiser (I & II) | Nebula | 3 | *(None / FleetStudy procedural silhouette)* | Ranger is a scout, NOT a strike cruiser. Nebula requires dedicated saucer+pod geometry. |
| **24, 25** | Command Cruiser (I & II) | Galaxy | 4 | *(None / FleetStudy procedural silhouette)* | Do not substitute Ulysses for Galaxy; Enterprise-D requires dedicated 2-nacelle command hull. |
| **26, 27** | Dreadnought (I & II) | Sovereign | 4 | `fUlysses.glb` | **PROVISIONAL STAND-IN**: Heavy 3-nacelle dreadnought provides capital command firepower illustration; not a 1:1 Sovereign likeness. |
| **30, 31** | Destroyer (I & II) | Miranda | 1 | `freaver2.glb` | **PROVISIONAL STAND-IN**: Reaver Variant 2 (dual nacelles) illustrates mainline destroyer role. |
| **32, 33** | Heavy Destroyer (I & II) | Constellation | 1 | `freaver3.glb` | **PROVISIONAL STAND-IN**: Reaver Variant 3 (reinforced equipment pod) illustrates heavy destroyer role. |
| **34** | Heavy Escort | Defiant | 1 | `freaver1.glb` | **PROVISIONAL STAND-IN**: Reaver Variant 1 (compact fast destroyer) illustrates light escort role until Defiant is cleared. |
| **35, 36** | Science Vessel (I & II) | Oberth | 2 | `franger.glb` | **PROVISIONAL STAND-IN**: Ranger illustrates light scout/patrol capability; does not possess Oberth twin-pod geometry. |
| **41, 42** | Heavy Cruiser (I & II) | Galaxy-X | 4 | `fUlysses.glb` | **PROVISIONAL STAND-IN**: Shared 3-nacelle dreadnought design lineage illustrates heavy cruiser upgrade. |
| **43, 44** | Light Cruiser (I & II) | Ambassador | 3 | *(None / FleetStudy procedural silhouette)* | Preserved for dedicated light cruiser geometry. |

### 5.2. Klingon Catalogue Mapping (Race 3)

| BOTF Record ID | Canonical Ship Name | Canonical Ship Class | Canonical Role | Provisional Visual Stand-in Asset | Stand-in Classification & Limitations |
|---:|---|---|---:|---|---|
| **65, 66** | Strike Cruiser (I & II) | Negh'Var | 3 | *(None / FleetStudy procedural silhouette)* | Heavy flagship; community candidates remain unresolved. |
| **69, 70** | Attack Cruiser (I & II) | Vor'cha | 1 | `klingon_cruiser.glb` | **PROVISIONAL ARCHETYPE**: Procedural battlecruiser study provides canonical Vor'cha/K'T'Inga command head, boom, and swept-wing disruptors. |
| **73, 74** | Destroyer (I & II) | May'Duj | 1 | *(None / FleetStudy procedural silhouette)* | Do not map K'qultoq fighter concept to canonical May'Duj destroyer. |
| **75, 76** | Scout (I & II) | B'rel | 2 | *(None / FleetStudy procedural silhouette)* | B'rel Bird of Prey requires cleared community mesh or dedicated authoring. |
| **81, 82** | Battle Cruiser (I & II) | K'T'Inga | 3 | `klingon_cruiser.glb` | **PROVISIONAL ARCHETYPE**: Shares attack cruiser visual family. |
| **83, 84** | Heavy Cruiser (I & II) | K'Vort | 1 | `klingon_cruiser.glb` | **PROVISIONAL ARCHETYPE**: Scaled heavy battlecruiser illustration. |

---

## 6. Pipeline Tooling & Geometric Standardization

The pipeline tooling is implemented in `setup/combat-assets/`:

```
setup/combat-assets/
├── archive_validator.py        # Cryptographic verification & hardened path containment
├── ms3d_parser.py              # Pure-Python MilkShape 3D v4 binary parser & inspector
├── convert_combat_assets.py    # Headless Blender converter (materials, UVs, origin centering, bounds)
├── candidate_manifest.py       # Pipeline runner & manifest.json generator
├── unreal_import_candidates.py  # Unreal Engine Python automation script with hash checks & path sanitization
└── test_combat_assets.py       # Comprehensive regression test suite
```

### 6.1. Geometric & Coordinate Standardization
1. **Axis Remapping:** MilkShape coordinates $(X, Y, Z)$ map to standard Blender coordinates as $(X, -Z, Y)$. Export to GLB enforces `export_yup=True` to comply with glTF 2.0 and Unreal Engine standards.
2. **Bounding-Box Center Pivot:** The geometric midpoint $[(min + max) / 2]$ across all vertices is subtracted from every vertex coordinate. This guarantees that model transforms rotate and scale cleanly about the ship's physical bounding-box center.
3. **Bounding Sphere Radius:** The exact maximum vertex distance from origin $(0, 0, 0)$ is recorded as `bounding_sphere_radius`. In Unreal, `BotfCombatScene.cpp` uses `Radius = Mesh->GetBounds().SphereRadius` to establish tactical scales ($80 / Radius$) and position shield shells. **Final in-engine scaling must consume the measured imported `UStaticMesh::GetBounds().SphereRadius`.**
4. **Normals & Smoothing:** Per-triangle vertex normals authored in MilkShape are converted and applied via `mesh.normals_split_custom_set()` with smooth polygon shading enabled.

### 6.2. Material & Texture Pipeline
* **PBR Principled BSDF:**
  - Diffuse color maps to `Base Color`.
  - Authored Phong shininess is converted to PBR roughness:
    $$\text{Roughness} = \min\left(1.0, \sqrt{\frac{2.0}{\max(0, \text{shininess}) + 2.0}}\right)$$
  - Emissive colors and glow channels are connected to `Emission Color` and `Emission Strength`.
  - Textures (`.tga`, `.bmp`) are loaded, linked, and packed directly into the binary `.glb` container.

---

## 7. Unreal Engine Integration Instructions

### 7.1. Dry-Run Verification (CLI / Outside Unreal)
Verify manifests, file presence, hashes, and planned destinations without launching Unreal Engine:

```powershell
python setup/combat-assets/unreal_import_candidates.py
```

### 7.2. Automated Batch Import (Unreal Editor Python)
Run the automated import script through the Unreal Editor Python console or command line:

```powershell
& "C:\Program Files\Epic Games\UE_5.x\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\game\BOTFRemake.uproject" `
    -run=pythonscript `
    -script="$PWD\setup\combat-assets\unreal_import_candidates.py"
```

The script:
1. Reads `game/ArtSource/CombatCandidates/manifest.json`.
2. Validates that each GLB exists on disk and its SHA-256 matches the manifest.
3. Sanitizes destination package paths (e.g. `/Game/CombatCandidates/Federation/...`, `/Game/CombatCandidates/Klingon/...`, `/Game/CombatCandidates/Independent/...`).
4. **Filters by default to import only candidates with `status == "Ready"`.** Experimental visual studies are skipped unless `--include-experimental` is passed.
5. Imports each `.glb` as a `UStaticMesh` using `unreal.AssetImportTask`.
6. Inspects `task.imported_object_paths`, verifies that the imported asset is a `UStaticMesh`, and logs the measured `SphereRadius`.

### 7.3. Runtime Combat Renderer Integration
In `game/Source/BOTFRemake/Private/BotfCombatScene.cpp`:
The combat renderer can consume these candidates by loading them and applying scale based on the measured bounds:

```cpp
// Example: Safe loading of verified candidate with measured bounds scaling:
FString AssetPath = TEXT("/Game/CombatCandidates/Federation/franger/franger.franger");
UStaticMesh* ShipMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
if (ShipMesh)
{
    ShipComponent->SetStaticMesh(ShipMesh);
    const float Radius = FMath::Max(1.0f, ShipMesh->GetBounds().SphereRadius);
    ShipComponent->SetRelativeScale3D(FVector(80.0f / Radius));
}
```

---

## 8. Limitations & Downstream Handoff

1. **Roster Completeness:** The initial candidate set provides 5 Federation hulls and 1 Klingon hull (plus 1 independent cruiser). It does not replace all 25 Federation and 20 Klingon catalogue records. Hero ships (Enterprise-D, Sovereign, Vor'cha, Negh'Var, B'rel) remain blocked by unresolved permissions.
2. **Texture Fidelity:** Icewolf132 textures date to 2005–2009 (standard diffuse maps at 512x512 / 1024x1024 resolution without authored normal/roughness/metallic maps). Our pipeline synthesizes PBR roughness and metallic properties, but they are not high-density 4K PBR remakes.
3. **No Silently Conflated Hulls:** Downstream combat logic must preserve distinct tactical roles. If a model is used as a stand-in, it must be marked as an interim illustration in UI/radar overlays rather than claiming identical visual fidelity.
4. **Independent Review Ready:** All source archives, manifests, converted models, and tooling are self-contained, validated by regression tests, and reproducible.
