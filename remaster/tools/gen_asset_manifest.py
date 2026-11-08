import json, re, collections
R = r"C:/Users/BiggDogg/Documents/ChatGPT/Game Remakes/remaster"
old = json.load(open(R + '/assets/manifest.json', encoding='utf8'))['assets']
sf = open(R + '/godot/scenes/battle/ship_factory.gd', encoding='utf8').read()
blk = sf[sf.index('const CANON_LENGTHS'):sf.index('static var _cache')]
ships = re.findall(r'"(ship\.[a-z0-9_]+\.[a-z0-9_]+)": (\d+)', blk)
assert len(ships) == 159
hm = json.load(open(R + '/game/LocalAssets/ships/godot-hero-manifest.json'))['heroes']
heroes = set(hm)
assert len(heroes) == 33 and heroes <= {k for k, _ in ships}
cues = json.load(open(R + '/game/LocalAssets/audio/cues.json', encoding='utf8'))['cues']
cat = open(R + '/game/Source/BotfSim/ReferenceCatalogue.h', encoding='utf8').read()

# ---- budgets (ART-BIBLE section 12). Targets set by the bible, not measured limits.
SHIP_TIERS = [  # (max length m, name, desktop LOD0 tris, web LOD0 tris)
    (100, 'small', 20000, 12000), (400, 'medium', 40000, 20000), (1000, 'large', 60000, 30000), (10 ** 9, 'huge', 100000, 45000)]
def ship_budget(length, station, hero):
    for mx, nm, dt, wt in SHIP_TIERS:
        if length < mx:
            break
    if station:
        nm, dt, wt = 'station', 80000, 40000
    kb = {'small': 900, 'medium': 1200, 'large': 1600, 'huge': 2200, 'station': 1600}[nm]
    d = {'size_tier': nm, 'lod0_tris': dt, 'lods': 4, 'textures': 'BC/N/ORM/E 2048 per material slot, VRAM-compressed (S3TC/BPTC)'}
    w = {'lod0_tris': wt, 'lods': 2, 'textures': 'slots atlased: BC 1024 + ORM 1024 + E 512 (N dropped below 200 m), ETC2/ASTC', 'download_kb': kb,
         'renderer': 'Compatibility: hero_hull.gdshader / hero_plated.gdshader only, no screen-space effects'}
    if not hero:
        d['current'] = 'procedural (ShipFactory); budget is for the target hero model'
        w['current'] = 'procedural hull ships no download today (4000 tris, hull.gdshader passes Compatibility); budget is for the target hero model'
    return d, w

def budget(category, key='', **kw):
    if category == 'planet':
        if key.startswith('planet.layer') or key.startswith('planet.state'):
            return ({'textures': 'clouds 2048x1024 R8, lights 2048x1024 RGB'}, {'textures': 'clouds 512x256 R8, lights 512x256', 'download_kb': 120})
        if key.startswith('star.'):
            return ({'textures': 'procedural shader + 1024 glow sprite'}, {'textures': 'procedural, 256 glow sprite', 'download_kb': 60})
        return ({'textures': 'albedo 4096x2048, normal 2048x1024, 6 variants per class', 'lods': 3, 'sphere_tris': 16384},
                {'textures': 'albedo 1024x512, no normal map (bake relief into albedo), 4 variants per class', 'lods': 2, 'sphere_tris': 2048, 'download_kb': 700})
    if category == 'portrait':
        return ({'textures': '1024x1024 PNG, animated portraits 4 layer sheets', 'download_kb_max': 1500},
                {'textures': '512x512 lossy WebP (portraits), 1280x720 WebP (hail backdrops), animation as 2 layers max', 'download_kb': 150})
    if category == 'structure_icon':
        return ({'textures': '256x256 per icon in an atlas'}, {'textures': '128x128 per icon in one shared atlas of 2048x2048 or less', 'download_kb': 8})
    if category in ('vfx',):
        return ({'particles_per_burst': 2000, 'concurrent_emitters': 24, 'shader_loops': 'any'},
                {'particles_per_burst': 600, 'concurrent_emitters': 8, 'shader_loops': 'fixed count of 4 or fewer', 'renderer': 'Compatibility: no hint_depth_texture, no fog shader, no volumetric fog',
                 'download_kb': 0})
    if category == 'environment':
        if key == 'env.battle.nebula_fog':
            return ({'shader': 'shader_type fog (volumetric fog)'}, {'shader': 'NOT SUPPORTED in Compatibility; replace with 6 to 8 billboard cloud quads (blend_add, 512x512 R8)', 'download_kb': 60})
        if key == 'env.battle.space_sky':
            return ({'shader': 'procedural sky shader'}, {'shader': 'bake the sky shader to a 2048x1024 panorama at pack time (its 27-sample loops are too heavy per frame on WebGL2)', 'download_kb': 600})
        if key == 'env.battle.light_rig':
            return ({'lights': '3 directional, shadows 2 splits, glow on'}, {'lights': '1 shadowed sun + fill and rim as unshadowed directionals (max 8 lights per mesh), glow on, no volumetric fog, no SSAO', 'download_kb': 0})
        return ({'shader': 'canvas_item shader'}, {'shader': 'canvas_item shader, 5 noise octaves or fewer', 'download_kb': 0})
    if category == 'ui':
        if key.startswith('ui.font'):
            return ({'format': 'woff2'}, {'format': 'woff2 subset to Latin and Latin-1, 60 KB or less each', 'download_kb': 60})
        return ({'textures': 'SVG sources, rasterised at 2x'}, {'textures': 'SVG rasterised at 1x, atlas', 'download_kb': 100})
    if category == 'music':
        return ({'format': 'Ogg Vorbis q6, 48 kHz stereo'}, {'format': 'Ogg Vorbis q3, 44.1 kHz stereo, streamed; lazily fetched per empire pack', 'download_kb_max': 2500})
    if category == 'voice':
        return ({'format': 'Ogg Vorbis q5, 48 kHz mono, -18 LUFS (VOICE-PLAN section 3)'}, {'format': 'Ogg Vorbis q2, 32 kHz mono, -18 LUFS; per-empire pack fetched on first use', 'download_kb_max': 120})
    if category == 'sfx':
        return ({'format': 'WAV or Ogg q5, 48 kHz mono'}, {'format': 'Ogg Vorbis q3, 32 kHz mono, 12 shared cues preloaded', 'download_kb_max': 80})
    raise KeyError(category)

rows = []
def add(key, category, name, source, status, ws, ms, planned=False, **extra):
    assert status in ('procedural', 'placeholder', 'final', 'missing'), status
    assert ms in ('M1', 'M2', 'M3', 'M4')
    r = dict(key=key, category=category, name=name, source=source, status=status, ws=ws, milestone=ms)
    if planned: r['planned'] = True
    r.update(extra)
    if 'desktop_budget' not in r:
        d, w = budget(category, key)
        r['desktop_budget'] = d; r['web_budget'] = w
    rows.append(r)

FAC = {'fed': 'Federation', 'kli': 'Klingon', 'rom': 'Romulan', 'car': 'Cardassian', 'fer': 'Ferengi'}
M2F = {'rom', 'car', 'fer', 'and', 'baj', 'vul', 'nau', 'tal'}
for key, ln in ships:
    fac = key.split('.')[1]
    o = old[key]
    cand = o['candidates'][0] if o['candidates'] else None
    hero = key in heroes
    if hero:
        src = 'hero glb, Starhelm Bridge Commander library (third-party, private, gitignored): game/LocalAssets/ships/' + key
        st = 'placeholder'
    else:
        src = 'procedural: ShipFactory (godot/scenes/battle/ship_factory.gd) owner look ' + (fac if fac in FAC else 'fed fallback')
        st = 'procedural'
    ms = 'M1' if fac in ('fed', 'kli') else ('M2' if fac in M2F else 'M3')
    station = bool(re.search(r'\.(starbase|outpost|platform|spacedock|nor)$', key))
    d, w = ship_budget(int(ln), station, hero)
    ex = dict(subcategory='station' if station else 'ship', faction=fac, length_m=int(ln), has_hero_model=hero,
              roster_name=o['displayName'], fallback_key=o.get('fallback'), desktop_budget=d, web_budget=w)
    if hero: ex['hero_lod0_tris'] = hm[key]['lods'][0]['triangles']
    if cand: ex['candidate_source'] = '%s (%s)' % (cand['sourceSite'], cand['permission'])
    add(key, 'ship', o['displayName'], src, st, 10, ms, **ex)

PL = [('m', 'Class M (Terran)', 'M', 'Terran'), ('o', 'Class O (Oceanic)', 'O', 'Oceanic'), ('p', 'Class P (Glacial)', 'P', 'Arctic'),
      ('l', 'Class L (Jungle)', 'L', 'Jungle'), ('h', 'Class H (Desert)', 'H', 'Desert'), ('k', 'Class K (Barren)', 'K', 'Barren'),
      ('y', 'Class Y (Demon)', 'Y', 'Volcanic'), ('gasgiant', 'Gas Giant', 'GasGiant', 'GasGiant'), ('asteroids', 'Asteroids', 'Asteroids', None)]
for k, n, letter, ref in PL:
    if k == 'gasgiant':
        add('planet.gasgiant', 'planet', n, 'procedural: planet.gdshader banded sphere (battle backdrop only); System screen draws a 2D circle', 'procedural', 10, 'M1', sim_class=letter, ref_type=ref)
    else:
        add('planet.' + k, 'planet', n, 'placeholder: 2D coloured circle, system_screen.gd PlanetOrb', 'placeholder', 10, 'M1', sim_class=letter, ref_type=ref)
for k, n in [('clouds', 'Cloud layer (animated)'), ('atmosphere', 'Atmosphere rim scattering'), ('night_lights', 'Night-side city lights (colonised)'),
             ('rings', 'Ring system (gas giants)'), ('moons', 'Moon set')]:
    add('planet.layer.' + k, 'planet', n, 'none yet', 'missing', 3, 'M1', planned=True)
for k, n in [('terraform_stage', 'Terraforming stage looks (per class)'), ('bombarded', 'Bombarded / ravaged surface')]:
    add('planet.state.' + k, 'planet', n, 'none yet', 'missing', 3, 'M2', planned=True)
for k, n in [('sun_yellow', 'Star, yellow main sequence'), ('sun_red', 'Star, red dwarf'), ('sun_blue', 'Star, blue giant'), ('sun_white', 'Star, white')]:
    add('star.' + k, 'planet', n, 'none yet (system view star, WS3)', 'missing', 3, 'M1', planned=True)

i = cat.index('RaceCatalogue{{')
races = re.findall(r'\{(\d+), "([^"]+)", "([^"]+)"\}', cat[i:])
assert len(races) == 35
majors = {0: 'car', 1: 'fed', 2: 'fer', 3: 'kli', 4: 'rom'}
for rid, name, icon in races:
    rid = int(rid)
    if rid in majors:
        f = majors[rid]
        add('portrait.' + f, 'portrait', name + ' (major race portrait)', 'none: core RaceCatalogue id %d names original %s; the file is not extracted locally' % (rid, icon),
            'missing', 7, 'M1' if f in ('fed', 'kli') else 'M2', race_id=rid, original_icon=icon)
    else:
        slug = re.sub(r'[^a-z0-9]+', '_', name.lower()).strip('_')
        add('portrait.minor.' + slug, 'portrait', name + ' (minor race portrait)', 'none: core RaceCatalogue id %d names original %s; the file is not extracted locally' % (rid, icon),
            'missing', 7, 'M2', race_id=rid, original_icon=icon)
for f in FAC:
    add('portrait.leader.' + f, 'portrait', FAC[f] + ' leader, animated hail portrait', 'none yet', 'missing', 7, 'M1' if f in ('fed', 'kli') else 'M2', planned=True)
    add('hail.bg.' + f, 'portrait', FAC[f] + ' bridge backdrop for hail scenes', 'none yet', 'missing', 7, 'M1' if f in ('fed', 'kli') else 'M2', planned=True)
for f in ['neutral'] + list(FAC):
    add('ui.emblem.' + f, 'ui', f.capitalize() + ' emblem outline', 'procedural: lcars.gd EMBLEMS vector polygon' + (' (Ferengi list is empty, no outline yet)' if f == 'fer' else ''),
        'missing' if f == 'fer' else 'procedural', 12, 'M1' if f in ('neutral', 'fed', 'kli') else 'M2')

j = cat.index('BuildingCatalogue{{'); k = cat.index('RaceCatalogue{{')
fam = collections.OrderedDict()
for m in re.finditer(r'\{(\d+), "([^"]+)", (\d+), (\d+),', cat[j:k]):
    n = m.group(2).strip()
    f = re.sub(r'^(Mark [IVX]+|Type \d|Class \d) ', '', n)
    fam.setdefault(f, []).append(int(m.group(1)))
assert sum(len(v) for v in fam.values()) == 318
for f, ids in fam.items():
    slug = re.sub(r'[^a-z0-9]+', '_', f.lower()).strip('_')
    ladder = len(ids) >= 5
    add('icon.structure.' + slug, 'structure_icon', f + (' (ladder, %d tiers)' % len(ids) if len(ids) > 1 else ''),
        'none: System and Database screens show text only', 'missing', 3, 'M1' if ladder else 'M2', building_ids=ids)

vfxkeys = [k for k in old if k.startswith('vfx.')]
assert len(vfxkeys) == 30
M1V = ('PhaserArray', 'PhotonTorpedo', 'Disruptor', 'generic', 'impact', 'explosion', 'cloak', 'decloak', 'warp', 'intercept', 'PulsePhaser', 'PhaserBank')
for key in vfxkeys:
    o = old[key]
    ms = 'M1' if key.split('.')[1] in M1V else 'M2'
    add(key, 'vfx', o['displayName'], 'procedural: battle_fx.gd beam/flare/trail/shockwave shaders (the key is read by BattleTimeline.vfx_weapon_key)', 'procedural', 6, ms)
for key, n, src, ws in [
        ('vfx.particle.sparks', 'Sparks (big)', 'battle_fx.gd _particle_material', 6), ('vfx.particle.sparks_small', 'Sparks (small)', 'battle_fx.gd', 6),
        ('vfx.particle.fire_small', 'Small fire', 'battle_fx.gd', 6), ('vfx.particle.fireball', 'Fireball (explosion core)', 'battle_fx.gd', 6),
        ('vfx.particle.smoke', 'Smoke', 'battle_fx.gd', 6), ('vfx.particle.debris', 'Debris', 'battle_fx.gd', 6),
        ('vfx.shield.bubble', 'Shield bubble', 'shaders/shield.gdshader (6-iteration loop)', 6), ('vfx.nav_lights', 'Navigation lights and strobes', 'shaders/blink.gdshader', 10),
        ('vfx.lens_flare', 'Lens flare / glare budget', 'shaders/flare.gdshader', 6), ('vfx.engine_trail', 'Engine / torpedo trail ribbon', 'shaders/trail.gdshader', 6),
        ('vfx.shockwave', 'Explosion shockwave', 'shaders/shockwave.gdshader', 6)]:
    add(key, 'vfx', n, 'procedural: godot/scenes/battle/' + src, 'procedural', ws, 'M1')
for key, n, src in [('vfx.galaxy.nebula', 'Nebula (galaxy map)', 'nebula.gdshader'), ('vfx.galaxy.ion_storm', 'Ion storm (galaxy map; blocky artefacts, lane L6 bug)', 'field.gdshader'),
                    ('vfx.galaxy.subspace_rift', 'Subspace rift', 'field.gdshader'), ('vfx.galaxy.wormhole', 'Wormhole', 'field.gdshader + MapDrawer arcs'),
                    ('vfx.galaxy.black_hole', 'Black hole', 'field.gdshader + MapDrawer disc'), ('vfx.galaxy.neutron_star', 'Neutron star pulsar', 'MapDrawer beam glyph'),
                    ('env.starfield', 'Galaxy starfield (parallax)', 'starfield.gdshader')]:
    add(key, 'vfx' if key.startswith('vfx') else 'environment', n, 'procedural: godot/scenes/galaxy/' + src, 'procedural', 2, 'M1')
for key, n, src in [('env.battle.space_sky', 'Battle sky', 'shaders/space_sky.gdshader'), ('env.battle.nebula_fog', 'Battle nebula bank (shader_type fog)', 'shaders/nebula_fog.gdshader'),
                    ('env.battle.light_rig', 'Battle lighting rig (sun, fill, rim, ambient)', 'battle_environment.gd')]:
    add(key, 'environment', n, 'procedural: godot/scenes/battle/' + src, 'procedural', 10, 'M1')
for key, n, ws, ms in [
        ('vfx.shield.facing_hit', 'Shield hit ripple per facing (fore, aft, port, starboard)', 6, 'M1'), ('vfx.subsystem.damage', 'Subsystem damage: venting, sparks, dead nacelle glow', 6, 'M1'),
        ('vfx.hull.fire_breach', 'Hull breach fire and decals', 6, 'M2'), ('vfx.debris.mesh', 'Debris meshes from the real hull', 6, 'M2'),
        ('vfx.warp.in', 'Warp-in flash and stretch', 6, 'M1'), ('vfx.warp.jump', 'Warp jump (strategic move)', 2, 'M1'), ('vfx.torpedo.detonation', 'Torpedo detonation per warhead', 6, 'M2'),
        ('vfx.firing_arc', 'Firing arc and range ring (plan phase)', 6, 'M1'), ('vfx.target_lock', 'Target lock / subsystem reticle', 6, 'M1'),
        ('vfx.territory_volume', 'Galaxy territory volumes (3D map)', 2, 'M1'), ('vfx.route_ribbon', 'Supply and trade route ribbons in space', 2, 'M1'),
        ('vfx.assault.landing', 'Planetary assault / ground combat', 6, 'M2'), ('vfx.borg.assimilation', 'Borg assimilation beam', 8, 'M3'), ('vfx.s8472.bioweapon', 'Species 8472 bio-plasma', 8, 'M3')]:
    add(key, 'vfx', n, 'none yet', 'missing', ws, ms, planned=True)

for key, n in [('ui.font.antonio', 'Antonio (display, LCARS headings)'), ('ui.font.inter', 'Inter (body)'), ('ui.font.share_tech_mono', 'Share Tech Mono (numerals)')]:
    add(key, 'ui', n, 'godot/scenes/ui/fonts (licence files included in repo)', 'final', 12, 'M1')
add('ui.theme.lcars', 'ui', 'LCARS theme (lcars.gd, theme/)', 'godot/scenes/ui/lcars.gd, scenes/ui/theme', 'placeholder', 12, 'M1', note='functional; lane L4 rebuilds it as the widget library')
for key, n in [('ui.icon_set', 'LCARS icon set (orders, resources, alerts)'), ('ui.cursor_set', 'Cursors'), ('ui.app_icon', 'Application icon (exe)')]:
    add(key, 'ui', n, 'none yet', 'missing', 12, 'M1', planned=True)

def has(key, f): return f in key.split('.')
for key in cues:
    p = key.split('.')[0]
    if p == 'vo':
        add(key, 'voice', key, 'original BOTF voice bank (private, LocalAssets/audio/voice), mapped by build_audio_cues.py', 'placeholder', 11, 'M1' if has(key, 'fed') or has(key, 'kli') else 'M2')
    elif p == 'mus':
        ms = 'M1' if has(key, 'fed') or has(key, 'kli') or key.split('.')[-1] in ('title', 'galaxy', 'combat', 'tension', 'all') else 'M2'
        add(key, 'music', key, 'original BOTF score (private, LocalAssets/audio/music); tracks 11-15 unidentified', 'placeholder', 11, ms)
    else:
        add(key, 'sfx', key, 'original BOTF effects (private, LocalAssets/audio/sfx), picked by spectrum, not by ear', 'placeholder', 11, 'M1')
for key, n, ms in [
        ('vo.order_ack.<order>.<fac>', 'Order acknowledgement per manoeuvre (not hooked to the order buttons today)', 'M1'),
        ('vo.advisor.<event>.<fac>', 'Advisor lines by event: turn, research, construction, colony, outpost, attack, fleet, contact, unrest, event', 'M1'),
        ('vo.diplomacy.<act>.<speaker>', 'Diplomacy acts: greet, propose, accept, refuse, war, peace, alliance', 'M1'),
        ('vo.combat.hail.<fac>', 'Hail, ram and cloak barks', 'M1'), ('vo.bark.hull_critical', 'Barks: hull critical, encounter, retreat, won, lost', 'M1'),
        ('vo.victory.scientific.<fac>', 'Scientific victory speech (missing today)', 'M2'), ('vo.minor.<race>', 'Minor race hail lines, about 3 each plus a shared alien fallback', 'M2'),
        ('vo.computer', 'Ship computer and narrator', 'M2'), ('vo.trek.library', "Lines cut from the Trek library on Simon's drive, per VOICE-PLAN.md source C", 'M2'),
        ('vo.eog.<fac>', 'End-of-game speeches (CEOG*.ogg exist locally, unmapped)', 'M4'), ('mus.adaptive.<state>', 'Adaptive music layers (explore, tension, combat, victory)', 'M2'),
        ('sfx.weapon.<type>', 'Per-weapon fire and impact sounds chosen by ear (beams and torpedoes are still a guess)', 'M1'),
        ('sfx.ui.<widget>', 'Redesigned UI sound set', 'M1')]:
    add(key, 'voice' if key.startswith('vo') else ('music' if key.startswith('mus') else 'sfx'), n,
        'none yet' if 'eog' not in key else 'local files exist, not mapped', 'missing', 11, ms, planned=True)

keys = [r['key'] for r in rows]
assert len(keys) == len(set(keys)), [k for k, c in collections.Counter(keys).items() if c > 1]
doc = {
    'schemaVersion': 1,
    'about': 'BOTF Remastered asset manifest (lane L3, 8 Nov 2026). One row per asset key the game uses, plus rows marked planned:true for keys the master plan needs and the game does not use yet. Rows only: no binaries, no third-party files. The older candidate-sources manifest (UE-era import settings) stays in assets/manifest.json; ship rows link to it by key. Every row carries desktop_budget and web_budget; the budgets are targets set in ART-BIBLE.md section 12, not measured limits.',
    'statusValues': {
        'procedural': 'generated by code at run time (shader, primitives, particle recipe); no authored art file',
        'placeholder': 'a stand-in that works but has not passed the review method (third-party hull, 2D circle, audio picked by spectrum); replace or review',
        'final': 'passed the ART-BIBLE review method and is signed off by the art director lane',
        'missing': 'nothing in the game for this key (the gap is the number that shrinks)'},
    'milestones': 'M1 vertical slice (Federation vs Klingon, small galaxy), M2 faithful parity (all five majors, vanilla minors), M3 mods and modern, M4 polish (MASTER-PLAN section 6)',
    'workstreams': 'MASTER-PLAN section 5 numbers: 2 galaxy, 3 systems and planets, 6 combat, 7 diplomacy and minors, 8 majors and threats, 10 art, 11 audio, 12 UX',
    'sources': {
        'ship keys': 'godot/scenes/battle/ship_factory.gd CANON_LENGTHS (159)', 'hero models': 'game/LocalAssets/ships/godot-hero-manifest.json (33)',
        'races': 'game/Source/BotfSim/ReferenceCatalogue.h RaceCatalogue (35)', 'structures': 'ReferenceCatalogue.h BuildingCatalogue (318 records, grouped to ladders)',
        'planets': 'Simulation.h PlanetClass (9), ReferencePlanets.h kPlanetTypes', 'audio': 'game/LocalAssets/audio/cues.json (261, private, built by tools/build_audio_cues.py)',
        'vfx': 'assets/manifest.json vfx.* (30) plus godot/scenes/battle/battle_fx.gd and godot/scenes/galaxy'},
    'assets': rows}
json.dump(doc, open(R + '/assets/asset-manifest.json', 'w', encoding='utf8'), indent=1, ensure_ascii=False)
print(len(rows), dict(collections.Counter(r['category'] for r in rows)), dict(collections.Counter(r['status'] for r in rows)))
print(collections.Counter((r['milestone']) for r in rows))
