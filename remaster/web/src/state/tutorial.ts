// "First Contact": a skippable, step-by-step introduction for new players. Each step names the
// element it points at and a condition on the game state that completes it, so the player learns
// by doing the real thing. Dismissal is remembered in (guarded) localStorage.
import { effect, signal } from '@preact/signals';
import type { GameView } from '../core/types';
import { readText, removeKey, writeText } from '../platform/storage';
import { briefingOpen, screen, selection, view, type Screen } from './store';

const KEY = 'botf.tutorial';

export interface TutorialStep {
  id: string;
  title: string;
  body: string;
  /** CSS selector of the element to spotlight (none: centred card). */
  target?: string;
  /** Screen the step happens on (the card offers a shortcut back to it). */
  screen?: Screen;
  /** Completion test against the live state; steps without one advance with "Next". */
  done?: (ctx: StepContext) => boolean;
}
interface StepContext { v: GameView; baseline: Baseline }
interface Baseline { turn: number; enqueued: number; moved: number }

const count = (v: GameView, type: string) => v.pending.filter((p) => p.type === type).length;
const home = (v: GameView) => v.colonies[0];

export const STEPS: TutorialStep[] = [
  {
    id: 'welcome', title: 'First contact',
    body: 'Welcome, Captain. Your empire begins with one home system, a scout and a colony ship. This short briefing walks you through a first turn — you can skip it at any time and replay it from Settings.',
  },
  {
    id: 'select-home', title: 'Select your home system', screen: 'galaxy', target: '[data-testid=galaxy-map]',
    body: 'Click your home system on the star chart. The inspector on the right shows its planets, population, food and industry.',
    done: ({ v }) => screen.value === 'galaxy' && !!home(v) && selection.value.systemId === home(v)!.systemId,
  },
  {
    id: 'manage', title: 'Manage the system', screen: 'galaxy', target: '[data-testid=open-system]',
    body: 'Press “Manage system” (or F2) to open the colony screen, where you staff the workforce and plan construction.',
    done: () => screen.value === 'system',
  },
  {
    id: 'queue', title: 'Queue construction', screen: 'system', target: '[data-testid=build-options]',
    body: 'Pick a structure or ship from Build options. Industry works on the first item in the queue each turn; items that cannot be built explain why.',
    done: ({ v, baseline }) => count(v, 'EnqueueBuilding') > baseline.enqueued,
  },
  {
    id: 'scout', title: 'Send out the scout', screen: 'galaxy', target: '[data-testid=galaxy-map]',
    body: 'Back on the chart (F1), click your scout, then right-click a sector beyond the dashed sensor horizon to set course. Surveying uncharted space reveals systems and neighbours.',
    done: ({ v, baseline }) => count(v, 'MoveFleet') + count(v, 'SetTaskForceOrder') > baseline.moved,
  },
  {
    id: 'end-turn', title: 'End the turn', target: '[data-testid=end-turn]',
    body: 'When your orders are drafted, end the turn (Enter). Every empire acts at once and the simulation core resolves the turn.',
    done: ({ v, baseline }) => v.meta.turn > baseline.turn,
  },
  {
    id: 'briefing', title: 'Read the briefing', target: '[data-testid=briefing]',
    body: 'The turn briefing lists what happened, grouped by category. Click an entry to jump to it, then Acknowledge to close the report.',
    done: ({ v, baseline }) => v.meta.turn > baseline.turn && !briefingOpen.value,
  },
  {
    id: 'finish', title: 'You have the conn',
    body: 'That is a full turn. Press “?” on any screen for help, hover anything for details, and open Settings from the menu for interface scale, reduced motion and a colour-blind palette.',
  },
];

export const tutorialStep = signal<number | null>(null);
let baseline: Baseline = { turn: 0, enqueued: 0, moved: 0 };

export function tutorialDismissed(): boolean {
  const state = readText(KEY);
  return state === 'done' || state === 'skipped';
}

function captureBaseline(): void {
  const v = view.value;
  if (!v) return;
  baseline = { turn: v.meta.turn, enqueued: count(v, 'EnqueueBuilding'), moved: count(v, 'MoveFleet') + count(v, 'SetTaskForceOrder') };
}

export function startTutorial(): void {
  captureBaseline();
  tutorialStep.value = 0;
}

/** Starts the tutorial for a fresh campaign unless the player dismissed it before. */
export function maybeStartTutorial(): void {
  if (!tutorialDismissed()) startTutorial();
}

export function nextTutorialStep(): void {
  const i = tutorialStep.value;
  if (i === null) return;
  if (i + 1 >= STEPS.length) { finishTutorial(); return; }
  // The turn baseline is re-taken before the end-turn step so earlier turns do not count.
  if (STEPS[i + 1]!.id === 'end-turn' && view.value) baseline = { ...baseline, turn: view.value.meta.turn };
  // The home system starts selected; ask for a real click.
  if (STEPS[i + 1]!.id === 'select-home') selection.value = { cell: null, systemId: null, fleetId: null, taskForceId: null };
  tutorialStep.value = i + 1;
}

export function skipTutorial(): void {
  tutorialStep.value = null;
  writeText(KEY, 'skipped');
}

export function finishTutorial(): void {
  tutorialStep.value = null;
  writeText(KEY, 'done');
}

export function resetTutorial(): void {
  removeKey(KEY);
}

// Advance automatically when the live state satisfies the current step.
effect(() => {
  const i = tutorialStep.value;
  const v = view.value;
  // Subscribe to the signals the conditions read.
  void screen.value; void selection.value; void briefingOpen.value;
  if (i === null || !v) return;
  const step = STEPS[i];
  if (step?.done && step.done({ v, baseline })) queueMicrotask(() => { if (tutorialStep.value === i) nextTutorialStep(); });
});
