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
// Threat actors (schema 20, docs/design/THREAT-ACTORS.md): monsters and Borg on at frequent, the sphere/8472/DS9 replacements, and a seated
// Borg empire beside the monsters, each after a scripted run long enough for roaming actors to spawn and fight. The recording keeps
// how many actors the wasm core saw spawn (nextThreatId) so a native core that silently stops spawning cannot pass.
{
  const runs = [];
  const base = { seed: 424242, era: 1, ruleset: 'reference', width: 22, height: 16, systems: 50 };
  const setups = [
    [{ ...base, faction: 1, empires: 5, threats: { monsters: true, borg: true, frequency: 'frequent' } }, 80],
    [{ ...base, faction: 1, empires: 5, threats: { monsters: true, borg: true, frequency: 'frequent', borgSphere: true, species8472: true, edoStation: true } }, 70],
    [{ ...base, faction: 1, lineup: ['federation', 'klingon', 'romulan', 'dominion', 'borg'], threats: { monsters: true, borg: true, frequency: 'normal' } }, 70],
  ];
  for (const [c, turns] of setups) {
    call('newCampaign', c);
    let ran = 0;
    for (let i = 0; i < turns; i++) { try { endTurn(); ran++; } catch (e) { if (String(e).includes('Campaign has ended')) break; throw e; } }
    const v = call('view');
    runs.push({ campaign: c, turns: ran, result: call('fingerprint'), threats: v.threatSetup, spawned: v.threatsSpawned });
  }
  dump('determinism-threats', 'fingerprint', { runs: runs.length }, runs);
}
// Campaign eras (schema 22, docs/design/ERAS.md): Ages and Galaxies at every start year, after a scripted 60-turn run long enough
// for the AI to build era ships and cross a year of the timeline. Records the year and research cap the wasm core reports, so a native
// core with a different clock or cap cannot pass. The native core must reproduce every fingerprint.
{
  const runs = [];
  const base = { faction: 1, seed: 424242, ruleset: 'reference', width: 22, height: 16, systems: 50 };
  for (const [preset, lineup] of [['ages', undefined], ['galaxies', ['federation', 'klingon', 'romulan', 'dominion', 'borg']]]) {
    for (let era = 1; era <= 5; era++) {
      const c = { ...base, era, preset, ...(lineup ? { lineup } : {}) };
      call('newCampaign', c);
      let ran = 0;
      for (let i = 0; i < 60; i++) { try { endTurn(); ran++; } catch (e) { if (String(e).includes('Campaign has ended')) break; throw e; } }
      const v = call('view');
      runs.push({ campaign: c, turns: ran, result: call('fingerprint'), year: v.meta.year, techCap: v.meta.techCap, level: call('research').fields[0].level });
    }
  }
  call('loadBaseContent');
  dump('determinism-eras', 'fingerprint', { runs: runs.length }, runs);
}
// Packs and presets (schema 17, docs/design/PACKS-AND-PRESETS.md): one wasm-recorded fingerprint and stack checksum per preset
// after a scripted 8-turn run. The native core must reproduce every one, so the overlay merge, the stack checksum and the
// preset flags are proven identical native against wasm. The preset list itself (meta.presets) rides in the meta fixture.
{
  const presets = [];
  for (const id of call('listPresets').map((p) => p.id)) {
    const c = { faction: 1, empires: 3, seed: 424242, era: 1, ruleset: 'reference', width: 16, height: 13, systems: 30, preset: id };
    call('newCampaign', c);
    for (let i = 0; i < 8; i++) endTurn();
    const info = call('contentInfo');
    presets.push({ campaign: c, turns: 8, result: call('fingerprint'), content: { id: info.id, checksum: info.checksum, stack: info.stack.length }, preset: call('view').meta.rules.preset });
  }
  call('loadBaseContent');
  dump('determinism-presets', 'fingerprint', { runs: presets.length }, presets);
}
// Dominion content (schema 21, docs/design/DOMINION.md): the Dominion War preset (race structures, traits, the new minors, Bajor for the
// Edo) as the Federation and as the Dominion, 30 turns. The native core must reproduce each fingerprint and the overlay stack checksum.
{
  const dom = [];
  for (const faction of [1, 6]) {
    const c = { faction, lineup: ['federation', 'klingon', 'romulan', 'cardassian', 'dominion'], seed: 424242, era: 3, ruleset: 'reference',
      width: 22, height: 16, systems: 48, preset: 'dominion-war', minorDensity: 'many' };
    call('newCampaign', c);
    for (let i = 0; i < 30; i++) endTurn();
    const info = call('contentInfo');
    dom.push({ campaign: c, turns: 30, result: call('fingerprint'), content: { id: info.id, checksum: info.checksum, stack: info.stack.length } });
  }
  call('loadBaseContent');
  dump('determinism-dominion', 'fingerprint', { runs: dom.length }, dom);
}
// Economy and rules mechanics (schema 23, docs/design/ECONOMY-MECHANICS.md): the presets that carry the economy flags (vanilla orders,
// automatic upgrades, the UDM set) for 50 turns of AI play. The native core must reproduce each fingerprint.
{
  const eco = [];
  for (const preset of ['remastered', 'ultimate', 'balance-of-power', 'dominion-war']) {
    const c = { faction: 1, empires: 5, seed: 31337, era: 2, ruleset: 'reference', preset };
    call('newCampaign', c);
    let played = 0;   // A campaign may end early under the schema-24 AI; the fixture records the turns actually played.
    for (let i = 0; i < 50; i++) { try { endTurn(); played++; } catch (e) { if (!/ended/.test(String(e.message))) throw e; break; } }
    eco.push({ campaign: c, turns: played, result: call('fingerprint') });
  }
  call('loadBaseContent');
  dump('determinism-economy', 'fingerprint', { runs: eco.length }, eco);
}
// Balance and AI (schema 24, docs/design/BALANCE-AI.md): every difficulty tier incl. Expert, the galaxy options (planets per star, extra
// start colonies, race star types) and the three balance presets, 40 turns of AI play. The native core must reproduce each fingerprint.
{
  const bal = [];
  const runs = [
    { difficulty: 'expert' }, { difficulty: 'impossible' }, { difficulty: 'hard' }, { difficulty: 'simple' },
    { difficulty: 'normal', startSystems: 2, planetsMin: 3, planetsMax: 8, starTypes: true },
    { preset: 'balance-of-power', difficulty: 'expert', startSystems: 1, planetsMin: 2, planetsMax: 9 },
    { preset: 'ultimate' }, { preset: 'dominion-war', lineup: ['federation', 'klingon', 'romulan', 'cardassian', 'dominion'], era: 3 },
  ];
  for (const r of runs) {
    const c = { faction: 1, empires: 5, seed: 20261112, era: 2, ruleset: 'reference', width: 22, height: 16, systems: 50, ...r };
    call('newCampaign', c);
    let played = 0;
    for (let i = 0; i < 40; i++) { try { endTurn(); played++; } catch (e) { if (!/ended/.test(String(e.message))) throw e; break; } }
    bal.push({ campaign: c, turns: played, result: call('fingerprint') });
  }
  call('loadBaseContent');
  dump('determinism-balance', 'fingerprint', { runs: bal.length }, bal);
}
