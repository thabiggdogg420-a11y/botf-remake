"""Prompts for the BOTF race portraits: one character per race, one treatment for all (docs/PORTRAIT-LOOKTEST.md).

No real-actor likeness is requested and no actor is named: each character is described by costume, makeup and bearing,
which is what makes them recognisable. Edits take the game's own original still (private, LocalAssets) as the reference image.
Each job: key -> (mode t2i|edit, character, description for t2i).
"""
T2I_STYLE = ("Cinematic photographic portrait, head and shoulders, the face centred in the upper middle of a wide 4:3 frame with space on both "
             "sides, 85mm lens, shallow depth of field, dramatic rim lighting from the left in cool teal and a warm key light on the face, "
             "dark navy-blue starship bridge background with soft out-of-focus lights, subtle film grain, highly detailed skin and fabric, "
             "convincing prosthetic makeup, science-fiction television still. Subject: ")
EDIT_STYLE = ("Restyle this portrait as a cinematic photographic still of the same character: keep the same face, species features, makeup, hair and "
              "costume exactly, and extend the picture to a wide 4:3 frame with the head and shoulders large in the frame, the face filling about half the frame height and centred. Dramatic rim lighting from the left in cool "
              "teal and a warm key light on the face, a dark navy-blue starship bridge background with soft out-of-focus lights, shallow depth of "
              "field, subtle film grain, sharp detail. Keep every skin colour, hair colour and costume colour exactly as in the original; the teal light is only a faint rim on the edges. ")
NEG = ("text, letters, watermark, logo, caption, extra fingers, deformed hands, blurry, low quality, cartoon, 3d render, extra eyes, "
       "asymmetrical face, cropped head")

JOBS = {
 "portrait.fed": ("t2i", "Picard, Federation captain", "a Federation starship captain: a bald man in his sixties with a lined, intelligent face and a calm commanding gaze, wearing a 24th-century Starfleet duty uniform, black with red command-division shoulder panels, four gold collar pips and a gold delta combadge"),
 "portrait.kli": ("t2i", "Gowron, Klingon chancellor", "a Klingon High Council chancellor in his fifties, deep brown skin, a prominent central bony ridge running up the forehead into the hairline with deep symmetrical bony ridges on either side of it, long dark wavy hair streaked with grey, a thick moustache and goatee, a cunning intense stare, wearing ornate black leather and bronze and silver shoulder armour with a diagonal baldric"),
 "portrait.rom": ("t2i", "Sela, Romulan commander", "a Romulan commander, a woman in her forties, angular face, a pronounced forward-swept V-shaped brow ridge, severe dark hair swept back, pointed ears, a cold calculating expression, wearing a Romulan military uniform, dark grey-green with metallic armoured shoulder pieces and a high collar, no badges or insignia on the chest"),
 "portrait.car": ("t2i", "Gul Dukat, Cardassian", "a Cardassian Gul in his fifties, silver-streaked swept-back black hair, a smooth confident half-smile, smooth pale grey-brown skin with only a faint scale texture, a raised ridge of skin in the centre of the forehead the same colour as the face, an upside-down teardrop shape broad at the hairline and narrowing between the brows, two thick tendon-like ridges down the sides of the neck, wearing a Cardassian armoured uniform in grey and black with broad shoulders and a high collar"),
 "portrait.fer": ("t2i", "Grand Nagus Zek, Ferengi", "the elderly Grand Nagus of the Ferengi Alliance: orange-brown wrinkled skin, a bulbous ridged forehead, very large lobed ears, sharp pointed teeth in a sly grin, small shrewd eyes, wearing an ornate gold and brown robe with a tall embroidered collar and a gold-trimmed cap"),
 "portrait.dom": ("t2i", "Weyoun, Vorta", "a Vorta diplomat of the Dominion: pale skin, short swept-back light brown hair, small pointed ears, an ingratiating bland smile, wearing a high-collared dark olive-brown tunic with raised ribbed shoulder detail, no badges or insignia on the chest"),
 "portrait.borg": ("t2i", "the Borg Queen", "the Borg Queen: pale grey-white skin, a smooth domed head fitted with dark metallic cybernetic implants and exposed tubing, one eye with a red-lit ocular implant, a calm composed expression, a black Borg bodysuit with cables and tubes rising up the neck from the collar"),
 "portrait.breen": ("t2i", "a Breen soldier", "a Breen soldier in a full helmet and refrigeration suit: a pale metallic silver-bronze ribbed helmet with a narrow horizontal slit and a rectangular speaking grille, a dark insulated suit with a heavy collar, no face visible"),
 "portrait.minor.bajorans": ("t2i", "Kira Nerys, Bajoran", "a Bajoran woman in her thirties, short auburn hair, distinctive ridges across the bridge of the nose, a gold Bajoran earring on the right ear, a determined fierce expression, wearing a dark Bajoran militia uniform"),
 "portrait.minor.vulcans": ("t2i", "Spock, Vulcan", "a Vulcan man, sharply upswept eyebrows, pointed ears, straight dark bowl-cut hair, pale olive skin, a calm unreadable expression, wearing a dark blue-grey science officer tunic"),
 "portrait.minor.andorians": ("t2i", "Shran, Andorian", "an Andorian man, blue skin, white hair swept back, two white antennae curving up from the top of the head, a sharp challenging look, wearing a dark military jacket"),
 "portrait.minor.betazoids": ("t2i", "Deanna Troi, Betazoid", "a Betazoid woman in her thirties, long dark curly hair, large dark eyes, a warm empathic expression, wearing a 24th-century Starfleet duty uniform with teal-blue counsellor-division shoulder panels"),
 "portrait.minor.trill": ("t2i", "Jadzia Dax, Trill", "a Trill woman in her late twenties, long brown hair pulled back, a column of dark spots running from the temple down the side of the face and neck, a warm wry smile, wearing a 24th-century Starfleet duty uniform with teal science-division shoulder panels"),
}
# Everything else among the 30 minors: the typical member, restyled from the game's own still (the key's original).
EDIT_KEYS = ["acamarians", "angosians", "antedeans", "anticans", "bandi", "benzites", "bolians", "bynars", "caldonians", "chalnoth", "edo",
             "ktarians", "malcorians", "mintakans", "mizarians", "nausicaans", "pakleds", "selay", "sheliak", "takarans", "talarians",
             "tamarians", "ullians", "yridians", "zakdorn"]
for _k in EDIT_KEYS:
    JOBS["portrait.minor." + _k] = ("edit", "typical " + _k[:-1] if _k.endswith("s") else "typical " + _k, "")

# The four majors that have an original BOTF still are restyled FROM that still (reference image: the face, ridges and hair stay), then
# dressed for the leader. The Federation, Dominion, Borg and Breen have no original still, so they stay text-to-image.
NO_BADGE = ("Starfleet delta badge, combadge, comm badge, delta insignia, any chest badge or insignia")
MAJOR_EDITS = {
 "portrait.kli": ("edit", "Klingon chancellor, ", "Keep this exact Klingon's face, forehead ridges and long hair. Dress him in ornate dark leather and bronze and silver chancellor's shoulder armour with a diagonal baldric. No badges or insignia."),
 "portrait.rom": ("edit", "Romulan commander, ", "Keep this exact Romulan's face, brow and ears. Dress him in a dark grey-green Romulan military uniform with metallic armoured shoulder pieces and a high collar. No badges or insignia of any kind on the chest."),
 "portrait.car": ("edit", "Cardassian Gul, ", "Keep this exact Cardassian's face, forehead ridge and neck ridges. Dress him in the grey and black Cardassian armoured uniform with broad shoulders. No badges or insignia of any kind on the chest."),
 "portrait.fer": ("edit", "Ferengi Grand Nagus, ", "Keep this exact Ferengi's face, ears and brow. Dress him in an ornate gold and brown robe with a tall embroidered collar and a gold-trimmed cap. No badges."),
}
JOBS.update(MAJOR_EDITS)
JOBS["portrait.dom"] = ("t2i", "Weyoun, Vorta", "a Vorta diplomat of the Dominion: a pale ghostly complexion, prominent pointed elf-like ears, short swept-back light brown hair, a smooth ingratiating smile, wearing a high-collared dark olive-brown tunic with raised ribbed shoulder detail, no badges or insignia of any kind on the chest")
