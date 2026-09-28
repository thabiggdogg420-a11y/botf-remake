// Accessibility & interface settings, persisted per browser (guarded storage; defaults otherwise).
import { computed, signal } from '@preact/signals';
import { readJson, writeJson } from '../platform/storage';
import { setColorblindPalette } from '../ui/theme';

export interface Settings {
  /** Interface scale in percent (90–130). */
  uiScale: number;
  reducedMotion: boolean;
  /** Owner colours from the Okabe–Ito colour-blind-safe palette (emblem shapes stay the primary cue). */
  colorblind: boolean;
  autosave: boolean;
  /** Sound volume in percent (0–100) for interface and battle cues. */
  volume: number;
}

const KEY = 'botf.settings';
export const UI_SCALE_MIN = 90;
export const UI_SCALE_MAX = 130;

function systemPrefersReducedMotion(): boolean {
  try { return !!window.matchMedia?.('(prefers-reduced-motion: reduce)').matches; } catch { return false; }
}

function defaults(): Settings {
  return { uiScale: 100, reducedMotion: systemPrefersReducedMotion(), colorblind: false, autosave: true, volume: 70 };
}

function sanitize(raw: Partial<Settings> | null | undefined): Settings {
  const d = defaults();
  const scale = Number(raw?.uiScale);
  return {
    uiScale: Number.isFinite(scale) ? Math.min(UI_SCALE_MAX, Math.max(UI_SCALE_MIN, Math.round(scale / 5) * 5)) : d.uiScale,
    reducedMotion: typeof raw?.reducedMotion === 'boolean' ? raw.reducedMotion : d.reducedMotion,
    colorblind: typeof raw?.colorblind === 'boolean' ? raw.colorblind : d.colorblind,
    autosave: typeof raw?.autosave === 'boolean' ? raw.autosave : d.autosave,
    volume: Number.isFinite(Number(raw?.volume)) && raw?.volume !== undefined ? Math.min(100, Math.max(0, Math.round(Number(raw.volume)))) : d.volume,
  };
}

export const settings = signal<Settings>(sanitize(readJson<Partial<Settings> | null>(KEY, null)));
export const reducedMotion = computed(() => settings.value.reducedMotion);
/** Current interface zoom factor (1 = 100%). */
export const uiScale = computed(() => settings.value.uiScale / 100);

export function applySettings(s: Settings = settings.value): void {
  setColorblindPalette(s.colorblind);
  try {
    const root = document.documentElement;
    root.style.setProperty('--ui-scale', String(s.uiScale / 100));
    root.style.setProperty('--ui-unscale', String(100 / s.uiScale));
    root.dataset.motion = s.reducedMotion ? 'reduced' : 'full';
    root.dataset.palette = s.colorblind ? 'colorblind' : 'standard';
  } catch { /* no DOM (tests) */ }
}

export function updateSettings(patch: Partial<Settings>): void {
  const next = sanitize({ ...settings.value, ...patch });
  settings.value = next;
  applySettings(next);
  writeJson(KEY, next);
}

export function resetSettings(): void {
  updateSettings(defaults());
}

applySettings();
