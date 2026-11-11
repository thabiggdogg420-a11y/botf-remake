// Records botf_call responses into ../fixtures/*.json using the committed wasm core
// (the same core and bridge the GDExtension compiles natively). Deterministic: reruns give identical files.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import createBotfCore from '../../web/src/generated/botf-core.mjs';

const out = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../fixtures');
fs.mkdirSync(out, { recursive: true });
const core = await createBotfCore();
const raw = core.cwrap('botf_call', 'string', ['string', 'string']);
function call(method, args = {}) {
  const reply = JSON.parse(raw(method, JSON.stringify(args)));
  if (!reply.ok) throw new Error(`${method}: ${reply.error}`);
  return reply.result;
}
function dump(name, method, args, result) {
  fs.writeFileSync(path.join(out, `${name}.json`), JSON.stringify({ method, args, result }, null, 1) + '\n');
  console.log(`fixtures/${name}.json`);
}
const rec = (name, method, args = {}) => { const r = call(method, args); dump(name, method, args, r); return r; };
function endTurn() {
  let s = call('endTurn');
  while (s.status === 'battle') { call('tacticalAuto'); s = call('endTurn'); }
  return s;
}

rec('meta', 'meta');
const campaign = { faction: 1, empires: 5, seed: 424242, era: 1, ruleset: 'reference' };
const view = rec('campaign-view', 'newCampaign', campaign);
rec('colony-detail', 'colony', { systemId: view.colonies[0].systemId });
rec('research', 'research');
rec('diplomacy', 'diplomacy');
rec('intel', 'intel');
let status;
for (let i = 0; i < 5; i++) status = endTurn();
dump('campaign-briefing-turn5', 'endTurn', {}, status);
rec('campaign-view-turn5', 'view');
rec('diplomacy-drill-view', 'newDiplomacyDrill');
rec('diplomacy-drill', 'diplomacy');
// Rules 3 battle (station drill, Remastered preset): encounter before and after rounds, with events.
call('newBattleDrill', { station: true });
dump('battle-rules3-endturn', 'endTurn', {}, call('endTurn'));
rec('battle-rules3-encounter', 'tactical');
call('tacticalOrder', { all: true, maneuver: 'Engage' });
rec('battle-rules3-round1', 'tacticalRound');
rec('battle-rules3-round2', 'tacticalRound');
rec('battle-rules3-final', 'tacticalAuto');
// Determinism references for the Godot tests: fingerprint after a scripted 10-turn run.
// determinism = the default (tactical rules 4, WEGO); determinism-rules3 = the same campaign with "wego": false.
call('newCampaign', campaign);
for (let i = 0; i < 10; i++) endTurn();
dump('determinism', 'fingerprint', { campaign, turns: 10 }, call('fingerprint'));
const campaign3 = { ...campaign, wego: false };
call('newCampaign', campaign3);
for (let i = 0; i < 10; i++) endTurn();
dump('determinism-rules3', 'fingerprint', { campaign: campaign3, turns: 10 }, call('fingerprint'));
// Galaxy generator (schema 15, docs/design/GALAXY-GENERATOR.md): one wasm-recorded fingerprint per shape, with a density and
// a difficulty each, after a scripted 10-turn run. The native core must reproduce every one.
{
  const galaxy = [];
  const combos = [['spiral', 'some', 'normal'], ['ring', 'many', 'hard'], ['irregular', 'few', 'impossible'], ['elliptical', 'none', 'simple']];
  for (const [shape, minorDensity, difficulty] of combos) {
    const c = { faction: 1, empires: 5, seed: 424242, era: 1, ruleset: 'reference', width: 22, height: 16, systems: 50, shape, minorDensity, difficulty };
    call('newCampaign', c);
    for (let i = 0; i < 10; i++) endTurn();
    galaxy.push({ campaign: c, turns: 10, result: call('fingerprint'), preview: call('galaxyPreview', c).stars.length });
  }
  dump('determinism-galaxy', 'fingerprint', { runs: galaxy.length }, galaxy);
}
// Empire slots (schema 17, docs/design/EMPIRE-SLOTS.md): lineups of two, six and eight majors, with Dominion and Borg, after a
// scripted 20-turn run. `faction` is the empire id (race slot). The native core must reproduce every fingerprint.
{
  const slots = [];
  const lineups = [[7, ['dominion', 'borg']], [6, ['federation', 'klingon', 'romulan', 'cardassian', 'ferengi', 'dominion']],
    [1, ['federation', 'klingon', 'romulan', 'cardassian', 'ferengi', 'dominion', 'borg', 'breen']]];
  for (const [faction, lineup] of lineups) {
    const c = { faction, lineup, seed: 424242, era: 1, ruleset: 'reference', width: 22, height: 16, systems: 50 };
    call('newCampaign', c);
    for (let i = 0; i < 20; i++) endTurn();
    slots.push({ campaign: c, turns: 20, result: call('fingerprint'), preview: call('galaxyPreview', c).stars.length });
  }
  dump('determinism-slots', 'fingerprint', { runs: slots.length }, slots);
}
