"""Generate the race portraits in one treatment (tools/art/portrait_jobs.py) through a running ComfyUI (default :8189).

    python tools/art/portrait_gen.py --out <dir> --src <original portraits dir> [--keys k1,k2] [--seeds 31,32] [--group majors|minors|all]
T2I: Qwen-Image-2512. Edit: Qwen-Image-Edit-2511 with the 4-step Lightning LoRA, reference = the original still upscaled 4x.
Writes <out>/<key>-s<seed>.png at 1024x768. tools/art/portrait_post.py applies the shared grade.
"""
import argparse, json, os, sys, time, urllib.error, urllib.request
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import portrait_jobs as J
BASE = os.environ.get("COMFY_URL", "http://127.0.0.1:8189")

def call(path, data=None, raw=False):
    req = urllib.request.Request(BASE + path, data=json.dumps(data).encode() if data else None, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=300) as r:
            b = r.read()
    except urllib.error.HTTPError as e:
        raise SystemExit("HTTP %d %s: %s" % (e.code, path, e.read().decode("utf-8", "replace")[:1500]))
    return b if raw else json.loads(b)

def upload(path, name):
    """POST the reference image to /upload/image (multipart, stdlib); returns the name ComfyUI stored it under."""
    b = "----botfportrait"
    CRLF = chr(13) + chr(10)
    head = "--" + b + CRLF + 'Content-Disposition: form-data; name="image"; filename="' + name + '"' + CRLF + "Content-Type: image/png" + CRLF + CRLF
    tail = CRLF + "--" + b + CRLF + 'Content-Disposition: form-data; name="overwrite"' + CRLF + CRLF + "true" + CRLF + "--" + b + "--" + CRLF
    body = head.encode() + open(path, "rb").read() + tail.encode()
    req = urllib.request.Request(BASE + "/upload/image", data=body, headers={"Content-Type": "multipart/form-data; boundary=" + b})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read())["name"]

def t2i(prompt, seed, w, h, neg=None):
    return {
     "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "qwen_image_2512_fp8_e4m3fn.safetensors", "weight_dtype": "default"}},
     "2": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen_2.5_vl_7b_fp8_scaled.safetensors", "type": "qwen_image", "device": "default"}},
     "3": {"class_type": "VAELoader", "inputs": {"vae_name": "qwen_image_vae.safetensors"}},
     "4": {"class_type": "ModelSamplingAuraFlow", "inputs": {"model": ["1", 0], "shift": 3.1}},
     "5": {"class_type": "CLIPTextEncode", "inputs": {"text": prompt, "clip": ["2", 0]}},
     "6": {"class_type": "CLIPTextEncode", "inputs": {"text": neg or J.NEG, "clip": ["2", 0]}},
     "7": {"class_type": "EmptySD3LatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
     "8": {"class_type": "KSampler", "inputs": {"model": ["4", 0], "positive": ["5", 0], "negative": ["6", 0], "latent_image": ["7", 0],
           "seed": seed, "steps": 28, "cfg": 4.0, "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
     "9": {"class_type": "VAEDecode", "inputs": {"samples": ["8", 0], "vae": ["3", 0]}},
     "10": {"class_type": "SaveImage", "inputs": {"images": ["9", 0], "filename_prefix": "botf_portrait"}}}

def edit(prompt, seed, w, h, image):
    return {
     "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "qwen_image_edit_2511_fp8mixed.safetensors", "weight_dtype": "default"}},
     "2": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen_2.5_vl_7b_fp8_scaled.safetensors", "type": "qwen_image", "device": "default"}},
     "3": {"class_type": "VAELoader", "inputs": {"vae_name": "qwen_image_vae.safetensors"}},
     "4": {"class_type": "LoraLoaderModelOnly", "inputs": {"model": ["1", 0], "lora_name": "Qwen-Image-Edit-2511-Lightning-4steps-V1.0-bf16.safetensors", "strength_model": 1.0}},
     "5": {"class_type": "ModelSamplingAuraFlow", "inputs": {"model": ["4", 0], "shift": 3.0}},
     "6": {"class_type": "LoadImage", "inputs": {"image": image}},
     "7": {"class_type": "TextEncodeQwenImageEditPlus", "inputs": {"clip": ["2", 0], "prompt": prompt, "vae": ["3", 0], "image1": ["6", 0]}},
     "8": {"class_type": "TextEncodeQwenImageEditPlus", "inputs": {"clip": ["2", 0], "prompt": "", "vae": ["3", 0], "image1": ["6", 0]}},
     "9": {"class_type": "EmptySD3LatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
     "10": {"class_type": "KSampler", "inputs": {"model": ["5", 0], "positive": ["7", 0], "negative": ["8", 0], "latent_image": ["9", 0],
            "seed": seed, "steps": 4, "cfg": 1.0, "sampler_name": "euler", "scheduler": "simple", "denoise": 1.0}},
     "11": {"class_type": "VAEDecode", "inputs": {"samples": ["10", 0], "vae": ["3", 0]}},
     "12": {"class_type": "SaveImage", "inputs": {"images": ["11", 0], "filename_prefix": "botf_portrait_edit"}}}

def run(graph, out):
    pid = call("/prompt", {"prompt": graph})["prompt_id"]
    while True:
        h = call("/history/" + pid)
        if pid in h:
            st = h[pid].get("status", {})
            if st.get("status_str") == "error":
                raise SystemExit(json.dumps(st)[:1200])
            img = list(h[pid]["outputs"].values())[0]["images"][0]
            open(out, "wb").write(call("/view?filename=%s&subfolder=%s&type=%s" % (img["filename"], img["subfolder"], img["type"]), raw=True))
            return
        time.sleep(1.5)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True); ap.add_argument("--src", required=True); ap.add_argument("--keys", default="")
    ap.add_argument("--seeds", default="31,32"); ap.add_argument("--group", default="all")
    ap.add_argument("--w", type=int, default=1024); ap.add_argument("--h", type=int, default=768)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    keys = a.keys.split(",") if a.keys else [k for k in J.JOBS if a.group == "all" or (a.group == "majors") == (".minor." not in k)]
    from PIL import Image
    for key in keys:
        mode, who, desc = J.JOBS[key]
        for s in [int(x) for x in a.seeds.split(",")]:
            f = os.path.join(a.out, "%s-s%d.png" % (key, s))
            if os.path.exists(f):
                continue
            t = time.time()
            if mode == "t2i":
                run(t2i(J.T2I_STYLE + desc + ".", s, a.w, a.h, J.NEG if key == "portrait.fed" else J.NEG + ", " + J.NO_BADGE), f)
            else:
                im = Image.open(os.path.join(a.src, key + ".png")).convert("RGB")
                im = im.resize((im.width * 4, im.height * 4), Image.LANCZOS)
                name = "botf_ref_%s.png" % key
                tmp = os.path.join(a.out, "_ref_" + key + ".png")
                im.save(tmp)
                name = upload(tmp, name)
                run(edit(J.EDIT_STYLE + "The character is a %s. %s" % (who.replace("typical ", ""), desc), s, a.w, a.h, name), f)
            print(f, "%.0fs" % (time.time() - t), flush=True)
