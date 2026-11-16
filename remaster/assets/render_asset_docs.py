#!/usr/bin/env python3
"""Render human-readable docs from manifest.json.

  python remaster/assets/render_asset_docs.py

Writes:
  remaster/docs/ASSET-SOURCES.md             full candidate table, per-faction coverage, gaps
  remaster/game/ASSET-CREDITS.md             replaces the block between the GENERATED markers

Run after every manifest edit so the docs never drift from the manifest. Local, per-PC credits for what
is actually installed are written separately by Import-Assets.ps1 (LocalAssets/ASSET-CREDITS.local.md).
"""
import collections
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
M = json.load(open(os.path.join(HERE, "manifest.json"), encoding="utf-8"))
A = M["assets"]
BEGIN = "<!-- GENERATED:manifest-credits BEGIN (render_asset_docs.py) -->"
END = "<!-- GENERATED:manifest-credits END -->"
USABLE = ("clear", "clear-personal", "unclear")


def cell(s):
    return (str(s) if s is not None else "").replace("|", "\\|").replace("\n", " ")


def link(c):
    p = c["page"]
    return "[%s](%s)" % (cell(c["sourceName"]), p) if p.startswith("http") else cell(c["sourceName"]) + " (`" + p + "`)"


def best(entry):
    for c in entry["candidates"]:
        if c["permission"] in USABLE:
            return c
    return None


def sources_doc():
    out = []
    w = out.append
    kinds = collections.Counter(v["kind"] for v in A.values())
    ncand = sum(len(v["candidates"]) for v in A.values())
    perm = collections.Counter(c["permission"] for v in A.values() for c in v["candidates"])
    sites = collections.Counter(c["sourceSite"] for v in A.values() for c in v["candidates"])
    w("# Asset sources (generated from `remaster/assets/manifest.json`)")
    w("")
    w("Research date %s. **Do not edit by hand**; edit the manifest and run `python remaster/assets/render_asset_docs.py`." % M.get("researchDate"))
    w("")
    w("**Policy.** Every third-party asset here (HD ship models, show audio, mod sounds, original BOTF voice files) is private and local only. "
      "The repo holds the manifest, importer scripts and credits, never the binaries. Files are downloaded by hand into a folder, "
      "resolved by `remaster/game/Import-Assets.ps1` into the gitignored `remaster/game/LocalAssets/`, and imported into the gitignored `/Game/Local`. "
      "A licence on an upload is the uploader's claim. It cannot grant rights in Star Trek designs or show audio. \"Permission unclear\" is the normal state for these assets.")
    w("")
    w("## Totals")
    w("")
    w("- %d asset keys: %s." % (len(A), ", ".join("%d %s" % (n, k) for k, n in sorted(kinds.items()))))
    w("- %d ranked source candidates: %s." % (ncand, ", ".join("%d %s" % (n, k) for k, n in sorted(sites.items(), key=lambda x: -x[1]))))
    w("- Permission flags: %s." % ", ".join("%d %s" % (n, k) for k, n in sorted(perm.items())))
    w("")
    w("Permission flags: **clear** means redistributable with credit (in-repo Icewolf132/project art only). **clear-personal** means the user's own game copy, for private use. "
      "**unclear** means an uploader licence or no stated terms; the franchise rights are unresolved. **restricted** means the author's terms forbid reuse or derivatives (NoDerivs, or mod "
      "readmes that deny reuse). The importer skips restricted candidates unless `-AllowRestricted` is passed.")
    w("")
    w("## Key scheme")
    w("")
    w("One scheme is used by the ship roster, this manifest, `Import-Assets.ps1` and the Unreal client (`UBotfAssetRegistry`):")
    w("")
    w("- `ship.<fac>.<class>`: ship models. Expanded roster classes use the roster `assetKey` verbatim; legacy catalogue classes use "
      "`ship.<fac>.<slug of class name>` (for example `ship.kli.vorcha`, `ship.fed.galaxy_x`) plus `ship.<fac>.starbase` / `ship.<fac>.outpost`.")
    w("- `weapon.<WeaponType>` for roster weapon types (`weapon.PhaserArray`), with optional faction variants `weapon.<WeaponType>.<fac>` "
      "(`weapon.Disruptor.rom`) and last-resort `weapon.generic.beam` / `weapon.generic.torpedo`.")
    w("- `study.<fac>`: in-repo placeholder meshes (project art). `sfx.*`, `vo.*`, `mus.*`: audio cues (AUDIO-PLAN.md).")
    w("")
    w("Every roster and legacy key has an entry, even with no candidate, so coverage is explicit; the entry's fallback names what is shown instead. "
      "`python remaster/assets/check_asset_keys.py` fails when a key is missing or a fallback chain is broken. When nothing in a chain resolves, "
      "the client uses its built-in placeholder for the faction.")
    w("")
    w("## Per-faction ship coverage")
    w("")
    w("| Faction | Keys | With a downloadable, non-restricted candidate | Only restricted / unresolved | Gaps (fallback) |")
    w("|---|---:|---|---|---|")
    fac = collections.OrderedDict()
    for k, v in A.items():
        if v["kind"] != "model" or k.startswith("study."):
            continue
        fac.setdefault(v["faction"], []).append((k, v))
    for f, items in fac.items():
        good = [k for k, v in items if any(c["permission"] in USABLE and c["sourceSite"] in ("sketchfab", "blendswap", "in-repo") for c in v["candidates"])]
        weak = [k for k, v in items if k not in good and v["candidates"]]
        gap = [k for k, v in items if not v["candidates"]]
        w("| %s | %d | %s | %s | %s |" % (f, len(items), ", ".join(good) or "none", ", ".join(weak) or "none",
                                         ", ".join("%s (-> %s)" % (k, A[k]["fallback"]) for k in gap) or "none"))
    w("")
    w("## Gaps")
    w("")
    w("- **Klingon Vor'cha.** No downloadable Sketchfab upload was found; phoenixbondi's is view-only. The remaining routes are Trekmeshes (entry unresolved), BC Remastered, "
      "which is restricted, and Armada 3, also restricted. Until one is chosen it falls back to the Negh'Var.")
    w("- **Cardassian Keldon and Hideki.** These exist only in mod packs whose terms deny reuse. They fall back to the Galor, which has a CC-BY octave767 upload and a Trekmeshes Mike Wright entry.")
    w("- **Ferengi D'Kora.** No free Sketchfab upload exists. Trekmeshes (Raul Mamoru OBJ, permission unclear) is the main route.")
    w("- **Starbases** for Klingon and Romulan have no candidates and use the Federation starbase. Outposts share the starbase key.")
    w("- **Weapon audio for Cardassian, Ferengi, pulse phaser and plasma torpedo** has no dedicated TrekCore clip. Stand-in clips are listed and flagged as such in the display name.")
    w("- **HD versus game-ready.** ZEOCMF uploads are the most detailed (1–5M triangles) but carry CC-BY-NC-ND. Converting and decimating them is arguably a derivative, so they are marked restricted. "
      "CrustyBread, wholock, octave767 and rossrobotics uploads are CC-BY and 10k–260k triangles.")
    w("- **Uploads ripped from games** (e.g. \"Star Trek Online | ...\" by LoganRolphh) are listed only as a last resort. The uploader cannot licence them.")
    w("- **Original BOTF voice** is available only from the user's own install. See AUDIO-PLAN.md.")
    w("- **Minor races and the Guardian/special ships** are not covered, except Borg, Breen and the Jem'Hadar fighter.")
    w("")
    w("## Mod and library sources consulted")
    w("")
    w("| Source | Where | Author/team | Stated terms | Use here |")
    w("|---|---|---|---|---|")
    w("| Star Trek: Armada 3 (Sins of a Solar Empire: Rebellion mod) | [ModDB](https://www.moddb.com/mods/star-trek-armada-3/downloads) | Armada 3 team + credited modellers | Readme: Aad Moerman models need special permission (not granted), and STAR remaster models may not be reused in other mods | restricted, reference only |")
    w("| Bridge Commander Remastered 1.2 | [GameFront](https://www.gamefront.com/games/bridge-commander/file/bridge-commander-remastered), [Nexus](https://www.nexusmods.com/startrekbridgecommmander/mods/4012) | BCR team + ship authors | Original author permission required to reuse | restricted. NIF plus hardpoint .py files are the best reference for weapon socket positions |")
    w("| Bridge Commander Kobayashi Maru | [ModDB](https://www.moddb.com/mods/bridge-commander-kobayashi-maru/downloads), [bckobayashimaru.de](http://bckobayashimaru.de/ws/index.php?id=downloads) | KM team | Per-ship readmes | not listed per class; use for hardpoint reference |")
    w("| BOTF mods: Ultimate Universe, Galaxies MOD (v0.4.0), Vanilla Upgrade/AIO | [Armada Fleet Command BOTF forum](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=1536) | mod teams | Forum downloads; no reuse grant | 2D ship art and sounds only, private reference; not in manifest |")
    w("| TrekCore audio archive | [trekcore.com/audio](https://www.trekcore.com/audio/) | show audio | No terms stated | sfx candidates (unclear) |")
    w("| Star Trek Sound Effects LP (1988) | [archive.org](https://archive.org/details/star-trek-tos-sound-effects) | Paramount | none | bulk TOS library (unclear) |")
    w("| Trekmeshes | [trekmeshes.eu](https://trekmeshes.eu/download/all) | per uploader | no reuse grant on pages; site closing | candidates (unclear) |")
    w("| BlendSwap | [blendswap.com](https://blendswap.com) | per uploader | uploader-declared CC | candidates (unclear) |")
    w("| Sketchfab | [sketchfab.com](https://sketchfab.com/tags/star-trek) | per uploader | uploader-declared CC, verified through the public API | main HD/game-ready candidate pool |")
    w("")
    w("## AI image models and reference inputs (race portraits, 16 Nov 2026)")
    w("")
    w("Rounds 1-2: local generation only, on the owner's RTX 4090 through ComfyUI (a locally installed backend). No online service, no account, no payment. One character per race, described by costume and makeup, never by an actor's name or face. No show screenshots were downloaded; the only image inputs are the game's own original stills (below). See PORTRAIT-LOOKTEST.md.")
    w("")
    w("| Model or input | Where | File | Licence | Use here |")
    w("|---|---|---|---|---|")
    w("| Qwen-Image-2512 (Alibaba Qwen team) | [Qwen/Qwen-Image-2512](https://huggingface.co/Qwen/Qwen-Image-2512), ComfyUI repack [Comfy-Org/Qwen-Image_ComfyUI](https://huggingface.co/Comfy-Org/Qwen-Image_ComfyUI) | `split_files/diffusion_models/qwen_image_2512_fp8_e4m3fn.safetensors` (20.4 GB) | Apache-2.0 (model card) | text-to-image: the eight major-power portraits and the named minor-race characters |")
    w("| Qwen-Image-Edit-2511 (Alibaba Qwen team) | [Qwen/Qwen-Image-Edit-2511](https://huggingface.co/Qwen/Qwen-Image-Edit-2511) | `qwen_image_edit_2511_fp8mixed.safetensors` | Apache-2.0 (model card) | image edit with a reference: restyles the game's own minor-race stills into the shared treatment |")
    w("| Qwen-Image-Edit-2511-Lightning 4-step LoRA (lightx2v) | [lightx2v/Qwen-Image-Edit-2511-Lightning](https://huggingface.co/lightx2v/Qwen-Image-Edit-2511-Lightning) | `Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors` | Apache-2.0 (model card) | speeds the edit model to 4 steps |")
    w("| Qwen2.5-VL 7B text encoder, fp8 scaled | [Comfy-Org/Qwen-Image_ComfyUI](https://huggingface.co/Comfy-Org/Qwen-Image_ComfyUI) | `qwen_2.5_vl_7b_fp8_scaled.safetensors` | Apache-2.0 (Qwen2.5-VL 7B) | prompt encoder |")
    w("| Qwen-Image VAE | [Comfy-Org/Qwen-Image_ComfyUI](https://huggingface.co/Comfy-Org/Qwen-Image_ComfyUI) | `qwen_image_vae.safetensors` | Apache-2.0 (model card) | encoder and decoder |")
    w("| Original BOTF race stills | `stbof.res` from the archive.org copy, extracted by `tools/art/extract_portraits.py` to `LocalAssets/portraits/` (160x170) | `portrait.minor.*.png` | the user's own game copy, private (clear-personal) | reference image for the 25 typical-member minor portraits; the results are derivatives and stay in `LocalAssets/portraits-remaster/`, never committed |")
    w("")
    w("**Round 3 (16 Nov 2026): the eight major-power portraits (Picard, Gowron, Tomalak, Gul Dukat, Grand Nagus Zek, Weyoun, the Borg Queen, a Breen soldier) are named-character stylised photos. Source: OpenAI image generation via Codex CLI from Memory Alpha reference stills, 16 Nov 2026.** The reference stills are private reference only and are not shipped; the generated results are the shipped files (originals in `Documents/ChatGPT/portrait-codex/`, shared grade and crop by `tools/art/portrait_post.py`, installed gitignored and packed into art_desktop.pck / art_web.pck by `tools/build_ship_packs.py --portraits`). Not Qwen. Reference stills are kept private in `portrait-v3/ref/`.")
    w("")
    w("**Round 3b (17 Nov 2026): the 30 minor-race portraits (`portrait.minor.<race>`, acamarians to zakdorn) are the same source and route: OpenAI image generation via Codex CLI from Memory Alpha reference stills, 17 Nov 2026.** The reference stills are private reference only and are not shipped; the generated results are the shipped files (originals in `Documents/ChatGPT/portrait-codex/minors/`, same `portrait_post.py --square` grade and crop as the majors, installed gitignored and packed into art_desktop.pck / art_web.pck). Not Qwen: the Qwen rows above describe rounds 1 and 2 only, which no installed portrait now uses.")
    w("")
    w("A model licence covers the model's use. Star Trek species, characters, uniforms and insignia remain Paramount's; the portraits are private, never distributed, like the rest of this game (the hosted browser build is the owner's recorded exception).")
    w("")
    for ln in generated_section():
        w(ln)
    w("## All candidates")
    w("")
    for k, v in A.items():
        w("### `%s` %s" % (k, cell(v["displayName"])))
        w("")
        w("Kind %s%s. Fallback: %s." % (v["kind"], (", faction " + v["faction"]) if v.get("faction") else "", "`%s`" % v["fallback"] if v["fallback"] else "none"))
        w("")
        if not v["candidates"]:
            w("_No candidate found._")
            w("")
            continue
        w("| # | Source | Author | Stated terms | Permission | Format | Tris |")
        w("|---:|---|---|---|---|---|---:|")
        for i, c in enumerate(v["candidates"], 1):
            w("| %d | %s | %s | %s | %s | %s | %s |" % (i, link(c), cell(c["author"]), cell(c["licence"]), c["permission"], c["format"],
                                                       "{:,}".format(c["faceCount"]) if c.get("faceCount") else ""))
        w("")
    return "\n".join(out) + "\n"


def generated_section():
    """Art generated for the game (generated-art.json): committed, so listed apart from the local-only candidates."""
    G = json.load(open(os.path.join(HERE, "generated-art.json"), encoding="utf-8"))
    out = ["## Generated art (in the repo)", "", G["note"], "", "| Set | Where | Source | Date | Terms | Scaling |", "|---|---|---|---|---|---|"]
    for g in G["sets"]:
        n = len([f for f in os.listdir(os.path.join(os.path.dirname(ROOT), g["path"])) if f.endswith(".png")]) if os.path.isdir(os.path.join(os.path.dirname(ROOT), g["path"])) else 0
        out.append("| %s (%d files) | `%s` | %s | %s | %s | %s |" % (cell(g["name"]), n, g["path"], cell(g["source"]), g["date"], cell(g["terms"]), cell(g["scaled"])))
    out += ["", "Prompt pattern: " + "; ".join("%s: %s" % (g["name"], cell(g["prompt"])) for g in G["sets"]), ""]
    return out


def credits_block():
    out = [BEGIN, "", "## Local-only asset candidates (generated from `../assets/manifest.json`)", "",
           "The assets below are **not in this repository**. They are candidates the user may download for private, local use. "
           "`Import-Assets.ps1` writes what is actually installed on a PC, with the author and stated terms, to `LocalAssets/ASSET-CREDITS.local.md`. "
           "Full detail: `../docs/ASSET-SOURCES.md`.", "",
           "| Key | First non-restricted candidate | Author | Stated terms |", "|---|---|---|---|"]
    for k, v in A.items():
        c = best(v)
        if not c or c["sourceSite"] == "in-repo":
            continue
        out.append("| %s | %s | %s | %s |" % (k, link(c), cell(c["author"]), cell(c["licence"])))
    out += ["", END]
    return "\n".join(out)


def main():
    with open(os.path.join(ROOT, "docs", "ASSET-SOURCES.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(sources_doc())
    cpath = os.path.join(ROOT, "game", "ASSET-CREDITS.md")
    text = open(cpath, encoding="utf-8").read()
    block = credits_block()
    if BEGIN in text and END in text:
        text = text[:text.index(BEGIN)] + block + text[text.index(END) + len(END):]
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    with open(cpath, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote docs/ASSET-SOURCES.md and ASSET-CREDITS.md block")


if __name__ == "__main__":
    main()
