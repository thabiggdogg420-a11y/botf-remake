import { useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { FleetView, GameView } from '../core/types';
import { cellActions, plotCourse, replaceIds } from '../state/orders';
import { dryRun, focusMap, goto, issue, selectFleet, selection, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Meter, Pill, Stat, tip } from '../ui/components';

interface Unit { lead: FleetView; members: FleetView[]; tf: GameView['taskForces'][number] | null }

function units(v: GameView): Unit[] {
  const out: Unit[] = [];
  const seen = new Set<number>();
  for (const f of v.fleets) {
    if (f.taskForceId) {
      if (seen.has(f.taskForceId)) continue;
      seen.add(f.taskForceId);
      const tf = v.taskForces.find((t) => t.id === f.taskForceId) ?? null;
      out.push({ lead: f, members: v.fleets.filter((m) => m.taskForceId === f.taskForceId), tf });
    } else out.push({ lead: f, members: [f], tf: null });
  }
  return out;
}

export function FleetsScreen() {
  const v = view.value!;
  const list = units(v);
  const sel = selection.value;
  const current = list.find((u) => u.members.some((m) => m.id === sel.fleetId)) ?? list[0] ?? null;
  const [marks, setMarks] = useState<number[]>([]);
  const [tfName, setTfName] = useState('');
  const place = (x: number, y: number) => v.systems.find((s) => s.x === x && s.y === y)?.name ?? `Sector ${x}.${y}`;
  const markedFleets = v.fleets.filter((f) => marks.includes(f.id));
  const formCmd = { type: 'CreateTaskForce', memberFleetIds: marks, taskForceName: tfName.trim() };
  const formReason = marks.length < 2 ? 'Tick at least two ungrouped ships in the same sector' : dryRun(formCmd);
  const bySector = new Map<string, Unit[]>();
  for (const u of list) {
    const key = place(u.lead.x, u.lead.y);
    (bySector.get(key) ?? bySector.set(key, []).get(key)!).push(u);
  }
  return (
    <div class="screen pad fleets-screen" data-testid="screen-fleets">
      <div class="fleets-grid">
        <Frame title="Fleet registry" sub={`${v.fleets.length} ships · ${v.taskForces.length} task forces`} class="fill"
          actions={<Pill small tone="ghost" onClick={() => setMarks([])} disabled={!marks.length}>Clear marks</Pill>}>
          {list.length === 0 ? <Empty>You have no ships.</Empty> : (
            <div class="registry">
              {[...bySector.entries()].map(([sector, group]) => (
                <div class="reg-sector" key={sector}>
                  <div class="reg-sector-head">{sector}<span>{group.reduce((s, u) => s + u.members.length, 0)} ships</span></div>
                  {group.map((u) => {
                    const active = current === u;
                    const hull = u.members.reduce((s, m) => s + m.hull, 0), max = u.members.reduce((s, m) => s + m.maxHull, 0);
                    return (
                      <div key={u.lead.id} class={`reg-row${active ? ' active' : ''}`}>
                        {!u.tf ? (
                          <input type="checkbox" aria-label={`Mark ${u.lead.name} for a task force`} checked={marks.includes(u.lead.id)}
                            onChange={(e) => { const on = (e.target as HTMLInputElement).checked; setMarks(on ? [...marks, u.lead.id] : marks.filter((m) => m !== u.lead.id)); }} />
                        ) : <span class="tf-tag" {...tip('Task force')}>TF</span>}
                        <button type="button" class="reg-main" onClick={() => selectFleet(u.lead.id)} data-testid={`fleet-row-${u.lead.id}`}>
                          <Emblem empire={v.meta.player} size={16} />
                          <span class="reg-name">{u.tf ? u.tf.name : u.lead.name}<small>{u.tf ? `${u.members.length} ships` : `${u.lead.className} #${u.lead.id}`}</small></span>
                          <Meter value={hull} max={max} tone="hull" height={4} />
                          <span class="reg-orders">{u.lead.route.length ? `→ ${place(u.lead.targetX, u.lead.targetY)} · ${u.lead.eta}T` : u.lead.hasOrders ? 'ordered' : u.lead.terraforming ? 'terraforming' : 'idle'}</span>
                        </button>
                      </div>
                    );
                  })}
                </div>
              ))}
            </div>
          )}
          <div class="form-tf">
            <input class="text-input" placeholder="Task force name (optional)" value={tfName} maxLength={64} aria-label="Task force name"
              onInput={(e) => setTfName((e.target as HTMLInputElement).value)} />
            <Pill tone="secondary" disabled={!!formReason} reason={formReason} testid="form-task-force"
              onClick={() => { if (issue(formCmd, { success: `Task force formed from ${markedFleets.length} ships.` })) { setMarks([]); setTfName(''); } }}>
              Form task force ({marks.length})
            </Pill>
          </div>
        </Frame>
        {current ? <UnitDetail v={v} unit={current} /> : <Frame title="Unit"><Empty>No ship selected.</Empty></Frame>}
      </div>
    </div>
  );
}

function UnitDetail({ v, unit }: { v: GameView; unit: Unit }) {
  const { lead, members, tf } = unit;
  const [dest, setDest] = useState<number | ''>('');
  const destinations = v.systems.filter((s) => !(s.x === lead.x && s.y === lead.y));
  const route = useMemo(() => {
    const s = destinations.find((d) => d.id === dest);
    if (!s) return null;
    try { return { s, r: core.route(lead.id, s.x, s.y) }; } catch { return null; }
  }, [dest, lead.id, v.meta.version]);
  const actions = cellActions(lead.y * v.meta.width + lead.x).filter((a) => a.group === 'orders');
  const cancelIds = replaceIds(lead);
  const drafted = v.pending.filter((p) => cancelIds.includes(p.id));
  const hull = members.reduce((s, m) => s + m.hull, 0), maxHull = members.reduce((s, m) => s + m.maxHull, 0);
  const shields = members.reduce((s, m) => s + m.shields, 0), maxShields = members.reduce((s, m) => s + m.maxShields, 0);
  const setStance = (stance: string) => {
    if (tf) issue({ type: 'SetTaskForceOrder', taskForceId: tf.id, targetX: tf.targetX, targetY: tf.targetY, stance, targetFleetId: tf.attackTarget }, { cancelIds, success: 'Stance updated.' });
    else issue({ type: 'SetTacticalStance', fleetId: lead.id, stance }, { cancelIds, success: 'Stance updated.' });
  };
  return (
    <Frame title={tf ? tf.name : lead.name} sub={tf ? `task force · ${members.length} ships` : `${lead.designClass ?? lead.className} · #${lead.id}`} class="fill" testid="unit-detail"
      actions={<Pill small tone="ghost" onClick={() => { selectFleet(lead.id); focusMap(lead.x, lead.y); goto('galaxy'); }}>Show on chart</Pill>}>
      <div class="bars">
        <div class="bar-row"><span>Hull</span><Meter value={hull} max={maxHull} tone="hull" /><b>{hull}/{maxHull}</b></div>
        <div class="bar-row"><span>Shields</span><Meter value={shields} max={maxShields} tone="shield" /><b>{shields}/{maxShields}</b></div>
      </div>
      <div class="stat-grid four">
        <Stat label="Attack" value={fmt(tf ? tf.attack : lead.attack)} />
        <Stat label="Speed" value={`${tf ? tf.speed : lead.speed}/turn`} tipText="A task force moves at its slowest ship's speed." />
        <Stat label="Range" value={Math.min(...members.map((m) => m.range))} tipText="Supply range from a powered shipyard (shortest-ranged member)." />
        <Stat label="Upkeep" value={members.reduce((s, m) => s + m.maintenance, 0)} tone="warn" />
      </div>
      <div class="send-row">
        <label class="label" for="dest">Send to</label>
        <select id="dest" class="text-input" value={dest === '' ? '' : String(dest)} data-testid="dest-select"
          onChange={(e) => { const val = (e.target as HTMLSelectElement).value; setDest(val ? Number(val) : ''); }}>
          <option value="">Choose a known system…</option>
          {destinations.map((s) => <option key={s.id} value={s.id}>{s.name} ({s.x}.{s.y})</option>)}
        </select>
        <span class="send-eta">{route ? (route.r.steps < 0 ? <span class="neg">out of range</span> : `${route.r.eta} turn${route.r.eta === 1 ? '' : 's'}`) : ''}</span>
        <Pill disabled={!route || route.r.steps < 0 || v.session.active} reason={v.session.active ? 'Finish the battle first' : !route ? 'Choose a destination' : 'Outside supply range'}
          onClick={() => { if (route) plotCourse(lead, route.s.x, route.s.y); }} testid="send-course">Set course</Pill>
      </div>
      {v.meta.advancedRules ? (
        <div class="stance-row">
          <span class="label">Stance</span>
          {['Balanced', 'Aggressive', 'Defensive', 'Withdraw'].map((s) => (
            <Pill key={s} small tone="ghost" active={(tf?.stance ?? lead.stance) === s} onClick={() => setStance(s)} disabled={v.session.active} reason="Finish the battle first">{s}</Pill>
          ))}
        </div>
      ) : null}
      {actions.length ? <div class="action-list">{actions.map((a) => (
        <Pill key={a.id} tone={a.tone ?? 'primary'} disabled={!!a.disabled} reason={a.disabled} hint={a.detail} onClick={() => a.run()} wide>{a.label}</Pill>
      ))}</div> : null}
      <div class="drafted">
        <h4>Orders this turn</h4>
        {drafted.length ? <ul>{drafted.map((d) => <li key={d.id}>{d.summary}</li>)}</ul> : <p class="muted">None — {lead.route.length ? 'continuing on course' : 'holding position'}.</p>}
      </div>
      <div class="member-table">
        {members.map((m) => (
          <div class="member" key={m.id} {...tip(`${m.designClass ?? ''} ${m.designName ?? m.className}\nHull ${m.hull}/${m.maxHull} · shields ${m.shields}/${m.maxShields} · attack ${m.attack}`)}>
            <span>{m.name} <small>#{m.id}</small></span>
            <Meter value={m.hull} max={m.maxHull} tone="hull" height={4} />
            {m.colonyShip ? <Badge tone="good">colony</Badge> : m.shipClass === 'Transport' ? <Badge tone="warn">troops</Badge> : null}
          </div>
        ))}
      </div>
      {tf ? <Pill tone="tertiary" small onClick={() => issue({ type: 'DisbandTaskForce', taskForceId: tf.id }, { success: `${tf.name} split into individual ships.` })}
        disabled={v.session.active} reason="Finish the battle first" testid="split-tf">Split task force</Pill> : null}
    </Frame>
  );
}
