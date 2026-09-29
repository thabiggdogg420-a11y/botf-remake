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
// Determinism reference for the Godot tests: fingerprint after a scripted 10-turn run.
call('newCampaign', campaign);
for (let i = 0; i < 10; i++) endTurn();
dump('determinism', 'fingerprint', { campaign, turns: 10 }, call('fingerprint'));
