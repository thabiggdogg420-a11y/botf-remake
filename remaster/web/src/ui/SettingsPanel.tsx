// Accessibility & interface settings: interface scale, reduced motion, colour-blind owner palette,
// sound and autosave. Everything is stored per browser (see state/settings.ts).
import { signal } from '@preact/signals';
import { useEffect, useRef } from 'preact/hooks';
import { resetSettings, settings, UI_SCALE_MAX, UI_SCALE_MIN, updateSettings } from '../state/settings';
import { view } from '../state/store';
import { resetTutorial, startTutorial } from '../state/tutorial';
import { audioEnabled, setAudio } from './audio';
import { Emblem, Frame, Pill } from './components';

export const settingsOpen = signal(false);

function Toggle({ label, detail, checked, onChange, testid }: { label: string; detail: string; checked: boolean; onChange: (on: boolean) => void; testid: string }) {
  return (
    <div class="setting-row">
      <div class="sr-text"><div class="sr-label" id={`${testid}-label`}>{label}</div><div class="sr-detail">{detail}</div></div>
      <button type="button" role="switch" aria-checked={checked} aria-labelledby={`${testid}-label`} class={`switch${checked ? ' on' : ''}`}
        data-testid={testid} onClick={() => onChange(!checked)}><span class="knob" /></button>
    </div>
  );
}

export function SettingsModal() {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => { if (settingsOpen.value) ref.current?.querySelector<HTMLElement>('input, button')?.focus(); }, [settingsOpen.value]);
  if (!settingsOpen.value) return null;
  const s = settings.value;
  const close = () => { settingsOpen.value = false; };
  const inGame = !!view.value;
  return (
    <div class="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) close(); }}
      onKeyDown={(e) => { if (e.key === 'Escape') { e.stopPropagation(); close(); } }}>
      <div class="modal settings-modal" role="dialog" aria-modal="true" aria-label="Settings" data-testid="settings" ref={ref}>
        <Frame title="Settings" sub="stored in this browser" actions={<Pill small tone="ghost" onClick={close} testid="settings-close">Close</Pill>}>
          <div class="setting-row">
            <div class="sr-text"><label class="sr-label" for="ui-scale">Interface scale</label><div class="sr-detail">Enlarges or shrinks text and controls ({UI_SCALE_MIN}–{UI_SCALE_MAX}%). The star chart keeps its own zoom.</div></div>
            <div class="scale-control">
              <input id="ui-scale" class="slider" type="range" min={UI_SCALE_MIN} max={UI_SCALE_MAX} step={5} value={s.uiScale} data-testid="setting-scale"
                style={{ '--pct': `${((s.uiScale - UI_SCALE_MIN) / (UI_SCALE_MAX - UI_SCALE_MIN)) * 100}%` } as never}
                onInput={(e) => updateSettings({ uiScale: Number((e.target as HTMLInputElement).value) })} />
              <b class="mono" data-testid="setting-scale-value">{s.uiScale}%</b>
            </div>
          </div>
          <Toggle label="Reduced motion" detail="Stops camera glides, pulsing effects and battle animations; rounds show their result at once." checked={s.reducedMotion}
            onChange={(on) => updateSettings({ reducedMotion: on })} testid="setting-motion" />
          <div class="setting-row">
            <div class="sr-text"><div class="sr-label" id="setting-colorblind-label">Colour-blind-safe owner colours</div>
              <div class="sr-detail">Okabe–Ito palette for empire colours on the chart and in battle. Emblem shapes always identify owners too.</div>
              <div class="swatches" aria-hidden="true">{[1, 2, 3, 4, 5].map((id) => <Emblem key={id} empire={id} size={18} />)}</div></div>
            <button type="button" role="switch" aria-checked={s.colorblind} aria-labelledby="setting-colorblind-label" class={`switch${s.colorblind ? ' on' : ''}`}
              data-testid="setting-colorblind" onClick={() => updateSettings({ colorblind: !s.colorblind })}><span class="knob" /></button>
          </div>
          <Toggle label="Interface sound" detail="Short synthesised cues for orders, turns and battle." checked={audioEnabled.value}
            onChange={(on) => setAudio(on)} testid="setting-sound" />
          <Toggle label="Autosave every turn" detail="Keeps one rolling autosave slot for the campaign in progress." checked={s.autosave}
            onChange={(on) => updateSettings({ autosave: on })} testid="setting-autosave" />
          <div class="settings-foot">
            {inGame ? <Pill small tone="secondary" onClick={() => { resetTutorial(); close(); startTutorial(); }} testid="replay-tutorial">Replay First Contact tutorial</Pill>
              : <Pill small tone="ghost" onClick={() => resetTutorial()} hint="The tutorial will start with your next new campaign">Show tutorial again</Pill>}
            <span class="grow" />
            <Pill small tone="ghost" onClick={() => resetSettings()} testid="settings-reset">Reset to defaults</Pill>
          </div>
        </Frame>
      </div>
    </div>
  );
}
