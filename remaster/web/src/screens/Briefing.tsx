import { useEffect, useRef, useState } from 'preact/hooks';
import type { BriefingEvent } from '../core/types';
import { briefing, briefingOpen, focusMap, goto, selectFleet, selectSystem, view } from '../state/store';
import { Badge, fmt, Pill, signed } from '../ui/components';
import { CATEGORY_LABELS } from '../ui/theme';

const ORDER = ['combat', 'diplomacy', 'intel', 'construction', 'research', 'exploration', 'fleets', 'economy'];

export function BriefingPanel() {
  const b = briefing.value!;
  const v = view.value!;
  const ref = useRef<HTMLDivElement>(null);
  const cats = ORDER.filter((c) => b.events.some((e) => e.category === c));
  for (const e of b.events) if (!cats.includes(e.category)) cats.push(e.category);
  const [filter, setFilter] = useState<string | 'all'>('all');
  useEffect(() => { ref.current?.querySelector<HTMLButtonElement>('.briefing-close')?.focus(); }, []);
  const close = () => { briefingOpen.value = false; };
  const open = (e: BriefingEvent) => {
    close();
    if (e.category === 'combat' && e.battle !== undefined) { goto('combat'); return; }
    if (e.systemId) {
      const own = v.colonies.some((c) => c.systemId === e.systemId);
      selectSystem(e.systemId);
      if (e.category === 'construction' && own) { goto('system'); return; }
    } else if (e.fleetId) selectFleet(e.fleetId);
    if (e.category === 'research') { goto('research'); return; }
    if (e.category === 'diplomacy' && e.x === undefined) { goto('diplomacy'); return; }
    if (e.category === 'intel' && e.x === undefined) { goto('intel'); return; }
    if (e.x !== undefined && e.y !== undefined) { goto('galaxy'); focusMap(e.x, e.y, 1.7); }
  };
  const credits = b.creditsAfter - b.creditsBefore;
  const shown = b.events.filter((e) => filter === 'all' || e.category === filter);
  return (
    <div class="modal-backdrop briefing-backdrop" onClick={(e) => { if (e.target === e.currentTarget) close(); }}>
      <div class="briefing" ref={ref} role="dialog" aria-modal="true" aria-label={`Turn ${b.turn} briefing`} data-testid="briefing">
        <header class="briefing-head">
          <div class="bh-title">
            <span class="bh-kicker">Turn briefing</span>
            <h2>Turn {b.turn} {b.outcome === 'victory' ? '— Victory' : b.outcome === 'defeat' ? '— Defeat' : 'report'}</h2>
          </div>
          <div class="bh-stats">
            <div><span>Credits</span><b class={credits < 0 ? 'neg' : 'pos'}>{signed(credits)}</b></div>
            <div><span>Treasury</span><b>{fmt(b.creditsAfter)}</b></div>
            <div><span>Research</span><b>+{fmt(b.researchGained)}</b></div>
            <div><span>Orders</span><b>{b.ordersApplied}</b></div>
          </div>
          <button type="button" class="briefing-close pill pill-primary" onClick={close} data-testid="briefing-close"><span class="pill-label">Acknowledge</span><kbd>Enter</kbd></button>
        </header>
        <div class="briefing-tabs" role="tablist">
          <button type="button" role="tab" aria-selected={filter === 'all'} class={`bt${filter === 'all' ? ' active' : ''}`} onClick={() => setFilter('all')}>All <span>{b.events.length}</span></button>
          {cats.map((c) => (
            <button type="button" role="tab" key={c} aria-selected={filter === c} class={`bt cat-${c}${filter === c ? ' active' : ''}`} onClick={() => setFilter(c)}>
              <i class={`cat-dot cat-${c}`} />{CATEGORY_LABELS[c] ?? c} <span>{b.events.filter((e) => e.category === c).length}</span>
            </button>
          ))}
        </div>
        <div class="briefing-body">
          {cats.filter((c) => filter === 'all' || filter === c).map((c) => (
            <section class={`brief-group cat-${c}`} key={c}>
              <h3><i class={`cat-dot cat-${c}`} />{CATEGORY_LABELS[c] ?? c}</h3>
              <ul>
                {shown.filter((e) => e.category === c).map((e, i) => {
                  const actionable = e.systemId !== undefined || e.fleetId !== undefined || e.x !== undefined || ['research', 'diplomacy', 'intel', 'combat'].includes(e.category);
                  return (
                    <li key={i} class={`brief-event sev-${e.severity}`}>
                      <button type="button" onClick={() => open(e)} disabled={!actionable} data-testid="briefing-event">
                        <span class="be-sev" aria-hidden="true" />
                        <span class="be-text">{e.text}</span>
                        {actionable ? <span class="be-go">{e.category === 'combat' && e.battle !== undefined ? 'Replay' : e.category === 'research' ? 'Research' : 'Show'} ›</span> : null}
                      </button>
                    </li>
                  );
                })}
              </ul>
            </section>
          ))}
          {b.events.length === 0 ? <p class="muted">A quiet turn. Production and research continue.</p> : null}
        </div>
        <footer class="briefing-foot">
          {b.colonies.map((c) => (
            <Badge key={c.systemId} tone={c.economy.foodDeficit ? 'alert' : 'dim'} tipText={`Food ${c.economy.food} · industry ${c.economy.industry} · research ${c.economy.research} · net ${signed(c.economy.net)}`}>
              {c.name}{c.completed ? ` ✓ ${c.completed}` : ''}
            </Badge>
          ))}
          <span class="grow" />
          <Pill small tone="ghost" onClick={close}>Close</Pill>
        </footer>
      </div>
    </div>
  );
}
