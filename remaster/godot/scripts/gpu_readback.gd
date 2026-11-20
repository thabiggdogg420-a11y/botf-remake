class_name GpuReadback
extends RefCounted
## GPU-to-CPU texture read-backs, guarded for the WebGPU browser build. The WebGPU fork (Godot 4.8-dev) aborts the
## whole page ("RuntimeError: null function" in the main loop) on a viewport's get_texture().get_image(), so on that
## renderer every read-back of a render target is skipped and the caller gets null. Desktop and the WebGL
## (Compatibility) build read back as before.

## True when this renderer can read a render target back to the CPU without crashing.
static func allowed() -> bool:
	return not webgpu()

## True on the WebGPU browser build: a web export not running the Compatibility (WebGL2) renderer.
static func webgpu() -> bool:
	return OS.has_feature("web") and RenderingServer.get_current_rendering_method() != "gl_compatibility"

## `tex`'s pixels, or null where read-back is not allowed (WebGPU) or the texture has none.
static func image(tex: Texture2D) -> Image:
	if tex == null or not allowed():
		return null
	return tex.get_image()
