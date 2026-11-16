"""Portrait look test: queue Qwen-Image (ComfyUI API) prompts for the Federation, Klingon and Cardassian portraits in two styles.

    python tools/art/portrait_looktest.py --out <dir> [--races fed,kli,car] [--styles photo,paint] [--seeds 11,12,13]

ComfyUI must already be serving (default http://127.0.0.1:8189, see docs/PORTRAIT-LOOKTEST.md). Standard library only.
No real-actor likeness is requested anywhere: every prompt describes an invented character.
Output: <out>/<race>-<style>-s<seed>.png at 832x896 (the originals are 160x170, so the slot aspect is unchanged).
"""
import argparse, json, os, time, urllib.request

BASE = os.environ.get("COMFY_URL", "http://127.0.0.1:8189")

RACES = {
    "fed": ("a human Starfleet command officer, a woman in her forties with short swept-back silver-blonde hair and calm direct eyes, "
            "wearing a 24th-century Starfleet duty uniform: black bodysuit with red command-division shoulder panels and a zipper front, "
            "gold-and-silver collar pips and a gold delta combadge"),
    "kli": ("a Klingon warrior-captain in his fifties, deep brown skin, a heavy sculpted bony forehead ridge pattern, long dark hair with grey "
            "streaks, a full beard and a fierce intent stare, wearing layered Klingon battle armour: dark leather and tarnished steel shoulder plates, "
            "a diagonal baldric strap and a silver clasp"),
    "car": ("a Cardassian military officer (a Gul), grey-brown scaly skin, a spoon-shaped ridge in the centre of the forehead, two thick "
            "cord-like ridges running down both sides of the neck, swept-back black hair, cold hooded eyes, wearing the Cardassian armoured uniform: "
            "broad structured shoulders in grey and black, a high grey-green collar"),
}
STYLES = {
    "photo": ("Cinematic photographic portrait, head and shoulders, 85mm lens, shallow depth of field, dramatic rim lighting from the left "
              "in cool teal and a warm key light on the face, deep blue-black starship bridge background with soft out-of-focus lights, "
              "film grain, highly detailed skin and fabric, realistic prosthetic makeup, science-fiction television still. "),
    "paint": ("Painted key-art illustration, head and shoulders, bold confident brushwork, rich saturated colour, strong rim light and a glowing "
              "backlit nebula behind, concept art for a science-fiction strategy game, poster composition, visible painterly texture, "
              "semi-stylised proportions. "),
}
NEG = ("text, letters, watermark, logo, signature, caption, extra fingers, deformed hands, blurry, low quality, cartoon, 3d render, "
       "celebrity, extra eyes, asymmetrical face, cropped head")

def graph(prompt, seed, w, h, steps, cfg):
    return {
        "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "qwen_image_2512_fp8_e4m3fn.safetensors", "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen_2.5_vl_7b_fp8_scaled.safetensors", "type": "qwen_image", "device": "default"}},
        "3": {"class_type": "VAELoader", "inputs": {"vae_name": "qwen_image_vae.safetensors"}},
        "4": {"class_type": "ModelSamplingAuraFlow", "inputs": {"model": ["1", 0], "shift": 3.1}},
        "5": {"class_type": "CLIPTextEncode", "inputs": {"text": prompt, "clip": ["2", 0]}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {"text": NEG, "clip": ["2", 0]}},
        "7": {"class_type": "EmptySD3LatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
        "8": {"class_type": "KSampler", "inputs": {"model": ["4", 0], "positive": ["5", 0], "negative": ["6", 0], "latent_image": ["7", 0],
              "seed": seed, "steps": steps, "cfg": cfg, "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
        "9": {"class_type": "VAEDecode", "inputs": {"samples": ["8", 0], "vae": ["3", 0]}},
        "10": {"class_type": "SaveImage", "inputs": {"images": ["9", 0], "filename_prefix": "botf_portrait"}},
    }

def call(path, data=None, raw=False):
    req = urllib.request.Request(BASE + path, data=json.dumps(data).encode() if data else None, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        b = r.read()
    return b if raw else json.loads(b)

def run(prompt, seed, out, w, h, steps, cfg):
    pid = call("/prompt", {"prompt": graph(prompt, seed, w, h, steps, cfg)})["prompt_id"]
    while True:
        hist = call("/history/" + pid)
        if pid in hist:
            h_ = hist[pid]
            if h_.get("status", {}).get("status_str") == "error":
                raise SystemExit(json.dumps(h_["status"])[:800])
            img = h_["outputs"]["10"]["images"][0]
            q = "filename=%s&subfolder=%s&type=%s" % (img["filename"], img["subfolder"], img["type"])
            open(out, "wb").write(call("/view?" + q, raw=True))
            return
        time.sleep(2)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True); ap.add_argument("--races", default="fed,kli,car"); ap.add_argument("--styles", default="photo,paint")
    ap.add_argument("--seeds", default="11,12,13"); ap.add_argument("--w", type=int, default=832); ap.add_argument("--h", type=int, default=896)
    ap.add_argument("--steps", type=int, default=28); ap.add_argument("--cfg", type=float, default=4.0)
    ap.add_argument("--extra", default="", help="appended to every prompt (a fix-up for the next round)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for race in a.races.split(","):
        for style in a.styles.split(","):
            for s in [int(x) for x in a.seeds.split(",")]:
                f = os.path.join(a.out, "%s-%s-s%d.png" % (race, style, s))
                if os.path.exists(f):
                    continue
                t = time.time()
                run(STYLES[style] + "Subject: " + RACES[race] + ". " + a.extra, s, f, a.w, a.h, a.steps, a.cfg)
                print(f, "%.0fs" % (time.time() - t), flush=True)
