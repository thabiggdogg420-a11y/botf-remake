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

## Round 2 (16 Nov 2026, Simon: "stylised photos of real characters, the most prominent per race")
Round 1 (text-to-image only) is superseded: the model does not know the show characters, so identity must come from a reference image.
- **Method.** Federation, Dominion, Borg, Breen have no original BOTF still (the Federation still is an unnamed officer): text-to-image, described by costume and makeup.
  Klingon, Romulan, Cardassian, Ferengi: Qwen-Image-Edit-2511 with the game's own original still as the reference image (face, ridges, ears and hair stay), dressed as the leader
  (`tools/art/portrait_jobs.py`, `portrait_gen.py`), then one shared grade (`portrait_post.py`). Minor races: the same edit from each race's still (`LocalAssets/portraits-remaster/`, private, uncommitted).
- **Not done, on purpose.** No actor photos or show screenshots were downloaded as reference. Doing that would copy a real person's likeness; the game's own stills are already in the game.
  Characters are therefore the BOTF stills dressed as the named leader (Gowron, Sela, Dukat, Zek), not their show faces. Weyoun, the Queen and the Breen are described, not matched.
- **Starfleet delta.** No non-Federation portrait may carry it; the negative prompt and "no badges" lines enforce it, and each pick was checked by eye.
- **PortraitSlot** now crops to cover (`cover_src`, `FOCUS_Y` 0.20), and the Database (400x300) and hail (300x225) slots are 4:3 so the caption bar stays off the face. Tests: `scenes/ui/tests/test_portrait_slot.gd`.
- Eight powers now have portraits (`portrait.dom|borg|breen` added to `MAJOR_KEYS`).
