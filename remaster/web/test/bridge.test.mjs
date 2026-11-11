// Bridge contract test: drives the WebAssembly module from Node exactly as the browser does.
//   node test/bridge.test.mjs
import assert from 'node:assert/strict';
import createBotfCore from '../src/generated/botf-core.mjs';

const core = await createBotfCore();
const raw = core.cwrap('botf_call', 'string', ['string', 'string']);
function call(method, args = {}) {
  const reply = JSON.parse(raw(method, JSON.stringify(args)));
  if (!reply.ok) throw new Error(`${method}: ${reply.error}`);
  return reply.result;
}
function attempt(method, args = {}) {
  return JSON.parse(raw(method, JSON.stringify(args)));
}

let passed = 0;
function test(name, fn) {
  const t0 = performance.now();
  fn();
  passed++;
  console.log(`  ok  ${name}  (${(performance.now() - t0).toFixed(0)} ms)`);
}

/** Ends a turn, fighting any player battle to completion under automatic command. */
function endTurn() {
  let status = call('endTurn');
  let guard = 0;
  while (status.status === 'battle') {
    const encounter = call('tactical');
    assert.ok(encounter && Array.isArray(encounter.ships), 'battle exposes a tactical encounter');
    call('tacticalAuto');
    status = call('endTurn');
    assert.ok(++guard < 20, 'battles terminate');
  }
  assert.equal(status.status, 'complete');
  return status.briefing;
}

console.log('BOTF web bridge');

test('meta lists five majors and the starting eras', () => {
  const meta = call('meta');
  assert.equal(meta.empires.length, 8); // five originals, Dominion, Borg, Breen (schema 17)
  assert.ok(meta.eras.length >= 1);
  assert.equal(meta.researchFields.length, 6);
  assert.ok(meta.maneuvers.some((m) => m.key === 'Engage'));
});

let view;
test('new reference campaign returns a fog-respecting view', () => {
  view = call('newCampaign', { faction: 2, empires: 5, seed: 424242, era: 1, ruleset: 'reference' });
  assert.equal(view.meta.player, 2);
  assert.equal(view.meta.turn, 0);
  assert.equal(view.fog.length, view.meta.width * view.meta.height);
  const home = view.colonies[0];
  assert.ok(home && home.population > 0);
  // Every star is listed; an unexplored one is uncharted: id, position, a catalogue designation and explored:false, nothing more.
  const uncharted = view.systems.filter((s) => s.explored === false);
  for (const system of view.systems.filter((s) => s.explored !== false)) assert.notEqual(view.fog[system.y * view.meta.width + system.x], '0');
  for (const system of uncharted) {
    assert.deepEqual(Object.keys(system).sort(), ['explored', 'id', 'name', 'x', 'y']);
    assert.match(system.name, /^FGC-\d{3}$/);
    assert.equal(view.fog[system.y * view.meta.width + system.x], '0');
  }
  assert.ok(uncharted.length > 0 && view.systems.length > uncharted.length);
  // Foreign ships only appear inside sensor range.
  for (const c of view.contacts) assert.equal(view.fog[c.y * view.meta.width + c.x], '2');
  assert.ok(view.fleets.length >= 2, 'player starts with ships');
  assert.ok(view.pending.length >= 1, 'starter build queue orders are drafted');
});

test('invalid orders are rejected with the core reason and change nothing', () => {
  const before = call('fingerprint');
  const bad = attempt('submit', { command: { type: 'Colonize', fleetId: 999 } });
  assert.equal(bad.ok, false);
  assert.match(bad.error, /\w/);
  assert.equal(call('fingerprint'), before);
  const dry = call('dryRun', { command: { type: 'AllocateResearch', researchAllocation: [50, 50, 50, 0, 0, 0] } });
  assert.equal(dry.ok, false);
  assert.match(dry.error, /100 percent/);
});

test('orders: move scout, staff workers, allocate research, queue construction', () => {
  const scout = view.fleets.find((f) => f.shipClass === 'Scout') ?? view.fleets[0];
  const supply = call('supply', { fleetId: scout.id });
  const w = view.meta.width;
  const target = [...supply].map((v, i) => (v === '1' ? i : -1)).filter((i) => i >= 0 && i !== scout.y * w + scout.x).pop();
  const route = call('route', { fleetId: scout.id, x: target % w, y: Math.floor(target / w) });
  assert.ok(route.cells.length > 1 && route.eta >= 1);
  call('submit', { command: { type: 'MoveFleet', fleetId: scout.id, targetX: target % w, targetY: Math.floor(target / w) } });

  const home = view.colonies[0];
  const detail = call('colony', { systemId: home.systemId });
  const pop = detail.colony.population;
  const food = Math.min(detail.capacityLimits.food, Math.ceil(pop / 2));
  const industry = Math.min(detail.capacityLimits.industry, pop - food);
  const preview = call('workforcePreview', { systemId: home.systemId, food, industry, research: 0 });
  assert.equal(preview.error, '');
  assert.equal(preview.workers.industry, industry);
  call('submit', { command: { type: 'AssignWorkers', systemId: home.systemId, foodWorkers: food, industryWorkers: industry, researchWorkers: 0 }, cancelIds: preview.cancelIds });

  call('submit', { command: { type: 'AllocateResearch', researchAllocation: [20, 20, 20, 20, 10, 10] } });
  const buildable = detail.choices.find((c) => !c.blocker && !c.isShip);
  assert.ok(buildable, 'something can be built at home');
  call('submit', { command: { type: 'EnqueueBuilding', systemId: home.systemId, building: buildable.product, referenceId: buildable.referenceId } });
  view = call('view');
  assert.ok(view.pending.some((p) => p.type === 'MoveFleet'));
  assert.ok(view.pending.some((p) => p.type === 'AllocateResearch'));
  const moved = view.fleets.find((f) => f.id === scout.id);
  assert.equal(moved.targetX, target % w);
  assert.ok(moved.hasOrders);
});

test('forecast predicts next turn without committing it', () => {
  const before = call('fingerprint');
  const forecast = call('forecast');
  assert.equal(forecast.error, '');
  assert.equal(call('fingerprint'), before);
  const briefing = endTurn();
  assert.equal(briefing.creditsAfter, forecast.creditsAfter, 'forecast matches the resolved turn');
});

test('50 turns resolve through the campaign session', () => {
  let briefings = 0;
  for (let i = 0; i < 49; i++) {
    const b = endTurn();
    assert.ok(Array.isArray(b.events));
    briefings++;
    if (b.outcome !== 'ongoing') break;
  }
  view = call('view');
  assert.ok(view.meta.turn >= 20, `reached turn ${view.meta.turn}`);
  assert.ok(briefings > 0);
  for (const c of view.contacts) assert.equal(view.fog[c.y * view.meta.width + c.x], '2');
  call('research');
  call('diplomacy');
  call('intel');
});

test('save -> load restores the identical campaign fingerprint', () => {
  const fp = call('fingerprint');
  const save = call('save');
  assert.equal(save.kind, 'game');
  const turn = call('view').meta.turn;
  endTurn();
  assert.notEqual(call('fingerprint'), fp);
  const loaded = call('load', { data: save.data });
  assert.equal(loaded.meta.turn, turn);
  assert.equal(call('fingerprint'), fp);
  // Determinism: the same end turn from the same save produces the same result.
  endTurn();
  const a = call('fingerprint');
  call('load', { data: save.data });
  endTurn();
  assert.equal(call('fingerprint'), a);
});

test('battle drill pauses the turn for tactical orders; mid-battle save round-trips', () => {
  call('newBattleDrill');
  const status = call('endTurn');
  assert.equal(status.status, 'battle');
  const encounter = call('tactical');
  const mine = encounter.ships.filter((s) => s.mine && s.alive);
  const foe = encounter.ships.find((s) => !s.mine && !s.hidden);
  assert.ok(mine.length >= 2);
  call('tacticalOrder', { fleetId: mine[0].id, maneuver: 'Charge', targetFleetId: foe ? foe.id : 0 });
  call('tacticalOrder', { all: true, maneuver: 'Engage' });
  const afterRound = call('tacticalRound');
  assert.equal(afterRound.round, 1);
  assert.equal(attempt('submit', { command: { type: 'AllocateResearch', researchAllocation: [100, 0, 0, 0, 0, 0] } }).ok, false,
    'strategic orders are locked during a battle');

  const save = call('save');
  assert.equal(save.kind, 'session');
  call('load', { data: save.data });
  const resumed = call('tactical');
  assert.equal(resumed.round, 1);
  const done = call('tacticalAuto');
  assert.equal(done.finished, true);
  const result = call('endTurn');
  assert.equal(result.status, 'complete');
  assert.ok(result.briefing.events.some((e) => e.category === 'combat'));
  assert.equal(call('replays').length, 1);
  const replay = call('replay', { index: 0 });
  assert.ok(replay.rounds.length >= 1);
  const v = call('view');
  assert.ok(v.battles.length >= 1);
  const report = call('battleReport', { index: v.battles[v.battles.length - 1].index });
  assert.ok(report.participants.length >= 2);
});

// Regression for the removed order-pinning workaround: rounds now resolve through plain
// session::resolveRound / session::autoResolve and must still verify in session::resume
// (Lane A made replay verification re-derive AI decisions). Each pattern mixes implicit
// (AI-decided) player ships with explicit orders in a multi-ship battle.
test('multi-ship battles resolved with implicit AI orders verify through session::resume', () => {
  const patterns = {
    'auto at once': () => call('tacticalAuto'),
    'implicit rounds then auto': () => { call('tacticalRound'); call('tacticalRound'); call('tacticalAuto'); },
    'one explicit ship, rest AI': (e) => {
      const mine = e.ships.filter((s) => s.mine && s.active);
      call('tacticalOrder', { fleetId: mine[0].id, maneuver: 'Charge' });
      call('tacticalRound');
      call('tacticalAuto');
    },
    'retreat all (disengage), then AI': () => {
      const n = attempt('tacticalOrder', { all: true, maneuver: 'Retreat' });
      const after = call('tacticalRound');
      if (n.ok) assert.ok(after.ships.some((s) => s.mine && (s.disengaging || s.retreated)) || after.finished, 'retreat disengages first');
      call('tacticalAuto');
    },
    'hail everyone': () => { call('tacticalOrder', { all: true, maneuver: 'Hail' }); call('tacticalRound'); if (!call('tactical').finished) call('tacticalAuto'); },
    'ram and hold': (e) => {
      const mine = e.ships.filter((s) => s.mine && s.active);
      call('tacticalOrder', { all: true, maneuver: 'Hold' });
      call('tacticalOrder', { fleetId: mine[mine.length - 1].id, maneuver: 'Ram' });
      for (let i = 0; i < 4 && !call('tactical').finished; i++) call('tacticalRound');
      if (!call('tactical').finished) call('tacticalAuto');
    },
    'group order then auto': (e) => {
      const g = e.ships.find((s) => s.mine && s.active && s.groupId).groupId;
      call('tacticalOrder', { groupId: g, maneuver: 'Circle' });
      call('tacticalRound');
      call('tacticalAuto');
    },
  };
  for (const [name, run] of Object.entries(patterns)) {
    call('newBattleDrill');
    assert.equal(call('endTurn').status, 'battle', name);
    const e = call('tactical');
    assert.ok(e.ships.filter((s) => s.mine).length >= 3, 'multi-ship battle');
    run(e);
    assert.equal(call('tactical').finished, true, `${name}: battle finished`);
    const done = call('endTurn');
    assert.equal(done.status, 'complete', `${name}: turn completes (replay verified)`);
    // Mid-battle save/load with AI-decided rounds also resumes.
  }
  call('newBattleDrill');
  call('endTurn');
  call('tacticalRound');
  const save = call('save');
  call('load', { data: save.data });
  call('tacticalAuto');
  assert.equal(call('endTurn').status, 'complete', 'mid-battle save of an implicit round verifies');
});

test('tactical view: AI order previews, hail acceptance, cloak/ambush and ram records', () => {
  call('newBattleDrill');
  call('endTurn');
  const e = call('tactical');
  assert.equal(e.rules, 2);
  assert.ok(e.hail && typeof e.hail.accepts === 'boolean' && e.hail.ourStrength > 0);
  for (const s of e.ships.filter((x) => x.active && !x.hidden)) {
    assert.ok(s.next && s.next.maneuver && ['order', 'standing', 'ai'].includes(s.next.source), 'every active ship has a next-order preview');
  }
  // Klingon ships start cloaked: hidden from us until they decloak to fire.
  assert.ok(e.ships.some((s) => !s.mine && s.cloaked), 'cloaked enemy present');
  const mine = e.ships.filter((s) => s.mine && s.active);
  call('tacticalOrder', { fleetId: mine[0].id, maneuver: 'Ram' });
  let r;
  for (let i = 0; i < 12; i++) { r = call('tacticalRound'); if (r.finished) break; }
  const rounds = r.rounds;
  assert.ok(rounds.some((x) => x.ships.some((st) => st.decloaked)), 'cloaked AI ships decloak to attack');
  assert.ok(rounds.some((x) => x.salvos.some((sv) => sv.ambush)), 'decloak volley is marked as an ambush');
  assert.ok(rounds.some((x) => x.ships.some((st) => st.ai)), 'AI decisions are marked');
  if (rounds.some((x) => x.salvos.some((sv) => sv.ram))) {
    const ramRound = rounds.find((x) => x.salvos.some((sv) => sv.ram));
    assert.ok(ramRound.salvos.filter((sv) => sv.ram).length >= 2, 'ram impact and recoil are both recorded');
  }
  const alive = r.ships.find((s) => s.mine && s.active);
  if (!r.finished && alive) {
    assert.ok(call('tacticalOrder', { groupId: alive.groupId, maneuver: 'Engage' }) >= 1);
    call('tacticalAuto');
  }
  assert.equal(call('endTurn').status, 'complete');
});

test('an auto-resolved session battle commits the same state as headless advance()', () => {
  call('newBattleDrill');
  const headless = call('headlessFingerprint');
  assert.equal(call('endTurn').status, 'battle');
  call('tacticalAuto');
  assert.equal(call('endTurn').status, 'complete');
  assert.equal(call('fingerprint'), headless);
});

test('diplomacy drill: incoming AI proposals, call to arms, betrayal, dissolved treaty, eliminated empire', () => {
  const v = call('newDiplomacyDrill');
  assert.equal(v.meta.scenario, 'diplomacy');
  const card = v.empires.find((e) => e.empire === 4);
  assert.equal(card.eliminated, true, 'Cardassia is eliminated');
  const d = call('diplomacy');
  const incoming = d.proposals.filter((p) => p.incoming);
  assert.ok(incoming.some((p) => p.kind === 'treaty' && p.proposer.id === 5), 'Ferengi trade proposal');
  const tribute = incoming.find((p) => p.kind === 'tribute');
  assert.ok(tribute && tribute.creditDemand === 150, 'Klingon tribute demand');
  assert.ok(incoming.every((p) => p.dueTurn === v.meta.turn + 2), 'AI proposals to the player allow two turns');
  assert.equal(d.callsToArms.length, 1);
  assert.equal(d.callsToArms[0].aggressor, 2);
  assert.equal(d.callsToArms[0].defender, 3);
  assert.ok(d.treatyHistory.some((t) => t.status === 'Dissolved' && t.counterparty.id === 4), 'dissolved treaty in history');
  assert.ok(d.news.some((n) => n.typeKey === 12), 'betrayal is galactic news');
  assert.equal(d.empires.find((e) => e.empire === 2).memory.theirBetrayals, 1, 'Klingon standing records the betrayal');
  assert.equal(d.empires.find((e) => e.empire === 3).allied, true);
  assert.ok(v.alerts.some((a) => a.kind === 'callToArms'));

  // Accept the trade pact, reject the tribute, honour the call to arms.
  const trade = incoming.find((p) => p.proposer.id === 5);
  call('submit', { command: { type: 'RespondDiplomaticProposal', proposalId: trade.id, proposalAccept: true } });
  call('submit', { command: { type: 'RespondDiplomaticProposal', proposalId: tribute.id, proposalAccept: false } });
  assert.equal(call('diplomacy').proposals.filter((p) => p.responseDrafted).length, 2);
  assert.equal(call('diplomacy').callsToArms[0].honorBlocker, '');
  call('submit', { command: { type: 'DeclareWar', targetEmpire: 2 } });
  assert.equal(call('diplomacy').callsToArms[0].honorDrafted, true);
  const b = endTurn();
  const after = call('diplomacy');
  assert.ok(after.treaties.some((t) => t.type === 'Trade' && t.counterparty.id === 5), 'trade pact signed');
  assert.ok(after.empires.find((e) => e.empire === 2).atWar, 'at war with the aggressor');
  assert.ok(b.events.some((e) => e.category === 'diplomacy'), 'briefing reports diplomacy');
  assert.ok(!b.events.some((e) => e.category === 'economy' && /Diplomacy|Galactic news/.test(e.text)), 'diplomacy is not misfiled');
});

test('diplomacy drill: an unanswered call to arms lapses with consequences', () => {
  call('newDiplomacyDrill');
  const trustBefore = call('diplomacy').empires.find((e) => e.empire === 3).trust;
  for (let i = 0; i < 4; i++) endTurn();
  const d = call('diplomacy');
  assert.equal(d.callsToArms.length, 0, 'call lapsed');
  assert.ok(d.empires.find((e) => e.empire === 3).trust < trustBefore, 'refusal costs trust');
  assert.ok(d.events.some((e) => e.typeKey === 15), 'AllianceRefused recorded');
});

// ---- Schema-13 gameplay layer -----------------------------------------------------------------
test('new campaigns default to the Remastered preset; Classic is selectable', () => {
  const remastered = call('newCampaign', { faction: 1, seed: 5150, era: 1, ruleset: 'reference' });
  assert.equal(remastered.meta.rules.preset, 'remastered');
  assert.ok(remastered.meta.rules.galaxyFeatures && remastered.meta.rules.curatedEvents && remastered.meta.rules.alternativeVictories);
  assert.equal(remastered.victory.conditions.length, 4);
  const classic = call('newCampaign', { faction: 1, seed: 5150, era: 1, ruleset: 'reference', preset: 'classic' });
  assert.equal(classic.meta.rules.preset, 'classic');
  assert.equal(classic.meta.rules.galaxyFeatures, false);
  assert.equal(classic.features.length, 0);
  assert.equal(attempt('newCampaign', { preset: 'modern' }).ok, false);
});

test('minor-race roster: new campaigns place roster minors; profiles reach diplomacy, empire and the database', () => {
  const enc = call('encyclopedia');
  assert.equal(enc.minorRaces.length, 30);
  assert.ok(enc.minorRaces.every((r) => r.temperament && r.bonus && r.description && r.homeworld));
  const v = call('newCampaign', { faction: 1, seed: 424242, era: 1, ruleset: 'reference' });
  assert.ok(v.empire.memberBonuses && v.empire.memberBonuses.members === 0);
  // Play until a minor is contacted, then its roster profile is in the diplomacy view.
  let d = call('diplomacy');
  for (let i = 0; i < 30 && !d.minors.length; i++) { endTurn(); d = call('diplomacy'); }
  if (d.minors.length) {
    const m = d.minors[0];
    assert.ok(m.race && m.race.temperament && m.race.bonus, 'contacted minors carry their roster profile');
    const sys = call('view').systems.find((s) => s.id === m.systemId);
    assert.ok(sys.minor.temperament);
  }
});

test('frontier drill: build an outpost, upgrade a station, resolve an event, chronicle briefing', () => {
  let v = call('newFrontierDrill');
  assert.equal(v.meta.scenario, 'frontier');
  assert.ok(v.features.length > 0, 'charted anomalies are visible');
  assert.ok(v.features.every((f) => f.effect.length > 0));
  const wormhole = v.features.find((f) => f.type === 'Wormhole');
  if (wormhole) assert.ok(wormhole.exit, 'charted wormholes expose their exit');
  const storm = v.features.find((f) => f.type === 'IonStorm');
  if (storm) assert.equal(storm.cells.length >= 1, true);
  // Stations: an existing outpost with funding options dry-run through the core.
  const outpost = v.stations.find((s) => s.mine);
  assert.ok(outpost && outpost.type === 'Outpost' && outpost.supplyRadius === 3);
  const detail = call('station', { stationId: outpost.id });
  assert.ok(detail.funding.length >= 1);
  const funder = detail.funding.find((f) => !f.blocker);
  assert.ok(funder, 'a colony can fund the starbase');
  call('submit', { command: { type: 'UpgradeStation', stationId: outpost.id, systemId: funder.systemId } });
  // Outpost assembly by the troop transport.
  const transport = v.fleets.find((f) => f.shipClass === 'Transport');
  assert.equal(call('dryRun', { command: { type: 'BuildOutpost', fleetId: transport.id } }).ok, true);
  call('submit', { command: { type: 'BuildOutpost', fleetId: transport.id } });
  // A pending decision on a galactic event.
  const event = v.events.find((e) => e.awaitingChoice);
  assert.ok(event && event.choices.length >= 2 && event.deadline >= v.meta.turn);
  call('submit', { command: { type: 'ResolveEventChoice', eventId: event.id, eventOption: 0 } });
  v = call('view');
  assert.ok(v.fleets.find((f) => f.id === transport.id).buildingOutpost);
  assert.equal(v.events.find((e) => e.id === event.id).drafted, 0);
  assert.ok(v.stations.find((s) => s.id === outpost.id).upgrade, 'upgrade drafted');
  for (const kind of ['BuildOutpost', 'UpgradeStation', 'ResolveEventChoice'])
    assert.ok(v.pending.some((p) => p.type === kind && !/^[A-Z][a-zA-Z]+$/.test(p.summary)), `${kind} has a readable summary`);
  const briefings = [];
  for (let i = 0; i < v.outpostTurns + 1; i++) briefings.push(endTurn());
  v = call('view');
  assert.ok(v.stations.filter((s) => s.mine).length >= 2, 'the outpost was assembled');
  assert.ok(!v.fleets.some((f) => f.id === transport.id), 'the transport was consumed');
  assert.ok(v.events.find((e) => e.id === event.id)?.choice !== -1 || !v.events.some((e) => e.id === event.id));
  const all = briefings.flatMap((b) => b.events);
  assert.ok(all.some((e) => e.category === 'stations' && e.stationId), 'chronicle files stations with a reference');
  assert.ok(all.some((e) => e.category === 'events' && e.eventId), 'chronicle files galactic events');
  // Save/load keeps the gameplay layer.
  const save = call('save');
  const reloaded = call('load', { data: save.data });
  assert.equal(reloaded.stations.length, v.stations.length);
});

test('build templates: define, apply, set default, delete', () => {
  let v = call('newCampaign', { faction: 1, seed: 424242, era: 1, ruleset: 'reference' });
  const home = v.colonies[0].systemId;
  call('submit', { command: { type: 'DefineBuildTemplate', templateId: 0, templateName: 'Core worlds', templateItems: [{ product: 'Farm' }, { product: 'Factory' }] } });
  v = call('view');
  const t = v.templates.find((x) => x.name === 'Core worlds');
  assert.ok(t && t.items.length === 2);
  const detail = call('colony', { systemId: home });
  assert.ok(detail.templates.some((x) => x.id === t.id));
  const queued = detail.queue.length;
  call('submit', { command: { type: 'ApplyBuildTemplate', templateId: t.id, systemId: home } });
  assert.ok(call('colony', { systemId: home }).queue.length > queued, 'template items queued');
  call('submit', { command: { type: 'SetDefaultBuildTemplate', templateId: t.id } });
  assert.ok(call('view').templates.find((x) => x.id === t.id).isDefault);
  call('submit', { command: { type: 'DeleteBuildTemplate', templateId: t.id } });
  assert.ok(!call('view').templates.some((x) => x.id === t.id));
  const mega = call('colony', { systemId: home }).megaproject;
  assert.match(mega.startBlocker, /turn|level/);
});

test('station drill: the starbase fights as an immobile combatant', () => {
  call('newBattleDrill', { station: true });
  const status = call('endTurn');
  assert.equal(status.status, 'battle');
  const e = call('tactical');
  const base = e.ships.find((s) => s.station);
  assert.ok(base && base.immobile && base.id >= 2000000 && base.mine);
  const at = { ...base.position };
  call('tacticalRound');
  const after = call('tactical').ships.find((s) => s.id === base.id);
  if (after.alive) assert.deepEqual(after.position, at, 'stations never move');
});

test('expanded ship roster: database, build options and typed weapon mounts', () => {
  const enc = call('encyclopedia');
  const expanded = enc.ships.filter((s) => s.id >= 1000);
  assert.equal(expanded.length, 98);
  assert.ok(enc.ships.filter((s) => s.id < 1000).every((s) => s.roster === null), 'legacy records carry roster: null');
  for (const s of expanded) {
    const r = s.roster;
    assert.ok(r && /^ship\.[a-z0-9]+\./.test(r.assetKey), `asset key for ${s.id}`);
    assert.ok(['Canon', 'ModDerived', 'Kelvin'].includes(r.sourceKind));
    assert.ok(typeof r.era === 'string' && typeof r.hullRole === 'string' && typeof r.faction === 'string');
    for (const m of r.weapons) {
      assert.equal(m.key, `weapon.${m.type}`);
      assert.ok(m.count > 0 && ['Forward', 'ForwardWide', 'Broadside', 'Aft', 'All'].includes(m.arc));
    }
  }
  const akira = expanded.find((s) => s.designClass === 'Akira');
  assert.equal(akira.roster.assetKey, 'ship.fed.akira');
  assert.equal(akira.roster.era, 'DominionWar');
  assert.ok(akira.roster.weapons.some((m) => m.type === 'PhotonTorpedo' && m.torpedo));
  assert.ok(expanded.some((s) => s.roster.sourceKind === 'ModDerived') && expanded.some((s) => s.roster.sourceKind === 'Kelvin'));

  // Remastered build options list buildable expanded classes with their roster detail; Classic never does.
  const v = call('newCampaign', { faction: 1, seed: 424242, era: 1, ruleset: 'reference' });
  const choices = call('colony', { systemId: v.colonies[0].systemId }).choices;
  const rosterChoices = choices.filter((c) => c.referenceId >= 1000);
  assert.ok(rosterChoices.length > 0, 'expanded classes are offered');
  assert.ok(rosterChoices.every((c) => c.roster && c.roster.faction === 'Federation' && c.roster.buildable));
  const nx = rosterChoices.find((c) => c.designClass === 'NX');
  assert.ok(nx && nx.blocker === '', 'the tier-0 NX is buildable from the start');
  call('submit', { command: { type: 'EnqueueBuilding', systemId: v.colonies[0].systemId, building: nx.product, referenceId: nx.referenceId } });
  const c = call('newCampaign', { faction: 1, seed: 424242, era: 1, ruleset: 'reference', preset: 'classic' });
  assert.ok(!call('colony', { systemId: c.colonies[0].systemId }).choices.some((x) => x.referenceId >= 1000));
});

test('presets: the registry, a stacked game, a save that carries its packs, and the plain pack coming back', () => {
  const list = call('listPresets');
  assert.deepEqual(list.map((p) => p.id), ['classic', 'remastered', 'dominion-war', 'ages', 'galaxies', 'balance-of-power', 'ultimate']);
  assert.equal(call('meta').presets.length, list.length, 'meta carries the presets');
  assert.ok(list.every((p) => p.blurb && p.options && typeof p.lineupPlayable === 'boolean'));
  assert.equal(list.find((p) => p.id === 'galaxies').lineupPlayable, true, 'Galaxies seats the Dominion and the Borg (MOD-2)');
  assert.equal(list.find((p) => p.id === 'dominion-war').lineupPlayable, true);
  assert.equal(list.find((p) => p.id === 'ultimate').lineupPlayable, true);
  assert.equal(attempt('newCampaign', { preset: 'no-such-preset', faction: 1, empires: 2, seed: 1 }).ok, false);
  const camp = { faction: 1, empires: 3, seed: 7, era: 1, ruleset: 'reference', width: 16, height: 13, systems: 30, preset: 'ultimate' };
  const v = call('newCampaign', camp);
  assert.equal(v.meta.rules.preset, 'ultimate');
  const info = call('contentInfo');
  assert.equal(info.id, 'botf-base+ultimate');
  assert.equal(info.stack.length, 1);
  for (let i = 0; i < 3; i++) endTurn();
  const fp = call('fingerprint');
  const save = call('save');
  assert.ok(String(save.data).includes('PACKS ultimate 1 ultimate '), 'the save records its packs');
  call('loadBaseContent');
  assert.equal(call('contentInfo').base, true);
  call('load', { data: save.data });
  assert.equal(call('contentInfo').id, 'botf-base+ultimate', 'load rebuilds the stack');
  assert.equal(call('fingerprint'), fp, 'the loaded game has the saved fingerprint');
  call('newCampaign', { ...camp, preset: 'remastered' });
  assert.equal(call('contentInfo').base, true, 'a built-in preset restores the base pack');
  const reg = call('registerOverlay', { bytes: '{"format":1,"kind":"overlay","id":"web-test","name":"t","revision":1,"requires":["botf-base"]}' });
  assert.equal(reg.id, 'web-test');
  assert.equal(attempt('registerOverlay', { bytes: '{"format":1,"kind":"overlay","id":"web-test","name":"t2","revision":1,"requires":["botf-base"]}' }).ok, false, 'same id, other bytes');
});

test('Dominion War (MOD-3): race structures in the build list and on the colony, minors, a save that carries them', () => {
  const camp = { faction: 6, seed: 424242, era: 3, ruleset: 'reference', width: 22, height: 16, systems: 48, preset: 'dominion-war',
    lineup: ['federation', 'klingon', 'romulan', 'cardassian', 'dominion'], minorDensity: 'many' };
  const v = call('newCampaign', camp);
  assert.equal(v.meta.playerName, 'Dominion');
  assert.equal(call('contentInfo').id, 'botf-base+dominion+dominion-war');
  const home = v.colonies[0].systemId;
  const col = call('colony', { systemId: home });
  assert.deepEqual(col.raceStructures.map((x) => x.name), ['Vorta Administrative Center', 'Ketracel-White Facility']);
  const rung = col.choices.filter((x) => x.product === 'Structure');
  assert.equal(rung.length, 10, 'ten Dominion structures are listed');
  assert.ok(rung.every((x) => x.referenceId >= 100000 && x.isShip === false && x.effect !== undefined));
  assert.equal(rung.find((x) => x.name === 'Vorta Administrative Center').blocker, 'Already built here');
  assert.ok(rung.find((x) => x.name === 'Changeling Shapeshifter Network').blocker.length > 0, 'the network is far up the ladder');
  assert.ok(v.minors.length > 0 && !v.minors.some((m) => m.name === 'Edo'), 'the Edo are replaced');
  assert.ok(v.minors.some((m) => m.name === 'Bajorans'), 'Bajor is always on the map');
  for (let i = 0; i < 3; i++) endTurn();
  const fp = call('fingerprint');
  const save = call('save');
  assert.ok(String(save.data).includes('
STRUCTURES 1
'), 'the save carries the structures');
  call('loadBaseContent');
  call('load', { data: save.data });
  assert.equal(call('fingerprint'), fp, 'the loaded game has the saved fingerprint');
  const fed = call('newCampaign', { ...camp, faction: 1 });
  const fedCol = call('colony', { systemId: fed.colonies[0].systemId });
  assert.ok(!fedCol.choices.some((x) => x.product === 'Structure'), 'the Federation is offered no Dominion structure');
  call('newCampaign', { faction: 1, seed: 1, preset: 'remastered' });
  assert.equal(call('contentInfo').base, true, 'a built-in preset restores the base pack');
});

test('tactical rules 3: rounds carry the core combat event stream and ships their mounts', () => {
  call('newBattleDrill', { station: true });   // Remastered preset -> flight model (rules 3)
  assert.equal(call('endTurn').status, 'battle');
  let e = call('tactical');
  assert.equal(e.rules, 3);
  assert.equal(e.ticksPerRound, 12);
  for (const s of e.ships.filter((x) => !x.hidden && !x.station)) {
    assert.ok(Array.isArray(s.weapons) && s.weapons.length > 0, `mounts for ${s.name}`);
    for (const m of s.weapons) assert.equal(m.key, `weapon.${m.type}`);
  }
  let fire = null;
  for (let i = 0; i < 12 && !fire && !e.finished; i++) {
    e = call('tacticalRound');
    const r = e.rounds[e.rounds.length - 1];
    assert.ok(r.events.length > 0, 'every rules 3 round has events');
    let tick = -1;
    for (const ev of r.events) { assert.ok(ev.tick >= tick && ev.tick < 12, 'ordered by tick'); tick = ev.tick; }
    assert.ok(r.events.some((ev) => ev.kind === 'ManeuverStart'));
    fire = r.events.find((ev) => ev.kind === 'Fire') ?? null;
  }
  assert.ok(fire, 'weapons fire within a few rounds');
  assert.equal(fire.key, `weapon.${fire.weapon}`);
  assert.ok(['beam', 'pulse', 'torpedo'].includes(fire.family));
  assert.ok(fire.impactTick >= fire.tick && typeof fire.hit === 'boolean' && fire.projectiles >= 1);
  const shooter = e.ships.find((s) => s.id === fire.fleetId);
  assert.equal(shooter.weapons[fire.mountIndex].type, fire.weapon, 'mountIndex indexes the tactical mounts');

  // Rules 2 (the classic drill) has no stream: clients fall back to deriving from salvos.
  call('newBattleDrill', {});
  call('endTurn');
  const r2 = call('tacticalRound');
  assert.equal(r2.rules, 2);
  assert.deepEqual(r2.rounds[0].events, []);
});

// Empire slots (schema 17): a lineup of race keys seats Dominion and Borg as majors; the preview marks their capitals by id.
test('a lineup seats Dominion and Borg as majors and the preview shows their capitals', () => {
  const args = { faction: 6, lineup: ['federation', 'dominion', 'borg'], seed: 424242, era: 1, ruleset: 'reference', width: 22, height: 16, systems: 50 };
  const v = call('newCampaign', args);
  assert.equal(v.meta.player, 6);
  assert.deepEqual(v.empires.map((e) => e.empire).sort(), [1, 7]);
  const kinds = new Set(call('galaxyPreview', args).stars.map((s) => s[2]));
  assert.ok(kinds.has(1) && kinds.has(6) && kinds.has(7) && kinds.has(9), 'capitals by empire id, 9 for a minor race');
  const meta = call('meta');
  assert.deepEqual(meta.empires.map((e) => e.key), ['federation', 'klingon', 'romulan', 'cardassian', 'ferengi', 'dominion', 'borg', 'breen']);
  const bad = attempt('newCampaign', { faction: 1, lineup: ['federation', 'gorn'] });
  assert.equal(bad.ok, false);
});

console.log(`${passed} bridge tests passed`);
