#!/usr/bin/env python3
"""Loader branch test (r3): real Chrome via Playwright, navigator.gpu stubbed per case, ?dry=1; prints each decision."""
import sys
from playwright.sync_api import sync_playwright
port = sys.argv[1]
GOOD = "{maxStorageBuffersPerShaderStage:10,maxBindGroups:4,maxTextureDimension2D:16384,maxStorageBufferBindingSize:2147483648}"
CASES = {
 "fallback-flag": "{isFallbackAdapter:true,info:{vendor:'google',architecture:'',device:'',description:''},limits:%s}" % GOOD,
 "swiftshader-name": "{info:{vendor:'google',architecture:'swiftshader',device:'',description:'SwiftShader Device (Subzero)'},limits:%s}" % GOOD,
 "microsoft-basic": "{info:{vendor:'microsoft',architecture:'',device:'',description:'Microsoft Basic Render Driver'},limits:%s}" % GOOD,
 "limited-storage": "{info:{vendor:'intel',architecture:'gen-9',device:'',description:''},limits:{maxStorageBuffersPerShaderStage:4,maxBindGroups:4,maxTextureDimension2D:8192,maxStorageBufferBindingSize:134217728}}",
 "small-binding": "{info:{vendor:'qualcomm',architecture:'',device:'',description:''},limits:{maxStorageBuffersPerShaderStage:8,maxBindGroups:4,maxTextureDimension2D:8192,maxStorageBufferBindingSize:67108864}}",
 "device-refused": "{info:{vendor:'nvidia',architecture:'',device:'',description:''},limits:%s,requestDevice:function(){return Promise.reject(new Error('x'))}}" % GOOD,
 "adapter-hangs": None,
}
with sync_playwright() as pw:
    b = pw.chromium.launch(channel="chrome", args=["--mute-audio"])
    for name, ad in CASES.items():
        ctx = b.new_context(); pg = ctx.new_page()
        body = "new Promise(function(){})" if ad is None else "Promise.resolve(Object.assign({requestDevice:function(){return Promise.resolve({destroy:function(){}})}}, %s))" % ad
        pg.add_init_script("Object.defineProperty(navigator,'gpu',{value:{requestAdapter:function(){return %s;}}});" % body)
        pg.goto("http://127.0.0.1:%s/index.html?dry=1&run=stub-%s" % (port, name))
        pg.wait_for_function("window.__botfChoice", timeout=8000)
        print(name, "->", pg.evaluate("window.__botfChoice.build + ' (' + window.__botfChoice.why + ')'"))
        ctx.close()
    b.close()
