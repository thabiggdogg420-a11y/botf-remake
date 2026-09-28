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
  assert.equal(meta.empires.length, 5);
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
  // Only surveyed systems are exposed.
  for (const system of view.systems) assert.notEqual(view.fog[system.y * view.meta.width + system.x], '0');
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

console.log(`${passed} bridge tests passed`);
