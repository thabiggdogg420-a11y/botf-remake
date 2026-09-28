import { useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { Encyclopedia } from '../core/types';
import { meta, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Tabs } from '../ui/components';
import { ERA_ORDER, eraLabel, factionLabel, roleLabel, sourceLabel, weaponLabel, arcLabel } from '../core/roster';

let cache: Encyclopedia | null = null;
function data(): Encyclopedia {
  cache ??= core.encyclopedia();
  return cache;
}
const ROLE_NAMES: Record<number, string> = { 0: 'Scout', 1: 'Destroyer', 2: 'Cruiser', 3: 'Strike cruiser', 4: 'Command ship', 5: 'Colony ship', 6: 'Outpost', 7: 'Starbase', 8: 'Station', 9: 'Troop transport' };
const RACE_TO_EMPIRE: Record<number, number> = { 1: 1, 3: 2, 4: 3, 0: 4, 2: 5 };
const RACE_TO_FACTION: Record<number, string> = { 1: 'Federation', 3: 'Klingon', 4: 'Romulan', 0: 'Cardassian', 2: 'Ferengi' };
const MAJOR_FACTIONS = ['Federation', 'Klingon', 'Romulan', 'Cardassian', 'Ferengi'];
type Ship = Encyclopedia['ships'][number];
/** Faction, role, era and source of any ship record; legacy records map onto the same vocabulary. */
export function shipFacets(s: Ship, raceName: (id: number) => string) {
  const r = s.roster;
  return {
    faction: r ? r.faction : RACE_TO_FACTION[s.race] ?? raceName(s.race),
    role: r ? roleLabel(r.hullRole) : ROLE_NAMES[s.role] ?? `Role ${s.role}`,
    era: r ? r.era : 'Legacy',
    source: r ? r.sourceKind : 'Legacy',
  };
}

type Tab = 'ships' | 'technologies' | 'structures' | 'races' | 'minors';

export function EncyclopediaScreen() {
  const d = data();
  const m = meta.value!;
  const v = view.value;
  const [tab, setTab] = useState<Tab>('ships');
  const [query, setQuery] = useState('');
  const playerRace = v ? (m.empires.find((e) => e.id === v.meta.player)?.race ?? -1) : -1;
  const [faction, setFaction] = useState<string>(playerRace >= 0 ? RACE_TO_FACTION[playerRace] ?? '' : '');
  const [role, setRole] = useState('');
  const [era, setEra] = useState('');
  const [source, setSource] = useState('');
  const [field, setField] = useState<number | -1>(-1);
  const [selected, setSelected] = useState<number | null>(null);
  const q = query.trim().toLowerCase();
  const raceName = (id: number) => d.races.find((r) => r.id === id)?.name ?? '—';

  const facets = useMemo(() => {
    const all = d.ships.map((s) => shipFacets(s, raceName));
    const uniq = (xs: string[]) => [...new Set(xs)];
    const factions = uniq(all.map((f) => f.faction));
    return {
      factions: [...MAJOR_FACTIONS, ...factions.filter((f) => !MAJOR_FACTIONS.includes(f)).sort()],
      roles: uniq(all.map((f) => f.role)).sort(),
      eras: ['Legacy', ...ERA_ORDER.filter((e) => all.some((f) => f.era === e))],
    };
  }, []);
  const rows = useMemo(() => {
    if (tab === 'ships') return d.ships.filter((s) => {
      const f = shipFacets(s, raceName);
      return (!faction || f.faction === faction) && (!role || f.role === role) && (!era || f.era === era) && (!source || f.source === source)
        && (!q || `${s.designClass} ${s.designName} ${s.description} ${s.roster?.source ?? ''}`.toLowerCase().includes(q));
    }).map((s) => {
      const f = shipFacets(s, raceName);
      return { id: s.id, title: `${s.designClass}`, sub: `${s.designName} · ${factionLabel(f.faction)}${s.roster ? ` · ${eraLabel(f.era)}` : ''}`,
        badge: s.roster && s.roster.sourceKind !== 'Canon' ? sourceLabel(s.roster.sourceKind) : `${fmt(s.industryCost)} ind` };
    });
    if (tab === 'technologies') return d.technologies.filter((t) => (field < 0 || t.field === field) && (!q || `${t.name} ${t.description}`.toLowerCase().includes(q)))
      .map((t) => ({ id: t.id, title: t.name, sub: `${m.researchFields[t.field]} level ${t.level}`, badge: `${fmt(t.cost)} pts` }));
    if (tab === 'structures') return d.buildings.filter((b) => (field < 0 || b.field === field) && (!q || `${b.name} ${b.description}`.toLowerCase().includes(q)))
      .map((b) => ({ id: b.id, title: b.name, sub: `${m.researchFields[b.field] ?? '—'} ${b.level}`, badge: `${fmt(b.industryCost)} ind` }));
    if (tab === 'minors') return (d.minorRaces ?? []).filter((r) => !q || `${r.raceName} ${r.homeworld} ${r.temperament} ${r.description}`.toLowerCase().includes(q))
      .map((r) => ({ id: r.raceId, title: r.raceName, sub: `${r.temperament} · ${r.homeworld}`, badge: r.bonus }));
    return d.races.filter((r) => !q || r.name.toLowerCase().includes(q)).map((r) => ({ id: r.id, title: r.name, sub: r.id <= 4 ? 'Major power' : 'Minor race', badge: '' }));
  }, [tab, q, faction, role, era, source, field]);
  const current = selected !== null ? rows.find((r) => r.id === selected) ?? rows[0] : rows[0];

  return (
    <div class="screen pad encyclopedia-screen" data-testid="screen-encyclopedia">
      <div class="ency-grid">
        <Frame title="Database" sub={`${rows.length} entries`} class="fill"
          actions={<Tabs value={tab} onChange={(t) => { setTab(t); setSelected(null); }} options={[
            { id: 'ships', label: 'Ships' }, { id: 'technologies', label: 'Tech' }, { id: 'structures', label: 'Structures' }, { id: 'races', label: 'Races' }, { id: 'minors', label: 'Minor races' },
          ]} />}>
          <div class="ency-filters">
            <input class="text-input grow" type="search" placeholder="Search…" value={query} aria-label="Search the database" data-testid="ency-search"
              onInput={(e) => setQuery((e.target as HTMLInputElement).value)} />
            {tab === 'ships' ? (
              <>
                <select class="text-input" value={faction} aria-label="Faction" data-testid="ency-faction" onChange={(e) => setFaction((e.target as HTMLSelectElement).value)}>
                  <option value="">All factions</option>
                  {facets.factions.map((f) => <option key={f} value={f}>{factionLabel(f)}</option>)}
                </select>
                <select class="text-input" value={role} aria-label="Role" data-testid="ency-role" onChange={(e) => setRole((e.target as HTMLSelectElement).value)}>
                  <option value="">All roles</option>
                  {facets.roles.map((r) => <option key={r} value={r}>{r}</option>)}
                </select>
                <select class="text-input" value={era} aria-label="Era" data-testid="ency-era" onChange={(e) => setEra((e.target as HTMLSelectElement).value)}>
                  <option value="">All eras</option>
                  {facets.eras.map((x) => <option key={x} value={x}>{x === 'Legacy' ? 'Legacy catalogue' : eraLabel(x)}</option>)}
                </select>
                <select class="text-input" value={source} aria-label="Source" data-testid="ency-source" onChange={(e) => setSource((e.target as HTMLSelectElement).value)}>
                  <option value="">All sources</option>
                  {['Legacy', 'Canon', 'ModDerived', 'Kelvin'].map((x) => <option key={x} value={x}>{x === 'Legacy' ? 'Legacy catalogue' : sourceLabel(x)}</option>)}
                </select>
              </>
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
          {!current ? <Empty>Select an entry.</Empty> : tab === 'minors' ? <MinorEntry id={current.id} /> : tab === 'ships' ? <ShipEntry id={current.id} /> : tab === 'technologies' ? <TechEntry id={current.id} /> : tab === 'structures' ? <StructureEntry id={current.id} /> : (
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

function MinorEntry({ id }: { id: number }) {
  const r = (data().minorRaces ?? []).find((x) => x.raceId === id);
  const m = meta.value!;
  if (!r) return <Empty>Unknown race.</Empty>;
  return (
    <div class="ency-entry" data-testid="minor-entry">
      <div class="ee-head"><span class="minor-glyph big">◇</span><div><h3>{r.raceName}</h3><div class="muted">Homeworld {r.homeworld} · {r.temperament}</div></div></div>
      <p>{r.description}</p>
      <div class="minor-facts">
        <div><span>Temperament</span><b>{r.temperament}</b></div>
        <div><span>Join bonus</span><b>{r.bonus}</b></div>
        <div><span>Tech level</span><b>{r.techLevel} (+ era)</b></div>
        <div><span>Population</span><b>{r.population}</b></div>
        {r.signatureShip ? <div><span>Signature ship</span><b>{r.signatureShip}</b></div> : null}
      </div>
      <div class="affinity-row" aria-label="Affinity toward the major powers">
        {r.affinity.map((a, i) => (
          <span key={i} class={`affinity a${a > 0 ? 'pos' : a < 0 ? 'neg' : 'zero'}`}><Emblem empire={i + 1} size={16} /> {m.empires.find((e) => e.id === i + 1)?.name ?? ''} {a > 0 ? `+${a}` : a}</span>
        ))}
      </div>
    </div>
  );
}

function ShipEntry({ id }: { id: number }) {
  const s = data().ships.find((x) => x.id === id)!;
  const m = meta.value!;
  const r = s.roster;
  const emp = RACE_TO_EMPIRE[s.race];
  return (
    <div class="ency-entry" data-testid="ship-entry">
      <div class="ee-head">{emp ? <Emblem empire={emp} size={46} /> : null}<div><h3>{s.designClass} class</h3>
        <div class="muted">{s.designName} · {r ? roleLabel(r.hullRole) : ROLE_NAMES[s.role] ?? `Role ${s.role}`}{r ? ` · ${factionLabel(r.faction)}` : ''}</div></div></div>
      {r ? (
        <div class="req-row roster-tags" data-testid="roster-tags">
          <Badge tone="accent">{eraLabel(r.era)}</Badge><Badge tone={r.sourceKind === 'Canon' ? 'good' : 'warn'}>{sourceLabel(r.sourceKind)}</Badge>
          <Badge tone="dim">tier {r.tier}</Badge>{r.buildable ? <Badge tone="good">buildable (Remastered)</Badge> : <Badge tone="dim">reference only</Badge>}
          {r.cloak ? <Badge tone="warn">cloak</Badge> : null}
        </div>
      ) : <div class="req-row"><Badge tone="dim">Legacy catalogue</Badge></div>}
      <div class="spec-grid">
        <div><span>Hull</span><b>{s.hull}</b></div><div><span>Shields</span><b>{s.shields}</b></div><div><span>Attack</span><b>{fmt(s.attack)}</b></div>
        <div><span>Speed</span><b>{s.speed}</b></div><div><span>Range</span><b>{s.range}</b></div>
        {r ? <div><span>Sensors</span><b>{r.sensors}</b></div> : <div><span>Troops</span><b>{s.troops >= 0 ? s.troops : '—'}</b></div>}
        <div><span>Industry</span><b>{fmt(s.industryCost)}</b></div><div><span>Upkeep</span><b>{s.maintenance}</b></div>
        {r ? <div><span>Torpedo share</span><b>{r.torpedoSharePercent}%</b></div> : null}
      </div>
      {r && r.weapons.length ? (<>
        <h4>Weapon mounts</h4>
        <ul class="weapon-list" data-testid="weapon-list">
          {r.weapons.map((w, i) => <li key={i} class={`wl-${w.torpedo ? 'torpedo' : 'beam'}`}><b>{w.count}×</b> {weaponLabel(w.type)} <span class="muted">{arcLabel(w.arc)} · {w.key}</span></li>)}
        </ul>
      </>) : null}
      <h4>Technology requirements</h4>
      <div class="req-row">{s.requires.map((q, i) => <Badge key={i} tone={q > 0 ? 'accent' : 'dim'}>{m.researchFields[i]} {q}</Badge>)}</div>
      <p class="ee-desc">{s.description}</p>
      {r ? <p class="muted small">Source: {r.source} · asset <code>{r.assetKey}</code></p> : null}
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
