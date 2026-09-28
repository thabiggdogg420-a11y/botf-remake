// Schema-13 gameplay layer UI: galactic event cards, the victory panel, the end-of-campaign screen,
// the station inspector, anomaly readouts, build templates and the megaproject.
// Everything shown comes from the core through the bridge (stations, features, events, victory).
import { useEffect, useMemo, useRef, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { ColonyDetail, FeatureView, GameView, StationDetail, StationView, VictoryCondition } from '../core/types';
import {
  briefing, briefingOpen, confirmDialog, eventsOpen, focusMap, goto, issue, outcomeDismissed, outcomePreview, quitToTitle,
  reportError, saveName, selectCell, selectSystem, toast, victoryOpen, view,
} from '../state/store';
import { saveToSlot } from '../state/saves';
import { Badge, Emblem, Empty, fmt, Frame, Meter, Pill, Stat, tip } from './components';

// ---------------------------------------------------------------------------------------------
// Galactic events
// ---------------------------------------------------------------------------------------------
export function pendingEventCount(v: GameView): number {
  return (v.events ?? []).filter((e) => e.awaitingChoice && e.drafted < 0).length;
}

function dueText(turn: number, deadline: number): string {
  const left = deadline - turn;
  return left <= 0 ? 'decide this turn — the default applies at turn end' : `decide by turn ${deadline} (${left} turn${left === 1 ? '' : 's'} left)`;
}

export function EventsPanel() {
  const v = view.value;
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => { ref.current?.querySelector<HTMLButtonElement>('button')?.focus(); }, []);
  if (!v || !eventsOpen.value) return null;
  const close = () => { eventsOpen.value = false; };
  const events = [...(v.events ?? [])].sort((a, b) => Number(b.awaitingChoice) - Number(a.awaitingChoice) || a.deadline - b.deadline || a.id - b.id);
  const locked = v.session.active;
  return (
    <div class="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) close(); }}>
      <div class="modal events-panel" ref={ref} role="dialog" aria-modal="true" aria-label="Galactic events" data-testid="events-panel">
        <header class="rp-head">
          <div><span class="rp-kicker">Galactic events</span><h2>Decisions & developments</h2></div>
          <Pill small tone="ghost" onClick={close} kbd="Esc" testid="events-close">Close</Pill>
        </header>
        {events.length === 0 ? <Empty>No galactic events concern your empire right now. New events are drawn every few turns.</Empty> : null}
        <div class="event-cards">
          {events.map((e) => {
            const chosen = e.drafted >= 0 ? e.drafted : e.choice;
            return (
              <article key={e.id} class={`event-card ek-${e.kind}${e.awaitingChoice ? ' open' : ''}`} data-testid={`event-card-${e.id}`}>
                <div class="ec-head">
                  <span class={`ec-glyph ek-${e.kind}`} aria-hidden="true" />
                  <div class="ec-title">
                    <h3>{e.name}</h3>
                    <div class="ec-sub">
                      {e.system ? `${e.system} · ` : e.x !== undefined ? `Sector ${e.x}.${e.y} · ` : ''}
                      since turn {e.startTurn}{e.endTurn > v.meta.turn ? ` · effects until turn ${e.endTurn}` : ''}
                    </div>
                  </div>
                  {e.awaitingChoice
                    ? <Badge tone={e.drafted >= 0 ? 'good' : e.deadline <= v.meta.turn ? 'alert' : 'warn'}>{e.drafted >= 0 ? 'decided' : `due T${e.deadline}`}</Badge>
                    : e.choice >= 0 ? <Badge tone="dim">{e.choiceApplied ? 'resolved' : 'decided'}</Badge> : <Badge tone="dim">active</Badge>}
                </div>
                <p class="ec-desc">{e.description}</p>
                {e.awaitingChoice ? <div class={`ec-due${e.deadline <= v.meta.turn ? ' urgent' : ''}`}>{dueText(v.meta.turn, e.deadline)}</div> : null}
                {e.choices.length ? (
                  <div class="ec-options" role="group" aria-label={`${e.name} options`}>
                    {e.choices.map((o) => {
                      const selected = chosen === o.index;
                      const blocked = locked ? 'Finish the battle in progress first' : o.blocker;
                      return (
                        <button type="button" key={o.index} class={`ec-option${selected ? ' selected' : ''}${!e.awaitingChoice ? ' past' : ''}`}
                          data-testid={`event-${e.id}-option-${o.index}`} aria-pressed={selected}
                          aria-disabled={!e.awaitingChoice || !!blocked ? 'true' : undefined}
                          onClick={() => {
                            if (!e.awaitingChoice || blocked || selected) return;
                            issue({ type: 'ResolveEventChoice', eventId: e.id, eventOption: o.index },
                              { cancelIds: e.draftIds, success: `${e.name}: “${o.label}” — takes effect next turn.` });
                          }}
                          {...tip(blocked ? `Unavailable: ${blocked}` : o.effect, o.label)}>
                          <span class="eo-label">{o.label}{o.isDefault ? <em>default</em> : null}</span>
                          <span class="eo-effect">{o.effect}</span>
                        </button>
                      );
                    })}
                  </div>
                ) : null}
                <div class="ec-foot">
                  {e.x !== undefined && e.y !== undefined && v.fog[e.y * v.meta.width + e.x] !== '0' ? (
                    <Pill small tone="ghost" onClick={() => { close(); goto('galaxy'); focusMap(e.x!, e.y!, 1.7); if (e.systemId) selectSystem(e.systemId); else selectCell(e.y! * v.meta.width + e.x!); }}>Show on chart</Pill>
                  ) : null}
                  {e.awaitingChoice && e.drafted >= 0 ? <span class="muted">Takes effect at the start of next turn. You can change your mind until then.</span> : null}
                </div>
              </article>
            );
          })}
        </div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Victory progress
// ---------------------------------------------------------------------------------------------
const CONDITION_BLURB: Record<VictoryCondition['condition'], string> = {
  Conquest: 'Be the last power holding colonies.',
  Domination: 'Hold 60% of all star systems after turn 20.',
  Scientific: 'Master every field, then complete the era megaproject.',
  Diplomatic: 'Lead an alliance bloc holding two-thirds of the galaxy’s population.',
};

export function VictoryConditions({ v, compact = false }: { v: GameView; compact?: boolean }) {
  return (
    <div class={`victory-list${compact ? ' compact' : ''}`}>
      {v.victory.conditions.map((c) => (
        <div key={c.condition} class={`victory-row vc-${c.condition}${c.achieved ? ' achieved' : ''}${c.enabled ? '' : ' disabled'}`} data-testid={`victory-${c.condition}`}>
          <div class="vr-head">
            <span class={`vr-glyph vc-${c.condition}`} aria-hidden="true" />
            <span class="vr-name">{c.name}</span>
            {c.achieved ? <Badge tone="good">achieved</Badge> : !c.enabled ? <Badge tone="dim">off in this ruleset</Badge> : null}
            <span class="vr-pct">{c.enabled ? `${c.percent}%` : '—'}</span>
          </div>
          <Meter value={c.enabled ? c.percent : 0} max={100} height={compact ? 5 : 8} tone={c.achieved ? 'good' : c.percent >= 75 ? 'warn' : 'accent'} label={`${c.name} progress`} />
          {compact ? null : <div class="vr-blurb">{CONDITION_BLURB[c.condition]}</div>}
          <div class="vr-expl">{c.explanation}</div>
        </div>
      ))}
    </div>
  );
}

export function VictoryPanel() {
  const v = view.value;
  if (!v || !victoryOpen.value) return null;
  const vic = v.victory;
  const close = () => { victoryOpen.value = false; };
  const mp = vic.megaproject;
  return (
    <div class="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) close(); }}>
      <div class="modal victory-panel" role="dialog" aria-modal="true" aria-label="Victory conditions" data-testid="victory-panel">
        <header class="rp-head">
          <div><span class="rp-kicker">Paths to victory · {v.meta.rules.presetName} rules</span><h2>Victory progress</h2></div>
          <Pill small tone="ghost" onClick={close} kbd="Esc" testid="victory-close">Close</Pill>
        </header>
        <VictoryConditions v={v} />
        {vic.alternative ? (
          <div class="megaproject-box">
            <div class="mb-title"><span class="vr-glyph vc-Scientific" aria-hidden="true" />{vic.megaprojectName}</div>
            {mp ? (
              <>
                <div class="mb-line">Under construction at <b>{mp.system}</b> since turn {mp.startTurn}.</div>
                <Meter value={mp.work} max={vic.megaprojectCost} height={8} tone="good" />
                <div class="mb-line mono">{fmt(mp.work)} / {fmt(vic.megaprojectCost)} industry · up to {vic.megaprojectWorkPerTurn}/turn</div>
              </>
            ) : (
              <div class="mb-line">
                Needs every research field at level {vic.threshold}, turn 50 or later and ¢{fmt(vic.megaprojectCredits)}; then {fmt(vic.megaprojectCost)} industry
                (up to {vic.megaprojectWorkPerTurn} per turn) from one colony. Start it from the System screen.
              </div>
            )}
            {vic.rivalProjects.length ? (
              <ul class="mb-rivals">
                {vic.rivalProjects.map((r) => (
                  <li key={r.owner}><Emblem empire={r.owner} size={14} /> {r.ownerName} began theirs on turn {r.startTurn}: {fmt(r.work)} / {fmt(vic.megaprojectCost)}
                    <Meter value={r.work} max={vic.megaprojectCost} height={4} tone="alert" /></li>
                ))}
              </ul>
            ) : null}
            <div class="mb-actions"><Pill small tone="secondary" onClick={() => { close(); goto('system'); }} testid="victory-open-system">Open System screen</Pill></div>
          </div>
        ) : <p class="hint">Classic rules: conquest and domination only.</p>}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// End of campaign
// ---------------------------------------------------------------------------------------------
export function OutcomeScreen() {
  const v = view.value;
  if (!v) return null;
  const preview = outcomePreview.value;
  const outcome = preview ?? (v.meta.outcome !== 'ongoing' && !outcomeDismissed.value ? v.meta.outcome : null);
  if (!outcome) return null;
  const won = outcome === 'victory';
  const achieved = v.victory.conditions.find((c) => c.achieved);
  const winner = v.meta.winner && v.meta.winner !== v.meta.player ? v.empires.find((e) => e.empire === v.meta.winner) : null;
  const rivalProject = winner ? v.victory.rivalProjects.find((r) => r.owner === winner.empire) : undefined;
  const reason = won
    ? achieved ? `${achieved.name} victory — ${achieved.explanation}` : 'Your empire commands the quadrant.'
    : winner ? (rivalProject ? `${winner.name} completed the ${v.victory.megaprojectName}.` : `${winner.name} prevailed.`)
      : v.colonies.length === 0 ? 'Your last colony has fallen.' : 'Your empire has been defeated.';
  const techs = v.empire.techs.reduce((s, t) => s + t.level, 0);
  const dismiss = () => { outcomePreview.value = null; outcomeDismissed.value = true; };
  const save = () => {
    try { saveToSlot(`${saveName.value || `${v.meta.playerName} campaign`} — ${won ? 'victory' : 'defeat'} turn ${v.meta.turn}`); toast('Campaign saved.', 'good'); }
    catch (error) { reportError(error); }
  };
  return (
    <div class={`outcome-screen ${won ? 'won' : 'lost'}`} role="dialog" aria-modal="true" aria-label={won ? 'Victory' : 'Defeat'} data-testid="outcome-screen">
      <div class="os-glow" aria-hidden="true" />
      <div class="os-card">
        <div class="os-emblem"><Emblem empire={won ? v.meta.player : winner?.empire ?? v.meta.player} size={72} /></div>
        <div class="os-kicker">Turn {v.meta.turn} · Stardate {(41000 + v.meta.turn * 25.7).toFixed(1)}</div>
        <h1 class="os-title" data-testid="outcome-title">{won ? 'Victory' : 'Defeat'}</h1>
        <p class="os-reason">{reason}</p>
        <div class="os-stats">
          <Stat label="Turns" value={v.meta.turn} />
          <Stat label="Colonies" value={v.colonies.length} />
          <Stat label="Population" value={fmt(v.empire.population)} />
          <Stat label="Ships" value={v.fleets.length} />
          <Stat label="Stations" value={v.stations.filter((s) => s.mine).length} />
          <Stat label="Treasury" value={fmt(v.empire.credits)} />
          <Stat label="Tech levels" value={techs} tipText={v.empire.techs.map((t) => `${t.name} ${t.level}`).join(' · ')} />
          <Stat label="Contacts" value={v.empires.filter((e) => e.contact).length} />
        </div>
        <div class="os-progress"><VictoryConditions v={v} compact /></div>
        <div class="os-actions">
          {briefing.value ? <Pill tone="ghost" onClick={() => { dismiss(); briefingOpen.value = true; }}>Final briefing</Pill> : null}
          <Pill tone="ghost" onClick={dismiss} testid="outcome-dismiss">View the galaxy</Pill>
          <Pill tone="secondary" onClick={save}>Save</Pill>
          <Pill tone="primary" onClick={() => { dismiss(); quitToTitle(); goto('new'); }}>New campaign</Pill>
          <Pill tone="tertiary" onClick={() => { dismiss(); quitToTitle(); }}>Title</Pill>
        </div>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Stations and anomalies (galaxy inspector)
// ---------------------------------------------------------------------------------------------
export function StationCard({ v, station }: { v: GameView; station: StationView }) {
  const detail = useMemo<StationDetail | null>(() => {
    try { return core.station(station.id); } catch { return null; }
  }, [station.id, v.meta.version]);
  const locked = v.session.active;
  const starbase = station.type === 'Starbase';
  return (
    <div class="card station-card" data-testid="station-card">
      <div class="card-head">
        <span class={`station-glyph ${starbase ? 'sg-starbase' : 'sg-outpost'}`} aria-hidden="true" style={{ '--sg': `var(--owner-${station.owner}, #9aa6b8)` } as never} />
        <div>
          <div class="card-title">{station.name}</div>
          <div class="card-sub"><Emblem empire={station.owner} size={12} /> {station.ownerName} {station.typeName.toLowerCase()} · sector {station.x}.{station.y} · built turn {station.builtTurn}</div>
        </div>
        <Badge tone={starbase ? 'accent' : 'dim'}>{station.typeName}</Badge>
      </div>
      <div class="bars">
        <div class="bar-row" {...tip(`Hull ${station.hull} of ${station.maxHull}. Stations regain 10% hull each turn without battle; one reduced to 0 is destroyed. At 50% or less an enemy transport can board it if no armed ship defends it.`, 'Hull')}>
          <span>Hull</span><Meter value={station.hull} max={station.maxHull} tone="hull" /><b>{station.hull}/{station.maxHull}</b>
        </div>
        <div class="bar-row" {...tip(`Shields ${station.shields} of ${station.maxShields}, fully restored on turns without battle.`, 'Shields')}>
          <span>Shields</span><Meter value={station.shields} max={station.maxShields} tone="shield" /><b>{station.shields}/{station.maxShields}</b>
        </div>
      </div>
      <div class="stat-grid four">
        <Stat label="Attack" value={`${station.beam + station.torpedo}`} tipText={`Beams ${station.beam} · torpedoes ${station.torpedo}. The station fights whenever an enemy at war enters its sector, immobile and braced (−20% damage taken).`} />
        <Stat label="Supply" value={station.supplyRadius} tipText={`Sectors within ${station.supplyRadius} count as supplied for all your ships.`} />
        <Stat label="Sensors" value={station.sensorRadius} tipText={`Reveals ships and colonies within ${station.sensorRadius} sectors.`} />
        <Stat label="Upkeep" value={station.upkeep} tone="warn" tipText="Credits per turn, counted as military upkeep." />
      </div>
      {starbase ? <p class="hint">Repairs friendly ships in this sector by {station.repairPercent}% hull per turn (when they did not fight and no enemy is present).</p> : null}
      {station.mine && station.upgrade ? (
        <div class="upgrade-box">
          <div class="label">Starbase upgrade · funded by {station.upgrade.funding}</div>
          <Meter value={station.upgrade.work} max={station.upgrade.cost} height={6} tone="warn" />
          <div class="mono small">{station.upgrade.work} / {station.upgrade.cost} industry — {station.upgrade.funding}’s construction queue is paused meanwhile.</div>
        </div>
      ) : null}
      {station.mine && !starbase && !station.upgrade && detail ? (
        <div class="upgrade-box" data-testid="station-upgrade">
          <div class="label" {...tip(`Starbase: hull ${detail.starbase.maxHull}, shields ${detail.starbase.maxShields}, attack ${detail.starbase.beam + detail.starbase.torpedo}, supply ${detail.starbase.supplyRadius}, sensors ${detail.starbase.sensorRadius}, repairs ${detail.starbase.repairPercent}%/turn, upkeep ${detail.starbase.upkeep}.`, 'Starbase')}>
            Upgrade to starbase · ¢{fmt(detail.upgradeCredits)} + {fmt(detail.upgradeIndustry)} industry
          </div>
          {detail.funding.length === 0 ? <div class="warn-line">No colony can fund the upgrade.</div> : null}
          {detail.funding.map((f) => (
            <div class="fund-row" key={f.systemId}>
              <span>{f.name}</span>
              <span class="muted">{f.distance} sectors · {f.industry} ind/turn{f.industry > 0 ? ` · ~${Math.ceil(detail.upgradeIndustry / f.industry)}T` : ''}</span>
              <Pill small tone="good" testid={`fund-${f.systemId}`} disabled={locked || !!f.blocker} reason={locked ? 'Finish the battle in progress first' : f.blocker}
                onClick={() => issue({ type: 'UpgradeStation', stationId: station.id, systemId: f.systemId }, { success: `${station.name}: starbase upgrade funded by ${f.name}.` })}>
                Fund
              </Pill>
            </div>
          ))}
        </div>
      ) : null}
      {detail && (detail.ownShipsHere || detail.contactsHere) ? <p class="hint">{detail.ownShipsHere} of your ships and {detail.contactsHere} contacts in this sector.</p> : null}
    </div>
  );
}

const FEATURE_ICON: Record<string, string> = {
  Nebula: 'fg-nebula', Wormhole: 'fg-wormhole', BlackHole: 'fg-blackhole', NeutronStar: 'fg-neutron', IonStorm: 'fg-storm', SubspaceRift: 'fg-rift',
};

export function featuresAtCell(v: GameView, cell: number): FeatureView[] {
  const W = v.meta.width;
  const x = cell % W, y = Math.floor(cell / W);
  return (v.features ?? []).filter((f) => (f.x === x && f.y === y) || (f.cells?.includes(cell) ?? false));
}

export function FeatureCard({ feature }: { feature: FeatureView }) {
  return (
    <div class={`feature-card ${FEATURE_ICON[feature.type] ?? ''}`} data-testid={`feature-${feature.type}`}>
      <div class="fc-head">
        <span class={`feature-glyph ${FEATURE_ICON[feature.type] ?? ''}`} aria-hidden="true" />
        <b>{feature.name}</b>
        {feature.passable ? null : <Badge tone="alert">impassable</Badge>}
        {feature.surveyed ? <Badge tone="good">surveyed</Badge> : <Badge tone="dim" tipText={`A scout ending its turn within 1 sector surveys it: ${feature.surveyReward}.`}>unsurveyed</Badge>}
      </div>
      <p>{feature.effect}</p>
      {feature.type === 'Wormhole' ? (
        <p class="muted">{feature.exit ? <>Exit at sector {feature.exit.x}.{feature.exit.y}. <button type="button" class="link" onClick={() => focusMap(feature.exit!.x, feature.exit!.y, 1.5)}>Show exit</button></> : 'The exit is uncharted — survey or explore it to use the link.'}</p>
      ) : null}
      {feature.type === 'IonStorm' && (feature.driftX || feature.driftY) ? <p class="muted">Drifting {driftName(feature.driftX ?? 0, feature.driftY ?? 0)}.</p> : null}
      {feature.expiresTurn ? <p class="muted">Collapses after turn {feature.expiresTurn}.</p> : null}
      {!feature.surveyed ? <p class="muted">Survey reward: {feature.surveyReward}.</p> : null}
    </div>
  );
}

function driftName(dx: number, dy: number): string {
  const ns = dy < 0 ? 'north' : dy > 0 ? 'south' : '';
  const ew = dx < 0 ? 'west' : dx > 0 ? 'east' : '';
  return `${ns}${ns && ew ? '-' : ''}${ew}` || 'nowhere';
}

// ---------------------------------------------------------------------------------------------
// System screen: build templates and the megaproject
// ---------------------------------------------------------------------------------------------
export function TemplatesFrame({ v, detail, locked }: { v: GameView; detail: ColonyDetail; locked: boolean }) {
  const [name, setName] = useState('');
  const sid = detail.colony.systemId;
  const items = detail.queue.map((q) => ({ product: q.product, referenceId: q.referenceId }));
  const lock = locked ? 'Finish the battle in progress first' : '';
  const define = (templateId: number, templateName: string) => {
    if (issue({ type: 'DefineBuildTemplate', templateId, templateName, templateItems: items },
      { success: templateId ? `Template “${templateName}” updated from this queue.` : `Template “${templateName}” saved.` })) setName('');
  };
  const trimmed = name.trim();
  const saveReason = lock || (!items.length ? 'The build queue is empty' : !trimmed ? 'Name the template' : '');
  return (
    <Frame title="Build templates" sub={`${v.templates.length} saved`} tone="tertiary" class="compact" testid="templates">
      {v.templates.length === 0 ? <p class="hint">Save this colony’s queue as a template, then apply it anywhere. The default template is applied to every new colony with an empty queue.</p> : null}
      <ul class="template-list">
        {v.templates.map((t) => {
          const applyBlocker = lock || detail.templates.find((x) => x.id === t.id)?.blocker || '';
          return (
            <li key={t.id} class={`template-row${t.isDefault ? ' is-default' : ''}`} data-testid={`template-${t.id}`}>
              <div class="tr-head">
                <b class="tr-name">{t.name}</b>
                {t.isDefault ? <Badge tone="good" tipText="Applied automatically to newly founded, joined or captured colonies whose queue is empty.">default</Badge> : null}
              </div>
              <div class="tr-items" {...tip(t.items.map((i, n) => `${n + 1}. ${i.name}`).join('\n'), t.name)}>{t.items.map((i) => i.name).join(' · ')}</div>
              <div class="tr-actions">
                <Pill small tone="secondary" testid={`template-apply-${t.id}`} disabled={!!applyBlocker} reason={applyBlocker}
                  hint="Append these items to this colony’s queue (illegal items are skipped)"
                  onClick={() => issue({ type: 'ApplyBuildTemplate', templateId: t.id, systemId: sid }, { success: `“${t.name}” applied to ${detail.colony.name}.` })}>Apply</Pill>
                <Pill small tone="ghost" testid={`template-default-${t.id}`} active={t.isDefault} disabled={!!lock} reason={lock}
                  hint={t.isDefault ? 'Stop applying it to new colonies' : 'Apply automatically to new colonies'}
                  onClick={() => issue({ type: 'SetDefaultBuildTemplate', templateId: t.isDefault ? 0 : t.id }, { success: t.isDefault ? 'Default template cleared.' : `“${t.name}” is the default template.` })}>Default</Pill>
                <Pill small tone="ghost" disabled={!!lock || !items.length} reason={lock || 'The build queue is empty'} hint="Replace its items with this colony’s queue"
                  onClick={() => define(t.id, t.name)}>Update</Pill>
                <Pill small tone="alert" testid={`template-delete-${t.id}`} disabled={!!lock} reason={lock}
                  onClick={() => { confirmDialog.value = { title: `Delete “${t.name}”?`, body: 'Queues it was applied to are not affected.', confirm: 'Delete', tone: 'alert', onConfirm: () => issue({ type: 'DeleteBuildTemplate', templateId: t.id }, { success: `Template “${t.name}” deleted.` }) }; }}>Delete</Pill>
              </div>
            </li>
          );
        })}
      </ul>
      <div class="template-new">
        <input class="text-input" type="text" maxLength={48} placeholder="Template name" value={name} aria-label="New template name" data-testid="template-name"
          onInput={(e) => setName((e.target as HTMLInputElement).value)} />
        <Pill small tone="secondary" testid="template-save" disabled={!!saveReason} reason={saveReason}
          hint={`Save the ${items.length} queued item${items.length === 1 ? '' : 's'} as a template`} onClick={() => define(0, trimmed)}>Save queue</Pill>
      </div>
      {detail.queueExplanation ? <p class="hint">{detail.queueExplanation}</p> : null}
    </Frame>
  );
}

export function MegaprojectFrame({ v, detail, locked }: { v: GameView; detail: ColonyDetail; locked: boolean }) {
  const m = detail.megaproject;
  if (!m.enabled) return null;
  const lock = locked ? 'Finish the battle in progress first' : '';
  const elsewhere = m.systemId !== undefined && !m.here;
  return (
    <Frame title={m.name} sub="scientific victory" tone="secondary" class="compact" testid="megaproject">
      {m.here || elsewhere ? (
        <>
          <div class="mb-line">{m.here ? 'Under construction here' : `Under construction at ${m.system}`}: {fmt(m.work ?? 0)} / {fmt(m.cost)} industry, up to {m.workPerTurn} per turn.</div>
          <Meter value={m.work ?? 0} max={m.cost} height={7} tone="good" />
          <div class="mb-actions">
            <Pill small tone="alert" testid="megaproject-cancel" disabled={!!(lock || m.cancelBlocker)} reason={lock || m.cancelBlocker}
              onClick={() => { confirmDialog.value = { title: `Abandon the ${m.name}?`, body: 'All work is lost and the credits are not refunded.', confirm: 'Abandon', tone: 'alert', onConfirm: () => issue({ type: 'CancelMegaproject' }, { success: `The ${m.name} was abandoned.` }) }; }}>Abandon</Pill>
          </div>
        </>
      ) : (
        <>
          <p class="hint">Completing the {m.name} wins a scientific victory. It costs ¢{fmt(m.credits)} up front, then absorbs up to {m.workPerTurn} industry per turn from this colony ({fmt(m.cost)} in all); the colony builds with the rest.</p>
          <div class="mb-actions">
            <Pill small tone="good" testid="megaproject-start" disabled={!!(lock || m.startBlocker)} reason={lock || m.startBlocker}
              onClick={() => issue({ type: 'StartMegaproject', systemId: detail.colony.systemId }, { success: `The ${m.name} begins at ${detail.colony.name}.` })}>Begin here</Pill>
            <Pill small tone="ghost" onClick={() => { victoryOpen.value = true; }}>Victory progress</Pill>
          </div>
          {m.startBlocker ? <div class="warn-line small">{m.startBlocker}</div> : null}
        </>
      )}
      {v.victory.rivalProjects.length ? <div class="warn-line small">Rival megaprojects under way: {v.victory.rivalProjects.map((r) => r.ownerName).join(', ')}.</div> : null}
    </Frame>
  );
}

export function FundingNotice({ detail }: { detail: ColonyDetail }) {
  const f = detail.fundsStation;
  if (!f) return null;
  return (
    <div class="warn-line" data-testid="funding-notice">
      All industry funds the starbase upgrade of {f.name} ({f.work}/{f.cost}); the construction queue is paused.
      <button type="button" class="link" onClick={() => { goto('galaxy'); focusMap(f.x, f.y, 1.6); selectCell(f.y * (view.value?.meta.width ?? 1) + f.x); }}>Show</button>
    </div>
  );
}
