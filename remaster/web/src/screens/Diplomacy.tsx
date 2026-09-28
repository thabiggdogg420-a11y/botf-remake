import { signal } from '@preact/signals';
import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { CallToArmsView, Command, DiplomacyView, DiplomaticEventView, GameView, ProposalView, RelationView } from '../core/types';
import { diplomacyFocus } from '../state/navigation';
import { cancelOrders, confirmDialog, dryRun, issue, meta, reportError, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Meter, Pill, Tabs, tip } from '../ui/components';

type Target = { minor: boolean; id: number };

/** Calls to arms the player chose to let lapse (UI intent only: the core resolves them at the deadline). */
const refusedCalls = signal<Set<string>>(new Set());
const callKey = (c: CallToArmsView) => `${c.defender}:${c.aggressor}:${c.issuedTurn}`;

// Consequence copy follows remaster/docs/DIPLOMACY.md (the numbers are the core's).
const TREATY_EFFECTS: Record<string, string> = {
  NonAggression: 'Honourable AIs will not attack while it holds. Trust grows +1 every 4 turns (up to 60).',
  Friendship: '+3 credits a turn to each party; trust +1 every 2 turns (up to 75).',
  Affiliation: '+5 credits a turn each; shared star charts and sensors; trust +1 a turn (up to 85).',
  Alliance: '+8 credits a turn each; shared charts, supply and mutual defence — you may be called to arms; trust +1 a turn.',
  Peace: 'Ends the war; binds honourable AIs for its term.',
  Trade: 'Enables trade routes between your empires (routes pay credits every turn).',
  Membership: 'The minor joins your empire as a member world.',
};

function proposalTitle(p: ProposalView): string {
  if (p.kind === 'tribute') return `Tribute demand: ¢${fmt(p.creditDemand)}`;
  if (p.kind === 'gift') return `Gift of ¢${fmt(p.creditOffer)}`;
  if (p.kind === 'war') return 'Declaration of war';
  return `${p.typeName} proposal`;
}

function proposalConsequences(p: ProposalView): { accept: string; reject: string } {
  if (p.kind === 'tribute') return {
    accept: `Pay ¢${fmt(p.creditDemand)}. Paying tribute costs 5 trust and leaves us a grievance against them.`,
    reject: 'They gain 2 grievances against us and trust falls by 3. Ignoring it still leaves them a grievance.',
  };
  if (p.kind === 'gift') return { accept: `Receive ¢${fmt(p.creditOffer)}; trust rises.`, reject: 'The gift is returned.' };
  const effect = TREATY_EFFECTS[p.type] ?? '';
  return {
    accept: `${p.typeName} signed for ${p.duration.toLowerCase()} (+5 trust).${effect ? ` ${effect}` : ''}${p.creditOffer ? ` They pay ¢${fmt(p.creditOffer)}.` : ''}${p.creditDemand ? ` We pay ¢${fmt(p.creditDemand)}.` : ''}`,
    reject: 'No treaty. Repeated refusals make them wait longer before proposing again.',
  };
}

function deadline(turn: number, due: number): { text: string; tone: 'alert' | 'warn' | 'dim' } {
  const left = due - turn;
  return {
    text: left <= 0 ? `due now (turn ${due})` : `answer by turn ${due} · ${left} turn${left === 1 ? '' : 's'} left`,
    tone: left <= 1 ? 'alert' : left <= 2 ? 'warn' : 'dim',
  };
}

export function DiplomacyScreen() {
  const v = view.value!;
  const d = useMemo<DiplomacyView | null>(() => { try { return core.diplomacy(); } catch (e) { reportError(e); return null; } }, [v.meta.version]);
  const [target, setTarget] = useState<Target | null>(diplomacyFocus.value);
  const [pane, setPane] = useState<'dossier' | 'ledger' | 'news'>('dossier');
  useEffect(() => { if (diplomacyFocus.value) { setTarget(diplomacyFocus.value); setPane('dossier'); diplomacyFocus.value = null; } }, [diplomacyFocus.value]);
  if (!d) return <div class="screen pad"><Empty>Diplomacy unavailable.</Empty></div>;
  const contacted = d.empires.filter((e) => e.contact);
  const sel: Target | null = target ?? (contacted.find((e) => !e.eliminated) ? { minor: false, id: contacted.find((e) => !e.eliminated)!.empire } : d.minors[0] ? { minor: true, id: d.minors[0].systemId } : null);
  const incoming = d.proposals.filter((p) => p.incoming && p.status === 'Pending');
  return (
    <div class="screen pad diplomacy-screen" data-testid="screen-diplomacy">
      <div class="dip-grid">
        <Frame title="Known powers" sub={`${contacted.length + d.minors.length} contacted`} class="fill race-list-frame">
          <div class="race-list">
            {d.empires.map((e) => (
              <button type="button" key={e.empire} class={`race-row${sel && !sel.minor && sel.id === e.empire ? ' active' : ''}${e.contact ? '' : ' unknown'}${e.eliminated ? ' eliminated' : ''}`}
                disabled={!e.contact} onClick={() => { setTarget({ minor: false, id: e.empire }); setPane('dossier'); }} data-testid={`race-${e.empire}`}
                {...tip(e.eliminated ? 'Eliminated: this empire holds no colonies. Its treaties are dissolved and it can no longer negotiate.' : undefined)}>
                <Emblem empire={e.empire} size={30} />
                <span class="rr-main">
                  <span class="rr-name">{e.name}</span>
                  <span class="rr-status">{relationStatus(e)}</span>
                </span>
                {e.contact && !e.eliminated ? <span class="rr-trust" {...tip(`Trust ${e.trust}/100`)}><Meter value={e.trust ?? 0} max={100} height={4} tone={e.atWar ? 'alert' : 'good'} /></span> : <span />}
                {e.eliminated ? <Badge tone="dim">OUT</Badge> : e.atWar ? <Badge tone="alert">WAR</Badge> : e.allied ? <Badge tone="good">ALLY</Badge> : <span />}
              </button>
            ))}
            {d.minors.length ? <div class="race-sep">Minor races</div> : null}
            {d.minors.map((m) => (
              <button type="button" key={m.systemId} class={`race-row minor${sel && sel.minor && sel.id === m.systemId ? ' active' : ''}`} onClick={() => { setTarget({ minor: true, id: m.systemId }); setPane('dossier'); }}>
                <span class="minor-glyph" aria-hidden="true">◇</span>
                <span class="rr-main">
                  <span class="rr-name">{m.name}</span>
                  <span class="rr-status">{m.memberOf ? `Member of ${v.empires.find((e) => e.empire === m.memberOf)?.name ?? (m.memberOf === v.meta.player ? 'your empire' : 'an empire')}` : m.friendship ? 'Friendship' : 'Independent'} · {m.system}</span>
                </span>
                <span class="rr-trust" {...tip(`Relations ${m.attitude}/100`)}><Meter value={m.attitude} max={100} height={4} tone="good" /></span>
                <span />
              </button>
            ))}
            {!contacted.length && !d.minors.length ? <Empty>No contact yet. Explore to meet other powers.</Empty> : null}
          </div>
          <Standing d={d} v={v} />
        </Frame>
        <div class="dip-main">
          {incoming.length || d.callsToArms.length ? <Inbox v={v} d={d} incoming={incoming} /> : null}
          <div class="dip-panes">
            <Tabs value={pane} onChange={setPane} testid="dip-panes" options={[
              { id: 'dossier', label: 'Dossier' },
              { id: 'ledger', label: 'Treaty ledger', count: d.treaties.length + d.treatyHistory.length },
              { id: 'news', label: 'Galactic news', count: d.news.length },
            ]} />
          </div>
          {pane === 'ledger' ? <Ledger d={d} /> : pane === 'news' ? <News d={d} />
            : sel ? <Dossier key={`${sel.minor ? 'm' : 'e'}${sel.id}`} v={v} d={d} target={sel} /> : <Frame title="Diplomacy"><Empty>Select a contacted race.</Empty></Frame>}
        </div>
      </div>
    </div>
  );
}

function relationStatus(e: RelationView): string {
  if (!e.contact) return 'No contact';
  if (e.eliminated) return 'Eliminated';
  const parts = [e.atWar ? 'At war' : e.allied ? 'Full alliance' : e.sensorSharing ? 'Affiliated' : e.treatyForbidsAttack ? 'Treaty in force' : 'Peace'];
  if (e.tradeTreaty) parts.push('trade pact');
  if ((e.memory?.theirBetrayals ?? 0) > 0) parts.push(`${e.memory!.theirBetrayals} betrayal${e.memory!.theirBetrayals === 1 ? '' : 's'}`);
  return parts.join(' · ');
}

function Standing({ d, v }: { d: DiplomacyView; v: GameView }) {
  const vic = d.victory;
  return (
    <div class="standing" data-testid="standing">
      <div class="standing-row" {...tip('Wars declared while bound by a treaty. Every betrayal costs −10 on all future proposals and makes you a target (DIPLOMACY.md §3.3).', 'Reputation')}>
        <span class="label">Reputation</span>
        {d.standing.betrayals ? <Badge tone="alert">{d.standing.betrayals} treaty betrayal{d.standing.betrayals === 1 ? '' : 's'} (last turn {d.standing.lastBetrayalTurn})</Badge> : <Badge tone="good">No betrayals</Badge>}
      </div>
      {vic && d.modern ? (
        <div class="standing-row" {...tip(`Your bloc (your colonies, member minors and Full Alliance partners) holds ${vic.bloc} of ${vic.galaxy} population. A strict majority with at least one ally or member minor is a diplomatic victory.`, 'Diplomatic victory')}>
          <span class="label">Bloc</span>
          <Meter value={vic.bloc} max={Math.max(1, vic.galaxy)} height={6} tone={vic.achieved ? 'good' : 'accent'} />
          <b>{Math.round((100 * vic.bloc) / Math.max(1, vic.galaxy))}%</b>
        </div>
      ) : null}
      {vic?.allies.length ? <div class="standing-allies">Allies: {vic.allies.map((id) => <span key={id}><Emblem empire={id} size={12} /> {v.empires.find((e) => e.empire === id)?.name}</span>)}</div> : null}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Inbox: AI proposals awaiting our answer, and calls to arms from our allies.
// ---------------------------------------------------------------------------------------------
function Inbox({ v, d, incoming }: { v: GameView; d: DiplomacyView; incoming: ProposalView[] }) {
  const locked = v.session.active ? 'Finish the battle first' : undefined;
  const responseFor = (id: number) => v.pending.find((c) => c.type === 'RespondDiplomaticProposal' && c.proposalId === id);
  const count = incoming.length + d.callsToArms.filter((c) => !c.honored).length;
  return (
    <Frame title="Incoming" sub={`${count} awaiting your answer`} tone="secondary" class="inbox" testid="diplomacy-inbox">
      <div class="inbox-list">
        {incoming.map((p) => {
          const due = deadline(d.turn, p.dueTurn);
          const cons = proposalConsequences(p);
          const drafted = responseFor(p.id);
          return (
            <div class={`inbox-item kind-${p.kind}`} key={`p${p.id}`} data-testid={`proposal-${p.id}`}>
              <Emblem empire={p.proposer.minor ? 0 : p.proposer.id} size={30} />
              <div class="ii-main">
                <div class="ii-title"><b>{p.proposer.name}</b> · {proposalTitle(p)} <Badge tone={due.tone}>{due.text}</Badge></div>
                <div class="ii-terms">{p.kind === 'treaty' ? `${p.duration} · ${p.tone.toLowerCase()} tone` : `${p.tone.toLowerCase()} tone`}{p.creditOffer && p.kind !== 'gift' ? ` · they offer ¢${fmt(p.creditOffer)}` : ''}{p.creditDemand && p.kind !== 'tribute' ? ` · they ask ¢${fmt(p.creditDemand)}` : ''}</div>
                <ul class="ii-cons">
                  <li><span class="pos">Accept:</span> {cons.accept}</li>
                  <li><span class="neg">Reject:</span> {cons.reject}</li>
                </ul>
              </div>
              <div class="ii-actions">
                {drafted ? (
                  <>
                    <Badge tone={drafted.proposalAccept ? 'good' : 'alert'}>{drafted.proposalAccept ? 'Acceptance ordered' : 'Rejection ordered'}</Badge>
                    <Pill small tone="ghost" disabled={!!locked} reason={locked} onClick={() => cancelOrders([drafted.id], 'Response withdrawn.')} testid={`undo-${p.id}`}>Undo</Pill>
                  </>
                ) : (
                  <>
                    <Pill small tone="good" disabled={!!locked} reason={locked} testid={`accept-${p.id}`}
                      onClick={() => issue({ type: 'RespondDiplomaticProposal', proposalId: p.id, proposalAccept: true }, { success: 'Acceptance ordered for turn end.' })}>Accept</Pill>
                    <Pill small tone="alert" disabled={!!locked} reason={locked} testid={`reject-${p.id}`}
                      onClick={() => issue({ type: 'RespondDiplomaticProposal', proposalId: p.id, proposalAccept: false }, { success: 'Rejection ordered for turn end.' })}>Reject</Pill>
                  </>
                )}
              </div>
            </div>
          );
        })}
        {d.callsToArms.map((c) => <CallCard key={callKey(c)} v={v} d={d} call={c} />)}
      </div>
    </Frame>
  );
}

function CallCard({ v, d, call }: { v: GameView; d: DiplomacyView; call: CallToArmsView }) {
  const due = deadline(d.turn, call.dueTurn);
  const refused = refusedCalls.value.has(callKey(call));
  const warOrder = v.pending.find((c) => c.type === 'DeclareWar' && c.targetEmpire === call.aggressor);
  const locked = v.session.active ? 'Finish the battle first' : undefined;
  const setRefused = (on: boolean) => {
    const next = new Set(refusedCalls.value);
    if (on) next.add(callKey(call)); else next.delete(callKey(call));
    refusedCalls.value = next;
  };
  const honor = () => {
    confirmDialog.value = {
      title: `Declare war on the ${call.aggressorName}?`,
      body: `Honouring the ${call.defenderName}’s call to arms declares war on the ${call.aggressorName} at turn end.`,
      lines: [
        `+10 trust with the ${call.defenderName}.`,
        call.boundByTreaty ? `We are bound by a treaty with the ${call.aggressorName}: declaring war is a betrayal (−15 trust with them, −10 with every other empire, and a permanent mark on our reputation).`
          : `Any trade routes with the ${call.aggressorName} end and our open proposals with them are cancelled.`,
      ],
      confirm: 'Declare war', tone: 'alert',
      onConfirm: () => { if (issue({ type: 'DeclareWar', targetEmpire: call.aggressor }, { success: 'War declaration ordered: the alliance is honoured.' })) setRefused(false); },
    };
  };
  const refuse = () => {
    if (warOrder) cancelOrders([warOrder.id]);
    setRefused(true);
  };
  return (
    <div class={`inbox-item kind-call${call.honored ? ' done' : ''}`} data-testid={`call-${call.aggressor}`}>
      <Emblem empire={call.defender} size={30} />
      <div class="ii-main">
        <div class="ii-title"><b>{call.defenderName}</b> · call to arms against <Emblem empire={call.aggressor} size={13} /> <b>{call.aggressorName}</b>
          {call.honored ? <Badge tone="good">honoured — at war</Badge> : <Badge tone={due.tone}>{`declare war by turn ${call.dueTurn} · ${Math.max(0, call.dueTurn - d.turn)} left`}</Badge>}</div>
        <ul class="ii-cons">
          <li><span class="pos">Honour:</span> we declare war on the {call.aggressorName}; +10 trust with the {call.defenderName}.{call.boundByTreaty ? ' Our treaty with the aggressor makes this a betrayal.' : ''}</li>
          <li><span class="neg">Refuse:</span> at the deadline the {call.defenderName} lose 20 trust in us and gain 2 grievances; if their trust falls below 40 they dissolve the alliance.</li>
        </ul>
        {refused && !call.honored ? <div class="ii-note warn">You will let this call lapse. The consequences apply on turn {call.dueTurn}.</div> : null}
      </div>
      <div class="ii-actions">
        {call.honored ? null : warOrder ? (
          <>
            <Badge tone="alert">War declaration ordered</Badge>
            <Pill small tone="ghost" disabled={!!locked} reason={locked} onClick={refuse} testid={`refuse-call-${call.aggressor}`}>Refuse instead</Pill>
          </>
        ) : refused ? (
          <Pill small tone="ghost" onClick={() => setRefused(false)} testid={`reconsider-call-${call.aggressor}`}>Reconsider</Pill>
        ) : (
          <>
            <Pill small tone="alert" disabled={!!(locked ?? call.honorBlocker)} reason={locked ?? call.honorBlocker} onClick={honor} testid={`honor-call-${call.aggressor}`}>Honour</Pill>
            <Pill small tone="ghost" onClick={refuse} testid={`refuse-call-${call.aggressor}`}>Refuse</Pill>
          </>
        )}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
function eventTone(typeKey: number): string {
  // DiplomaticEventType: 6 broken by war, 8 war, 12 betrayed, 15 refused, 16 seceded → alert; 9 peace, 4 treaty, 14 honoured → good.
  if ([6, 8, 12].includes(typeKey)) return 'alert';
  if ([2, 3, 13, 15, 16, 17, 18].includes(typeKey)) return 'warn';
  if ([1, 4, 9, 10, 11, 14].includes(typeKey)) return 'good';
  return 'dim';
}

function EventRow({ e, both }: { e: DiplomaticEventView; both?: boolean }) {
  return (
    <li class={`ev tone-${eventTone(e.typeKey)}`}>
      <span class="ev-turn">T{e.turn}</span>
      <span class="ev-type">{e.type}</span>
      {both ? <span class="ev-parties"><Emblem empire={e.a.minor ? 0 : e.a.id} size={12} /> {e.a.name}{e.a.id !== e.b.id || e.a.minor !== e.b.minor ? <> → <Emblem empire={e.b.minor ? 0 : e.b.id} size={12} /> {e.b.name}</> : null}</span> : null}
      <span class="ev-text">{e.text}</span>
    </li>
  );
}

function Ledger({ d }: { d: DiplomacyView }) {
  const statusTone = (s: string) => (s === 'Dissolved' || s === 'BrokenByWar' || s.includes('War') ? 'alert' : s === 'Superseded' ? 'accent' : 'dim');
  return (
    <Frame title="Treaty ledger" sub="in force and ended" class="fill" testid="treaty-ledger">
      <h4>In force</h4>
      {d.treaties.length ? (
        <ul class="ledger">
          {d.treaties.map((t) => (
            <li key={`a${t.id}`}><Emblem empire={t.counterparty.minor ? 0 : t.counterparty.id} size={14} /> <b>{t.typeName}</b> with {t.counterparty.name}
              <span class="muted"> · since turn {t.startTurn} · {t.turnsRemaining >= 0 ? `${t.turnsRemaining} turns left` : 'indefinite'}</span></li>
          ))}
        </ul>
      ) : <p class="muted">No treaties in force.</p>}
      <h4>Ended</h4>
      {d.treatyHistory.length ? (
        <ul class="ledger">
          {d.treatyHistory.map((t) => (
            <li key={`h${t.id}`} class={t.status === 'Dissolved' ? 'dissolved' : ''}><Emblem empire={t.counterparty.minor ? 0 : t.counterparty.id} size={14} /> <b>{t.typeName}</b> with {t.counterparty.name}
              <span class="muted"> · from turn {t.startTurn}</span> <Badge tone={statusTone(t.status)}>{t.status === 'BrokenByWar' ? 'Broken by war' : t.status}</Badge></li>
          ))}
        </ul>
      ) : <p class="muted">No ended treaties yet.</p>}
    </Frame>
  );
}

function News({ d }: { d: DiplomacyView }) {
  return (
    <Frame title="Galactic news" sub="between the powers we know" class="fill" testid="galactic-news">
      <p class="hint">Every empire remembers betrayals, refused calls to arms and wars. These events shape who trusts whom — and who they trust with you.</p>
      <ul class="event-list">
        {d.news.length ? d.news.map((e, i) => <EventRow key={i} e={e} both />) : <li class="muted">No news yet.</li>}
      </ul>
    </Frame>
  );
}

function Dossier({ v, d, target }: { v: GameView; d: DiplomacyView; target: Target }) {
  const [tab, setTab] = useState<'overview' | 'propose' | 'history'>('overview');
  const rel = !target.minor ? d.empires.find((e) => e.empire === target.id) : null;
  const minor = target.minor ? d.minors.find((m) => m.systemId === target.id) : null;
  const name = rel?.name ?? minor?.name ?? 'Unknown';
  const treaties = d.treaties.filter((t) => t.counterparty.minor === target.minor && t.counterparty.id === target.id);
  const ended = d.treatyHistory.filter((t) => t.counterparty.minor === target.minor && t.counterparty.id === target.id);
  const proposals = d.proposals.filter((p) => (p.incoming ? p.proposer : p.target).id === target.id && (p.incoming ? p.proposer : p.target).minor === target.minor);
  const events = d.events.filter((e) => [e.a, e.b].some((p) => p.id === target.id && p.minor === target.minor));
  const dossier = !target.minor ? d.dossiers.find((x) => x.target === target.id) : null;
  const mem = rel?.memory;
  return (
    <Frame title={name} sub={target.minor ? 'Minor race' : rel?.personality ? `Major power · ${rel.personality} temperament` : 'Major power'} class={`fill dossier${rel?.eliminated ? ' eliminated' : ''}`} testid="dossier"
      actions={<Tabs value={tab} onChange={setTab} options={[
        { id: 'overview', label: 'Overview' }, { id: 'propose', label: 'Negotiate' }, { id: 'history', label: 'History', count: events.length + ended.length },
      ]} />}>
      <div class="dossier-head">
        {target.minor ? <span class="minor-glyph big">◇</span> : <Emblem empire={target.id} size={64} />}
        <div class="dh-main">
          <div class="dh-status">
            {rel?.eliminated ? <Badge tone="dim">Eliminated — no colonies remain</Badge> : rel ? (rel.atWar ? <Badge tone="alert">At war since turn {rel.warStartTurn}</Badge> : <Badge tone="good">At peace</Badge>) : null}
            {rel?.allied ? <Badge tone="good">Full alliance · mutual defence</Badge> : rel?.sensorSharing ? <Badge tone="accent">Shared sensors</Badge> : null}
            {rel?.treatyForbidsAttack ? <Badge tone="secondary" tipText="A non-aggression pact or stronger (or a timed peace) is in force: declaring war on them would be a betrayal.">Treaty forbids attack</Badge> : null}
            {rel?.tradeTreaty ? <Badge tone="accent">Trade pact</Badge> : null}
            {minor ? <Badge tone={minor.memberOf ? 'accent' : 'dim'}>{minor.memberOf ? `Member of ${minor.memberOf === v.meta.player ? 'your empire' : v.empires.find((e) => e.empire === minor.memberOf)?.name}` : 'Independent'}</Badge> : null}
            {minor?.friendship ? <Badge tone="good">Friendship</Badge> : null}
            {treaties.map((t) => <Badge key={t.id} tone="secondary">{t.typeName}{t.turnsRemaining >= 0 ? ` · ${t.turnsRemaining}T left` : ''}</Badge>)}
          </div>
          {!rel?.eliminated ? (
            <div class="dh-trust" {...tip(target.minor ? 'Attitude toward your empire (0–100). Friendship needs goodwill; membership needs strong relations and an earlier friendship.' : 'Trust between your governments (0–100). Treaties, gifts and incidents move it.')}>
              <span>{target.minor ? 'Relations' : 'Trust'}</span>
              <Meter value={rel?.trust ?? minor?.attitude ?? 0} max={100} height={8} tone={rel?.atWar ? 'alert' : 'good'} />
              <b>{rel?.trust ?? minor?.attitude ?? 0}</b>
            </div>
          ) : null}
        </div>
      </div>
      {tab === 'overview' ? (
        <div class="dossier-body">
          {mem ? (
            <div class="memory-grid" data-testid="relation-memory">
              <div {...tip('Grievances they hold against us (refused tribute, exposed spies, refused calls to arms…). Each weighs −3 on our proposals and fuels war plans.')}><span>Their grievances</span><b class={mem.theirGrievances ? 'neg' : ''}>{mem.theirGrievances}</b></div>
              <div {...tip('Grievances we hold against them.')}><span>Our grievances</span><b>{mem.ourGrievances}</b></div>
              <div {...tip('Treaties they have betrayed by declaring war while bound. The whole galaxy remembers.')}><span>Their betrayals</span><b class={mem.theirBetrayals ? 'neg' : ''}>{mem.theirBetrayals}</b></div>
              <div {...tip('Our proposals they refused or ignored in a row; each doubles their patience before they propose again.')}><span>Our refused proposals</span><b>{mem.ourRefusedProposals ?? 0}</b></div>
              <div><span>Last war / peace</span><b>{mem.lastWarTurn !== undefined && mem.lastWarTurn >= 0 ? `T${mem.lastWarTurn}` : '—'} / {mem.lastPeaceTurn !== undefined && mem.lastPeaceTurn >= 0 ? `T${mem.lastPeaceTurn}` : '—'}</b></div>
            </div>
          ) : null}
          {dossier ? <p>Intelligence (turn {dossier.turn}): {dossier.knownColonies} known colonies, about ¢{fmt(dossier.estimatedCredits)} in reserve.</p> : !target.minor && !rel?.eliminated ? <p class="muted">No intelligence dossier yet — run espionage from the Intel screen.</p> : null}
          <h4>Proposals</h4>
          {proposals.length ? <ul class="prop-list">{proposals.map((p) => (
            <li key={p.id}>{p.incoming ? 'From them' : 'From you'}: {proposalTitle(p)} · due turn {p.dueTurn} <Badge tone="dim">{p.status}</Badge></li>
          ))}</ul> : <p class="muted">No proposals in progress.</p>}
          {d.majorOffers.filter((o) => o.proposer === target.id || o.target === target.id).map((o) => (
            <p key={o.id}>{o.proposer === v.meta.player ? 'Your' : 'Their'} {o.type === 'OfferPeace' ? 'peace' : 'trade treaty'} offer is answered on turn {o.dueTurn}.</p>
          ))}
          {d.minorOffers.filter((o) => o.systemId === target.id).map((o) => (
            <p key={o.id}>Your {o.type === 'SendGift' ? 'gift' : o.type === 'OfferMembership' ? 'membership offer' : 'friendship offer'} arrives on turn {o.dueTurn}.</p>
          ))}
          <h4>Treaties</h4>
          {treaties.length ? <ul class="prop-list">{treaties.map((t) => <li key={t.id}>{t.typeName} since turn {t.startTurn} · {t.turnsRemaining >= 0 ? `${t.turnsRemaining} turns remaining` : 'indefinite'}{TREATY_EFFECTS[t.type] ? <span class="muted"> — {TREATY_EFFECTS[t.type]}</span> : null}</li>)}</ul> : <p class="muted">No treaties in force.</p>}
        </div>
      ) : tab === 'propose' ? (rel?.eliminated ? <Empty>The {name} has been eliminated and can no longer negotiate.</Empty> : <Negotiate v={v} d={d} target={target} rel={rel ?? null} />) : (
        <div class="dossier-history">
          {ended.length ? (
            <ul class="ledger">
              {ended.map((t) => <li key={t.id}><b>{t.typeName}</b> <span class="muted">from turn {t.startTurn}</span> <Badge tone={t.status === 'Dissolved' || t.status === 'BrokenByWar' ? 'alert' : 'dim'}>{t.status === 'BrokenByWar' ? 'Broken by war' : t.status}</Badge></li>)}
            </ul>
          ) : null}
          <ul class="event-list">
            {events.length ? events.map((e, i) => <EventRow key={i} e={e} />) : <li class="muted">No recorded history.</li>}
          </ul>
        </div>
      )}
    </Frame>
  );
}

function Negotiate({ v, d, target, rel }: { v: GameView; d: DiplomacyView; target: Target; rel: RelationView | null }) {
  const m = meta.value!;
  const elig = d.eligibility.find((e) => e.target === target.id && e.minor === target.minor);
  const firstOk = elig?.treaties.find((t) => t.ok)?.type ?? 'NonAggression';
  const [type, setType] = useState(firstOk);
  const [duration, setDuration] = useState(25);
  const [tone, setTone] = useState('Cordial');
  const [offer, setOffer] = useState(0);
  const [demand, setDemand] = useState(0);
  const locked = v.session.active ? 'Finish the battle first' : undefined;
  const proposal: Command = target.minor
    ? { type: 'SubmitDiplomaticProposal', proposalTargetIsMinor: true, targetSystemId: target.id, proposalTreatyType: type, proposalDuration: duration, proposalTone: tone, proposalCreditOffer: offer, proposalCreditDemand: demand }
    : { type: 'SubmitDiplomaticProposal', targetEmpire: target.id, proposalTreatyType: type, proposalDuration: duration, proposalTone: tone, proposalCreditOffer: offer, proposalCreditDemand: demand };
  const chosen = elig?.treaties.find((t) => t.type === type);
  const reason = locked ?? (chosen && !chosen.ok ? chosen.reason : dryRun(proposal)) ?? undefined;
  const warBody = rel?.treatyForbidsAttack
    ? `We are bound by a treaty with the ${rel.name}. Declaring war now is a betrayal: they lose 15 more trust, every other empire loses 10, and our reputation carries the mark for good.`
    : 'War breaks treaties and allows attacks on their ships and colonies. Their allies may be called to arms against us.';
  const classic: { cmd: Command; label: string; tone: 'alert' | 'primary' | 'secondary'; confirm?: string; detail: string }[] = target.minor ? [
    { cmd: { type: 'OfferFriendship', systemId: target.id }, label: 'Offer friendship', tone: 'primary', detail: 'Delivered at turn end; their attitude decides the answer.' },
    { cmd: { type: 'SendGift', systemId: target.id }, label: 'Send gift', tone: 'secondary', detail: 'Credits are paid at turn end; the gift arrives later and improves relations.' },
    { cmd: { type: 'OfferMembership', systemId: target.id }, label: 'Offer membership', tone: 'primary', detail: 'Needs strong relations and a friendship signed in an earlier turn.' },
  ] : [
    { cmd: { type: 'DeclareWar', targetEmpire: target.id }, label: 'Declare war', tone: 'alert', confirm: warBody, detail: rel?.treatyForbidsAttack ? 'A treaty is in force: this would be a betrayal.' : 'Required before attacking.' },
    { cmd: { type: 'OfferPeace', targetEmpire: target.id }, label: 'Propose peace', tone: 'primary', detail: 'They answer after a delay.' },
    { cmd: { type: 'ProposeTradeTreaty', targetEmpire: target.id }, label: 'Propose trade pact', tone: 'secondary', detail: 'Enables trade routes; they answer after a delay.' },
  ];
  return (
    <div class="negotiate">
      {d.modern ? (
        <div class="treaty-builder" data-testid="treaty-builder">
          <h4>Treaty proposal</h4>
          <div class="tb-types" role="radiogroup" aria-label="Treaty type">
            {(elig?.treaties ?? []).map((t) => (
              <button type="button" role="radio" aria-checked={type === t.type} key={t.type} class={`tb-type${type === t.type ? ' active' : ''}${t.ok ? '' : ' blocked'}`}
                onClick={() => setType(t.type)} {...tip(`${t.ok ? 'Eligible' : `Not eligible: ${t.reason}`}${TREATY_EFFECTS[t.type] ? `\n${TREATY_EFFECTS[t.type]}` : ''}`, t.name)}>{t.name}</button>
            ))}
          </div>
          <div class="tb-row">
            <span class="label">Duration</span>
            {m.durations.map((x) => <Pill key={x.value} small tone="ghost" active={duration === x.value} onClick={() => setDuration(x.value)}>{x.name}</Pill>)}
          </div>
          <div class="tb-row">
            <span class="label">Tone</span>
            {m.tones.map((x) => <Pill key={x.key} small tone="ghost" active={tone === x.key} onClick={() => setTone(x.key)}>{x.name}</Pill>)}
          </div>
          <div class="tb-row">
            <label class="label" for="offer">Offer ¢</label>
            <input id="offer" class="text-input mono small" type="number" min={0} step={50} value={offer} onInput={(e) => setOffer(Math.max(0, Number((e.target as HTMLInputElement).value) || 0))} />
            <label class="label" for="demand">Demand ¢</label>
            <input id="demand" class="text-input mono small" type="number" min={0} step={50} value={demand} onInput={(e) => setDemand(Math.max(0, Number((e.target as HTMLInputElement).value) || 0))} />
            <span class="muted">Treasury ¢{fmt(v.empire.draftCredits)}</span>
          </div>
          <div class="tb-send">
            <span class={reason ? 'neg' : 'pos'}>{reason ? reason : 'Terms are ready to send.'}</span>
            <Pill disabled={!!reason} reason={reason} testid="send-proposal" onClick={() => issue(proposal, { success: 'Proposal ordered for turn end.' })}>Send proposal</Pill>
          </div>
        </div>
      ) : null}
      <div class="classic-actions">
        <h4>{target.minor ? 'Envoys' : 'Statecraft'}</h4>
        <div class="action-list">
          {classic.map((c) => {
            const why = locked ?? (dryRun(c.cmd) || undefined);
            return (
              <Pill key={c.label} tone={c.tone} wide disabled={!!why} reason={why} hint={c.detail} testid={`dip-${String(c.cmd.type)}`}
                onClick={() => {
                  const go = () => issue(c.cmd, { success: `${c.label}: ordered for turn end.` });
                  if (c.confirm) confirmDialog.value = { title: `${c.label}?`, body: c.confirm, confirm: c.label, tone: 'alert', onConfirm: go };
                  else go();
                }}>{c.label}</Pill>
            );
          })}
        </div>
      </div>
    </div>
  );
}
