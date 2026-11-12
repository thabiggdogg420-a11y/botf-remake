import { useEffect, useState } from 'preact/hooks';
import { back, meta, startCampaign, view } from '../state/store';
import { maybeStartTutorial } from '../state/tutorial';
import { Emblem, Frame, Pill, tip } from '../ui/components';
import { Starfield } from '../ui/Starfield';
import { FACTIONS, palette } from '../ui/theme';
import type { PresetInfo, ScenarioInfo } from '../core/types';

const SIZES = [
  // The one place the web client's galaxy sizes live (twin: godot/scripts/viewmodels/new_campaign_vm.gd SIZES).
  { id: 'small', label: 'Small', width: 16, height: 13, systems: 30, blurb: 'Quick, crowded — contact comes early.' },
  { id: 'medium', label: 'Medium', width: 22, height: 16, systems: 50, blurb: 'The default: room to grow, rivals within reach.' },
  { id: 'large', label: 'Large', width: 28, height: 20, systems: 80, blurb: 'Room to expand before borders meet.' },
  { id: 'huge', label: 'Huge', width: 34, height: 24, systems: 120, blurb: 'A long campaign across a wide quadrant.' },
] as const;

const TIMELINE_BLURBS: Record<number, string> = {
  1: 'The Original Series age: NX-era ships retire, Constitution-class cruisers enter service in 2245.',
  2: 'The film era: refit Constitutions, the Excelsior from 2285, D7s and Romulan Birds-of-Prey.',
  3: 'The dawn of the next generation: the Challenger and Cheyenne lines enter service from 2340.',
  4: 'Deep in the next generation; the Dominion War fleet (Intrepid, Akira) arrives from 2368.',
  5: 'The war is over: Prometheus, Mogai, Negh\'Tev and the Nemesis-era fleets, research to level 14.',
};
const ERA_BLURBS: Record<number, string> = {
  1: 'Warp-capable newcomers. Build everything from the ground up.',
  2: 'Established powers with developed technology.',
  3: 'Expanded empires, advanced shipyards.',
  4: 'Advanced civilisations on the brink of dominance.',
  5: 'Supreme technology from turn one.',
};

function randomSeed(): number { return Math.floor(Math.random() * 900000) + 100000; }

// Galaxy setup, read by the core since schema 15 (docs/design/GALAXY-GENERATOR.md).
const SHAPES = ['spiral', 'ring', 'irregular', 'elliptical'] as const;
const DENSITIES = ['none', 'few', 'some', 'many'] as const;
const DIFFICULTIES = ['simple', 'easy', 'normal', 'hard', 'impossible'] as const;
const cap = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);

// What a core without a preset list offers: the two built-ins (twin: new_campaign_vm.gd PRESET_FALLBACK).
const STOCK = ['Federation', 'Klingon', 'Romulan', 'Cardassian', 'Ferengi'];
const FALLBACK_PRESETS: PresetInfo[] = [
  { id: 'classic', name: 'Classic', rulesBase: 'classic', overlays: [], flags: {}, lineup: STOCK, lineupPlayable: true, lineupNote: '', needs: [],
    blurb: 'The original BOTF rules: outposts and starbases, the original random events, conquest and domination only. Even, uniform galaxy.',
    options: { empires: 5, era: 1, size: 'medium', shape: 'elliptical', minorDensity: 'some', difficulty: 'normal', events: true, aiAggression: true } },
  { id: 'remastered', name: 'Remastered', rulesBase: 'remastered', overlays: [], flags: {}, lineup: STOCK, lineupPlayable: true, lineupNote: '', needs: [],
    blurb: 'The default: every BOTF feature plus our layers. Galaxy features, curated events, WEGO combat, the expanded ship roster and the extra victories.',
    options: { empires: 5, era: 1, size: 'medium', shape: 'spiral', minorDensity: 'some', difficulty: 'normal', events: true, aiAggression: true } },
];
function presetText(p: PresetInfo): string {
  const lines = [p.blurb, `Powers: ${p.lineup.join(' · ')}`];
  if (!p.lineupPlayable) lines.push(`Not seated yet: ${p.lineupNote} This game plays the stock powers.`);
  if (p.needs.length) lines.push(`Content still to come: ${p.needs.join(', ')}.`);
  return lines.join('\n');
}

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
  const presets = m.presets && m.presets.length ? m.presets : FALLBACK_PRESETS;
  const [preset, setPreset] = useState<string>('remastered');
  const scenarios: ScenarioInfo[] = m.scenarios ?? [];
  const [scenario, setScenario] = useState<string>('');
  const scenarioDef = scenarios.find((q) => q.id === scenario);
  const presetDef = presets.find((q) => q.id === preset) ?? presets[0];
  const [shape, setShape] = useState<(typeof SHAPES)[number]>('spiral');
  const [minorDensity, setMinorDensity] = useState<(typeof DENSITIES)[number]>('some');
  const [difficulty, setDifficulty] = useState<(typeof DIFFICULTIES)[number]>('normal');
  const chosen = FACTIONS.find((f) => f.id === faction)!;
  const p = palette(faction);
  const sz = SIZES.find((s) => s.id === size)!;
  const effectiveEmpires = Math.max(empires, faction);
  const eraInfo = m.eras.find((e) => e.id === era);
  // Picking a preset fills the options it sets; the rest keep the player's choice. Powers in play never drop below the faction.
  const pickPreset = (q: PresetInfo) => {
    setPreset(q.id);
    const o = q.options;
    if (o.era !== undefined) setEra(o.era);
    if (o.size && SIZES.some((s) => s.id === o.size)) setSize(o.size);
    if (o.shape && (SHAPES as readonly string[]).includes(o.shape)) setShape(o.shape as (typeof SHAPES)[number]);
    if (o.minorDensity && (DENSITIES as readonly string[]).includes(o.minorDensity)) setMinorDensity(o.minorDensity as (typeof DENSITIES)[number]);
    if (o.difficulty && (DIFFICULTIES as readonly string[]).includes(o.difficulty)) setDifficulty(o.difficulty as (typeof DIFFICULTIES)[number]);
    if (o.events !== undefined) setEvents(o.events);
    if (o.aiAggression !== undefined) setAggression(o.aiAggression);
    if (o.empires !== undefined) setEmpires(Math.max(o.empires, faction));
  };
  const start = () => {
    startCampaign({
      faction, empires: effectiveEmpires, seed, era, ruleset, width: sz.width, height: sz.height, systems: sz.systems, events, aiAggression: aggression, preset, shape, minorDensity, difficulty,
      ...(scenarioDef ? { scenario: scenarioDef.id, faction: scenarioDef.playerId } : {}),
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
            <div class="option-label">Shape</div>
            <div class="option-row" role="radiogroup" aria-label="Galaxy shape">
              {SHAPES.map((s) => <Pill key={s} tone="ghost" active={shape === s} onClick={() => setShape(s)} testid={`shape-${s}`}>{cap(s)}</Pill>)}
            </div>
            <div class="option-label">Minor races</div>
            <div class="option-row" role="radiogroup" aria-label="Minor-race density">
              {DENSITIES.map((d) => <Pill key={d} tone="ghost" active={minorDensity === d} onClick={() => setMinorDensity(d)} testid={`density-${d}`}>{cap(d)}</Pill>)}
            </div>
            <div class="option-label">Difficulty</div>
            <div class="option-row" role="radiogroup" aria-label="Difficulty">
              {DIFFICULTIES.map((d) => <Pill key={d} tone="ghost" active={difficulty === d} onClick={() => setDifficulty(d)} testid={`difficulty-${d}`}>{cap(d)}</Pill>)}
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
              {m.eras.map((e) => {
                // Under a preset with the eras rule (Ages, Galaxies) the picker shows the timeline year and that preset's start level.
                const tl = presetDef.eras;
                const level = tl ? tl.startTech[e.id - 1]! : ruleset === 'reference' ? e.techLevel : e.prototypeTechLevel;
                const name = tl ? String(tl.years[e.id - 1]) : e.name;
                const blurb = tl ? TIMELINE_BLURBS[e.id] ?? '' : ERA_BLURBS[e.id] ?? '';
                return (
                  <button type="button" key={e.id} role="radio" aria-checked={era === e.id} class={`era-item${era === e.id ? ' selected' : ''}`} onClick={() => setEra(e.id)}
                    {...tip(`${blurb}\nStarting technology level ${level} in every field.${tl ? ` Research runs to level ${tl.techCap}; ships enter service and retire on the timeline.` : ''}`, tl ? `${name} start` : `${e.name} era`)}>
                    <span class="era-name">{name}</span><span class="era-level">T{level}</span>
                  </button>
                );
              })}
            </div>
            <div class="option-row">
              <Pill tone="ghost" active={ruleset === 'reference'} onClick={() => setRuleset('reference')}
                hint="Reference catalogue ships and technologies, planets & terraforming, task forces, modern diplomacy and intelligence.">Reference rules</Pill>
              <Pill tone="ghost" active={ruleset === 'prototype'} onClick={() => setRuleset('prototype')}
                hint="The original compact prototype ruleset with generic hulls.">Prototype rules</Pill>
            </div>
            {scenarios.length > 0 && (<>
              <div class="option-label">Scenario</div>
              <div class="option-row" role="radiogroup" aria-label="Scenario">
                <Pill tone="ghost" active={!scenarioDef} onClick={() => setScenario('')} testid="scenario-none" hint="A free game with the options below.">Free game</Pill>
                {scenarios.map((q) => (
                  <Pill key={q.id} tone="ghost" active={scenario === q.id} onClick={() => setScenario(q.id)} testid={`scenario-${q.id}`} hint={q.blurb}>{q.name}</Pill>
                ))}
              </div>
              {scenarioDef && <p class="option-note" data-testid="scenario-note">{scenarioDef.blurb} {scenarioDef.briefing.join(' ')} Preset, powers, era and map are set by the scenario.</p>}
            </>)}
            <div class="option-label">Preset</div>
            <div class="option-row" role="radiogroup" aria-label="Preset">
              {presets.map((q) => (
                <Pill key={q.id} tone="ghost" active={preset === q.id} onClick={() => pickPreset(q)} testid={`preset-${q.id}`} hint={presetText(q)}>{q.name}</Pill>
              ))}
            </div>
            <p class="option-note" data-testid="preset-note" style={{ whiteSpace: 'pre-line' }}>{presetText(presetDef)}</p>
            <div class="option-row toggles">
              <label class="check"><input type="checkbox" checked={events} onChange={(e) => setEvents((e.target as HTMLInputElement).checked)} /> {presetDef.rulesBase === 'remastered' ? 'Galactic events' : 'Random events'}</label>
              <label class="check"><input type="checkbox" checked={aggression} onChange={(e) => setAggression((e.target as HTMLInputElement).checked)} /> Aggressive rivals</label>
            </div>
          </Frame>
          <div class="setup-summary" style={{ borderColor: p.primary }}>
            <div class="ss-head"><Emblem empire={faction} size={40} /><div><div class="ss-name">{m.empires.find((e) => e.id === faction)?.name}</div><div class="ss-sub">from {chosen.capital}</div></div></div>
            <ul>
              <li>{effectiveEmpires} major powers · {sz.systems} systems · {sz.width}×{sz.height} sectors</li>
              <li>{presetDef.eras ? `${presetDef.eras.years[era - 1]} start` : `${eraInfo?.name ?? ''} era`} · {ruleset === 'reference' ? 'reference' : 'prototype'} rules · {presetDef.name} preset</li>
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
