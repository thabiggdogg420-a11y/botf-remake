import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { Forecast, ResearchView } from '../core/types';
import { issue, reportError, view } from '../state/store';
import { Badge, Empty, fmt, Frame, IconButton, Meter, Pill, Slider, Stat, tip } from '../ui/components';
import { RESEARCH_BLURBS } from '../ui/theme';

/** Rebalances unlocked fields so the allocation keeps summing to 100 (input handling only). */
function rebalance(alloc: number[], field: number, value: number, locked: boolean[]): number[] {
  const next = [...alloc];
  const lockedTotal = next.reduce((s, v, i) => (i !== field && locked[i] ? s + v : s), 0);
  next[field] = Math.max(0, Math.min(100 - lockedTotal, Math.round(value)));
  const others = next.map((_, i) => i).filter((i) => i !== field && !locked[i]);
  let remaining = 100 - lockedTotal - next[field]!;
  if (!others.length) { next[field] = 100 - lockedTotal; return next; }
  const current = others.reduce((s, i) => s + next[i]!, 0);
  others.forEach((i) => { next[i] = current > 0 ? Math.floor((next[i]! / current) * remaining) : Math.floor(remaining / others.length); });
  let diff = remaining - others.reduce((s, i) => s + next[i]!, 0);
  for (let k = 0; diff > 0; k = (k + 1) % others.length, diff--) next[others[k]!]! += 1;
  remaining = 0;
  return next;
}

export function ResearchScreen() {
  const v = view.value!;
  const research = useMemo<ResearchView | null>(() => { try { return core.research(); } catch (e) { reportError(e); return null; } }, [v.meta.version]);
  const forecast = useMemo<Forecast | null>(() => { try { return core.forecast(); } catch { return null; } }, [v.meta.version]);
  const current = v.empire.techs.map((t) => t.allocation);
  const [alloc, setAlloc] = useState<number[]>(current);
  const [locked, setLocked] = useState<boolean[]>([false, false, false, false, false, false]);
  const [focus, setFocus] = useState(0);
  useEffect(() => setAlloc(v.empire.techs.map((t) => t.allocation)), [v.meta.version]);
  const dirty = alloc.some((a, i) => a !== current[i]);
  const locked_ = v.session.active;
  const apply = (next = alloc) => {
    issue({ type: 'AllocateResearch', researchAllocation: next },
      { cancelIds: v.pending.filter((p) => p.type === 'AllocateResearch').map((p) => p.id), success: 'Research allocation ordered for this turn.' });
  };
  if (!research) return <div class="screen pad"><Empty>Research data unavailable.</Empty></div>;
  const field = research.fields[focus]!;
  const next = field.levels[field.level + 1];
  return (
    <div class="screen pad research-screen" data-testid="screen-research">
      <header class="research-head">
        <div>
          <h1>Research</h1>
          <div class="sub">Science output is pooled empire-wide and divided by allocation.</div>
        </div>
        <div class="stat-grid four">
          <Stat label="Output" value={`${research.output}/turn`} tipText="Sum of every colony's laboratory output (core economy)." />
          <Stat label="Next turn" value={forecast && !forecast.error ? `+${fmt(forecast.research)}` : '—'} tipText="Research gained when this turn resolves, from a forecast of the turn with your current orders." />
          <Stat label="Reserve" value={fmt(research.reserve)} tipText="Points banked in fields that have reached the final level." />
          <Stat label="Allocated" value={`${alloc.reduce((a, b) => a + b, 0)}%`} tone={alloc.reduce((a, b) => a + b, 0) === 100 ? 'good' : 'alert'} />
        </div>
        <div class="research-actions">
          <Pill tone="ghost" small onClick={() => setAlloc([17, 17, 17, 17, 16, 16])} disabled={locked_} reason="Finish the battle first">Even split</Pill>
          <Pill tone="ghost" small onClick={() => setAlloc(current)} disabled={!dirty}>Revert</Pill>
          <Pill tone="primary" onClick={() => apply()} disabled={!dirty || locked_} reason={locked_ ? 'Finish the battle first' : 'No changes'} testid="apply-research">Apply allocation</Pill>
        </div>
      </header>
      <div class="research-grid">
        {research.fields.map((f, i) => {
          const cost = f.levels[f.level + 1]?.cost ?? 0;
          const gained = forecast?.fields?.find((x) => x.field === i)?.gained ?? 0;
          const turns = cost > 0 && gained > 0 ? Math.ceil((cost - f.progress) / gained) : null;
          return (
            <div class={`field-card fc-${i}${focus === i ? ' focus' : ''}`} key={i} data-testid={`field-${i}`}>
              <button type="button" class="field-top" onClick={() => setFocus(i)} {...tip(RESEARCH_BLURBS[i], f.name)}>
                <span class={`field-icon fi-${i}`} aria-hidden="true" />
                <span class="field-name">{f.name}</span>
                <span class="field-level">L{f.level}<small>/{research.techCap ?? 10}</small></span>
              </button>
              <div class="field-tech">
                <span class="ft-now">{f.current}</span>
                {f.levels[f.level + 1] ? <span class="ft-next">→ {f.levels[f.level + 1]!.name}</span> : <span class="ft-next">Mastered</span>}
              </div>
              <div class="field-progress" {...tip(cost ? `${fmt(f.progress)} of ${fmt(cost)} points toward level ${f.level + 1}.\n${gained ? `+${gained} expected next turn` : 'No points expected next turn'}${turns ? ` — about ${turns} turn${turns === 1 ? '' : 's'} at this rate.` : '.'}` : 'All levels researched.')}>
                <Meter value={f.progress} max={Math.max(1, cost)} height={6} tone="accent" />
                <span>{fmt(f.progress)}/{fmt(cost)}{turns ? <em> · {turns}T</em> : null}</span>
              </div>
              <div class="field-alloc">
                <Slider value={alloc[i]!} max={100} label={`${f.name} allocation`} disabled={locked[i] || locked_} testid={`alloc-${i}`}
                  onInput={(val) => setAlloc(rebalance(alloc, i, val, locked))} />
                <span class="alloc-val">{alloc[i]}%</span>
                <IconButton label={locked[i] ? 'Unlock this share' : 'Lock this share while adjusting others'} onClick={() => setLocked(locked.map((l, k) => (k === i ? !l : l)))}>
                  {locked[i] ? '🔒' : '🔓'}
                </IconButton>
              </div>
              <div class="field-foot">
                <Pill small tone="ghost" onClick={() => setAlloc(Array.from({ length: 6 }, (_, k) => (k === i ? 50 : 10)))} disabled={locked_} reason="Finish the battle first"
                  hint="50% here, 10% to every other field">Focus</Pill>
                <Pill small tone="ghost" active={focus === i} onClick={() => setFocus(i)}>Details</Pill>
              </div>
            </div>
          );
        })}
      </div>
      <Frame title={`${field.name} — technology ladder`} sub={`level ${field.level} of ${research.techCap ?? 10}`} tone="secondary" class="research-detail">
        <div class="ladder" role="list" style={{ gridTemplateColumns: `repeat(${field.levels.length}, minmax(0, 1fr))` }}>
          {field.levels.map((l) => (
            <div role="listitem" key={l.level} class={`rung${l.level <= field.level ? ' done' : ''}${l.level === field.level + 1 ? ' next' : ''}`}
              {...tip(`${l.description ?? l.name}\nCost ${fmt(l.cost)} points.`, `${l.level}. ${l.name}`)}>
              <span class="rung-level">{l.level}</span>
              <span class="rung-name">{l.name}</span>
              {l.ships.length || l.structures.length ? <span class="rung-unlocks">{l.ships.length + l.structures.length}</span> : null}
            </div>
          ))}
        </div>
        {next ? (
          <div class="next-tech">
            <div class="nt-head"><h3>Next: {next.name}</h3><Badge tone="accent">{fmt(next.cost)} pts</Badge></div>
            {next.description ? <p class="nt-desc">{next.description.replace(/^[A-Z0-9 '\-]+:\s*/, '')}</p> : null}
            <div class="unlocks">
              <div>
                <h4>Starships requiring {field.name} {next.level}</h4>
                {next.ships.length === 0 ? <p class="muted">None for your empire.</p> : (
                  <ul>{next.ships.map((s) => (
                    <li key={s.id} {...tip(`Requirements: ${s.requires.map((r, k) => `${research.fields[k]!.name} ${r}`).join(', ')}`)}>
                      {s.name} <small>{fmt(s.cost)} ind</small>{s.othersMet ? <Badge tone="good">ready</Badge> : <Badge tone="dim">needs more</Badge>}
                    </li>
                  ))}</ul>
                )}
              </div>
              <div>
                <h4>Reference structures at this level</h4>
                {next.structures.length === 0 ? <p class="muted">None catalogued.</p> : (
                  <ul>{next.structures.slice(0, 8).map((s) => <li key={s.id}>{s.name} <small>{fmt(s.cost)} ind · ⚡{s.energy}</small></li>)}</ul>
                )}
              </div>
            </div>
            {research.blockers.some((b) => b.blocker) ? (
              <p class="hint">Still locked in this ruleset: {research.blockers.filter((b) => b.blocker).map((b) => `${b.name} (${b.blocker})`).join(' · ')}</p>
            ) : null}
          </div>
        ) : <Empty>This field is fully researched.</Empty>}
      </Frame>
    </div>
  );
}
