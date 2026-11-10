class_name ShipSilhouette
extends Control
## The big hull picture on the fleets and database screens: a ShipPicture on a faint plate. The baked 3D icon when
## LocalAssets has one for the class, else the in-house vector hull of the class's family (never a random shape).
var seed_id := 0
var color := Color.WHITE
var owner_id := 0
var klass := ""
var view := "top"
var _pic: ShipPicture

func _ready() -> void:
	_pic = ShipPicture.new()
	_pic.plate = true
	_pic.set_anchors_preset(Control.PRESET_FULL_RECT)
	_pic.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_pic.owner_id = owner_id
	_pic.klass = klass
	_pic.view = view
	_pic.color = color
	add_child(_pic)
