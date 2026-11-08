class_name ViewModel
extends RefCounted
## Base of the view-model layer. A view model reads the core through an AppSession and exposes plain data
## (Dictionaries, Arrays, numbers, strings) and commands. It never creates nodes, so it is unit-tested headless
## (scripts/viewmodels/tests). Presentation scripts own one and rebuild from it. See docs/CLIENT-ARCHITECTURE.md.

## Emitted when the model's own state (tabs, drafts, filters) changes and the presentation should redraw.
signal changed

var session: AppSession

func _init(s: AppSession = null) -> void:
	session = s

func view() -> Dictionary:
	return session.view

func meta() -> Dictionary:
	return session.meta

func player() -> int:
	return session.player()

## "" when orders may be drafted, else the reason every order control is disabled.
func lock_reason() -> String:
	return session.lock_reason()

## A core query; the value or null (the error is in session.core.last_error).
func query(method: String, args: Dictionary = {}) -> Variant:
	return session.core.request(method, args)

## A core query that must return a Dictionary; {} on failure.
func query_dict(method: String, args: Dictionary = {}) -> Dictionary:
	var r: Variant = session.core.request(method, args)
	return r if r is Dictionary else {}

func issue(command: Dictionary, cancel_ids: Array = [], success: String = "") -> bool:
	return session.issue(command, cancel_ids, success)

func cancel(ids: Array, message: String = "") -> void:
	session.cancel_orders(ids, message)

func dry_run(command: Dictionary, cancel_ids: Array = []) -> String:
	return session.dry_run(command, cancel_ids)

## The system name at a sector, else `fallback` formatted with x and y.
func place(x: int, y: int, fallback: String = "%d.%d") -> String:
	var s := session.system_at(x, y)
	return str(s["name"]) if not s.is_empty() else fallback % [x, y]

## The first reason that is not "" (lock first, then each candidate in order).
static func first_reason(reasons: Array) -> String:
	for r in reasons:
		if str(r) != "":
			return str(r)
	return ""
