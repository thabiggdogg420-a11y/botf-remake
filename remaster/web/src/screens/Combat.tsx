import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { BattleReport, Encounter, HailPreview, TacticalShip } from '../core/types';
import { encounter, endTurn, goto, reportError, toast, view } from '../state/store';
import { reducedMotion } from '../state/settings';
import { blip } from '../ui/audio';
import { Badge, Emblem, Empty, Frame, Meter, Pill, tip } from '../ui/components';
import { CombatArena, frameAt, roundLog } from './CombatArena';

// Tactical rules 2 (remaster/docs/COMBAT-RULES.md). Percentages are the core's; these hints only explain them.
const ATTACK = 'Attack order: a cloaked ship decloaks and fires a +25% ambush volley.';
export const MANEUVERS: { key: string; name: string; hint: string }[] = [
  { key: 'Engage', name: 'Engage', hint: `Advance to optimal weapon range and fire.\n${ATTACK}` },
  { key: 'Assault', name: 'Assault', hint: `Disciplined advance to the medium beam/torpedo envelope.\n${ATTACK}` },
  { key: 'Charge', name: 'Charge', hint: `Close to point-blank range: +20% beam damage, but +15% incoming.\n${ATTACK}` },
  { key: 'Strafe', name: 'Strafe', hint: `Fast close pass: +25% beam, −20% torpedo, +10% incoming.\n${ATTACK}` },
  { key: 'Flyby', name: 'Flyby', hint: `Fast pass at medium range: −15% incoming.\n${ATTACK}` },
  { key: 'Harry', name: 'Harry', hint: `Close evasive circling: −10% beam, −20% incoming.\n${ATTACK}` },
  { key: 'Circle', name: 'Circle', hint: 'Stand-off orbit: −15% beam, −25% incoming. Keeps a cloak (cloaked ships cannot fire).' },
  { key: 'Evade', name: 'Evade', hint: 'Long-range evasion: −30% fire, −35% incoming. Keeps a cloak.' },
  { key: 'Hold', name: 'Hold', hint: 'Brace in place: −20% incoming damage. Shields do not recharge. Keeps a cloak.' },
  { key: 'Ram', name: 'Ram', hint: 'Collision: the target takes the rammer’s hull as damage and the rammer takes the target’s hull + shields as recoil. Usually both ships die.' },
  { key: 'Hail', name: 'Hail', hint: 'Offer a truce. If every one of our active ships hails and the enemy accepts, the battle ends before any fire.' },
  { key: 'Retreat', name: 'Retreat', hint: 'Disengage: one round at −25% incoming with weapons offline, then escape if still retreating. Needs a safe supplied sector; any other order aborts it.' },
];
const MANEUVER_NAME: Record<string, string> = Object.fromEntries(MANEUVERS.map((m) => [m.key, m.name]));

export function CombatScreen() {
  const v = view.value!;
  const live = encounter.value;
  if (live && v.session.battlePending) return <LiveBattle e={live} />;
  return <BattleArchive />;
}

const active = (s: TacticalShip) => s.alive && s.active && !s.retreated;
const shortName = (s: TacticalShip | undefined) => (s ? (s.hidden ? 'Cloaked contact' : s.name.split(' — ')[0]!) : 'nearest enemy');

function sourceLabel(src: string | undefined): string {
  return src === 'ai' ? 'AI' : src === 'standing' ? 'standing' : src === 'order' ? 'ordered' : '';
}

/** "If all our ships hail…" from the core's Encounter::aiAcceptsHail. */
function hailOutlook(e: Encounter, foeName: string): { text: string; tone: 'good' | 'alert' } {
  const h: HailPreview = e.hail;
  const why = h.atWar
    ? `formally at war · trust ${h.trust} · their strength ${h.theirStrength} vs ours ${h.ourStrength}`
    : 'not formally at war';
  return h.accepts
    ? { text: `If every one of our ships hails, the ${foeName} would accept a truce (${why}).`, tone: 'good' }
    : { text: `The ${foeName} would refuse a truce unless they hail too (${why}). They accept when strength × 100 ≤ ours × (100 + trust).`, tone: 'alert' };
}

function LiveBattle({ e }: { e: Encounter }) {
  const mine = e.ships.filter((s) => s.mine);
  const foes = e.ships.filter((s) => !s.mine);
  const [ship, setShip] = useState<number | null>(mine.find(active)?.id ?? null);
  const [target, setTarget] = useState<number | null>(foes.find((s) => active(s) && !s.hidden)?.id ?? null);
  const [scope, setScope] = useState<'all' | 'group' | 'ship'>('all');
  const [choice, setChoice] = useState(e.round === 0);
  useEffect(() => {
    if (ship !== null && !mine.find((s) => s.id === ship && active(s))) setShip(mine.find(active)?.id ?? null);
    if (target !== null && !foes.find((s) => s.id === target && active(s) && !s.hidden)) setTarget(foes.find((s) => active(s) && !s.hidden)?.id ?? null);
  }, [e]);
  const foeName = foes[0]?.ownerName ?? 'enemy';
  const pick = (id: number) => { const s = e.ships.find((x) => x.id === id); if (!s || s.hidden) return; if (s.mine) { setShip(id); if (scope === 'all') setScope('ship'); } else setTarget(id); blip('select'); };
  const selected = e.ships.find((s) => s.id === ship);
  const order = (maneuver: string) => {
    try {
      const t = maneuver === 'Retreat' || maneuver === 'Hail' ? 0 : target ?? 0;
      const n = core.tacticalOrder(scope === 'all' ? { all: true, maneuver, targetFleetId: t }
        : scope === 'group' ? { groupId: selected?.groupId ?? 0, maneuver, targetFleetId: t }
        : { fleetId: ship ?? 0, maneuver, targetFleetId: t });
      encounter.value = core.tactical();
      toast(`${MANEUVER_NAME[maneuver] ?? maneuver} ordered for ${n} ship${n === 1 ? '' : 's'}${t ? `, target #${t}` : ''}.`, 'info', 1800);
      blip('confirm');
    } catch (error) { reportError(error); }
  };
  const round = () => { try { encounter.value = core.tacticalRound(); setChoice(false); blip('fire'); } catch (error) { reportError(error); } };
  const auto = () => { try { encounter.value = core.tacticalAuto(); setChoice(false); blip('fire'); } catch (error) { reportError(error); } };
  const hail = () => { try { core.tacticalOrder({ all: true, maneuver: 'Hail' }); encounter.value = core.tacticalRound(); setChoice(false); } catch (error) { reportError(error); } };
  const lastRound = e.rounds[e.rounds.length - 1];
  const outlook = hailOutlook(e, foeName);
  const scopeLabel = scope === 'all' ? 'all active ships' : scope === 'group' ? (selected?.group ? `${selected.group} group` : 'group') : selected ? shortName(selected) : 'select a ship';
  return (
    <div class="screen combat-screen" data-testid="screen-combat">
      <header class="combat-head">
        <div class="ch-title"><span class="red-alert" aria-hidden="true" />
          <div><h1>{e.finished ? (lastRound?.truce ? 'Truce agreed' : 'Engagement concluded') : 'Red alert'}</h1>
            <div class="sub">{e.location} · sector {e.sectorX}.{e.sectorY} · round {e.round}{e.finished ? '' : ` of up to ${e.maxRounds}`}</div></div>
        </div>
        <div class="ch-forces">
          <Force ships={mine} label="Our forces" />
          <span class="vs">VS</span>
          <Force ships={foes} label={foeName} />
        </div>
        <div class="ch-actions">
          {e.finished ? (
            <Pill tone="primary" onClick={() => endTurn()} testid="continue-turn" kbd="Enter">Continue turn</Pill>
          ) : (
            <>
              <Pill tone="secondary" onClick={round} testid="resolve-round" hint="Resolve one round: every ship executes its order (or the AI decides)">Resolve round</Pill>
              <Pill tone="alert" onClick={auto} testid="auto-resolve" hint="Let your captains fight the battle to its end">Auto-resolve</Pill>
            </>
          )}
        </div>
      </header>
      <div class="combat-grid">
        <ShipList title="Our ships" ships={mine} all={e.ships} selected={ship} onPick={pick} />
        <div class="arena-wrap">
          <CombatArena encounter={e} round={e.rounds.length} selected={ship} target={target} onPick={pick} animateKey={`live-${e.turn}-${e.sectorX}.${e.sectorY}`} intent />
          {choice && !e.finished ? (
            <div class="encounter-choice" data-testid="encounter-choice">
              <Frame title="Enemy contact" tone="alert">
                <p>{mine.length} of our ships meet {foes.length} {foeName} ship{foes.length === 1 ? '' : 's'} at {e.location}. The strategic turn is paused until the battle is decided.</p>
                {foes.some((s) => s.hidden) ? <p class="hint">Sensors read {foes.filter((s) => s.hidden).length} cloaked contact{foes.filter((s) => s.hidden).length === 1 ? '' : 's'}: they must decloak to fire, and their first volley gains a +25% ambush bonus.</p> : null}
                <p class={`hail-outlook tone-${outlook.tone}`} data-testid="hail-outlook">{outlook.text}</p>
                <div class="modal-actions">
                  <Pill tone="alert" onClick={() => setChoice(false)} testid="choice-fight" hint="Command your ships round by round">Take command</Pill>
                  <Pill tone="secondary" onClick={auto} testid="choice-auto" hint="Your ships fight under automatic command">Auto-resolve</Pill>
                  <Pill tone="ghost" onClick={hail} testid="choice-hail" hint="Every ship hails and one round resolves">Hail</Pill>
                </div>
              </Frame>
            </div>
          ) : null}
          <div class="arena-legend" aria-hidden="true">
            <span><i class="al al-ring" />hull / shields</span><span><i class="al al-intent" />next order</span><span><i class="al al-ai" />AI decision</span>
          </div>
        </div>
        <ShipList title="Hostiles" ships={foes} all={e.ships} selected={target} onPick={pick} hostile />
      </div>
      <div class="combat-bottom">
        <Frame title="Orders" sub={scopeLabel} class="orders-frame" testid="tactical-orders"
          actions={
            <div class="scope-toggle" role="radiogroup" aria-label="Order scope">
              {([['all', 'All ships', 'Every active ship'], ['group', 'Group', 'The selected ship’s like-class tactical group'], ['ship', 'Ship', 'Only the selected ship']] as const).map(([k, label, hint]) => (
                <Pill key={k} small tone="ghost" active={scope === k} onClick={() => setScope(k)} hint={hint} testid={`scope-${k}`}
                  disabled={k !== 'all' && (!selected || !active(selected))} reason="Select one of your active ships">{label}</Pill>
              ))}
            </div>
          }>
          <div class="maneuvers">
            {MANEUVERS.map((mv) => {
              const current = scope === 'ship' && selected?.maneuver === mv.key;
              const cannot = e.finished ? 'The battle is over'
                : mv.key === 'Retreat' && scope === 'ship' && selected && !selected.canRetreat ? 'No safe supplied withdrawal sector'
                : scope !== 'all' && (!selected || !active(selected)) ? 'Select one of your active ships' : undefined;
              return <Pill key={mv.key} small tone={mv.key === 'Retreat' || mv.key === 'Ram' ? 'alert' : mv.key === 'Hail' ? 'ghost' : 'secondary'} active={current}
                disabled={!!cannot} reason={cannot} hint={`${mv.hint}${target && mv.key !== 'Retreat' && mv.key !== 'Hail' ? `\nTarget: ${shortName(e.ships.find((s) => s.id === target))} #${target}` : ''}`}
                testid={`maneuver-${mv.key}`} onClick={() => order(mv.key)}>{mv.name}</Pill>;
            })}
          </div>
          <p class={`hail-outlook small tone-${outlook.tone}`}>{outlook.text}</p>
          {selected?.cloaked && scope !== 'all' ? <p class="hint">{shortName(selected)} is cloaked: it cannot fire while cloaked. An attack order decloaks it with a +25% ambush volley.</p> : null}
        </Frame>
        <Frame title="Combat log" sub={lastRound ? `round ${lastRound.index}` : 'awaiting first round'} class="log-frame">
          <ul class="combat-log" data-testid="combat-log">
            {lastRound ? roundLog(e, lastRound).map((l, i) => <li key={i} class={`log-${l.kind}`}>{l.text}</li>) : <li class="muted">Ships deploy at opposite edges of the arena. Dashed lines show what each ship will do next round.</li>}
          </ul>
        </Frame>
      </div>
    </div>
  );
}

function Force({ ships, label }: { ships: TacticalShip[]; label: string }) {
  const alive = ships.filter((s) => s.alive && !s.retreated).length;
  const known = ships.filter((s) => !s.hidden);
  const hull = known.reduce((s, x) => s + Math.max(0, x.hull), 0), max = known.reduce((s, x) => s + x.maxHull, 0);
  return (
    <div class="force" {...tip(`${alive} of ${ships.length} ships still fighting${ships.some((s) => s.hidden) ? ' (cloaked contacts not shown in the hull total)' : ''}`)}>
      <Emblem empire={ships[0]?.owner ?? 0} size={26} />
      <div><div class="force-name">{label}</div><Meter value={hull} max={max} tone="hull" height={5} /><div class="force-count">{alive}/{ships.length} ships</div></div>
    </div>
  );
}

function ShipList({ title, ships, all, selected, onPick, hostile }: {
  title: string; ships: TacticalShip[]; all: TacticalShip[]; selected: number | null; onPick: (id: number) => void; hostile?: boolean;
}) {
  const nameOf = (id: number) => { const s = all.find((x) => x.id === id); return s ? `${shortName(s)} #${id}` : `#${id}`; };
  return (
    <div class={`ship-list${hostile ? ' hostile' : ''}`}>
      <h3>{title}</h3>
      {ships.map((s) => {
        const next = s.next;
        const status = !s.alive ? 'DESTROYED' : s.retreated ? 'WITHDRAWN' : null;
        return (
          <button type="button" key={s.id} class={`ship-card${selected === s.id ? ' active' : ''}${!s.alive ? ' dead' : s.retreated ? ' gone' : ''}`}
            onClick={() => onPick(s.id)} disabled={s.hidden} data-testid={`ship-${s.id}`}>
            <div class="sc-head"><Emblem empire={s.owner} size={14} /><span class="sc-name">{shortName(s)}</span><span class="sc-id">#{s.id}</span></div>
            {s.hidden ? <div class="sc-state"><span class="sc-flag cloak">Cloaked</span><span>decloaks to fire</span></div> : (
              <>
                <div class="sc-bars">
                  <Meter value={s.shields} max={s.maxShields} tone="shield" height={3} />
                  <Meter value={s.hull} max={s.maxHull} tone="hull" height={4} />
                </div>
                <div class="sc-state">
                  {status ? <span>{status}</span> : next ? (
                    <span class="sc-next" data-testid={`next-${s.id}`}>
                      {hostile ? 'will ' : 'next: '}<b>{MANEUVER_NAME[next.maneuver] ?? next.maneuver}</b>{next.target ? ` → ${nameOf(next.target)}` : ''}
                    </span>
                  ) : <span>beam {s.beam} · torp {s.torpedo}</span>}
                  {next && !status ? <span class={`sc-src src-${next.source}`}>{sourceLabel(next.source)}</span> : null}
                </div>
                <div class="sc-flags">
                  {s.cloaked && s.alive && !s.retreated ? <span class="sc-flag cloak">Cloaked</span> : null}
                  {s.disengaging && s.next ? <span class="sc-flag disengage" {...tip('Weapons offline, −25% incoming. Escapes at the next movement phase if still retreating.')}>Disengaging</span> : null}
                  {s.group ? <span class="sc-group">{s.group}</span> : null}
                  {!hostile && s.alive && !s.retreated ? <span class="sc-weap">B{s.beam} · T{s.torpedo}</span> : null}
                </div>
              </>
            )}
          </button>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
function BattleArchive() {
  const v = view.value!;
  const replays = useMemo(() => { try { return core.replays(); } catch { return []; } }, [v.meta.version]);
  const [sel, setSel] = useState<{ kind: 'replay' | 'report'; index: number } | null>(replays.length ? { kind: 'replay', index: replays.length - 1 } : v.battles.length ? { kind: 'report', index: v.battles[v.battles.length - 1]!.index } : null);
  return (
    <div class="screen pad archive-screen" data-testid="screen-combat">
      <div class="archive-grid">
        <Frame title="Battle records" sub={`${v.battles.length} recent`} class="fill">
          {!replays.length && !v.battles.length ? <Empty>No battles fought yet. Declare war and engage an enemy to see tactical combat, or try the Tactical Drill from the title screen.</Empty> : null}
          {replays.length ? <h4 class="list-head">Last turn — full replays</h4> : null}
          {replays.map((r) => (
            <button type="button" key={`r${r.index}`} class={`battle-row${sel?.kind === 'replay' && sel.index === r.index ? ' active' : ''}`} onClick={() => setSel({ kind: 'replay', index: r.index })} data-testid={`replay-${r.index}`}>
              <span class="br-title">Turn {r.turn} · sector {r.x}.{r.y}</span>
              <span class="br-sub">{r.rounds} rounds · {r.winner === v.meta.player ? 'victory' : r.winner ? 'defeat' : 'no victor'}</span>
            </button>
          ))}
          {v.battles.length ? <h4 class="list-head">Reports</h4> : null}
          {[...v.battles].reverse().map((b) => (
            <button type="button" key={`b${b.index}`} class={`battle-row${sel?.kind === 'report' && sel.index === b.index ? ' active' : ''}`} onClick={() => setSel({ kind: 'report', index: b.index })}>
              <span class="br-title"><Emblem empire={b.first} size={12} /> vs <Emblem empire={b.second} size={12} /> Turn {b.turn} · {b.x}.{b.y}</span>
              <span class="br-sub">{b.rounds} rounds · {b.lost} lost · {b.winner === v.meta.player ? 'victory' : b.winner ? `${v.empires.find((e) => e.empire === b.winner)?.name ?? 'enemy'} won` : 'no victor'}</span>
            </button>
          ))}
          <Pill tone="ghost" small onClick={() => goto('galaxy')}>Back to chart</Pill>
        </Frame>
        {sel?.kind === 'replay' ? <Replay key={`r${sel.index}`} index={sel.index} /> : sel?.kind === 'report' ? <Report key={`b${sel.index}`} index={sel.index} /> : <Frame title="Replay"><Empty>Select a battle.</Empty></Frame>}
      </div>
    </div>
  );
}

function Replay({ index }: { index: number }) {
  const e = useMemo<Encounter | null>(() => { try { return core.replay(index); } catch (err) { reportError(err); return null; } }, [index]);
  const [k, setK] = useState(0);
  const [playing, setPlaying] = useState(false);
  useEffect(() => {
    if (!playing || !e) return;
    const id = setInterval(() => setK((x) => { if (x >= e.rounds.length) { setPlaying(false); return x; } return x + 1; }), reducedMotion.value ? 900 : 2500);
    return () => clearInterval(id);
  }, [playing, e]);
  if (!e) return <Frame title="Replay"><Empty>Replay unavailable.</Empty></Frame>;
  const frame = frameAt(e, k);
  const r = k > 0 ? e.rounds[k - 1] : undefined;
  return (
    <Frame title={`Replay — ${e.location}`} sub={`round ${k} of ${e.rounds.length}`} class="fill replay-frame" testid="replay">
      <div class="replay-arena"><CombatArena encounter={e} round={k} selected={null} target={null} onPick={() => undefined} animateKey={`replay-${index}`} /></div>
      <div class="replay-controls">
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(0); }} ariaLabel="First round">⏮</Pill>
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(Math.max(0, k - 1)); }} disabled={k === 0} ariaLabel="Previous round">◀</Pill>
        <Pill small onClick={() => { if (k >= e.rounds.length) setK(0); setPlaying(!playing); }} testid="replay-play">{playing ? 'Pause' : 'Play'}</Pill>
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(Math.min(e.rounds.length, k + 1)); }} disabled={k >= e.rounds.length} testid="replay-next" ariaLabel="Next round">▶</Pill>
        <input type="range" class="slider grow" min={0} max={e.rounds.length} value={k} aria-label="Replay round" onInput={(ev) => { setPlaying(false); setK(Number((ev.target as HTMLInputElement).value)); }} />
      </div>
      <div class="replay-status">
        {e.ships.map((s) => { const f = frame.get(s.id)!; return (
          <span key={s.id} class={`rs-ship${!f.alive ? ' dead' : f.retreated ? ' gone' : ''}`}>
            <Emblem empire={s.owner} size={11} /> {shortName(s)} <Meter value={Math.max(0, f.hull)} max={s.maxHull} tone="hull" height={3} />
            {f.cloaked && f.alive && !f.retreated ? <Badge tone="dim">cloaked</Badge> : null}
            {f.disengaging ? <Badge tone="warn">disengaging</Badge> : null}
          </span>
        ); })}
      </div>
      <ul class="combat-log">{r ? roundLog(e, r).map((l, i) => <li key={i} class={`log-${l.kind}`}>{l.text}</li>) : <li class="muted">Deployment.</li>}</ul>
    </Frame>
  );
}

function Report({ index }: { index: number }) {
  const b = useMemo<BattleReport | null>(() => { try { return core.battleReport(index); } catch (err) { reportError(err); return null; } }, [index]);
  const [k, setK] = useState(0);
  if (!b) return <Frame title="Report"><Empty>Report unavailable.</Empty></Frame>;
  const hull = new Map(b.participants.map((p) => [p.id, p.hull]));
  for (let i = 0; i < k; i++) for (const d of b.rounds[i]!.damage) hull.set(d.id, d.hullAfter);
  const round = k > 0 ? b.rounds[k - 1] : undefined;
  return (
    <Frame title={`Battle at sector ${b.x}.${b.y}`} sub={`turn ${b.turn} · ${b.rounds.length} rounds`} class="fill" testid="battle-report">
      <div class="replay-controls">
        <Pill small tone="ghost" onClick={() => setK(Math.max(0, k - 1))} disabled={k === 0} ariaLabel="Previous round">◀</Pill>
        <input type="range" class="slider grow" min={0} max={b.rounds.length} value={k} aria-label="Round" onInput={(ev) => setK(Number((ev.target as HTMLInputElement).value))} />
        <Pill small tone="ghost" onClick={() => setK(Math.min(b.rounds.length, k + 1))} disabled={k >= b.rounds.length} ariaLabel="Next round">▶</Pill>
        <Badge tone="accent">round {k}</Badge>
      </div>
      <div class="report-ships">
        {b.participants.map((p) => {
          const h = hull.get(p.id) ?? 0;
          const lost = b.rounds.slice(0, k).some((r) => r.lost.includes(p.id));
          const withdrew = b.rounds.slice(0, k).some((r) => r.retreated.includes(p.id));
          return (
            <div class={`rs-row${lost ? ' dead' : ''}`} key={p.id}>
              <Emblem empire={p.owner} size={16} /><span>{p.name} #{p.id}</span>
              <Meter value={h} max={p.hull} tone="hull" height={5} /><b>{h}/{p.hull}</b>
              {lost ? <Badge tone="alert">lost</Badge> : withdrew ? <Badge tone="warn">withdrew</Badge> : null}
            </div>
          );
        })}
      </div>
      {round ? <p class="hint">Round {round.index}: {round.damage.filter((d) => d.hullAfter < d.hullBefore || d.shieldAfter < d.shieldBefore).length} ships hit{round.lost.length ? `, ${round.lost.length} destroyed` : ''}{round.retreated.length ? `, ${round.retreated.length} withdrew` : ''}.</p> : null}
    </Frame>
  );
}
