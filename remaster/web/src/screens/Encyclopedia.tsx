import { useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { Encyclopedia } from '../core/types';
import { meta, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Tabs } from '../ui/components';

let cache: Encyclopedia | null = null;
function data(): Encyclopedia {
  cache ??= core.encyclopedia();
  return cache;
}
const ROLE_NAMES: Record<number, string> = { 0: 'Scout', 1: 'Destroyer', 2: 'Cruiser', 3: 'Strike cruiser', 4: 'Command ship', 5: 'Colony ship', 6: 'Outpost', 7: 'Starbase', 8: 'Station', 9: 'Troop transport' };
const RACE_TO_EMPIRE: Record<number, number> = { 1: 1, 3: 2, 4: 3, 0: 4, 2: 5 };

type Tab = 'ships' | 'technologies' | 'structures' | 'races';

export function EncyclopediaScreen() {
  const d = data();
  const m = meta.value!;
  const v = view.value;
  const [tab, setTab] = useState<Tab>('ships');
  const [query, setQuery] = useState('');
  const [race, setRace] = useState<number | -1>(v ? (m.empires.find((e) => e.id === v.meta.player)?.race ?? -1) : -1);
  const [field, setField] = useState<number | -1>(-1);
  const [selected, setSelected] = useState<number | null>(null);
  const q = query.trim().toLowerCase();
  const raceName = (id: number) => d.races.find((r) => r.id === id)?.name ?? '—';

  const rows = useMemo(() => {
    if (tab === 'ships') return d.ships.filter((s) => (race < 0 || s.race === race) && (!q || `${s.designClass} ${s.designName} ${s.description}`.toLowerCase().includes(q)))
      .map((s) => ({ id: s.id, title: `${s.designClass}`, sub: `${s.designName} · ${raceName(s.race)}`, badge: `${fmt(s.industryCost)} ind` }));
    if (tab === 'technologies') return d.technologies.filter((t) => (field < 0 || t.field === field) && (!q || `${t.name} ${t.description}`.toLowerCase().includes(q)))
      .map((t) => ({ id: t.id, title: t.name, sub: `${m.researchFields[t.field]} level ${t.level}`, badge: `${fmt(t.cost)} pts` }));
    if (tab === 'structures') return d.buildings.filter((b) => (field < 0 || b.field === field) && (!q || `${b.name} ${b.description}`.toLowerCase().includes(q)))
      .map((b) => ({ id: b.id, title: b.name, sub: `${m.researchFields[b.field] ?? '—'} ${b.level}`, badge: `${fmt(b.industryCost)} ind` }));
    return d.races.filter((r) => !q || r.name.toLowerCase().includes(q)).map((r) => ({ id: r.id, title: r.name, sub: r.id <= 4 ? 'Major power' : 'Minor race', badge: '' }));
  }, [tab, q, race, field]);
  const current = selected !== null ? rows.find((r) => r.id === selected) ?? rows[0] : rows[0];

  return (
    <div class="screen pad encyclopedia-screen" data-testid="screen-encyclopedia">
      <div class="ency-grid">
        <Frame title="Database" sub={`${rows.length} entries`} class="fill"
          actions={<Tabs value={tab} onChange={(t) => { setTab(t); setSelected(null); }} options={[
            { id: 'ships', label: 'Ships' }, { id: 'technologies', label: 'Tech' }, { id: 'structures', label: 'Structures' }, { id: 'races', label: 'Races' },
          ]} />}>
          <div class="ency-filters">
            <input class="text-input grow" type="search" placeholder="Search…" value={query} aria-label="Search the database" data-testid="ency-search"
              onInput={(e) => setQuery((e.target as HTMLInputElement).value)} />
            {tab === 'ships' ? (
              <select class="text-input" value={race} aria-label="Race" onChange={(e) => setRace(Number((e.target as HTMLSelectElement).value))}>
                <option value={-1}>All races</option>
                {d.races.slice(0, 5).map((r) => <option key={r.id} value={r.id}>{r.name}</option>)}
              </select>
            ) : tab === 'technologies' || tab === 'structures' ? (
              <select class="text-input" value={field} aria-label="Field" onChange={(e) => setField(Number((e.target as HTMLSelectElement).value))}>
                <option value={-1}>All fields</option>
                {m.researchFields.map((f, i) => <option key={f} value={i}>{f}</option>)}
              </select>
            ) : null}
          </div>
          <div class="ency-list" role="listbox" aria-label="Entries">
            {rows.length === 0 ? <Empty>No matches.</Empty> : rows.slice(0, 400).map((r) => (
              <button type="button" role="option" aria-selected={current?.id === r.id} key={r.id} class={`ency-row${current?.id === r.id ? ' active' : ''}`} onClick={() => setSelected(r.id)}>
                <span class="er-title">{r.title}</span><span class="er-sub">{r.sub}</span>{r.badge ? <span class="er-badge">{r.badge}</span> : null}
              </button>
            ))}
          </div>
        </Frame>
        <Frame title={current?.title ?? 'Entry'} sub={current?.sub} tone="secondary" class="fill ency-detail" testid="ency-detail">
          {!current ? <Empty>Select an entry.</Empty> : tab === 'ships' ? <ShipEntry id={current.id} /> : tab === 'technologies' ? <TechEntry id={current.id} /> : tab === 'structures' ? <StructureEntry id={current.id} /> : (
            <div class="ency-entry">
              {RACE_TO_EMPIRE[current.id] ? <Emblem empire={RACE_TO_EMPIRE[current.id]!} size={80} /> : <span class="minor-glyph big">◇</span>}
              <p>{current.id <= 4 ? 'One of the five major powers of the quadrant.' : 'A minor race that can be befriended, courted for membership, or absorbed into an empire.'}</p>
              <p class="muted">{d.ships.filter((s) => s.race === current.id).length} ship designs in the reference catalogue.</p>
            </div>
          )}
        </Frame>
      </div>
    </div>
  );
}

function ShipEntry({ id }: { id: number }) {
  const s = data().ships.find((x) => x.id === id)!;
  const m = meta.value!;
  const emp = RACE_TO_EMPIRE[s.race];
  return (
    <div class="ency-entry">
      <div class="ee-head">{emp ? <Emblem empire={emp} size={46} /> : null}<div><h3>{s.designClass} class</h3><div class="muted">{s.designName} · {ROLE_NAMES[s.role] ?? `Role ${s.role}`}</div></div></div>
      <div class="spec-grid">
        <div><span>Hull</span><b>{s.hull}</b></div><div><span>Shields</span><b>{s.shields}</b></div><div><span>Attack</span><b>{fmt(s.attack)}</b></div>
        <div><span>Speed</span><b>{s.speed}</b></div><div><span>Range</span><b>{s.range}</b></div><div><span>Troops</span><b>{s.troops >= 0 ? s.troops : '—'}</b></div>
        <div><span>Industry</span><b>{fmt(s.industryCost)}</b></div><div><span>Upkeep</span><b>{s.maintenance}</b></div>
      </div>
      <h4>Technology requirements</h4>
      <div class="req-row">{s.requires.map((r, i) => <Badge key={i} tone={r > 0 ? 'accent' : 'dim'}>{m.researchFields[i]} {r}</Badge>)}</div>
      <p class="ee-desc">{s.description}</p>
    </div>
  );
}
function TechEntry({ id }: { id: number }) {
  const t = data().technologies.find((x) => x.id === id)!;
  const m = meta.value!;
  const v = view.value;
  const have = v ? v.empire.techs[t.field]!.level >= t.level : false;
  return (
    <div class="ency-entry">
      <div class="ee-head"><span class={`field-icon fi-${t.field} big`} aria-hidden="true" /><div><h3>{t.name}</h3><div class="muted">{m.researchFields[t.field]} · level {t.level} · {fmt(t.cost)} points</div></div>{v ? <Badge tone={have ? 'good' : 'dim'}>{have ? 'researched' : 'not yet'}</Badge> : null}</div>
      <p class="ee-desc">{t.description.replace(/^[A-Z0-9 '\-]+:\s*/, '')}</p>
    </div>
  );
}
function StructureEntry({ id }: { id: number }) {
  const b = data().buildings.find((x) => x.id === id)!;
  const m = meta.value!;
  return (
    <div class="ency-entry">
      <div class="ee-head"><span class="choice-icon ci-Factory big" aria-hidden="true" /><div><h3>{b.name}</h3><div class="muted">{m.researchFields[b.field] ?? '—'} level {b.level}</div></div></div>
      <div class="spec-grid">
        <div><span>Industry</span><b>{fmt(b.industryCost)}</b></div><div><span>Energy</span><b>{b.energyCost}</b></div>
        <div><span>Population</span><b>{b.populationCost}</b></div><div><span>Morale</span><b>{b.moraleBonus}</b></div>
      </div>
      <p class="ee-desc">{b.description}</p>
      <p class="muted">Reference catalogue entry. The remake's colony economy currently builds the core's own structure set.</p>
    </div>
  );
}
