extends SceneTree
## MOD-9 build step: turns a PNG frame sequence and a WAV into an Ogg Theora movie with Godot's own Movie Maker writer
## (this machine's ffmpeg 8.1 libtheora writes streams that no decoder reads cleanly; Godot's writer and player agree).
##   godot --path remaster/tools/mod9/ogv_project --write-movie <out.ogv> --fixed-fps 24 --resolution 1024x384 --position -10000,-10000 \
##       -s res://ogv_from_frames.gd -- <frames_dir with f00001.png ...> [<audio.wav>]
## One frame per engine frame; quits after the last. Build tool only (tools/ is not exported).

var frames: PackedStringArray = []
var rect: TextureRect
var player: AudioStreamPlayer
var n := 0


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var dir := str(args[0])
	for f in DirAccess.get_files_at(dir):
		if f.ends_with(".png"):
			frames.append(dir.path_join(f))
	frames.sort()
	rect = TextureRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	rect.stretch_mode = TextureRect.STRETCH_SCALE
	get_root().add_child(rect)
	if args.size() > 1:
		player = AudioStreamPlayer.new()
		player.stream = AudioStreamWAV.load_from_file(str(args[1]))
		get_root().add_child(player)
		player.play()
	print("OGV frames ", frames.size())


func _process(_delta: float) -> bool:
	if n >= frames.size():
		print("OGV wrote ", n, " frames")
		return true
	rect.texture = ImageTexture.create_from_image(Image.load_from_file(frames[n]))
	n += 1
	return false
