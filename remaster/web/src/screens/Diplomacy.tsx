import { useEffect, useMemo, useState } from 'preact/hooks';
import { core } from '../core/engine';
import type { Command, DiplomacyView, GameView } from '../core/types';
import { diplomacyFocus } from '../state/navigation';
import { confirmDialog, dryRun, issue, meta, reportError, view } from '../state/store';
import { Badge, Emblem, Empty, fmt, Frame, Meter, Pill, Tabs, tip } from '../ui/components';

type Target = { minor: boolean; id: number };

export function DiplomacyScreen() {
  const v = view.value!;
  const d = useMemo<DiplomacyView | null>(() => { try { return core.diplomacy(); } catch (e) { reportError(e); return null; } }, [v.meta.version]);
  const [target, setTarget] = useState<Target | null>(diplomacyFocus.value);
  useEffect(() => { if (diplomacyFocus.value) { setTarget(diplomacyFocus.value); diplomacyFocus.value = null; } }, [diplomacyFocus.value]);
  if (!d) return <div class="screen pad"><Empty>Diplomacy unavailable.</Empty></div>;
  const contacted = d.empires.filter((e) => e.contact);
  const sel: Target | null = target ?? (contacted[0] ? { minor: false, id: contacted[0].empire } : d.minors[0] ? { minor: true, id: d.minors[0].systemId } : null);
  const incoming = d.proposals.filter((p) => p.incoming);
  return (
    <div class="screen pad diplomacy-screen" data-testid="screen-diplomacy">
      <div class="dip-grid">
        <Frame title="Known powers" sub={`${contacted.length + d.minors.length} contacted`} class="fill race-list-frame">
          <div class="race-list">
            {d.empires.map((e) => (
              <button type="button" key={e.empire} class={`race-row${sel && !sel.minor && sel.id === e.empire ? ' active' : ''}${e.contact ? '' : ' unknown'}`}
                disabled={!e.contact} onClick={() => setTarget({ minor: false, id: e.empire })} data-testid={`race-${e.empire}`}>
                <Emblem empire={e.empire} size={30} />
                <span class="rr-main">
                  <span class="rr-name">{e.name}</span>
                  <span class="rr-status">{!e.contact ? 'No contact' : e.atWar ? 'At war' : e.tradeTreaty ? 'Peace · trade pact' : 'Peace'}</span>
                </span>
                {e.contact ? <span class="rr-trust" {...tip(`Trust ${e.trust}/100`)}><Meter value={e.trust ?? 0} max={100} height={4} tone={e.atWar ? 'alert' : 'good'} /></span> : null}
                {e.atWar ? <Badge tone="alert">WAR</Badge> : null}
              </button>
            ))}
            {d.minors.length ? <div class="race-sep">Minor races</div> : null}
            {d.minors.map((m) => (
              <button type="button" key={m.systemId} class={`race-row minor${sel && sel.minor && sel.id === m.systemId ? ' active' : ''}`} onClick={() => setTarget({ minor: true, id: m.systemId })}>
                <span class="minor-glyph" aria-hidden="true">◇</span>
                <span class="rr-main">
                  <span class="rr-name">{m.name}</span>
                  <span class="rr-status">{m.memberOf ? `Member of ${v.empires.find((e) => e.empire === m.memberOf)?.name ?? (m.memberOf === v.meta.player ? 'your empire' : 'an empire')}` : m.friendship ? 'Friendship' : 'Independent'} · {m.system}</span>
                </span>
                <span class="rr-trust" {...tip(`Relations ${m.attitude}/100`)}><Meter value={m.attitude} max={100} height={4} tone="good" /></span>
              </button>
            ))}
            {!contacted.length && !d.minors.length ? <Empty>No contact yet. Explore to meet other powers.</Empty> : null}
          </div>
        </Frame>
        <div class="dip-main">
          {incoming.length ? (
            <div class="incoming-banner" data-testid="incoming-proposals">
              {incoming.map((p) => (
                <div class="incoming" key={p.id}>
                  <Emblem empire={p.proposer.minor ? 0 : p.proposer.id} size={22} />
                  <span><b>{p.proposer.name}</b> proposes a <b>{p.typeName}</b> ({p.duration}, {p.tone.toLowerCase()} tone){p.creditOffer ? `, offering ¢${fmt(p.creditOffer)}` : ''}{p.creditDemand ? `, demanding ¢${fmt(p.creditDemand)}` : ''} — due turn {p.dueTurn}</span>
                  {p.responseDrafted ? <Badge tone="good">answered</Badge> : (
                    <>
                      <Pill small tone="good" disabled={v.session.active} reason="Finish the battle first" onClick={() => issue({ type: 'RespondDiplomaticProposal', proposalId: p.id, proposalAccept: true }, { success: 'Acceptance ordered.' })}>Accept</Pill>
                      <Pill small tone="alert" disabled={v.session.active} reason="Finish the battle first" onClick={() => issue({ type: 'RespondDiplomaticProposal', proposalId: p.id, proposalAccept: false }, { success: 'Rejection ordered.' })}>Reject</Pill>
                    </>
                  )}
                </div>
              ))}
            </div>
          ) : null}
          {sel ? <Dossier key={`${sel.minor ? 'm' : 'e'}${sel.id}`} v={v} d={d} target={sel} /> :<Frame title="Diplomacy"><Empty>Select a contacted race.</Empty></Frame>}
        </div>
      </div>
    </div>
  );
}

function Dossier({ v, d, target }: { v: GameView; d: DiplomacyView; target: Target }) {
  const [tab, setTab] = useState<'overview' | 'propose' | 'history'>('overview');
  const rel = !target.minor ? d.empires.find((e) => e.empire === target.id) : null;
  const minor = target.minor ? d.minors.find((m) => m.systemId === target.id) : null;
  const name = rel?.name ?? minor?.name ?? 'Unknown';
  const treaties = d.treaties.filter((t) => t.counterparty.minor === target.minor && t.counterparty.id === target.id);
  const proposals = d.proposals.filter((p) => (p.incoming ? p.proposer : p.target).id === target.id && (p.incoming ? p.proposer : p.target).minor === target.minor);
  const events = d.events.filter((e) => [e.a, e.b].some((p) => p.id === target.id && p.minor === target.minor));
  const dossier = !target.minor ? d.dossiers.find((x) => x.target === target.id) : null;
  return (
    <Frame title={name} sub={target.minor ? 'Minor race' : 'Major power'} class="fill dossier" testid="dossier"
      actions={<Tabs value={tab} onChange={setTab} options={[
        { id: 'overview', label: 'Overview' }, { id: 'propose', label: 'Negotiate' }, { id: 'history', label: 'History', count: events.length },
      ]} />}>
      <div class="dossier-head">
        {target.minor ? <span class="minor-glyph big">◇</span> : <Emblem empire={target.id} size={64} />}
        <div class="dh-main">
          <div class="dh-status">
            {rel ? (rel.atWar ? <Badge tone="alert">At war since turn {rel.warStartTurn}</Badge> : <Badge tone="good">At peace</Badge>) : null}
            {rel?.tradeTreaty ? <Badge tone="accent">Trade pact</Badge> : null}
            {minor ? <Badge tone={minor.memberOf ? 'accent' : 'dim'}>{minor.memberOf ? `Member of ${minor.memberOf === v.meta.player ? 'your empire' : v.empires.find((e) => e.empire === minor.memberOf)?.name}` : 'Independent'}</Badge> : null}
            {minor?.friendship ? <Badge tone="good">Friendship</Badge> : null}
            {treaties.map((t) => <Badge key={t.id} tone="secondary">{t.typeName}{t.turnsRemaining >= 0 ? ` · ${t.turnsRemaining}T left` : ''}</Badge>)}
          </div>
          <div class="dh-trust" {...tip(target.minor ? 'Attitude toward your empire (0–100). Friendship needs goodwill; membership needs strong relations and an earlier friendship.' : 'Trust between your governments (0–100). Treaties, gifts and incidents move it.')}>
            <span>{target.minor ? 'Relations' : 'Trust'}</span>
            <Meter value={rel?.trust ?? minor?.attitude ?? 0} max={100} height={8} tone={rel?.atWar ? 'alert' : 'good'} />
            <b>{rel?.trust ?? minor?.attitude ?? 0}</b>
          </div>
        </div>
      </div>
      {tab === 'overview' ? (
        <div class="dossier-body">
          {dossier ? <p>Intelligence (turn {dossier.turn}): {dossier.knownColonies} known colonies, about ¢{fmt(dossier.estimatedCredits)} in reserve.</p> : !target.minor ? <p class="muted">No intelligence dossier yet — run espionage from the Intel screen.</p> : null}
          <h4>Proposals</h4>
          {proposals.length ? <ul class="prop-list">{proposals.map((p) => (
            <li key={p.id}>{p.incoming ? 'From them' : 'From you'}: {p.typeName} · {p.duration} · due turn {p.dueTurn} <Badge tone="dim">{p.status}</Badge></li>
          ))}</ul> : <p class="muted">No proposals in progress.</p>}
          {d.majorOffers.filter((o) => o.proposer === target.id || o.target === target.id).map((o) => (
            <p key={o.id}>{o.proposer === v.meta.player ? 'Your' : 'Their'} {o.type === 'OfferPeace' ? 'peace' : 'trade treaty'} offer is answered on turn {o.dueTurn}.</p>
          ))}
          {d.minorOffers.filter((o) => o.systemId === target.id).map((o) => (
            <p key={o.id}>Your {o.type === 'SendGift' ? 'gift' : o.type === 'OfferMembership' ? 'membership offer' : 'friendship offer'} arrives on turn {o.dueTurn}.</p>
          ))}
          <h4>Treaties</h4>
          {treaties.length ? <ul class="prop-list">{treaties.map((t) => <li key={t.id}>{t.typeName} since turn {t.startTurn} · {t.turnsRemaining >= 0 ? `${t.turnsRemaining} turns remaining` : 'indefinite'}</li>)}</ul> : <p class="muted">No treaties.</p>}
        </div>
      ) : tab === 'propose' ? <Negotiate v={v} d={d} target={target} /> : (
        <ul class="event-list">
          {events.length ? events.map((e, i) => <li key={i}><span class="ev-turn">T{e.turn}</span><span class="ev-type">{e.type}</span><span>{e.text}</span></li>) : <li class="muted">No recorded history.</li>}
        </ul>
      )}
    </Frame>
  );
}

function Negotiate({ v, d, target }: { v: GameView; d: DiplomacyView; target: Target }) {
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
  const classic: { cmd: Command; label: string; tone: 'alert' | 'primary' | 'secondary'; confirm?: string; detail: string }[] = target.minor ? [
    { cmd: { type: 'OfferFriendship', systemId: target.id }, label: 'Offer friendship', tone: 'primary', detail: 'Delivered at turn end; their attitude decides the answer.' },
    { cmd: { type: 'SendGift', systemId: target.id }, label: 'Send gift', tone: 'secondary', detail: 'Credits are paid at turn end; the gift arrives later and improves relations.' },
    { cmd: { type: 'OfferMembership', systemId: target.id }, label: 'Offer membership', tone: 'primary', detail: 'Needs strong relations and a friendship signed in an earlier turn.' },
  ] : [
    { cmd: { type: 'DeclareWar', targetEmpire: target.id }, label: 'Declare war', tone: 'alert', confirm: 'War breaks treaties and allows attacks on their ships and colonies.', detail: 'Required before attacking.' },
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
                onClick={() => setType(t.type)} {...tip(t.ok ? 'Eligible' : `Not eligible: ${t.reason}`, t.name)}>{t.name}</button>
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
