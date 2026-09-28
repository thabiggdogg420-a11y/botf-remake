import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { Command, IntelView } from '../core/types';
import { intelFocus } from '../state/navigation';
import { dryRun, issue, meta, reportError, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Meter, Pill, Slider, Stat, Tabs, tip } from '../ui/components';

interface Budget { security: number; rivals: Record<number, { espionage: number; sabotage: number }> }

export function IntelScreen() {
  const v = view.value!;
  const intel = useMemo<IntelView | null>(() => { try { return core.intel(); } catch (e) { reportError(e); return null; } }, [v.meta.version]);
  const [tab, setTab] = useState<'operations' | 'reports'>('operations');
  const fromView = (i: IntelView | null): Budget => ({
    security: i?.budget.security ?? 100,
    rivals: Object.fromEntries((i?.rivals ?? []).map((r) => {
      const a = i?.budget.rivals.find((x) => x.id === r.id);
      return [r.id, { espionage: a?.espionage ?? 0, sabotage: a?.sabotage ?? 0 }];
    })),
  });
  const [budget, setBudget] = useState<Budget>(() => fromView(intel));
  useEffect(() => setBudget(fromView(intel)), [intel]);
  const [highlight, setHighlight] = useState<number | null>(intelFocus.value);
  useEffect(() => { if (intelFocus.value) { setHighlight(intelFocus.value); intelFocus.value = null; } }, []);
  if (!intel) return <div class="screen pad"><Empty>Intelligence unavailable.</Empty></div>;

  const original = fromView(intel);
  const dirty = JSON.stringify(original) !== JSON.stringify(budget);
  const rivalIds = intel.rivals.map((r) => r.id);
  const setShare = (id: number, kind: 'espionage' | 'sabotage', value: number) => {
    const cur = budget.rivals[id] ?? { espionage: 0, sabotage: 0 };
    const others = Object.entries(budget.rivals).reduce((s, [rid, a]) => s + (Number(rid) === id ? (kind === 'espionage' ? a.sabotage : a.espionage) : a.espionage + a.sabotage), 0);
    const clamped = Math.max(0, Math.min(100 - others, value));
    setBudget({ security: 100 - others - clamped, rivals: { ...budget.rivals, [id]: { ...cur, [kind]: clamped } } });
  };
  const budgetCommand: Command = {
    type: 'SetIntelBudget', intelInternalSecurity: budget.security, intelRivalIds: rivalIds,
    intelEspionagePcts: rivalIds.map((id) => budget.rivals[id]?.espionage ?? 0), intelSabotagePcts: rivalIds.map((id) => budget.rivals[id]?.sabotage ?? 0),
  };
  const budgetCancel = v.pending.filter((p) => p.type === 'SetIntelBudget').map((p) => p.id);
  const budgetReason = v.session.active ? 'Finish the battle first' : dirty ? (dryRun(budgetCommand, budgetCancel) || undefined) : 'No changes';

  return (
    <div class="screen pad intel-screen" data-testid="screen-intel">
      <header class="research-head">
        <div><h1>Intelligence</h1><div class="sub">Divide the intelligence budget between internal security and operations against each rival.</div></div>
        <div class="stat-grid four">
          <Stat label="Reserve" value={fmt(intel.reserve)} tipText="Intelligence points available to classic operations." />
          <Stat label="Output" value={`${fmt(intel.output)}/turn`} tipText="Intelligence generated each turn, split by the budget below." />
          <Stat label="Security" value={intel.defenseStrengthMilli !== undefined ? (intel.defenseStrengthMilli / 1000).toFixed(1) : fmt(intel.securityStockpile)} tipText={`Counter-intelligence strength (20-turn security average + 2% of the ${fmt(intel.securityStockpile)} stockpile, at full morale). Rival operations are measured against it.`} />
          <Stat label="Security share" value={`${budget.security}%`} tone={budget.security < 50 ? 'warn' : undefined} />
        </div>
        <div class="research-actions">
          <Pill small tone="ghost" disabled={!dirty} onClick={() => setBudget(original)}>Revert</Pill>
          <Pill disabled={!!budgetReason} reason={budgetReason} testid="apply-intel" onClick={() => issue(budgetCommand, { cancelIds: budgetCancel, success: 'Intelligence budget ordered.' })}>Apply budget</Pill>
        </div>
      </header>
      <Frame title={tab === 'operations' ? 'Operations' : 'Reports'} class="fill"
        actions={<Tabs value={tab} onChange={setTab} options={[{ id: 'operations', label: 'Operations' }, { id: 'reports', label: 'Reports', count: intel.reports.length + intel.operations.length }]} />}>
        {tab === 'operations' ? (
          <div class="intel-grid">
            <div class="intel-card security-card">
              <div class="ic-head"><Emblem empire={v.meta.player} size={28} /><div><div class="ic-name">Internal security</div><div class="ic-sub">Protects your colonies from rival operations</div></div></div>
              <Meter value={budget.security} max={100} height={10} tone="good" />
              <div class="ic-big">{budget.security}%</div>
              <p class="hint">Security receives whatever is not assigned to operations.</p>
            </div>
            {intel.rivals.map((r) => {
              const a = budget.rivals[r.id] ?? { espionage: 0, sabotage: 0 };
              return <RivalCard key={r.id} r={r} a={a} highlight={highlight === r.id} onShare={(k, val) => setShare(r.id, k, val)} locked={v.session.active} />;
            })}
          </div>
        ) : (
          <div class="report-list">
            {intel.operations.map((o, i) => (
              <div class={`report ${o.mine ? (o.success ? 'good' : 'warn') : 'alert'}`} key={`o${i}`}>
                <span class="ev-turn">T{o.turn}</span>
                <span class="rp-main"><b>{o.mine ? `${o.area} vs ${o.defenderName}` : `Intrusion by ${o.attackerName}`}</b> · {o.department} · {o.outcome}
                  {o.creditsStolen ? ` · ¢${fmt(o.creditsStolen)} taken` : ''}{o.researchStolen ? ` · ${fmt(o.researchStolen)} research taken` : ''}
                  {o.details ? <span class="rp-detail">{o.details}</span> : null}</span>
                {o.detected ? <Badge tone="alert" tipText={o.mine ? 'Their counter-intelligence exposed our agents: trust suffers and they hold a grievance.' : 'Our counter-intelligence caught their agents.'}>{o.mine ? 'exposed' : 'detected'}</Badge> : null}
              </div>
            ))}
            {intel.reports.map((r, i) => (
              <div class={`report ${r.success ? 'good' : 'warn'}`} key={`r${i}`}>
                <span class="ev-turn">T{r.turn}</span>
                <span class="rp-main"><b>{r.targetName}</b> · {r.success ? `${r.knownColonies} colonies, about ¢${fmt(r.estimatedCredits)}` : 'operation failed'}{r.system ? ` · ${r.system}` : ''}</span>
              </div>
            ))}
            {!intel.operations.length && !intel.reports.length ? <Empty>No operations have completed yet.</Empty> : null}
          </div>
        )}
      </Frame>
    </div>
  );
}

function RivalCard({ r, a, onShare, locked, highlight }: {
  r: IntelView['rivals'][number]; a: { espionage: number; sabotage: number }; onShare: (k: 'espionage' | 'sabotage', v: number) => void; locked: boolean; highlight: boolean;
}) {
  const m = meta.value!;
  const [target, setTarget] = useState<number>(r.visibleColonies[0]?.systemId ?? 0);
  const gather: Command = { type: 'GatherIntelligence', targetEmpire: r.id };
  const sabotage: Command = { type: 'Sabotage', targetEmpire: r.id, targetSystemId: target };
  const gatherReason = locked ? 'Finish the battle first' : dryRun(gather) || undefined;
  const sabotageReason = locked ? 'Finish the battle first' : !target ? 'No colony of theirs is within sensor range' : dryRun(sabotage) || undefined;
  return (
    <div class={`intel-card${highlight ? ' highlight' : ''}${r.contact && !r.eliminated ? '' : ' unknown'}`} data-testid={`intel-${r.id}`}>
      <div class="ic-head">
        <Emblem empire={r.id} size={28} />
        <div><div class="ic-name">{r.name}</div><div class="ic-sub">{r.eliminated ? 'Eliminated — no longer a target' : !r.contact ? 'No contact' : r.atWar ? 'At war — sabotage permitted' : 'At peace'}</div></div>
      </div>
      <div class="ic-row" {...tip('Share of intelligence output spent gathering information and stealing credits or research.', 'Espionage')}>
        <span>Espionage</span><Slider value={a.espionage} max={100} label={`Espionage against ${r.name}`} onInput={(val) => onShare('espionage', val)} disabled={locked} /><b>{a.espionage}%</b>
      </div>
      <div class="ic-row" {...tip('Share spent disrupting their facilities.', 'Sabotage')}>
        <span>Sabotage</span><Slider value={a.sabotage} max={100} label={`Sabotage against ${r.name}`} onInput={(val) => onShare('sabotage', val)} disabled={locked} /><b>{a.sabotage}%</b>
      </div>
      {r.focus ? (
        <div class="ic-row focus-row">
          <span {...tip('Department your agents concentrate on against this rival.')}>Focus</span>
          <div class="seg">
            {m.departments.map((dpt) => (
              <Pill key={dpt.key} small tone="ghost" active={r.focus === dpt.key} disabled={locked} reason="Finish the battle first"
                onClick={() => issue({ type: 'SetIntelFocus', targetEmpire: r.id, intelDepartmentFocus: dpt.key }, { success: `Focus on ${dpt.name.toLowerCase()} targets.` })}>{dpt.name}</Pill>
            ))}
          </div>
        </div>
      ) : null}
      {r.espionageStockpile !== undefined ? <div class="ic-stock">Stockpiles: espionage {fmt(r.espionageStockpile)} · sabotage {fmt(r.sabotageStockpile ?? 0)}{r.attackStrengthMilli !== undefined ? <span {...tip('Our attack strength against them: the 20-turn average of espionage + sabotage output, doubled for concentration. Parity with their security share means even odds; weak operations are often exposed.')}> · strength {(r.attackStrengthMilli / 1000).toFixed(1)}</span> : null}</div> : null}
      <div class="ic-ops">
        <Pill small tone="secondary" disabled={!!gatherReason} reason={gatherReason} onClick={() => issue(gather, { success: 'Espionage mission ordered.' })}>Gather intelligence</Pill>
        {r.visibleColonies.length ? (
          <select class="text-input small" value={target} aria-label="Sabotage target" onChange={(e) => setTarget(Number((e.target as HTMLSelectElement).value))}>
            {r.visibleColonies.map((c) => <option key={c.systemId} value={c.systemId}>{c.name}</option>)}
          </select>
        ) : null}
        <Pill small tone="alert" disabled={!!sabotageReason} reason={sabotageReason} onClick={() => issue(sabotage, { success: 'Sabotage mission ordered.' })}>Sabotage</Pill>
      </div>
    </div>
  );
}
