extends SceneTree
## WebGPU r6: a hero glb parsed through BattleAssets.strip_glb_images (placeholders, then each image decoded from the
## original bytes) must give the same materials and texture sizes as a plain parse.
##   godot --headless -s res://scenes/battle/tests/test_glb_strip.gd -- <file.glb> [...]

func _init() -> void:
	var failures := 0
	var args := OS.get_cmdline_user_args()
	for path: String in args:
		var t0 := Time.get_ticks_msec()
		var d1 := GLTFDocument.new(); var s1 := GLTFState.new()
		if d1.append_from_file(path, s1) != OK:
			print("FAIL plain parse ", path); failures += 1; continue
		var t1 := Time.get_ticks_msec()
		var st := BattleAssets.strip_glb_images(path)
		var d2 := GLTFDocument.new(); var s2 := GLTFState.new()
		if st.is_empty() or d2.append_from_buffer(st.glb, path.get_base_dir(), s2) != OK:
			print("FAIL stripped parse ", path); failures += 1; continue
		var t2 := Time.get_ticks_msec()
		var n1 := d1.generate_scene(s1); var n2 := d2.generate_scene(s2)
		var swap := {}
		for job in BattleAssets._jobs_for_stripped(s2, st.jobs):
			var tex := BattleAssets._mip_one(job)
			if tex != null:
				swap[job.id] = tex
		BattleAssets._apply_mips(n2, swap)
		var a := _sig(n1); var b := _sig(n2)
		var ok := a.size() == b.size() and a.size() > 0
		for i in mini(a.size(), b.size()):
			if a[i] != b[i]:
				ok = false; print("  differs: ", a[i], " vs ", b[i])
		print("%s %s: plain parse %d ms, stripped parse %d ms, %d images, %d material slots" % ["ok" if ok else "FAIL", path.get_file(), t1 - t0, t2 - t1, st.jobs.size(), a.size()])
		if not ok: failures += 1
		n1.free(); n2.free()
	print("glb strip tests: %d failures" % failures)
	quit(1 if failures else 0)

func _sig(n: Node) -> Array:
	var out := []
	for mi: MeshInstance3D in n.find_children("*", "MeshInstance3D", true, false):
		for s in mi.mesh.get_surface_count():
			var m := mi.mesh.surface_get_material(s) as BaseMaterial3D
			if m == null: out.append("none"); continue
			var e := [m.resource_name, m.transparency]
			for p in ["albedo_texture", "normal_texture", "roughness_texture", "metallic_texture", "ao_texture", "emission_texture"]:
				var t: Texture2D = m.get(p)
				e.append(p + "=" + ("-" if t == null else "%dx%d" % [t.get_width(), t.get_height()]))
			out.append(str(e))
	return out
