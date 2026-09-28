// Cross-screen navigation targets (diplomacy dossiers, intel cards) that other screens can open.
import { signal } from '@preact/signals';
import type { GameView, SystemView } from '../core/types';
import { goto } from './store';

/** Selected counterpart in the diplomacy screen: a major empire id, or a minor's system id. */
export const diplomacyFocus = signal<{ minor: boolean; id: number } | null>(null);
export const intelFocus = signal<number | null>(null);

export function diplomacyTarget(v: GameView, system: SystemView): { name: string; open: () => void } | null {
  if (system.minor && !system.minor.unknown) {
    return { name: system.minor.name, open: () => { diplomacyFocus.value = { minor: true, id: system.id }; goto('diplomacy'); } };
  }
  if (system.owner && system.owner !== v.meta.player) {
    const rel = v.empires.find((e) => e.empire === system.owner);
    if (rel?.contact) return { name: rel.name, open: () => { diplomacyFocus.value = { minor: false, id: rel.empire }; goto('diplomacy'); } };
  }
  return null;
}

export function intelTarget(v: GameView, system: SystemView): { name: string; open: () => void } | null {
  if (!system.owner || system.owner === v.meta.player || !system.visible) return null;
  const rel = v.empires.find((e) => e.empire === system.owner);
  if (!rel?.contact) return null;
  return { name: rel.name, open: () => { intelFocus.value = rel.empire; goto('intel'); } };
}
