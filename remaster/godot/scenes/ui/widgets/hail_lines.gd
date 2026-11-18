class_name HailLines
extends RefCounted
## One short greeting per power for the hail scene, by standing (written in-house; presentation only, no rule reads it).

const LINES := {
	1: {"war": "We will defend the Federation, %s. But this channel stays open.", "peace": "%s, the Federation welcomes your call.", "ally": "Good to see you, %s. What do you need?"},
	2: {"war": "You hail us mid-battle, %s? Speak, then die.", "peace": "%s. The Empire is listening. Be brief.", "ally": "Our blades are yours, %s. Speak."},
	3: {"war": "The Star Empire has nothing to say to %s.", "peace": "%s. The Senate grants you a moment.", "ally": "Our interests align, %s. For now."},
	4: {"war": "Cardassia will remember this, %s.", "peace": "%s. The Central Command is prepared to listen.", "ally": "The Union values its friends, %s."},
	5: {"war": "War is bad for business, %s. Make me an offer.", "peace": "%s! Let's talk profit.", "ally": "Partner! %s, what can I sell you?"},
	6: {"war": "The Founders are disappointed in %s.", "peace": "%s. The Dominion extends its greetings.", "ally": "The Founders are pleased with %s."},
	7: {"war": "We are the Borg. %s will be assimilated.", "peace": "We are the Borg. %s is noted.", "ally": "We are the Borg. %s serves the Collective."},
	8: {"war": "[static] %s. The Confederacy does not negotiate.", "peace": "[static] %s. State your terms.", "ally": "[static] %s. Agreed."},
}
const TERRAN := {"war": "Long live the Empire, %s. Kneel, or burn.", "peace": "%s. The Empire permits you to speak.", "ally": "The Empire rewards useful allies, %s."}
const MINOR := {"war": "Leave our space, %s.", "peace": "Greetings, %s. We are listening.", "ally": "Welcome, friends of %s."}

static func greeting(power: int, standing: String, us: String) -> String:
	var set_: Dictionary = TERRAN if power == 1 and HailBust.mirror_active() else LINES.get(power, MINOR)
	var k := "war" if standing == "At war" else ("ally" if standing in ["Allied", "Member world"] else "peace")
	return str(set_[k]) % us
