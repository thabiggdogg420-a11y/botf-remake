import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { BattleReport, Encounter, TacticalRound, TacticalShip } from '../core/types';
import { encounter, endTurn, goto, reportError, toast, view } from '../state/store';
import { blip } from '../ui/audio';
import { Badge, Emblem, Empty, Frame, Meter, Pill, tip } from '../ui/components';
import { CombatArena, frameAt } from './CombatArena';

const MANEUVERS: { key: string; name: string; hint: string }[] = [
  { key: 'Engage', name: 'Engage', hint: 'Advance to optimal weapon range and fire.' },
  { key: 'Assault', name: 'Assault', hint: 'Disciplined advance to the medium beam/torpedo envelope.' },
  { key: 'Charge', name: 'Charge', hint: 'Close at speed to point-blank range: heavier beam fire, but more exposed.' },
  { key: 'Strafe', name: 'Strafe', hint: 'High-speed passing run at close range.' },
  { key: 'Flyby', name: 'Flyby', hint: 'High-speed passing run at medium range.' },
  { key: 'Harry', name: 'Harry', hint: 'Close-range evasive circling and harassment.' },
  { key: 'Circle', name: 'Circle', hint: 'Stand-off evasive orbit: harder to hit, less accurate.' },
  { key: 'Evade', name: 'Evade', hint: 'Defensive evasion, firing only at targets of opportunity.' },
  { key: 'Hold', name: 'Hold', hint: 'Stay put and reinforce shields.' },
  { key: 'Ram', name: 'Ram', hint: 'Desperate collision attack.' },
  { key: 'Hail', name: 'Hail', hint: 'Offer mutual disengagement.' },
  { key: 'Retreat', name: 'Retreat', hint: 'Disengage toward the arena edge with weapons offline. Needs a safe supplied sector.' },
];

export function CombatScreen() {
  const v = view.value!;
  const live = encounter.value;
  if (live && v.session.battlePending) return <LiveBattle e={live} />;
  return <BattleArchive />;
}

function roundLog(e: Encounter, r: TacticalRound): string[] {
  const name = (id: number) => { const s = e.ships.find((x) => x.id === id); return s ? `${s.hidden ? 'Cloaked contact' : s.name.split(' — ')[0]} #${id}` : `#${id}`; };
  const lines: string[] = [];
  for (const st of r.ships) {
    if (st.beam + st.torpedo > 0) lines.push(`${name(st.id)} — ${st.maneuver}: ${st.beam} beam + ${st.torpedo} torpedo at ${st.target ? name(st.target) : 'nearest enemy'}`);
    if (st.decloaked) lines.push(`${name(st.id)} decloaks.`);
    if (st.hailed) lines.push(`${name(st.id)} hails the enemy.`);
  }
  for (const id of r.lost) lines.push(`${name(id)} destroyed.`);
  for (const id of r.retreatedIds) lines.push(`${name(id)} withdrew.`);
  if (r.truce) lines.push('A truce ends the engagement.');
  if (r.ended) lines.push(r.winner ? `Battle over: ${e.ships.find((s) => s.owner === r.winner)?.ownerName ?? 'victor'} hold the field.` : 'Battle over: no victor.');
  if (!lines.length) lines.push('Ships manoeuvre for position.');
  return lines;
}

function LiveBattle({ e }: { e: Encounter }) {
  const v = view.value!;
  const mine = e.ships.filter((s) => s.mine);
  const foes = e.ships.filter((s) => !s.mine);
  const active = (s: TacticalShip) => s.alive && s.active && !s.retreated;
  const [ship, setShip] = useState<number | null>(mine.find(active)?.id ?? null);
  const [target, setTarget] = useState<number | null>(foes.find((s) => active(s) && !s.hidden)?.id ?? null);
  const [all, setAll] = useState(true);
  const [choice, setChoice] = useState(e.round === 0);
  useEffect(() => {
    if (ship !== null && !mine.find((s) => s.id === ship && active(s))) setShip(mine.find(active)?.id ?? null);
    if (target !== null && !foes.find((s) => s.id === target && active(s) && !s.hidden)) setTarget(foes.find((s) => active(s) && !s.hidden)?.id ?? null);
  }, [e]);
  const locationName = e.location;
  const pick = (id: number) => { const s = e.ships.find((x) => x.id === id); if (!s || s.hidden) return; if (s.mine) setShip(id); else setTarget(id); blip('select'); };
  const order = (maneuver: string) => {
    try {
      const n = core.tacticalOrder(all ? { all: true, maneuver, targetFleetId: target ?? 0 } : { fleetId: ship ?? 0, maneuver, targetFleetId: target ?? 0 });
      encounter.value = core.tactical();
      toast(`${maneuver} ordered for ${n} ship${n === 1 ? '' : 's'}${target && maneuver !== 'Retreat' ? `, target #${target}` : ''}.`, 'info', 1800);
      blip('confirm');
    } catch (error) { reportError(error); }
  };
  const round = () => { try { encounter.value = core.tacticalRound(); blip('fire'); } catch (error) { reportError(error); } };
  const auto = () => { try { encounter.value = core.tacticalAuto(); setChoice(false); blip('fire'); } catch (error) { reportError(error); } };
  const hail = () => { try { core.tacticalOrder({ all: true, maneuver: 'Hail' }); encounter.value = core.tacticalRound(); setChoice(false); } catch (error) { reportError(error); } };
  const selected = e.ships.find((s) => s.id === ship);
  const lastRound = e.rounds[e.rounds.length - 1];
  return (
    <div class="screen combat-screen" data-testid="screen-combat">
      <header class="combat-head">
        <div class="ch-title"><span class="red-alert" aria-hidden="true" />
          <div><h1>{e.finished ? 'Engagement concluded' : 'Red alert'}</h1><div class="sub">{locationName} · sector {e.sectorX}.{e.sectorY} · round {e.round}{e.finished ? '' : ` of up to ${e.maxRounds}`}</div></div>
        </div>
        <div class="ch-forces">
          <Force ships={mine} label="Our forces" />
          <span class="vs">VS</span>
          <Force ships={foes} label={foes[0]?.ownerName ?? 'Enemy'} />
        </div>
        <div class="ch-actions">
          {e.finished ? (
            <Pill tone="primary" onClick={() => endTurn()} testid="continue-turn" kbd="Enter">Continue turn</Pill>
          ) : (
            <>
              <Pill tone="secondary" onClick={round} testid="resolve-round">Resolve round</Pill>
              <Pill tone="alert" onClick={auto} testid="auto-resolve" hint="Let your captains fight the battle to its end">Auto-resolve</Pill>
            </>
          )}
        </div>
      </header>
      <div class="combat-grid">
        <ShipList title="Our ships" ships={mine} selected={ship} onPick={pick} />
        <div class="arena-wrap">
          <CombatArena encounter={e} round={e.rounds.length} selected={ship} target={target} onPick={pick} animateKey={`live-${e.turn}`} />
          {choice && !e.finished ? (
            <div class="encounter-choice" data-testid="encounter-choice">
              <Frame title="Enemy contact" tone="alert">
                <p>{mine.length} of our ships meet {foes.length} {foes[0]?.ownerName ?? 'enemy'} ship{foes.length === 1 ? '' : 's'} at {locationName}. The strategic turn is paused until the battle is decided.</p>
                <div class="modal-actions">
                  <Pill tone="alert" onClick={() => setChoice(false)} testid="choice-fight" hint="Command your ships round by round">Take command</Pill>
                  <Pill tone="secondary" onClick={auto} testid="choice-auto" hint="Your ships fight under automatic command">Auto-resolve</Pill>
                  <Pill tone="ghost" onClick={hail} hint="Attempt a mutual disengagement">Hail</Pill>
                </div>
              </Frame>
            </div>
          ) : null}
        </div>
        <ShipList title="Hostiles" ships={foes} selected={target} onPick={pick} hostile />
      </div>
      <div class="combat-bottom">
        <Frame title="Orders" sub={all ? 'all ships' : selected ? selected.name.split(' — ')[0] : 'select a ship'} class="orders-frame" testid="tactical-orders"
          actions={<Pill small tone="ghost" active={all} onClick={() => setAll(!all)} hint="Apply the manoeuvre to every active ship">All ships</Pill>}>
          <div class="maneuvers">
            {MANEUVERS.map((mv) => {
              const current = !all && selected?.maneuver === mv.key;
              const cannot = e.finished ? 'The battle is over' : mv.key === 'Retreat' && !all && selected && !selected.canRetreat ? 'No safe supplied withdrawal sector' : !all && (!selected || !active(selected)) ? 'Select one of your active ships' : undefined;
              return <Pill key={mv.key} small tone={mv.key === 'Retreat' || mv.key === 'Ram' ? 'alert' : mv.key === 'Hail' ? 'ghost' : 'secondary'} active={current}
                disabled={!!cannot} reason={cannot} hint={`${mv.hint}${target && mv.key !== 'Retreat' && mv.key !== 'Hail' ? `\nTarget: #${target}` : ''}`} testid={`maneuver-${mv.key}`} onClick={() => order(mv.key)}>{mv.name}</Pill>;
            })}
          </div>
          <p class="hint">Click ships in the arena or lists: yours to command, theirs to target. Orders persist until changed.</p>
        </Frame>
        <Frame title="Combat log" sub={lastRound ? `round ${lastRound.index}` : 'awaiting first round'} class="log-frame">
          <ul class="combat-log" data-testid="combat-log">
            {lastRound ? roundLog(e, lastRound).map((l, i) => <li key={i}>{l}</li>) : <li class="muted">Ships deploy at opposite edges of the arena.</li>}
          </ul>
        </Frame>
      </div>
      {v.session.battleFinished || e.finished ? null : null}
    </div>
  );
}

function Force({ ships, label }: { ships: TacticalShip[]; label: string }) {
  const alive = ships.filter((s) => s.alive && !s.retreated).length;
  const hull = ships.reduce((s, x) => s + Math.max(0, x.hull), 0), max = ships.reduce((s, x) => s + x.maxHull, 0);
  return (
    <div class="force" {...tip(`${alive} of ${ships.length} ships still fighting`)}>
      <Emblem empire={ships[0]?.owner ?? 0} size={26} />
      <div><div class="force-name">{label}</div><Meter value={hull} max={max} tone="hull" height={5} /><div class="force-count">{alive}/{ships.length} ships</div></div>
    </div>
  );
}

function ShipList({ title, ships, selected, onPick, hostile }: { title: string; ships: TacticalShip[]; selected: number | null; onPick: (id: number) => void; hostile?: boolean }) {
  return (
    <div class={`ship-list${hostile ? ' hostile' : ''}`}>
      <h3>{title}</h3>
      {ships.map((s) => (
        <button type="button" key={s.id} class={`ship-card${selected === s.id ? ' active' : ''}${!s.alive ? ' dead' : s.retreated ? ' gone' : ''}`}
          onClick={() => onPick(s.id)} disabled={s.hidden} data-testid={`ship-${s.id}`}>
          <div class="sc-head"><Emblem empire={s.owner} size={14} /><span class="sc-name">{s.hidden ? 'Cloaked contact' : s.name.split(' — ')[0]}</span><span class="sc-id">#{s.id}</span></div>
          {s.hidden ? <div class="sc-state">Sensor ghost — decloaks to fire</div> : (
            <>
              <div class="sc-bars">
                <Meter value={s.shields} max={s.maxShields} tone="shield" height={3} />
                <Meter value={s.hull} max={s.maxHull} tone="hull" height={4} />
              </div>
              <div class="sc-state">
                {!s.alive ? 'DESTROYED' : s.retreated ? 'WITHDRAWN' : s.mine ? `${s.maneuver ?? 'Engage'}${s.target ? ` → #${s.target}` : ''}` : `beam ${s.beam} · torp ${s.torpedo}`}
                {s.group ? <span class="sc-group">{s.group}</span> : null}
              </div>
            </>
          )}
        </button>
      ))}
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
    const id = setInterval(() => setK((x) => { if (x >= e.rounds.length) { setPlaying(false); return x; } return x + 1; }), 1250);
    return () => clearInterval(id);
  }, [playing, e]);
  if (!e) return <Frame title="Replay"><Empty>Replay unavailable.</Empty></Frame>;
  const frame = frameAt(e, k);
  const r = k > 0 ? e.rounds[k - 1] : undefined;
  return (
    <Frame title={`Replay — ${e.location}`} sub={`round ${k} of ${e.rounds.length}`} class="fill replay-frame" testid="replay">
      <div class="replay-arena"><CombatArena encounter={e} round={k} selected={null} target={null} onPick={() => undefined} animateKey={`replay-${index}`} /></div>
      <div class="replay-controls">
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(0); }}>⏮</Pill>
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(Math.max(0, k - 1)); }} disabled={k === 0}>◀</Pill>
        <Pill small onClick={() => { if (k >= e.rounds.length) setK(0); setPlaying(!playing); }} testid="replay-play">{playing ? 'Pause' : 'Play'}</Pill>
        <Pill small tone="ghost" onClick={() => { setPlaying(false); setK(Math.min(e.rounds.length, k + 1)); }} disabled={k >= e.rounds.length} testid="replay-next">▶</Pill>
        <input type="range" class="slider grow" min={0} max={e.rounds.length} value={k} aria-label="Replay round" onInput={(ev) => { setPlaying(false); setK(Number((ev.target as HTMLInputElement).value)); }} />
      </div>
      <div class="replay-status">
        {e.ships.map((s) => { const f = frame.get(s.id)!; return (
          <span key={s.id} class={`rs-ship${!f.alive ? ' dead' : f.retreated ? ' gone' : ''}`}><Emblem empire={s.owner} size={11} /> {s.name.split(' — ')[0]} <Meter value={Math.max(0, f.hull)} max={s.maxHull} tone="hull" height={3} /></span>
        ); })}
      </div>
      <ul class="combat-log">{r ? roundLog(e, r).map((l, i) => <li key={i}>{l}</li>) : <li class="muted">Deployment.</li>}</ul>
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
        <Pill small tone="ghost" onClick={() => setK(Math.max(0, k - 1))} disabled={k === 0}>◀</Pill>
        <input type="range" class="slider grow" min={0} max={b.rounds.length} value={k} aria-label="Round" onInput={(ev) => setK(Number((ev.target as HTMLInputElement).value))} />
        <Pill small tone="ghost" onClick={() => setK(Math.min(b.rounds.length, k + 1))} disabled={k >= b.rounds.length}>▶</Pill>
        <Badge tone="accent">round {k}</Badge>
      </div>
      <div class="report-ships">
        {b.participants.map((p) => {
          const h = hull.get(p.id) ?? 0;
          const lost = b.rounds.slice(0, k).some((r) => r.lost.includes(p.id));
          return (
            <div class={`rs-row${lost ? ' dead' : ''}`} key={p.id}>
              <Emblem empire={p.owner} size={16} /><span>{p.name} #{p.id}</span>
              <Meter value={h} max={p.hull} tone="hull" height={5} /><b>{h}/{p.hull}</b>
              {lost ? <Badge tone="alert">lost</Badge> : null}
            </div>
          );
        })}
      </div>
      {round ? <p class="hint">Round {round.index}: {round.damage.filter((d) => d.hullAfter < d.hullBefore || d.shieldAfter < d.shieldBefore).length} ships hit{round.lost.length ? `, ${round.lost.length} destroyed` : ''}{round.retreated.length ? `, ${round.retreated.length} withdrew` : ''}.</p> : null}
    </Frame>
  );
}
