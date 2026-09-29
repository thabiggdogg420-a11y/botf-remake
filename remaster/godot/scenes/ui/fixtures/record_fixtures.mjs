// Records botf_call responses from the wasm core into JSON fixtures for the Godot fixture backend.
//   node remaster/godot/scenes/ui/fixtures/record_fixtures.mjs
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
const here = dirname(fileURLToPath(import.meta.url));
const { default: createBotfCore } = await import(pathToFileURL(join(here, '../../../../web/src/generated/botf-core.mjs')).href);
const core = await createBotfCore();
const raw = core.cwrap('botf_call', 'string', ['string', 'string']);
const call = (m, a = {}) => { const r = JSON.parse(raw(m, JSON.stringify(a))); if (!r.ok) throw new Error(`${m}: ${r.error}`); return r.result; };
function endTurn() {
  let s = call('endTurn');
  while (s.status === 'battle') { call('tacticalAuto'); s = call('endTurn'); }
  return s.briefing;
}
function dump(set, name, value) {
  const dir = join(here, set); mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, `${name}.json`), JSON.stringify(value));
}

// Campaign: Federation, played 24 turns so the map has territory and history.
dump('campaign', 'meta', call('meta'));
dump('campaign', 'encyclopedia', call('encyclopedia'));
dump('campaign', 'newCampaign', call('newCampaign', { faction: 1, empires: 5, seed: 424242, era: 3, ruleset: 'reference', preset: 'remastered', width: 22, height: 16, systems: 48, events: true, aiAggression: true }));
let briefing = null;
let rng = 7;
const rand = (n) => { rng = (rng * 1103515245 + 12345) % 2147483648; return rng % n; };
for (let i = 0; i < 40; i++) {
  const cur = call('view');
  const w = cur.meta.width;
  for (const c of cur.colonies) {
    if (c.governor.workforce !== 'Balanced') try { call('submit', { command: { type: 'SetColonyGovernor', systemId: c.systemId, governorWorkforce: 'Balanced', governorQueue: 'Balanced' } }); } catch (e) { console.log(String(e)); }
  }
  for (const f of cur.fleets) {
    if (f.colonyShip) {
      if (call('dryRun', { command: { type: 'Colonize', fleetId: f.id } }).ok) { call('submit', { command: { type: 'Colonize', fleetId: f.id } }); continue; }
    }
    if (f.hasOrders && (f.targetX !== f.x || f.targetY !== f.y)) continue;
    const sup = call('supply', { fleetId: f.id });
    const cells = [...sup].map((c, k) => (c === '1' && cur.fog[k] !== '2' ? k : -1)).filter((k) => k >= 0);
    const pool = cells.length ? cells : [...sup].map((c, k) => (c === '1' ? k : -1)).filter((k) => k >= 0);
    if (!pool.length) continue;
    const t = pool[rand(pool.length)];
    try { call('submit', { command: { type: 'MoveFleet', fleetId: f.id, targetX: t % w, targetY: Math.floor(t / w) } }); } catch {}
  }
  briefing = endTurn();
}
const v = call('view');
dump('campaign', 'endTurn', { status: 'complete', turn: v.meta.turn, briefing });
dump('campaign', 'briefing', briefing);
dump('campaign', 'view', v);
for (const c of v.colonies) dump('campaign', `colony_${c.systemId}`, call('colony', { systemId: c.systemId }));
const home = v.colonies[0];
const d = call('colony', { systemId: home.systemId });
dump('campaign', 'workforcePreview', call('workforcePreview', { systemId: home.systemId, ...d.committedWorkers }));
for (const f of v.fleets) dump('campaign', `supply_${f.id}`, call('supply', { fleetId: f.id }));
const f0 = v.fleets[0];
dump('campaign', 'route', call('route', { fleetId: f0.id, x: Math.min(v.meta.width - 1, f0.x + 3), y: f0.y }));
for (const s of v.stations.filter((s) => s.mine)) dump('campaign', `station_${s.id}`, call('station', { stationId: s.id }));
dump('campaign', 'research', call('research'));
dump('campaign', 'forecast', call('forecast'));
dump('campaign', 'diplomacy', call('diplomacy'));
dump('campaign', 'intel', call('intel'));
dump('campaign', 'replays', call('replays'));
dump('campaign', 'save', call('save'));
dump('campaign', 'fingerprint', call('fingerprint'));
dump('campaign', 'dryRun', { ok: true, error: '' });
const menu = ['Colonize', 'BuildOutpost', 'Terraform', 'Invade'].map((type) => ({ command: { type, fleetId: f0.id } }));
dump('campaign', 'dryRunMany', call('dryRunMany', { items: menu }));

// Frontier drill: anomalies, stations, galactic events.
dump('frontier', 'view', call('newFrontierDrill'));
dump('diplomacy', 'view', call('newDiplomacyDrill'));
dump('diplomacy', 'diplomacy', call('diplomacy'));
// Battle drill: tactical encounter and a resolved round.
call('newBattleDrill');
call('endTurn');
dump('battle', 'tactical', call('tactical'));
dump('battle', 'tacticalRound', call('tacticalRound'));
console.log('fixtures recorded; turn', v.meta.turn);
