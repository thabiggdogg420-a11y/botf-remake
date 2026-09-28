// Application state (Preact signals) and the actions that talk to the simulation core.
import { batch, computed, signal } from '@preact/signals';
import { core, CoreError } from '../core/engine';
import type { Briefing, Command, Encounter, GameView, Meta } from '../core/types';
import { blip } from '../ui/audio';
import { autosave, type SaveEnvelope } from './saves';

export type Screen =
  | 'title' | 'new' | 'saves'
  | 'galaxy' | 'system' | 'empire' | 'research' | 'fleets' | 'diplomacy' | 'intel' | 'encyclopedia' | 'combat';

export const GAME_SCREENS: { id: Screen; label: string; key: string; hint: string }[] = [
  { id: 'galaxy', label: 'Galaxy', key: 'F1', hint: 'Star chart, fleets and orders' },
  { id: 'system', label: 'System', key: 'F2', hint: 'Colony management' },
  { id: 'empire', label: 'Empire', key: 'F3', hint: 'All colonies and fleets' },
  { id: 'research', label: 'Research', key: 'F4', hint: 'Technology and allocation' },
  { id: 'fleets', label: 'Fleets', key: 'F5', hint: 'Ships and task forces' },
  { id: 'diplomacy', label: 'Diplomacy', key: 'F6', hint: 'Empires, minors and treaties' },
  { id: 'intel', label: 'Intel', key: 'F7', hint: 'Espionage and security' },
  { id: 'encyclopedia', label: 'Database', key: 'F8', hint: 'Ships, technologies, structures' },
];

export interface Selection { cell: number | null; systemId: number | null; fleetId: number | null; taskForceId: number | null }
export interface Toast { id: number; text: string; tone: 'info' | 'good' | 'warn' | 'alert' }
export interface ConfirmDialog {
  title: string; body: string; lines?: string[]; confirm: string; cancel?: string; tone?: 'alert' | 'primary';
  onConfirm: () => void;
}
export interface MapLayers { territory: boolean; supply: boolean; routes: boolean; grid: boolean; labels: boolean; trade: boolean }

export const meta = signal<Meta | null>(null);
export const view = signal<GameView | null>(null);
export const screen = signal<Screen>('title');
export const history = signal<Screen[]>([]);
export const selection = signal<Selection>({ cell: null, systemId: null, fleetId: null, taskForceId: null });
export const briefing = signal<Briefing | null>(null);
export const briefingOpen = signal(false);
export const encounter = signal<Encounter | null>(null);
export const toasts = signal<Toast[]>([]);
export const confirmDialog = signal<ConfirmDialog | null>(null);
export const mapLayers = signal<MapLayers>({ territory: true, supply: true, routes: true, grid: true, labels: true, trade: true });
export const focusRequest = signal<{ x: number; y: number; zoom?: number; frame?: boolean; seq: number } | null>(null);
export const busy = signal(false);
export const menuOpen = signal(false);
export const saveName = signal<string>('');

export const player = computed(() => view.value?.meta.player ?? 0);
export const inGame = computed(() => view.value !== null);

let toastSeq = 0;
export function toast(text: string, tone: Toast['tone'] = 'info', ms = 3400): void {
  const id = ++toastSeq;
  toasts.value = [...toasts.value.slice(-4), { id, text, tone }];
  setTimeout(() => { toasts.value = toasts.value.filter((t) => t.id !== id); }, ms);
}

export function reportError(error: unknown): void {
  const message = error instanceof CoreError ? error.message : error instanceof Error ? error.message : String(error);
  toast(message, 'alert', 6000);
  blip('error');
}

export function refresh(): GameView | null {
  try {
    const next = core.view();
    view.value = next;
    return next;
  } catch (error) {
    reportError(error);
    return null;
  }
}

export function goto(next: Screen): void {
  if (next === screen.value) return;
  if (next === 'system') {
    const sel = selection.value;
    const colonies = view.value?.colonies ?? [];
    if (!colonies.some((c) => c.systemId === sel.systemId) && colonies[0]) {
      const first = colonies[0];
      selectSystem(first.systemId);
    }
  }
  history.value = [...history.value.slice(-12), screen.value];
  screen.value = next;
  menuOpen.value = false;
  blip('nav');
}

export function back(): void {
  const stack = history.value;
  const previous = stack[stack.length - 1];
  if (!previous) { if (screen.value !== 'galaxy' && view.value) screen.value = 'galaxy'; return; }
  history.value = stack.slice(0, -1);
  screen.value = previous;
}

export function focusMap(x: number, y: number, zoom?: number): void {
  focusRequest.value = { x, y, zoom, seq: (focusRequest.value?.seq ?? 0) + 1 };
}

/** Frames the camera on the charted part of the galaxy (every surveyed or sensed sector). */
export function frameKnownRegion(): void {
  focusRequest.value = { x: 0, y: 0, frame: true, seq: (focusRequest.value?.seq ?? 0) + 1 };
}

export function selectCell(cell: number | null): void {
  const v = view.value;
  if (!v || cell === null) { selection.value = { cell: null, systemId: null, fleetId: null, taskForceId: null }; return; }
  const x = cell % v.meta.width, y = Math.floor(cell / v.meta.width);
  const system = v.systems.find((s) => s.x === x && s.y === y);
  selection.value = { cell, systemId: system?.id ?? null, fleetId: null, taskForceId: null };
}

export function selectSystem(systemId: number): void {
  const v = view.value;
  const system = v?.systems.find((s) => s.id === systemId);
  if (!v || !system) return;
  selection.value = { cell: system.y * v.meta.width + system.x, systemId, fleetId: null, taskForceId: null };
}

export function selectFleet(fleetId: number): void {
  const v = view.value;
  if (!v) return;
  const fleet = v.fleets.find((f) => f.id === fleetId);
  if (fleet) {
    const cell = fleet.y * v.meta.width + fleet.x;
    const system = v.systems.find((s) => s.x === fleet.x && s.y === fleet.y);
    selection.value = { cell, systemId: system?.id ?? null, fleetId, taskForceId: fleet.taskForceId || null };
    return;
  }
  const contact = v.contacts.find((c) => c.id === fleetId);
  if (contact) {
    const cell = contact.y * v.meta.width + contact.x;
    const system = v.systems.find((s) => s.x === contact.x && s.y === contact.y);
    selection.value = { cell, systemId: system?.id ?? null, fleetId, taskForceId: null };
  }
}

/** Submits an order through the core; on rejection shows the core's reason. */
export function issue(command: Command, options: { cancelIds?: number[]; quiet?: boolean; success?: string } = {}): boolean {
  if (!view.value) return false;
  try {
    core.submit(command, { cancelIds: options.cancelIds ?? [] });
    refresh();
    if (options.success) toast(options.success, 'good', 2600);
    blip('confirm');
    return true;
  } catch (error) {
    if (!options.quiet) reportError(error);
    return false;
  }
}

export function cancelOrders(ids: number[], message?: string): void {
  if (!ids.length) return;
  try {
    core.cancel(ids);
    refresh();
    if (message) toast(message, 'info', 2200);
    blip('cancel');
  } catch (error) {
    reportError(error);
  }
}

/** Orders drafted this turn that a fresh order for the same fleet/task force must replace. */
export function unitOrderIds(fleetId: number | null, taskForceId: number | null, types?: string[]): number[] {
  const pending = view.value?.pending ?? [];
  return pending
    .filter((p) => {
      if (types && !types.includes(p.type)) return false;
      if (taskForceId) return p.taskForceId === taskForceId && p.type === 'SetTaskForceOrder';
      return fleetId !== null && p.fleetId === fleetId && p.type !== 'CreateTaskForce';
    })
    .map((p) => p.id);
}

export function dryRun(command: Command, cancelIds: number[] = []): string {
  try {
    return core.dryRun(command, { cancelIds }).error;
  } catch (error) {
    return error instanceof Error ? error.message : String(error);
  }
}

// ---------------------------------------------------------------------------------------------
// Campaign lifecycle
// ---------------------------------------------------------------------------------------------
function enterGame(next: GameView, name: string): void {
  batch(() => {
    view.value = next;
    briefing.value = null;
    briefingOpen.value = false;
    encounter.value = null;
    history.value = [];
    saveName.value = name;
    screen.value = 'galaxy';
    const home = next.colonies[0];
    if (home) selectSystem(home.systemId);
    else selection.value = { cell: null, systemId: null, fleetId: null, taskForceId: null };
  });
  frameKnownRegion();
}

export function startCampaign(options: Parameters<typeof core.newCampaign>[0]): void {
  try {
    const next = core.newCampaign(options);
    enterGame(next, `${next.meta.playerName} campaign ${String(next.meta.seed)}`);
    toast(`Campaign begins. ${next.meta.playerName}, turn ${next.meta.turn}.`, 'good');
    blip('start');
    autosave();
  } catch (error) {
    reportError(error);
  }
}

export function startDrill(): void {
  try {
    const next = core.newBattleDrill();
    enterGame(next, 'Tactical drill');
    toast('Tactical drill: a Klingon patrol holds Sol and war has been drafted. End the turn to engage.', 'warn', 5000);
    blip('start');
  } catch (error) {
    reportError(error);
  }
}

export function startDiplomacyDrill(): void {
  try {
    const next = core.newDiplomacyDrill();
    enterGame(next, 'Diplomacy drill');
    toast('Diplomacy drill: proposals await your answer and an ally calls you to arms. Open Diplomacy (F6).', 'warn', 5000);
    blip('start');
  } catch (error) {
    reportError(error);
  }
}

export function loadEnvelope(envelope: SaveEnvelope): boolean {
  try {
    const next = core.load(envelope.data, envelope.scenario ?? 'campaign', envelope.briefing ?? '');
    enterGame(next, envelope.name);
    briefing.value = envelope.briefing ? (JSON.parse(envelope.briefing) as Briefing) : null;
    if (next.session.battlePending) {
      encounter.value = core.tactical();
      screen.value = 'combat';
    }
    toast(`Loaded "${envelope.name}" (turn ${next.meta.turn}).`, 'good');
    return true;
  } catch (error) {
    reportError(error);
    return false;
  }
}

export function quitToTitle(): void {
  batch(() => {
    screen.value = 'title';
    history.value = [];
    menuOpen.value = false;
    briefingOpen.value = false;
  });
}

// ---------------------------------------------------------------------------------------------
// Turn flow
// ---------------------------------------------------------------------------------------------
export function endTurnWarnings(v: GameView): string[] {
  const out: string[] = [];
  const idle = v.colonies.filter((c) => c.queueLength === 0).length;
  if (idle) out.push(`${idle} ${idle === 1 ? 'colony has' : 'colonies have'} nothing under construction.`);
  const starving = v.alerts.filter((a) => a.kind === 'food').length;
  if (starving) out.push(`${starving} ${starving === 1 ? 'colony faces' : 'colonies face'} a food shortage.`);
  const unassigned = v.colonies.reduce((sum, c) => sum + Math.max(0, c.workers.unassigned), 0);
  if (unassigned > 0) out.push(`${unassigned} citizens are unassigned.`);
  const idleShips = v.fleets.filter((f) => !f.hasOrders && f.x === f.targetX && f.y === f.targetY && !f.terraforming).length;
  if (idleShips) out.push(`${idleShips} ${idleShips === 1 ? 'ship has' : 'ships have'} no orders.`);
  if (v.empire.income.net < 0) out.push(`Treasury falls by ${-v.empire.income.net} credits per turn.`);
  return out;
}

export function requestEndTurn(skipConfirm = false): void {
  const v = view.value;
  if (!v || busy.value) return;
  if (v.meta.outcome !== 'ongoing') { toast('The campaign has ended. Start a new one from the menu.', 'warn'); return; }
  if (v.session.battlePending && !v.session.battleFinished) { openCombat(); return; }
  const warnings = endTurnWarnings(v);
  if (skipConfirm || v.session.active) { endTurn(); return; }
  confirmDialog.value = {
    title: `End turn ${v.meta.turn}?`,
    body: warnings.length ? 'Before the turn resolves, note:' : 'All systems report ready. Orders will be carried out.',
    lines: warnings,
    confirm: 'End turn',
    cancel: 'Keep planning',
    tone: 'primary',
    onConfirm: () => endTurn(),
  };
}

export function openCombat(): void {
  try {
    encounter.value = core.tactical();
    if (encounter.value) { if (screen.value !== 'combat') goto('combat'); }
  } catch (error) {
    reportError(error);
  }
}

export function endTurn(): void {
  if (busy.value) return;
  busy.value = true;
  blip('endturn');
  try {
    const result = core.endTurn();
    refresh();
    if (result.status === 'battle') {
      encounter.value = core.tactical();
      goto('combat');
      toast('Enemy contact! The turn is paused for your tactical orders.', 'alert', 4000);
      return;
    }
    encounter.value = null;
    if (screen.value === 'combat') screen.value = 'galaxy';
    if (result.briefing) {
      briefing.value = result.briefing;
      briefingOpen.value = result.briefing.events.length > 0 || result.briefing.outcome !== 'ongoing';
    }
    autosave();
    const v = view.value;
    if (v && v.meta.outcome !== 'ongoing') toast(v.meta.outcome === 'victory' ? 'VICTORY — your empire commands the quadrant.' : 'DEFEAT — your empire has fallen.', v.meta.outcome === 'victory' ? 'good' : 'alert', 12000);
  } catch (error) {
    reportError(error);
    refresh();
  } finally {
    busy.value = false;
  }
}

export function loadMeta(): void {
  meta.value = core.meta();
}
