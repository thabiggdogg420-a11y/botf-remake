// Hooks for end-to-end tests and debugging (window.__botf). They expose the current view and map
// geometry so tests can click precise canvas positions, plus camera focus; orders always go through
// the UI.
import { core } from './core/engine';
import { focusMap, view } from './state/store';

export interface MapHooks {
  cellCenter(cell: number): { x: number; y: number } | null;
  fleetMarker(id: number): { x: number; y: number } | null;
  settled(): boolean;
}

interface BotfHooks {
  view: () => unknown;
  fingerprint: () => unknown;
  focus: (x: number, y: number, zoom?: number) => void;
  map?: MapHooks;
}

declare global {
  interface Window { __botf?: BotfHooks }
}

function base(): BotfHooks {
  return {
    view: () => view.value,
    fingerprint: () => { try { return core.fingerprint(); } catch { return null; } },
    focus: (x, y, zoom) => focusMap(x, y, zoom),
  };
}

export function exposeTestHooks(): void {
  window.__botf = { ...base(), map: window.__botf?.map };
}

export function registerMapHooks(hooks: MapHooks | undefined): void {
  window.__botf = { ...(window.__botf ?? base()), map: hooks };
}
