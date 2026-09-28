import type { ComponentChildren } from 'preact';
import { useState } from 'preact/hooks';
import type { GameView } from '../core/types';
import { focusMap, goto, selectFleet, selectSystem, view } from '../state/store';
import { Badge, Emblem, fmt, Frame, Meter, signed, Stat, Tabs, tip } from '../ui/components';

interface Column<T> { key: string; label: string; value: (row: T) => number | string; render?: (row: T) => ComponentChildren; num?: boolean; tip?: string }

function SortTable<T>({ rows, columns, onRow, testid, initial }: {
  rows: T[]; columns: Column<T>[]; onRow?: (row: T) => void; testid?: string; initial?: string;
}) {
  const [sort, setSort] = useState<{ key: string; dir: 1 | -1 }>({ key: initial ?? columns[0]!.key, dir: 1 });
  const col = columns.find((c) => c.key === sort.key) ?? columns[0]!;
  const sorted = [...rows].sort((a, b) => {
    const x = col.value(a), y = col.value(b);
    return (typeof x === 'number' && typeof y === 'number' ? x - y : String(x).localeCompare(String(y))) * sort.dir;
  });
  return (
    <div class="table-wrap">
      <table class="data-table" data-testid={testid}>
        <thead>
          <tr>
            {columns.map((c) => (
              <th key={c.key} class={c.num ? 'num' : ''} aria-sort={sort.key === c.key ? (sort.dir > 0 ? 'ascending' : 'descending') : 'none'} title={c.tip}>
                <button type="button" onClick={() => setSort({ key: c.key, dir: sort.key === c.key ? (-sort.dir as 1 | -1) : c.num ? -1 : 1 })}>
                  {c.label}<span class="sort-ind">{sort.key === c.key ? (sort.dir > 0 ? '▲' : '▼') : ''}</span>
                </button>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {sorted.map((r, i) => (
            <tr key={i} onClick={() => onRow?.(r)} class={onRow ? 'clickable' : ''} tabIndex={onRow ? 0 : undefined}
              onKeyDown={(e) => { if (e.key === 'Enter' && onRow) { e.stopPropagation(); onRow(r); } }}>
              {columns.map((c) => <td key={c.key} class={c.num ? 'num' : ''}>{c.render ? c.render(r) : c.value(r)}</td>)}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export function EmpireScreen() {
  const v = view.value!;
  const [tab, setTab] = useState<'colonies' | 'fleets'>('colonies');
  const e = v.empire;
  const totals = v.colonies.reduce((t, c) => ({
    food: t.food + c.economy.food - c.population, industry: t.industry + c.economy.industry, research: t.research + c.economy.research,
    net: t.net + c.economy.net, pop: t.pop + c.population, cap: t.cap + c.capacity,
  }), { food: 0, industry: 0, research: 0, net: 0, pop: 0, cap: 0 });
  const place = (x: number, y: number) => v.systems.find((s) => s.x === x && s.y === y)?.name ?? `${x}.${y}`;
  return (
    <div class="screen pad empire-screen" data-testid="screen-empire">
      <div class="empire-summary">
        <div class="es-id"><Emblem empire={v.meta.player} size={52} /><div><h1>{v.meta.playerName}</h1><div class="sub">Turn {v.meta.turn} · {v.colonies.length} colonies · {v.fleets.length} ships · {v.empires.filter((r) => r.contact).length} contacts</div></div></div>
        <div class="stat-grid six">
          <Stat label="Treasury" value={fmt(e.credits)} sub={<span class={e.income.net < 0 ? 'neg' : 'pos'}>{signed(e.income.net)}/turn</span>}
            tipText={`Colonies ${signed(e.income.colonies)} · trade ${signed(e.income.trade)} · ship upkeep −${e.income.ships}`} />
          <Stat label="Population" value={fmt(totals.pop)} sub={`capacity ${fmt(totals.cap)}`} />
          <Stat label="Industry" value={fmt(totals.industry)} sub="per turn" />
          <Stat label="Research" value={fmt(totals.research)} sub="per turn" />
          <Stat label="Food balance" value={signed(totals.food)} tone={totals.food < 0 ? 'alert' : 'good'} />
          <Stat label="Intelligence" value={fmt(e.intelligence)} sub="reserve" />
        </div>
      </div>
      <MemberStrip v={v} />
      <Frame title={tab === 'colonies' ? 'Colonies' : 'Fleets'} class="fill"
        actions={<Tabs value={tab} onChange={setTab} testid="empire-tabs" options={[
          { id: 'colonies', label: 'Colonies', count: v.colonies.length },
          { id: 'fleets', label: 'Fleets', count: v.fleets.length },
        ]} />}>
        {tab === 'colonies' ? <ColonyTable v={v} /> : (
          <SortTable testid="fleet-table" rows={v.fleets} initial="name"
            onRow={(f) => { selectFleet(f.id); focusMap(f.x, f.y); goto('galaxy'); }}
            columns={[
              { key: 'name', label: 'Ship', value: (f) => f.name, render: (f) => <span class="cell-name"><Emblem empire={f.owner} size={14} />{f.name} <small>#{f.id}</small></span> },
              { key: 'class', label: 'Role', value: (f) => f.className },
              { key: 'loc', label: 'Location', value: (f) => place(f.x, f.y) },
              { key: 'hull', label: 'Hull', num: true, value: (f) => f.hull / Math.max(1, f.maxHull), render: (f) => <span class="cell-meter"><Meter value={f.hull} max={f.maxHull} tone="hull" height={4} />{f.hull}</span> },
              { key: 'shields', label: 'Shields', num: true, value: (f) => f.shields },
              { key: 'attack', label: 'Attack', num: true, value: (f) => f.attack },
              { key: 'speed', label: 'Speed', num: true, value: (f) => f.speed },
              { key: 'upkeep', label: 'Upkeep', num: true, value: (f) => f.maintenance },
              { key: 'orders', label: 'Orders', value: (f) => (f.route.length ? f.eta : f.hasOrders ? 0.5 : 999), render: (f) => f.route.length ? <Badge tone="accent">→ {place(f.targetX, f.targetY)} · {f.eta}T</Badge> : f.hasOrders ? <Badge tone="good">ordered</Badge> : f.terraforming ? <Badge tone="secondary">terraforming</Badge> : <span class="muted">idle</span> },
              { key: 'tf', label: 'Group', value: (f) => v.taskForces.find((t) => t.id === f.taskForceId)?.name ?? '', render: (f) => f.taskForceId ? <Badge tone="accent">{v.taskForces.find((t) => t.id === f.taskForceId)?.name ?? 'TF'}</Badge> : '' },
            ]} />
        )}
      </Frame>
    </div>
  );
}

/** Member minor races and the bonuses they grant every colony (MinorRaces.h memberBonuses). */
function MemberStrip({ v }: { v: GameView }) {
  const b = v.empire.memberBonuses;
  if (!b) return null;
  const parts = [
    b.research ? `+${b.research}% research` : '', b.industry ? `+${b.industry}% industry` : '', b.food ? `+${b.food}% food` : '',
    b.credits ? `+${b.credits}% credit income` : '', b.morale ? `+${b.morale} morale` : '', b.groundDefense ? `+${b.groundDefense}% ground defense` : '',
  ].filter(Boolean);
  const stations = v.stations?.filter((s) => s.mine) ?? [];
  return (
    <div class="member-strip" data-testid="member-bonuses">
      <span class="ms-label" {...tip('Minor races that joined your empire. Each adds its join bonus to every colony; the bonus ends if the member secedes or is conquered.', 'Member worlds')}>Member worlds</span>
      {b.members ? (
        <>
          <span class="ms-bonus">{parts.join(' · ') || 'no bonus'}</span>
          <span class="ms-list">{b.minors.map((m) => <Badge key={m.systemId} tone="accent" tipText={m.bonus}>◇ {m.name}</Badge>)}</span>
        </>
      ) : <span class="muted">None yet — befriend minor races in Diplomacy and invite them to join for their bonuses.</span>}
      {stations.length ? <span class="ms-stations">{stations.filter((s) => s.type === 'Starbase').length} starbases · {stations.filter((s) => s.type === 'Outpost').length} outposts</span> : null}
    </div>
  );
}

function ColonyTable({ v }: { v: GameView }) {
  return (
    <SortTable testid="colony-table" rows={v.colonies} initial="name"
      onRow={(c) => { selectSystem(c.systemId); goto('system'); }}
      columns={[
        { key: 'name', label: 'System', value: (c) => c.name, render: (c) => <span class="cell-name"><Emblem empire={v.meta.player} size={14} />{c.name}</span> },
        { key: 'pop', label: 'Pop', num: true, value: (c) => c.population, render: (c) => <span>{c.population}<small>/{c.capacity}</small></span> },
        { key: 'growth', label: 'Growth', num: true, value: (c) => c.growth.max, render: (c) => <span class={c.growth.max < 0 ? 'neg' : 'pos'}>{signed(c.growth.max)}</span> },
        { key: 'morale', label: 'Morale', num: true, value: (c) => c.morale },
        { key: 'food', label: 'Food', num: true, value: (c) => c.economy.food - c.population, render: (c) => <span class={c.economy.foodDeficit ? 'neg' : 'pos'}>{signed(c.economy.food - c.population)}</span> },
        { key: 'industry', label: 'Industry', num: true, value: (c) => c.economy.industry },
        { key: 'research', label: 'Research', num: true, value: (c) => c.economy.research },
        { key: 'energy', label: 'Energy', num: true, value: (c) => c.economy.energyUsed, render: (c) => <span>{c.economy.energyUsed}<small>/{c.economy.energyAvailable}</small></span> },
        { key: 'credits', label: 'Credits', num: true, value: (c) => c.economy.net, render: (c) => <span class={c.economy.net < 0 ? 'neg' : ''}>{signed(c.economy.net)}</span> },
        { key: 'building', label: 'Building', value: (c) => c.building?.name ?? '', render: (c) => c.building ? <span>{c.building.name} <Badge tone="accent">{c.building.eta > 0 ? `${c.building.eta}T` : '—'}</Badge></span> : <span class="warn">idle</span> },
        { key: 'gov', label: 'Governor', value: (c) => c.governor.workforce, render: (c) => c.governor.workforce === 'Manual' ? <span class="muted">manual</span> : <Badge tone="secondary">{c.governor.workforce}</Badge> },
      ]} />
  );
}
