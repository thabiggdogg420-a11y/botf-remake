import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import type { ContactView, FleetView, GameView, SystemView } from '../core/types';
import { contextMenu, GalaxyMap, hoverInfo } from '../map/GalaxyMap';
import {
  cancelOrders, dryRun, focusMap, goto, issue, mapLayers, selectFleet, selectSystem, selection, view,
} from '../state/store';
import { cellActions, replaceIds, sectorLabel, selectedUnit, type Action } from '../state/orders';
import { Badge, Emblem, Empty, fmt, IconButton, Meter, Pill, signed, Stat, tip } from '../ui/components';
import { palette } from '../ui/theme';

export function GalaxyScreen() {
  const v = view.value!;
  return (
    <div class="screen galaxy-screen" data-testid="screen-galaxy">
      <GalaxyMap />
      <MapHud v={v} />
      <HoverTip v={v} />
      <ContextMenu v={v} />
      <Inspector v={v} />
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
function MapHud({ v }: { v: GameView }) {
  const layers = mapLayers.value;
  const [ordersOpen, setOrdersOpen] = useState(false);
  const [alertsOpen, setAlertsOpen] = useState(true);
  const toggle = (k: keyof typeof layers) => { mapLayers.value = { ...layers, [k]: !layers[k] }; };
  const alertTone = v.alerts.some((a) => a.severity === 'alert') ? 'alert' : v.alerts.length ? 'warn' : 'good';
  return (
    <>
      <div class="map-hud-top">
        <div class={`hud-card alerts-card tone-${alertTone}`} data-testid="alerts">
          <button type="button" class="hud-card-head" onClick={() => setAlertsOpen(!alertsOpen)} aria-expanded={alertsOpen}>
            <span class="dot" /> {v.alerts.length ? `${v.alerts.length} alert${v.alerts.length === 1 ? '' : 's'}` : 'All systems nominal'}
            <span class="chev">{alertsOpen ? '▾' : '▸'}</span>
          </button>
          {alertsOpen && v.alerts.length ? (
            <ul class="alert-list">
              {v.alerts.slice(0, 7).map((a, i) => (
                <li key={i} class={`sev-${a.severity}`}>
                  <button type="button" onClick={() => {
                    if (a.systemId) { selectSystem(a.systemId); const s = v.systems.find((x) => x.id === a.systemId); if (s) focusMap(s.x, s.y); }
                    else if (a.fleetId) { selectFleet(a.fleetId); const f = v.fleets.find((x) => x.id === a.fleetId); if (f) focusMap(f.x, f.y); }
                    else if (a.kind === 'proposal' || a.kind === 'offer') goto('diplomacy');
                  }}>{a.text}</button>
                </li>
              ))}
            </ul>
          ) : null}
        </div>
      </div>
      <div class="map-hud-bottom">
        <div class="layer-toggles" role="group" aria-label="Map layers">
          {([
            ['territory', 'Territory', 'Colony influence of every known empire'],
            ['supply', 'Supply', 'Supply range of the selected unit (from the core)'],
            ['routes', 'Routes', 'Plotted courses and ETAs'],
            ['trade', 'Trade', 'Trade routes'],
            ['grid', 'Grid', 'Sector grid and coordinates'],
            ['labels', 'Labels', 'System names'],
          ] as const).map(([k, label, hint]) => (
            <Pill key={k} small tone="ghost" active={layers[k]} onClick={() => toggle(k)} hint={hint}>{label}</Pill>
          ))}
        </div>
        <div class={`hud-card orders-card${ordersOpen ? ' open' : ''}`} data-testid="orders-card">
          <button type="button" class="hud-card-head" onClick={() => setOrdersOpen(!ordersOpen)} aria-expanded={ordersOpen}>
            Orders this turn <span class="count">{v.pending.length}</span><span class="chev">{ordersOpen ? '▾' : '▴'}</span>
          </button>
          {ordersOpen ? (
            <ul class="order-list">
              {v.pending.length === 0 ? <li class="muted">No orders drafted yet.</li> : null}
              {v.pending.map((p) => (
                <li key={p.id}>
                  <span>{p.summary}</span>
                  <IconButton label="Cancel this order" disabled={v.session.active} reason="Finish the battle in progress first"
                    onClick={() => cancelOrders([p.id], 'Order cancelled.')}>✕</IconButton>
                </li>
              ))}
            </ul>
          ) : null}
        </div>
        <div class="map-legend" aria-hidden="true">
          <span><i class="lg lg-own" />Your ships</span>
          <span><i class="lg lg-foreign" />Contacts</span>
          <span><i class="lg lg-tf" />Task force</span>
          <span><i class="lg lg-fog" />Unexplored</span>
        </div>
      </div>
    </>
  );
}

function HoverTip({ v }: { v: GameView }) {
  const h = hoverInfo.value;
  if (!h || contextMenu.value) return null;
  const W = v.meta.width;
  const x = h.cell % W, y = Math.floor(h.cell / W);
  const system = v.systems.find((s) => s.x === x && s.y === y);
  const unit = selectedUnit(v);
  const fog = v.fog[h.cell];
  const own = v.fleets.filter((f) => f.x === x && f.y === y).length;
  const foreign = v.contacts.filter((f) => f.x === x && f.y === y);
  let order = '';
  if (unit && !(unit.fleet.x === x && unit.fleet.y === y)) {
    order = h.eta === null ? (h.ok ? '' : 'Out of supply range') : `Right-click: set course — ${h.eta} turn${h.eta === 1 ? '' : 's'}`;
  }
  return (
    <div class="hover-tip" style={{ left: `${h.x + 18}px`, top: `${h.y + 14}px` }} aria-hidden="true">
      <div class="ht-title">{system ? system.name : fog === '0' ? 'Uncharted space' : 'Open space'} <span class="ht-coord">{x}.{y}</span></div>
      {system?.owner ? <div class="ht-line"><Emblem empire={system.owner} size={12} /> {system.owner === v.meta.player ? 'Your colony' : v.empires.find((e) => e.empire === system.owner)?.name ?? 'Colony'}</div> : null}
      {system?.minor ? <div class="ht-line">◇ {system.minor.name}</div> : null}
      {fog === '1' ? <div class="ht-line muted">Surveyed — outside sensor range</div> : null}
      {own ? <div class="ht-line">{own} of your ship{own === 1 ? '' : 's'}</div> : null}
      {foreign.length ? <div class="ht-line warn">{foreign.length} contact{foreign.length === 1 ? '' : 's'}{foreign.some((f) => f.atWar) ? ' — hostile' : ''}</div> : null}
      {order ? <div class={`ht-order ${h.eta === null ? 'bad' : ''}`}>{order}</div> : null}
    </div>
  );
}

function ContextMenu({ v }: { v: GameView }) {
  const menu = contextMenu.value;
  const ref = useRef<HTMLDivElement>(null);
  const actions = useMemo(() => (menu ? cellActions(menu.cell) : []), [menu, v.meta.version]);
  const [pos, setPos] = useState<{ left: number; top: number } | null>(null);
  useEffect(() => {
    if (!menu || !ref.current) { setPos(null); return; }
    const parent = ref.current.parentElement!.getBoundingClientRect();
    const r = ref.current.getBoundingClientRect();
    const left = Math.min(Math.max(8, menu.x + 6), parent.width - r.width - 8);
    const top = Math.min(Math.max(8, menu.y + 6), parent.height - r.height - 8);
    setPos({ left, top });
    ref.current.querySelector<HTMLButtonElement>('button:not([aria-disabled="true"])')?.focus();
  }, [menu, actions.length]);
  useEffect(() => {
    if (!menu) return;
    const close = (e: Event) => { if (ref.current && !ref.current.contains(e.target as Node)) contextMenu.value = null; };
    const esc = (e: KeyboardEvent) => { if (e.key === 'Escape') { e.stopPropagation(); contextMenu.value = null; } };
    window.addEventListener('pointerdown', close, true);
    window.addEventListener('keydown', esc, true);
    return () => { window.removeEventListener('pointerdown', close, true); window.removeEventListener('keydown', esc, true); };
  }, [menu]);
  if (!menu) return null;
  const groups: Action['group'][] = ['orders', 'units', 'system', 'view'];
  const run = (a: Action) => { if (a.disabled) return; contextMenu.value = null; a.run(); };
  return (
    <div ref={ref} class="context-menu" role="menu" data-testid="context-menu"
      style={pos ? { left: `${pos.left}px`, top: `${pos.top}px` } : { left: `${menu.x}px`, top: `${menu.y}px`, visibility: 'hidden' }}>
      <div class="cm-head">
        <span class="cm-anchor" aria-hidden="true" />
        {sectorLabel(v, menu.cell)}
        <span class="cm-coord">{menu.cell % v.meta.width}.{Math.floor(menu.cell / v.meta.width)}</span>
      </div>
      {groups.map((g) => {
        const items = actions.filter((a) => a.group === g);
        if (!items.length) return null;
        return (
          <div class="cm-group" key={g}>
            {items.map((a) => (
              <button type="button" role="menuitem" key={a.id} class={`cm-item tone-${a.tone ?? 'primary'}${a.disabled ? ' disabled' : ''}`}
                aria-disabled={a.disabled ? 'true' : undefined} data-action={a.id}
                onClick={() => run(a)} {...tip(a.disabled ? `Unavailable: ${a.disabled}` : a.detail)}>
                <span class="cm-label">{a.label}</span>
                {a.disabled ? <span class="cm-reason">{a.disabled}</span> : a.detail ? <span class="cm-detail">{a.detail}</span> : null}
              </button>
            ))}
          </div>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Inspector
// ---------------------------------------------------------------------------------------------
function Inspector({ v }: { v: GameView }) {
  const sel = selection.value;
  const [collapsed, setCollapsed] = useState(false);
  const fleet = sel.fleetId ? v.fleets.find((f) => f.id === sel.fleetId) ?? null : null;
  const contact = sel.fleetId && !fleet ? v.contacts.find((c) => c.id === sel.fleetId) ?? null : null;
  const cell = sel.cell;
  const system = cell !== null ? v.systems.find((s) => s.y * v.meta.width + s.x === cell) ?? null : null;
  return (
    <aside class={`inspector${collapsed ? ' collapsed' : ''}`} aria-label="Inspector" data-testid="inspector">
      <button type="button" class="inspector-toggle" onClick={() => setCollapsed(!collapsed)} aria-label={collapsed ? 'Show inspector' : 'Hide inspector'}>
        {collapsed ? '◂' : '▸'}
      </button>
      <div class="inspector-body">
        {fleet ? <UnitCard v={v} fleet={fleet} /> : null}
        {contact ? <ContactCard contact={contact} /> : null}
        {cell !== null ? <SectorCard v={v} cell={cell} system={system} /> : (
          <div class="inspector-empty">
            <h3>Star chart</h3>
            <p>Select a system or ship. <b>Right-click</b> a sector for orders; with a ship selected, right-click open space to set course.</p>
            <ul class="keys">
              <li><kbd>Drag</kbd> pan · <kbd>Wheel</kbd> zoom · <kbd>F</kbd> fit · <kbd>H</kbd> home</li>
              <li><kbd>Tab</kbd> next ship · <kbd>Enter</kbd> end turn</li>
            </ul>
          </div>
        )}
      </div>
    </aside>
  );
}

function orderText(v: GameView, fleet: FleetView): string {
  const drafted = v.pending.filter((p) => p.fleetId === fleet.id || (fleet.taskForceId && p.taskForceId === fleet.taskForceId && p.type === 'SetTaskForceOrder'));
  if (drafted.length) return drafted.map((d) => d.summary.replace(/^.*?: /, '')).join('; ');
  if (fleet.colonize) return 'Colonizing at turn end';
  if (fleet.terraforming) {
    const s = v.systems.find((x) => x.id === fleet.terraformSystemId);
    const p = s?.planets[fleet.terraformPlanetIndex];
    return `Terraforming ${p?.name ?? 'planet'}${p ? ` (${p.terraformWork}/${p.terraformNeeded})` : ''}`;
  }
  if (fleet.attackTarget) return `Pursuing contact #${fleet.attackTarget}`;
  if (fleet.route.length) {
    const s = v.systems.find((x) => x.x === fleet.targetX && x.y === fleet.targetY);
    return `En route to ${s?.name ?? `${fleet.targetX}.${fleet.targetY}`}`;
  }
  return 'Awaiting orders';
}

function UnitCard({ v, fleet }: { v: GameView; fleet: FleetView }) {
  const tf = fleet.taskForceId ? v.taskForces.find((t) => t.id === fleet.taskForceId) ?? null : null;
  const members = tf ? v.fleets.filter((f) => tf.fleetIds.includes(f.id)) : [fleet];
  const here = v.fleets.filter((f) => f.x === fleet.x && f.y === fleet.y && !f.taskForceId && f.id !== fleet.id);
  const [marks, setMarks] = useState<number[]>([]);
  useEffect(() => setMarks([]), [fleet.id]);
  const actions = cellActions(fleet.y * v.meta.width + fleet.x).filter((a) => a.group === 'orders');
  const p = palette(v.meta.player);
  const stanceIds = replaceIds(fleet);
  const setStance = (stance: string) => {
    if (tf) issue({ type: 'SetTaskForceOrder', taskForceId: tf.id, targetX: tf.targetX, targetY: tf.targetY, stance, targetFleetId: tf.attackTarget }, { cancelIds: stanceIds, success: `${tf.name}: ${stance.toLowerCase()} stance.` });
    else issue({ type: 'SetTacticalStance', fleetId: fleet.id, stance }, { cancelIds: stanceIds, success: `Stance set to ${stance.toLowerCase()}.` });
  };
  const hull = tf ? tf.hull : fleet.hull;
  const maxHull = members.reduce((s, m) => s + m.maxHull, 0);
  const shields = tf ? tf.shields : fleet.shields;
  const maxShields = members.reduce((s, m) => s + m.maxShields, 0);
  const formIds = [fleet.id, ...marks];
  const formCommand = { type: 'CreateTaskForce', memberFleetIds: formIds };
  const formReason = marks.length ? dryRun(formCommand) : 'Mark at least one more ungrouped ship in this sector';
  return (
    <div class="card unit-card" data-testid="unit-card" style={{ '--card-accent': p.primary } as never}>
      <div class="card-head">
        <Emblem empire={v.meta.player} size={26} />
        <div>
          <div class="card-title">{tf ? tf.name : fleet.name}</div>
          <div class="card-sub">{tf ? `Task force · ${members.length} ships` : `${fleet.designClass ? `${fleet.designClass} class · ` : ''}${fleet.className} · #${fleet.id}`}</div>
        </div>
      </div>
      <div class="bars">
        <div class="bar-row" {...tip(`Hull integrity ${hull} of ${maxHull}. Ships repair at a powered shipyard when no enemy is present.`, 'Hull')}>
          <span>Hull</span><Meter value={hull} max={maxHull} tone="hull" /><b>{hull}/{maxHull}</b>
        </div>
        <div class="bar-row" {...tip(`Shields ${shields} of ${maxShields}. Shields regenerate each turn.`, 'Shields')}>
          <span>Shields</span><Meter value={shields} max={maxShields} tone="shield" /><b>{shields}/{maxShields}</b>
        </div>
      </div>
      <div class="stat-grid four">
        <Stat label="Attack" value={fmt(tf ? tf.attack : fleet.attack)} tipText="Combined strategic attack rating from the core (fleetAttack / taskForceAttack)." />
        <Stat label="Speed" value={`${tf ? tf.speed : fleet.speed}/turn`} tipText="Sectors moved per turn. A task force moves at its slowest member's speed." />
        <Stat label="Range" value={fleet.range} tipText="Base supply range in sectors from a powered shipyard. Propulsion technology extends it." />
        <Stat label="Upkeep" value={members.reduce((s, m) => s + m.maintenance, 0)} tipText="Credits deducted every turn for maintenance." tone="warn" />
      </div>
      <div class="orders-line" data-testid="unit-orders"><span class="label">Orders</span> {orderText(v, fleet)}{fleet.eta ? <Badge tone="accent">{fleet.eta}T</Badge> : null}</div>
      {fleet.routeBlocked ? <div class="warn-line">Course leaves supply range — the unit will wait.</div> : null}
      {v.meta.advancedRules ? (
        <div class="stance-row" role="group" aria-label="Tactical stance">
          <span class="label" {...tip('How this unit behaves when battle is joined. Changing stance uses this turn’s order for the unit.')}>Stance</span>
          {v.meta && ['Balanced', 'Aggressive', 'Defensive', 'Withdraw'].map((s) => (
            <Pill key={s} small tone="ghost" active={(tf?.stance ?? fleet.stance) === s} onClick={() => setStance(s)}
              disabled={v.session.active} reason="Finish the battle in progress first">{s}</Pill>
          ))}
        </div>
      ) : null}
      {actions.length ? (
        <div class="action-list">
          {actions.map((a) => (
            <Pill key={a.id} tone={a.tone ?? 'primary'} disabled={!!a.disabled} reason={a.disabled} onClick={() => a.run()} hint={a.detail} wide testid={`action-${a.id}`}>
              {a.label}
            </Pill>
          ))}
        </div>
      ) : <p class="hint">Right-click a sector to plot a course or attack.</p>}
      {tf ? (
        <div class="tf-members">
          {members.map((m) => (
            <div class="member" key={m.id}><span>{m.name} #{m.id}</span><Meter value={m.hull} max={m.maxHull} tone="hull" height={4} /></div>
          ))}
          <Pill tone="tertiary" small disabled={v.session.active} reason="Finish the battle in progress first"
            onClick={() => issue({ type: 'DisbandTaskForce', taskForceId: tf.id }, { success: `${tf.name} disbanded.` })}>Split task force</Pill>
        </div>
      ) : here.length ? (
        <div class="tf-form">
          <div class="label">Form task force with ships here</div>
          {here.slice(0, 8).map((f) => (
            <label key={f.id} class="check">
              <input type="checkbox" checked={marks.includes(f.id)} onChange={(e) => {
                const on = (e.target as HTMLInputElement).checked;
                setMarks(on ? [...marks, f.id] : marks.filter((m) => m !== f.id));
              }} /> {f.name} #{f.id}
            </label>
          ))}
          <Pill small tone="secondary" disabled={!!formReason} reason={formReason} testid="form-tf"
            onClick={() => { if (issue(formCommand, { success: 'Task force formed. It moves and fights as one.' })) setMarks([]); }}>Form task force</Pill>
        </div>
      ) : null}
    </div>
  );
}

function ContactCard({ contact }: { contact: ContactView }) {
  return (
    <div class="card contact-card" data-testid="contact-card">
      <div class="card-head">
        <Emblem empire={contact.owner} size={26} />
        <div>
          <div class="card-title">{contact.name}</div>
          <div class="card-sub">{contact.ownerName} · {contact.className} · #{contact.id}</div>
        </div>
        {contact.atWar ? <Badge tone="alert">At war</Badge> : <Badge tone="dim">At peace</Badge>}
      </div>
      <div class="bars">
        <div class="bar-row"><span>Hull</span><Meter value={contact.hull} max={contact.maxHull} tone="hull" /><b>{contact.hull}/{contact.maxHull}</b></div>
        <div class="bar-row"><span>Shields</span><Meter value={contact.shields} max={contact.maxShields} tone="shield" /><b>{contact.shields}/{contact.maxShields}</b></div>
      </div>
      <div class="stat-grid four"><Stat label="Attack" value={fmt(contact.attack)} /><Stat label="Sector" value={`${contact.x}.${contact.y}`} /></div>
      <p class="hint">{contact.atWar ? 'Select one of your warships, then right-click this sector to attack.' : 'Declare war in Diplomacy before engaging.'} Contacts are only tracked inside your sensor range.</p>
    </div>
  );
}

function SectorCard({ v, cell, system }: { v: GameView; cell: number; system: SystemView | null }) {
  const W = v.meta.width;
  const x = cell % W, y = Math.floor(cell / W);
  const colony = system ? v.colonies.find((c) => c.systemId === system.id) : undefined;
  const units = v.fleets.filter((f) => f.x === x && f.y === y);
  const contacts = v.contacts.filter((f) => f.x === x && f.y === y);
  const fog = v.fog[cell];
  const ownerRel = system?.owner && system.owner !== v.meta.player ? v.empires.find((e) => e.empire === system.owner) : null;
  return (
    <div class="card sector-card" data-testid="sector-card">
      <div class="card-head">
        {system?.owner ? <Emblem empire={system.owner} size={26} /> : <span class="star-dot" />}
        <div>
          <div class="card-title">{system ? system.name : fog === '0' ? 'Uncharted space' : 'Open space'}</div>
          <div class="card-sub">
            Sector {x}.{y} · {fog === '2' ? 'in sensor range' : fog === '1' ? 'surveyed' : 'unexplored'}
            {system?.owner ? ` · ${system.owner === v.meta.player ? 'your colony' : ownerRel?.name ?? 'colonised'}` : system?.minor ? ` · ${system.minor.name}` : system ? ' · unclaimed' : ''}
          </div>
        </div>
      </div>
      {colony ? (
        <div class="colony-brief">
          <div class="stat-grid four">
            <Stat label="Pop" value={`${colony.population}/${colony.capacity}`} tipText={`Population ${colony.population}; this system currently supports ${colony.capacity}. Terraforming raises capacity.`} />
            <Stat label="Food" value={signed(colony.economy.food - colony.population)} tone={colony.economy.foodDeficit ? 'alert' : 'good'}
              tipText={`Farms produce ${colony.economy.food} food for ${colony.population} people.${colony.economy.foodDeficit ? ` Shortfall of ${colony.economy.foodDeficit}: population will fall.` : ''}`} />
            <Stat label="Industry" value={colony.economy.industry} tipText="Industry per turn, invested in the first build-queue item." />
            <Stat label="Credits" value={signed(colony.economy.net)} tone={colony.economy.net < 0 ? 'alert' : undefined} tipText={`Income ${colony.economy.income} − structure upkeep ${colony.economy.upkeep}.`} />
          </div>
          <div class="building-line">
            {colony.building ? <>Building <b>{colony.building.name}</b> <Meter value={colony.building.work} max={colony.building.cost} height={4} /> <Badge tone="accent">{colony.building.eta > 0 ? `${colony.building.eta}T` : '—'}</Badge></> : <span class="warn">Nothing under construction</span>}
          </div>
          {colony.queueBlocker ? <div class="warn-line">{colony.queueBlocker}</div> : null}
          <Pill wide testid="open-system" onClick={() => { selectSystem(colony.systemId); goto('system'); }} kbd="F2">Manage system</Pill>
        </div>
      ) : null}
      {system && system.planets.length ? (
        <div class="planet-list">
          {system.planets.map((p) => (
            <div class="planet-row" key={p.index} {...tip(`${p.className}\nCapacity now ${p.capacity}, up to ${p.maxCapacity} when terraformed.${p.terraformNeeded ? `\nTerraforming: ${p.terraformWork}/${p.terraformNeeded} work.` : ''}`, p.name)}>
              <span class={`planet-glyph pc-${p.class}`} aria-hidden="true" />
              <span class="planet-name">{p.name}</span>
              <span class="planet-class">{p.class === 'GasGiant' ? 'Gas giant' : p.class === 'Asteroids' ? 'Asteroids' : `Class ${p.class}`}</span>
              <span class="planet-cap">{p.capacity > 0 || p.maxCapacity > 0 ? `${p.capacity}/${p.maxCapacity}` : '—'}</span>
            </div>
          ))}
          <div class="planet-total" {...tip('Total population capacity of the system now, and after terraforming every planet.')}>Capacity {system.capacity} · max {system.maxCapacity}</div>
        </div>
      ) : system ? null : <p class="hint">{fog === '0' ? 'Send a scout to survey this region.' : 'No star system in this sector.'}</p>}
      {system?.minor ? (
        <div class="minor-line">
          {system.minor.unknown ? 'An inhabited world. Make contact to learn more.' : <>◇ <b>{system.minor.name}</b> · {system.minor.memberOf ? `member of ${v.empires.find((e) => e.empire === system.minor!.memberOf)?.name ?? 'an empire'}` : 'independent'}{system.minor.attitude !== undefined ? ` · relations ${system.minor.attitude}` : ''}</>}
        </div>
      ) : null}
      {units.length || contacts.length ? (
        <div class="unit-list">
          {units.map((f) => (
            <button type="button" key={f.id} class={`unit-row${selection.value.fleetId === f.id ? ' active' : ''}`} onClick={() => selectFleet(f.id)} data-testid={`unit-row-${f.id}`}>
              <Emblem empire={f.owner} size={14} /><span>{f.name}</span>{f.taskForceId ? <Badge tone="accent">TF</Badge> : null}{f.hasOrders ? <Badge tone="good">ordered</Badge> : null}
            </button>
          ))}
          {contacts.map((c) => (
            <button type="button" key={c.id} class={`unit-row foreign${selection.value.fleetId === c.id ? ' active' : ''}`} onClick={() => selectFleet(c.id)}>
              <Emblem empire={c.owner} size={14} /><span>{c.name}</span>{c.atWar ? <Badge tone="alert">hostile</Badge> : null}
            </button>
          ))}
        </div>
      ) : null}
      {!colony && (system || units.length) ? <Empty>Right-click the sector for available orders.</Empty> : null}
    </div>
  );
}
