import { useEffect, useState } from 'preact/hooks';
import { back, meta, startCampaign, view } from '../state/store';
import { maybeStartTutorial } from '../state/tutorial';
import { Emblem, Frame, Pill, tip } from '../ui/components';
import { Starfield } from '../ui/Starfield';
import { FACTIONS, palette } from '../ui/theme';

const SIZES = [
  // The one place the web client's galaxy sizes live (twin: godot/scripts/viewmodels/new_campaign_vm.gd SIZES).
  { id: 'small', label: 'Small', width: 16, height: 13, systems: 30, blurb: 'Quick, crowded — contact comes early.' },
  { id: 'medium', label: 'Medium', width: 22, height: 16, systems: 50, blurb: 'The default: room to grow, rivals within reach.' },
  { id: 'large', label: 'Large', width: 28, height: 20, systems: 80, blurb: 'Room to expand before borders meet.' },
  { id: 'huge', label: 'Huge', width: 34, height: 24, systems: 120, blurb: 'A long campaign across a wide quadrant.' },
] as const;

const ERA_BLURBS: Record<number, string> = {
  1: 'Warp-capable newcomers. Build everything from the ground up.',
  2: 'Established powers with developed technology.',
  3: 'Expanded empires, advanced shipyards.',
  4: 'Advanced civilisations on the brink of dominance.',
  5: 'Supreme technology from turn one.',
};

function randomSeed(): number { return Math.floor(Math.random() * 900000) + 100000; }

export function NewCampaignScreen() {
  const m = meta.value!;
  const [faction, setFaction] = useState(1);
  const [empires, setEmpires] = useState(5);
  const [size, setSize] = useState<(typeof SIZES)[number]['id']>('medium');
  const [era, setEra] = useState(1);
  const [ruleset, setRuleset] = useState<'reference' | 'prototype'>('reference');
  const [seed, setSeed] = useState<number>(randomSeed());
  const [events, setEvents] = useState(true);
  const [aggression, setAggression] = useState(true);
  const [preset, setPreset] = useState<'remastered' | 'classic'>('remastered');
  const chosen = FACTIONS.find((f) => f.id === faction)!;
  const p = palette(faction);
  const sz = SIZES.find((s) => s.id === size)!;
  const effectiveEmpires = Math.max(empires, faction);
  const eraInfo = m.eras.find((e) => e.id === era);
  const start = () => {
    startCampaign({
      faction, empires: effectiveEmpires, seed, era, ruleset, width: sz.width, height: sz.height, systems: sz.systems, events, aiAggression: aggression, preset,
    });
    if (view.value) maybeStartTutorial();
  };
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === 'INPUT' || t.tagName === 'BUTTON')) return;
      if (e.key === 'Enter') { e.preventDefault(); start(); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });
  return (
    <div class="setup-screen" data-testid="screen-new" style={{ '--faction': p.primary, '--faction-2': p.secondary } as never}>
      <Starfield accent={p.primary} />
      <div class="setup-grid">
        <header class="setup-head">
          <Pill tone="ghost" small onClick={() => back()} kbd="Esc">Back</Pill>
          <h1>New campaign</h1>
          <span class="setup-sub">Choose a power and shape the quadrant</span>
        </header>

        <div class="faction-cards" role="radiogroup" aria-label="Faction">
          {FACTIONS.map((f) => {
            const fp = palette(f.id);
            const name = m.empires.find((e) => e.id === f.id)?.name ?? f.short;
            return (
              <button type="button" key={f.id} role="radio" aria-checked={faction === f.id} data-testid={`faction-${f.id}`}
                class={`faction-card${faction === f.id ? ' selected' : ''}`}
                style={{ '--fc': fp.primary, '--fc2': fp.secondary, '--fglow': fp.glow } as never}
                onClick={() => { setFaction(f.id); if (empires < f.id) setEmpires(f.id); }}>
                <span class="fc-band" aria-hidden="true" />
                <span class="fc-emblem"><Emblem empire={f.id} size={46} /></span>
                <span class="fc-name">{name}</span>
                <span class="fc-capital">Capital · {f.capital}</span>
                <span class="fc-motto">“{f.motto}”</span>
                <span class="fc-traits">{f.traits.map((t) => <span key={t}>{t}</span>)}</span>
              </button>
            );
          })}
        </div>

        <div class="setup-options">
          <Frame title="Galaxy" tone="secondary">
            <div class="option-row" role="radiogroup" aria-label="Galaxy size">
              {SIZES.map((s) => (
                <Pill key={s.id} tone="ghost" active={size === s.id} onClick={() => setSize(s.id)} hint={`${s.width}×${s.height} sectors, ${s.systems} star systems. ${s.blurb}`}>{s.label}</Pill>
              ))}
            </div>
            <div class="option-label">Powers in play</div>
            <div class="option-row" role="radiogroup" aria-label="Empire count">
              {[2, 3, 4, 5].map((n) => (
                <Pill key={n} tone="ghost" active={effectiveEmpires === n} onClick={() => setEmpires(n)} disabled={n < faction}
                  reason={`The ${chosen.short} are power #${faction}: powers join in canonical order (Federation, Klingon, Romulan, Cardassian, Ferengi).`}
                  hint={`${n} major powers compete.`}>{n}</Pill>
              ))}
            </div>
            <div class="option-label">Seed</div>
            <div class="option-row">
              <input class="text-input mono" type="number" min={0} value={seed} aria-label="Galaxy seed" data-testid="seed-input"
                onInput={(e) => setSeed(Math.max(0, Math.floor(Number((e.target as HTMLInputElement).value) || 0)))} />
              <Pill tone="ghost" small onClick={() => setSeed(randomSeed())} hint="Roll a new galaxy">Randomise</Pill>
            </div>
          </Frame>
          <Frame title="Era & rules" tone="tertiary">
            <div class="era-list" role="radiogroup" aria-label="Starting era">
              {m.eras.map((e) => (
                <button type="button" key={e.id} role="radio" aria-checked={era === e.id} class={`era-item${era === e.id ? ' selected' : ''}`} onClick={() => setEra(e.id)}
                  {...tip(`${ERA_BLURBS[e.id] ?? ''}\nStarting technology level ${ruleset === 'reference' ? e.techLevel : e.prototypeTechLevel} in every field.`, `${e.name} era`)}>
                  <span class="era-name">{e.name}</span><span class="era-level">T{ruleset === 'reference' ? e.techLevel : e.prototypeTechLevel}</span>
                </button>
              ))}
            </div>
            <div class="option-row">
              <Pill tone="ghost" active={ruleset === 'reference'} onClick={() => setRuleset('reference')}
                hint="Reference catalogue ships and technologies, planets & terraforming, task forces, modern diplomacy and intelligence.">Reference rules</Pill>
              <Pill tone="ghost" active={ruleset === 'prototype'} onClick={() => setRuleset('prototype')}
                hint="The original compact prototype ruleset with generic hulls.">Prototype rules</Pill>
            </div>
            <div class="option-label">Gameplay</div>
            <div class="option-row" role="radiogroup" aria-label="Ruleset preset">
              <Pill tone="ghost" active={preset === 'remastered'} onClick={() => setPreset('remastered')} testid="preset-remastered"
                hint="Every modern layer: outposts and starbases, nebulae, wormholes, black holes, neutron stars and drifting ion storms, curated galactic events with decisions, and scientific and diplomatic victories alongside conquest.">Remastered</Pill>
              <Pill tone="ghost" active={preset === 'classic'} onClick={() => setPreset('classic')} testid="preset-classic"
                hint="Original-like: outposts and starbases, the original random events, and conquest or domination victories only. No anomalies.">Classic</Pill>
            </div>
            <p class="option-note">{preset === 'remastered'
              ? 'Anomalies, galactic events with choices, stations and four paths to victory.'
              : 'Close to the 1999 original: stations, simple random events, conquest or domination.'}</p>
            <div class="option-row toggles">
              <label class="check"><input type="checkbox" checked={events} onChange={(e) => setEvents((e.target as HTMLInputElement).checked)} /> {preset === 'remastered' ? 'Galactic events' : 'Random events'}</label>
              <label class="check"><input type="checkbox" checked={aggression} onChange={(e) => setAggression((e.target as HTMLInputElement).checked)} /> Aggressive rivals</label>
            </div>
          </Frame>
          <div class="setup-summary" style={{ borderColor: p.primary }}>
            <div class="ss-head"><Emblem empire={faction} size={40} /><div><div class="ss-name">{m.empires.find((e) => e.id === faction)?.name}</div><div class="ss-sub">from {chosen.capital}</div></div></div>
            <ul>
              <li>{effectiveEmpires} major powers · {sz.systems} systems · {sz.width}×{sz.height} sectors</li>
              <li>{eraInfo?.name ?? ''} era · {ruleset === 'reference' ? 'reference' : 'prototype'} rules · {preset === 'remastered' ? 'Remastered' : 'Classic'} gameplay</li>
              <li>Seed {seed}{events ? ' · events' : ''}{aggression ? ' · aggressive AI' : ''}</li>
            </ul>
            <button type="button" class="launch" data-testid="start-campaign" onClick={start}>
              <span>Launch campaign</span><kbd>Enter</kbd>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
