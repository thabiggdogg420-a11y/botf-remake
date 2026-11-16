# Portrait look test (16 Nov 2026)

Simon: "make the race photos a little more up dated". Look test only: nothing here replaces a shipped portrait.

## Where the portraits come from today
35 original BOTF stills (`LocalAssets/portraits/<key>.png`, 160x170, `tools/art/extract_portraits.py`), packed into `art_desktop.pck` / `art_web.pck`
by `tools/build_ship_packs.py --portraits` and read by `AppLoader.art(key, "portrait")`. Keys: `portrait.fed|kli|rom|car|fer` and 30 `portrait.minor.<race>`.
`portrait.leader.*` and `hail.bg.*` (the planned leader portraits and hail backdrops) have no art yet.

| Screen | File | Slot (px) |
|---|---|---|
| New campaign, race row (8 powers; only the first five have art, Dominion/Borg/Breen use emblems) | `new_campaign_screen.gd` | 76x62 |
| Diplomacy, hail panel (majors and minors) | `diplomacy_screen.gd` | 300x170 |
| Database > Races (majors) and Minor races | `database_screen.gd` | 420x236 |
| Empire banner (player) | `empire_screen.gd` | 112x112 |
| Saves list (player's power) | `saves_screen.gd` | 96x96 |

No advisor panel uses a portrait today. `PortraitSlot` stretches the texture to the slot (`draw_texture_rect(..., false)`), so a 0.94 image is squashed in 300x170 and 420x236: a cover-fit crop would fix this for old and new art alike (not done here).

## The test
Qwen-Image-2512 (Apache-2.0), locally in ComfyUI on the RTX 4090, 28 steps, 832x896, invented characters, no real-actor likeness and no original portrait as input.
Prompts and the runner: `tools/art/portrait_looktest.py`. Source and licence: `docs/ASSET-SOURCES.md` (AI image models).
- **photo**: cinematic photographic portrait, rim lighting, dark bridge background.
- **paint**: painted key-art, bold brushwork, glowing nebula behind.

Files here: `<race>-photo.jpg`, `<race>-paint.jpg`, `hail-<race>-before-photo-paint.png` (real hail panel, before / photo / paint), `raceselect-<race>-<before|photo|paint>.jpg`.
Full-size captures (all screens, crops): `Documents\ChatGPT\portrait-looktest-captures\` (before, after-photo, after-paint, gen1-3 candidates). Capture driver: `scenes/ui/tools/portrait_capture.gd`.

## Rejected during the test
- Cardassian: "spoon-shaped ridge" was drawn as a literal metal spoon / a metal pod on the forehead (round 2); reptile snout and grin in painted round 1. Fixed by asking for skin-coloured raised ridge, "upside-down teardrop", no snout (round 3, seed 22 chosen).
- Klingon: round 2 "ridged crest" produced a bone-coloured crown; round 1 seeds kept (photo s12, paint s11). Painted Klingon reads a little Viking; ridge is skin-toned and correct.
- Federation: uniform is a generic 24th-century red/black with the delta badge; shoulder panels are not an exact show uniform.
- No text artefacts, no hand problems (shoulders-up crops).

## Pick
See the report: photo style. Open items before any build-out: exact-uniform pass (fix with a LoRA or img2img on a reference), Klingon and Federation variety, the 35-portrait set, 3 other majors, a cover-fit PortraitSlot.
