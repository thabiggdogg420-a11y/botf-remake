import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { ColonyDetail, ConstructionChoice, Economy, GameView, Planet, WorkforcePreview } from '../core/types';
import {
  cancelOrders, confirmDialog, goto, issue, meta, reportError, selectSystem, selection, view,
} from '../state/store';
import { Badge, Empty, fmt, Frame, IconButton, Meter, Pill, signed, Slider, Stat, Tabs, tip } from '../ui/components';
import { eraLabel, sourceLabel, weaponSummary } from '../core/roster';
import { FundingNotice, MegaprojectFrame, TemplatesFrame } from '../ui/Remaster';

export function SystemScreen() {
  const v = view.value!;
  const sel = selection.value;
  const colonies = v.colonies;
  const systemId = colonies.some((c) => c.systemId === sel.systemId) ? sel.systemId! : colonies[0]?.systemId ?? null;
  const detail = useMemo<ColonyDetail | null>(() => {
    if (systemId === null) return null;
    try { return core.colony(systemId); } catch (error) { reportError(error); return null; }
  }, [systemId, v.meta.version]);
  const idx = colonies.findIndex((c) => c.systemId === systemId);
  const cycle = (d: number) => { const next = colonies[(idx + d + colonies.length) % colonies.length]; if (next) selectSystem(next.systemId); };
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA') || e.ctrlKey || e.metaKey) return;
      if (e.key === 'q' || e.key === 'Q') cycle(-1);
      else if (e.key === 'e' || e.key === 'E') cycle(1);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  if (systemId === null || !detail) {
    return <div class="screen pad" data-testid="screen-system"><Frame title="System"><Empty>You have no colonies.</Empty></Frame></div>;
  }
  const c = detail.colony;
  const e = c.economy;
  const locked = v.session.active;

  return (
    <div class="screen system-screen" data-testid="screen-system">
      <header class="system-head">
        <div class="sh-nav">
          <IconButton label="Previous colony (Q)" onClick={() => cycle(-1)} disabled={colonies.length < 2} reason="Only one colony">◂</IconButton>
          <div class="sh-title">
            <h1>{c.name}</h1>
            <div class="sh-sub">Colony {idx + 1} of {colonies.length} · governor {c.governor.workforce === 'Manual' ? 'manual' : c.governor.workforce}</div>
          </div>
          <IconButton label="Next colony (E)" onClick={() => cycle(1)} disabled={colonies.length < 2} reason="Only one colony">▸</IconButton>
          <Pill small tone="ghost" onClick={() => goto('galaxy')} hint="Back to the star chart">Chart</Pill>
        </div>
        <div class="sh-stats">
          <Stat label="Population" value={`${c.population}/${c.capacity}`}
            sub={c.growth.enabled ? <span class={c.growth.max < 0 ? 'neg' : 'pos'}>{c.growth.min === c.growth.max ? signed(c.growth.min) : `${signed(c.growth.min)} to ${signed(c.growth.max)}`}/turn</span> : 'fixed'}
            tipText={`Population ${c.population} of capacity ${c.capacity} (max ${detail.maxCapacity} after terraforming every planet).\nNext turn: ${signed(c.growth.min)}${c.growth.max !== c.growth.min ? ` to ${signed(c.growth.max)} (${c.growth.upChance.toFixed(0)}% chance of the higher)` : ''}.\nFood surplus drives growth; a shortfall shrinks the colony.`} />
          <Stat label="Morale" value={c.morale} tone={c.morale < 50 ? 'warn' : undefined} tipText="Morale drifts toward 70 each turn; events can raise or lower it." />
          <Stat label="Food" value={signed(e.food - c.population)} tone={e.foodDeficit ? 'alert' : 'good'}
            tipText={`${c.structures.farms} farms staffed by ${c.workers.food} workers produce ${e.food} food.\n${c.population} citizens eat ${c.population}.${e.foodDeficit ? `\nShortfall ${e.foodDeficit}: the colony will shrink.` : ''}`} />
          <Stat label="Industry" value={e.industry} tipText={`${c.workers.industry} workers in ${c.structures.factories} factories${e.inactiveFactories ? ` (${e.inactiveFactories} idle for lack of power)` : ''}.\nIndustry builds the first item in the queue each turn.`} />
          <Stat label="Research" value={e.research} tipText={`${c.workers.research} workers in ${c.structures.laboratories} laboratories${e.inactiveLabs ? ` (${e.inactiveLabs} unpowered)` : ''}.\nPooled empire-wide and split by your research allocation.`} />
          <Stat label="Energy" value={`${e.energyUsed}/${e.energyAvailable}`} tone={e.energyUsed > e.energyAvailable ? 'alert' : undefined}
            tipText={`Structures draw ${e.energyUsed} of ${e.energyAvailable} available energy. Power plants add capacity; unpowered structures go idle.`} />
          <Stat label="Credits" value={signed(e.net)} tone={e.net < 0 ? 'alert' : undefined} tipText={`Income ${e.income} − structure upkeep ${e.upkeep} = ${signed(e.net)} per turn.`} />
        </div>
      </header>
      <div class="system-grid">
        <div class="sys-col">
          <PlanetStrip planets={detail.planets} capacity={c.capacity} max={detail.maxCapacity} />
          <Workforce v={v} detail={detail} locked={locked} />
          <Governor detail={detail} locked={locked} />
          <Energy detail={detail} locked={locked} />
        </div>
        <div class="sys-col">
          <BuildOptions detail={detail} locked={locked} />
        </div>
        <div class="sys-col narrow">
          <FundingNotice detail={detail} />
          <BuildQueue v={v} detail={detail} locked={locked} />
          <TemplatesFrame v={v} detail={detail} locked={locked} />
          <MegaprojectFrame v={v} detail={detail} locked={locked} />
        </div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
function PlanetStrip({ planets, capacity, max }: { planets: Planet[]; capacity: number; max: number }) {
  return (
    <Frame title="Planets" sub={`capacity ${capacity} · max ${max}`} tone="tertiary" class="compact">
      <div class="planet-strip" data-testid="planet-strip">
        {planets.map((p) => (
          <div class={`planet-card state-${p.terraformState.replace(/\s/g, '')}`} key={p.index}
            {...tip(`${p.className}\n${p.terraformState}${p.terraformNeeded ? ` — ${p.terraformWork}/${p.terraformNeeded} terraforming work` : ''}\nSupports ${p.capacity} now, ${p.maxCapacity} when terraformed.\nA colony ship at this system can terraform it.`, p.name)}>
            <span class={`planet-orb pc-${p.class}`} aria-hidden="true" />
            <span class="pc-name">{p.name.replace(/^.* (?=[IVX]+$)/, '')}</span>
            <span class="pc-class">{p.class === 'GasGiant' ? 'Gas giant' : p.class === 'Asteroids' ? 'Belt' : `Class ${p.class}`}</span>
            <Meter value={p.capacity} max={Math.max(1, p.maxCapacity)} ghost={p.maxCapacity} height={3} tone="good" />
            <span class="pc-cap">{p.maxCapacity ? `${p.capacity}/${p.maxCapacity}` : '—'}</span>
          </div>
        ))}
      </div>
    </Frame>
  );
}

// ---------------------------------------------------------------------------------------------
const FIELDS = [
  { key: 'food', label: 'Food', structure: 'farms', out: (e: Economy) => e.food, unit: 'food' },
  { key: 'industry', label: 'Industry', structure: 'factories', out: (e: Economy) => e.industry, unit: 'industry' },
  { key: 'research', label: 'Research', structure: 'laboratories', out: (e: Economy) => e.research, unit: 'research' },
] as const;
type FieldKey = (typeof FIELDS)[number]['key'];

function Workforce({ v, detail, locked }: { v: GameView; detail: ColonyDetail; locked: boolean }) {
  const c = detail.colony;
  const base = { food: c.workers.food, industry: c.workers.industry, research: c.workers.research };
  const [draft, setDraft] = useState(base);
  const [preview, setPreview] = useState<WorkforcePreview | null>(null);
  useEffect(() => { setDraft(base); setPreview(null); }, [c.systemId, v.meta.version]);
  const pop = c.population;
  const used = draft.food + draft.industry + draft.research;
  const dirty = draft.food !== base.food || draft.industry !== base.industry || draft.research !== base.research;

  const update = (key: FieldKey, value: number) => {
    const others = used - draft[key];
    const limit = Math.min(detail.capacityLimits[key], pop - others);
    const next = { ...draft, [key]: Math.max(0, Math.min(limit, value)) };
    setDraft(next);
    try { setPreview(core.workforcePreview(c.systemId, next.food, next.industry, next.research)); } catch (error) { reportError(error); }
  };
  const apply = (next = draft) => {
    const p = core.workforcePreview(c.systemId, next.food, next.industry, next.research);
    if (p.error) { setPreview(p); return; }
    issue({ type: 'AssignWorkers', systemId: c.systemId, foodWorkers: next.food, industryWorkers: next.industry, researchWorkers: next.research },
      { cancelIds: p.cancelIds ?? [], success: 'Staffing updated.' });
  };
  const eco = preview && !preview.error && preview.economy ? preview.economy : c.economy;
  const growth = preview && !preview.error && preview.growth ? preview.growth : c.growth;
  const autoBalance = (industryFirst: boolean) => {
    // Feed the colony first (preview-checked), then fill factories / labs.
    let food = 0;
    for (let f = 0; f <= Math.min(pop, detail.capacityLimits.food); f++) {
      const p = core.workforcePreview(c.systemId, f, 0, 0);
      food = f;
      if (!p.error && p.economy && p.economy.foodDeficit === 0) break;
    }
    const rest = pop - food;
    const research = industryFirst ? Math.max(0, Math.min(detail.capacityLimits.research, rest - detail.capacityLimits.industry)) : Math.min(detail.capacityLimits.research, Math.floor(rest / 2));
    const industry = Math.min(detail.capacityLimits.industry, rest - research);
    const next = { food, industry, research: Math.min(detail.capacityLimits.research, rest - industry) };
    setDraft(next);
    apply(next);
  };
  return (
    <Frame title="Workforce" sub={`${pop - used} unassigned`} tone="primary" class="compact" testid="workforce"
      actions={<>
        <Pill small tone="ghost" disabled={locked} reason="Finish the battle first" onClick={() => autoBalance(false)} hint="Feed the colony, then split industry and research">Balance</Pill>
        <Pill small tone="ghost" disabled={locked} reason="Finish the battle first" onClick={() => autoBalance(true)} hint="Feed the colony, then maximise industry">Industry</Pill>
      </>}>
      {FIELDS.map((f) => {
        const limit = detail.capacityLimits[f.key];
        const count = c.structures[f.structure];
        const now = f.out(c.economy), next = f.out(eco);
        return (
          <div class="wf-row" key={f.key}>
            <div class="wf-head">
              <span class={`wf-icon wf-${f.key}`} aria-hidden="true" />
              <span class="wf-label">{f.label}</span>
              <span class="wf-struct" {...tip(`${count} ${f.structure}, each staffed by up to 10 workers (${limit} jobs).`)}>{count} {f.structure}</span>
              <span class="wf-out" {...tip(`Output with these workers: ${next} ${f.unit}${next !== now ? ` (now ${now})` : ''}.`)}>
                {next}{next !== now ? <em class={next > now ? 'pos' : 'neg'}> {signed(next - now)}</em> : null}
                {f.key === 'food' ? <span class="wf-need"> / {pop} needed</span> : null}
              </span>
            </div>
            <div class="wf-control">
              <Slider value={draft[f.key]} max={limit} label={`${f.label} workers`} disabled={locked} testid={`wf-${f.key}`}
                onInput={(val) => update(f.key, val)} onChange={() => { if (!locked) apply(); }} />
              <span class="wf-count">{draft[f.key]}<small>/{limit}</small></span>
            </div>
          </div>
        );
      })}
      <div class="wf-pool">
        <span>Labour pool</span>
        <Meter value={pop - used} max={Math.max(1, pop)} tone="secondary" height={5} />
        <span>{pop - used} idle</span>
      </div>
      <div class="wf-forecast" data-testid="wf-forecast">
        {preview?.error ? <span class="neg">{preview.error}</span> : (
          <>
            <span {...tip('Population change next turn with this staffing (core population forecast).')}>Growth {growth.min === growth.max ? signed(growth.min) : `${signed(growth.min)}…${signed(growth.max)}`}</span>
            <span>Food {signed(eco.food - pop)}</span>
            <span>Credits {signed(eco.net)}</span>
            {dirty ? <Pill small onClick={() => apply()} disabled={locked} reason="Finish the battle first">Apply</Pill> : null}
          </>
        )}
      </div>
      {v.meta.populationEnabled ? null : <p class="hint">Population is fixed in this ruleset.</p>}
    </Frame>
  );
}

function Governor({ detail, locked }: { detail: ColonyDetail; locked: boolean }) {
  const g = detail.colony.governor;
  const m = meta.value!;
  const set = (workforce: string, queue: string) =>
    issue({ type: 'SetColonyGovernor', systemId: detail.colony.systemId, governorWorkforce: workforce, governorQueue: queue }, { success: 'Governor orders updated.' });
  return (
    <Frame title="Governor" sub="automation" tone="secondary" class="compact">
      <div class="gov-row">
        <span class="label" {...tip('Automatically assigns citizens each turn. Manual staffing orders pause it until resumed.')}>Workforce</span>
        <div class="seg">
          {m.governors.map((o) => <Pill key={o.key} small tone="ghost" active={g.workforce === o.key} disabled={locked} reason="Finish the battle first" onClick={() => set(o.key, g.queue)}>{o.name}</Pill>)}
        </div>
      </div>
      <div class="gov-row">
        <span class="label" {...tip('Queues construction automatically whenever the build queue runs empty.')}>Queue</span>
        <div class="seg">
          {m.governors.map((o) => <Pill key={o.key} small tone="ghost" active={g.queue === o.key} disabled={locked} reason="Finish the battle first" onClick={() => set(g.workforce, o.key)}>{o.name}</Pill>)}
        </div>
      </div>
      {g.override && g.workforce !== 'Manual' ? (
        <div class="warn-line">Manual staffing is overriding the governor.
          <Pill small onClick={() => issue({ type: 'ClearGovernorOverride', systemId: detail.colony.systemId }, { success: 'Governor resumed.' })}>Resume governor</Pill></div>
      ) : null}
      {g.warning ? <div class="warn-line">{g.warning}</div> : null}
      {g.workforce !== 'Manual' && g.workforceExplanation ? <p class="hint">{g.workforceExplanation}</p> : null}
      {g.queue !== 'Manual' && g.queueExplanation ? <p class="hint">{g.queueExplanation}</p> : null}
    </Frame>
  );
}

function Energy({ detail, locked }: { detail: ColonyDetail; locked: boolean }) {
  const c = detail.colony, e = c.economy;
  return (
    <Frame title="Energy & shipyard" tone="tertiary" class="compact">
      <div class="bar-row" {...tip(`${e.energyUsed} of ${e.energyAvailable} energy in use.`)}>
        <span>Power</span><Meter value={e.energyUsed} max={Math.max(1, e.energyAvailable)} segments={20} height={8} tone={e.energyUsed > e.energyAvailable ? 'alert' : 'secondary'} /><b>{e.energyUsed}/{e.energyAvailable}</b>
      </div>
      <div class="energy-lines">
        <span {...tip('Power plants give 10 energy each (1 credit upkeep); fusion plants give 40 (2 upkeep). Both can be scrapped from the structures list.')}>
          {c.structures.powerPlants} power plant{c.structures.powerPlants === 1 ? '' : 's'}{c.structures.fusionPlants ? ` · ${c.structures.fusionPlants} fusion plant${c.structures.fusionPlants === 1 ? '' : 's'}` : ''}
        </span>
        {e.inactiveFactories || e.inactiveLabs ? <span class="warn">{e.inactiveFactories} factories, {e.inactiveLabs} labs idle</span> : <span>All structures powered</span>}
        {c.structures.defenseBatteries ? <span>{e.poweredBatteries}/{c.structures.defenseBatteries} batteries powered</span> : null}
      </div>
      <div class="shipyard-line">
        <span>{c.structures.shipyards ? (e.shipyardPowered ? 'Shipyard powered — builds ships and extends range' : 'Shipyard offline') : 'No shipyard'}</span>
        {c.structures.shipyards ? (
          <Pill small tone="ghost" disabled={locked || !!detail.shipyardToggleBlocker} reason={locked ? 'Finish the battle first' : detail.shipyardToggleBlocker}
            onClick={() => issue({ type: 'ToggleShipyard', systemId: c.systemId }, { success: 'Shipyard power change ordered for turn end.' })}>
            Switch {c.shipyardEnabled ? 'off' : 'on'}
          </Pill>
        ) : null}
      </div>
    </Frame>
  );
}

// ---------------------------------------------------------------------------------------------
function BuildOptions({ detail, locked }: { detail: ColonyDetail; locked: boolean }) {
  const [tab, setTab] = useState<'structures' | 'ships' | 'installed'>('structures');
  const structures = detail.choices.filter((c) => !c.isShip);
  const ships = detail.choices.filter((c) => c.isShip);
  const enqueue = (ch: ConstructionChoice) => issue(
    { type: 'EnqueueBuilding', systemId: detail.colony.systemId, building: ch.product, referenceId: ch.referenceId },
    { success: `${ch.name} added to the queue.` });
  const list = tab === 'structures' ? structures : ships;
  return (
    <Frame title="Construction" tone="primary" class="fill" testid="build-options"
      actions={<Tabs value={tab} onChange={setTab} options={[
        { id: 'structures', label: 'Structures', count: structures.filter((s) => !s.blocker).length },
        { id: 'ships', label: 'Starships', count: ships.filter((s) => !s.blocker).length },
        { id: 'installed', label: 'Installed' },
      ]} />}>
      {tab === 'installed' ? (
        <div class="installed-list">
          {detail.scrap.map((s) => (
            <div class="installed-row" key={s.building}>
              <span class="inst-name">{s.name}</span>
              <span class="inst-count">×{s.count}</span>
              <span class="inst-meta" {...tip(`Energy ${s.energyCost} each · upkeep ${s.upkeep} credits each`)}>⚡{s.energyCost} · ¢{s.upkeep}</span>
              <Pill small tone="alert" disabled={locked || !!s.blocker || s.count === 0} reason={locked ? 'Finish the battle first' : s.blocker || 'None installed'}
                onClick={() => confirmDialog.value = {
                  title: `Scrap a ${s.name.toLowerCase()}?`, body: 'The structure is demolished at turn end. Cancel the order to undo it before then.', confirm: 'Scrap', tone: 'alert',
                  onConfirm: () => issue({ type: 'ScrapStructure', systemId: detail.colony.systemId, building: s.building }, { success: `${s.name} will be scrapped.` }),
                }}>Scrap</Pill>
            </div>
          ))}
        </div>
      ) : list.length === 0 ? <Empty>{tab === 'ships' ? 'No starships available — build and power a shipyard.' : 'Nothing can be built here right now.'}</Empty> : (
        <div class="choice-list" role="list">
          {list.map((ch) => {
            const disabled = locked || !!ch.blocker;
            const reason = locked ? 'Finish the battle first' : ch.blocker;
            const details = ch.isShip && ch.hull !== undefined
              ? `${ch.designClass ?? ''} ${ch.designName ?? ''}${ch.roster ? ` · ${eraLabel(ch.roster.era)} · ${sourceLabel(ch.roster.sourceKind)}` : ''}\nHull ${ch.hull} · shields ${ch.shields} · attack ${ch.attack}\nSpeed ${ch.speed} · range ${ch.range} · upkeep ${ch.maintenance}${ch.roster?.weapons.length ? `\nWeapons: ${weaponSummary(ch.roster.weapons)}` : ''}\n${ch.description ?? ''}`
              : `${ch.name}\nIndustry ${ch.cost}${ch.energyCost !== undefined ? ` · energy ${ch.energyCost}` : ''} · upkeep ${ch.upkeep} credits/turn`;
            return (
              <button type="button" role="listitem" key={`${ch.product}-${ch.referenceId}`} class={`choice${disabled ? ' disabled' : ''}`}
                aria-disabled={disabled ? 'true' : undefined} data-testid={`choice-${ch.product}-${ch.referenceId}`}
                onClick={() => { if (!disabled) enqueue(ch); }}
                {...tip(disabled ? `Unavailable: ${reason}` : details, ch.name)}>
                <span class={`choice-icon ci-${ch.isShip ? 'ship' : ch.product}`} aria-hidden="true" />
                <span class="choice-main">
                  <span class="choice-name">{ch.name}{ch.roster ? <span class="roster-chip" data-testid="roster-chip">{eraLabel(ch.roster.era)}</span> : null}</span>
                  <span class="choice-meta">
                    {ch.isShip && ch.hull !== undefined ? <>H{ch.hull} · S{ch.shields} · A{ch.attack} · </> : null}
                    {fmt(ch.cost)} ind · {ch.upkeep ? `¢${ch.upkeep}/t` : 'no upkeep'}
                  </span>
                  {disabled && reason ? <span class="choice-block">{reason}</span> : null}
                </span>
                <span class="choice-turns">{ch.turns > 0 ? `${ch.turns}T` : '∞'}</span>
                <span class="choice-add" aria-hidden="true">+</span>
              </button>
            );
          })}
        </div>
      )}
    </Frame>
  );
}

function BuildQueue({ v, detail, locked }: { v: GameView; detail: ColonyDetail; locked: boolean }) {
  const sid = detail.colony.systemId;
  const pendingBuy = v.pending.find((p) => p.type === 'BuyConstruction' && p.systemId === sid);
  const move = (from: number, to: number) => issue({ type: 'MoveConstruction', systemId: sid, queueIndex: from, queueDestination: to });
  return (
    <Frame title="Build queue" sub={`${detail.queue.length} item${detail.queue.length === 1 ? '' : 's'}`} tone="secondary" class="fill" testid="build-queue">
      {detail.queueBlocker ? <div class="warn-line">{detail.queueBlocker}</div> : null}
      {detail.queue.length === 0 ? <Empty>The queue is empty. Choose a structure or starship to build.</Empty> : (
        <ol class="queue-list">
          {detail.queue.map((q, i) => (
            <li key={`${i}-${q.name}`} class={`queue-item${i === 0 ? ' current' : ''}${q.pinned ? ' pinned' : ''}`} data-testid={`queue-${i}`}>
              <div class="qi-head">
                <span class="qi-index">{i + 1}</span>
                <span class="qi-name">{q.name}</span>
                <span class="qi-eta" {...tip(q.eta > 0 ? `Estimated completion in ${q.eta} turn${q.eta === 1 ? '' : 's'} at the current industry of ${detail.colony.economy.industry}/turn (items complete in order).` : 'No industry: this item makes no progress.')}>
                  {q.eta > 0 ? `${q.eta}T` : '—'}
                </span>
              </div>
              <Meter value={q.work} max={q.cost} height={4} tone={i === 0 ? 'accent' : 'secondary'} />
              <div class="qi-foot">
                <span>{fmt(q.work)} / {fmt(q.cost)}</span>
                <span class="qi-actions">
                  <IconButton label={q.pinned ? 'Unpin (allow the governor to change it)' : 'Pin (protect from governor changes)'} disabled={locked} reason="Finish the battle first"
                    onClick={() => issue({ type: 'SetQueuePin', systemId: sid, queueIndex: i, queuePinned: !q.pinned })}>{q.pinned ? '◆' : '◇'}</IconButton>
                  <IconButton label="Move up" disabled={locked || i === 0} reason={locked ? 'Finish the battle first' : 'Already first'} onClick={() => move(i, i - 1)}>▲</IconButton>
                  <IconButton label="Move down" disabled={locked || i === detail.queue.length - 1} reason={locked ? 'Finish the battle first' : 'Already last'} onClick={() => move(i, i + 1)}>▼</IconButton>
                  <IconButton label="Remove (invested work is lost)" disabled={locked} reason="Finish the battle first" testid={`queue-remove-${i}`}
                    onClick={() => issue({ type: 'RemoveConstruction', systemId: sid, queueIndex: i }, { success: `${q.name} removed.` })}>✕</IconButton>
                </span>
              </div>
            </li>
          ))}
        </ol>
      )}
      <div class="buy-box">
        {pendingBuy ? (
          <>
            <span>Purchase ordered — completes at turn end.</span>
            <Pill small tone="ghost" onClick={() => cancelOrders([pendingBuy.id], 'Purchase cancelled.')}>Cancel</Pill>
          </>
        ) : (
          <>
            <span {...tip(`Buying finishes the current item at turn end. Cost ${fmt(detail.rushCost)} credits for the remaining work. Treasury ${fmt(v.empire.draftCredits)}.`, 'Buy')}>
              Buy current item <b>¢{fmt(detail.rushCost)}</b>
            </span>
            <Pill small tone="secondary" disabled={locked || !!detail.buyBlocker || detail.queue.length === 0} testid="buy"
              reason={locked ? 'Finish the battle first' : detail.buyBlocker || 'Nothing to buy'}
              onClick={() => issue({ type: 'BuyConstruction', systemId: sid }, { success: 'Purchase ordered.' })}>Buy</Pill>
          </>
        )}
      </div>
      <div class="queue-summary">
        <Badge tone="accent" tipText="Industry invested in the first item each turn">{detail.colony.economy.industry} ind/turn</Badge>
        {detail.overflow ? <Badge tone="good" tipText="Surplus carried from the last completed item">+{detail.overflow} carried</Badge> : null}
      </div>
    </Frame>
  );
}
