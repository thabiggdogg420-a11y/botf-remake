// Contextual "?" help: one popover per screen explaining what it is for and how to drive it.
import { signal } from '@preact/signals';
import { useEffect, useRef } from 'preact/hooks';
import { screen, type Screen } from '../state/store';

export const helpOpen = signal(false);

interface HelpEntry { title: string; intro: string; points: string[] }

export const HELP: Partial<Record<Screen, HelpEntry>> = {
  galaxy: {
    title: 'Star chart',
    intro: 'The galaxy as your empire knows it. Charted sectors are clear; uncharted space is hatched, and the dashed line marks your sensor horizon.',
    points: [
      'Click a system or ship to inspect it; double-click your own system to manage it.',
      'Right-click a sector for orders. With a ship selected, right-click open space to set course at once (Shift+right-click always opens the menu).',
      'Hover a sector with a ship selected to preview the route and its ETA; the shaded area is the ship’s supply range.',
      'Drag or arrow keys pan, the wheel zooms, F fits the galaxy, H returns home, Tab cycles your ships.',
      'Alerts (top left) link to the problem; “Orders this turn” lists and cancels drafted orders.',
    ],
  },
  system: {
    title: 'System management',
    intro: 'Run one colony: its workforce, energy, construction queue and structures.',
    points: [
      'Workforce sliders split citizens between food, industry and research; the forecast updates live from the core.',
      'Build options come from the core with their cost and ETA; greyed items say why they cannot be built.',
      'Industry flows into the first queue item. Reorder, pin, remove or buy the current item with credits.',
      'Power plants (10 energy) and fusion plants (40) keep factories, labs, shipyards and batteries running.',
      'Governors can manage workforce and queue automatically; Q/E switch between colonies.',
    ],
  },
  empire: {
    title: 'Empire overview',
    intro: 'Every colony and ship at a glance, with the treasury breakdown.',
    points: ['Sort and scan colonies for idle queues, food shortages and unassigned citizens.', 'Click a colony to jump to it.'],
  },
  research: {
    title: 'Research',
    intro: 'Six fields advance with your research output, split by the allocation you set.',
    points: ['Allocations must total 100%; apply them to draft the order.', 'Each level lists the ships and structures it unlocks.', 'The forecast shows progress expected at turn end.'],
  },
  fleets: {
    title: 'Fleets',
    intro: 'All your ships and task forces, their orders, stances and condition.',
    points: [
      'Stances become standing tactical orders in battle: Aggressive charges, Defensive evades, Withdraw retreats.',
      'Form task forces from ships in one sector: they move at the slowest member’s speed and fight together.',
    ],
  },
  diplomacy: {
    title: 'Diplomacy',
    intro: 'Relations with the major powers and minor races, your treaties and the proposals awaiting your answer.',
    points: [
      'Incoming: AI proposals and tribute demands have a deadline — unanswered ones are ignored. Calls to arms from allies must be honoured by declaring war before they fall due.',
      'Every empire remembers betrayals (war declared while bound by a treaty), refusals and grievances.',
      'The treaty ledger shows ended treaties, including those dissolved when an empire is eliminated.',
      'Declaring war takes effect at turn end and breaks every treaty with that empire.',
    ],
  },
  intel: {
    title: 'Intelligence',
    intro: 'Split intelligence output between internal security and operations against each rival.',
    points: [
      'Attack and defence strengths are 20-turn averages; a concentrated operation counts double.',
      'Failed operations may be exposed, costing trust and giving the target a grievance.',
      'Sabotage needs a war and a rival colony within sensor range.',
    ],
  },
  encyclopedia: {
    title: 'Database',
    intro: 'The reference catalogue: ships, technologies, structures and races.',
    points: ['Search by name; entries show costs, requirements and descriptions.'],
  },
  combat: {
    title: 'Tactical combat',
    intro: 'The strategic turn pauses while you command your ships round by round. Dashed lines show what every ship will do next round — “AI” marks decisions of the tactical AI.',
    points: [
      'Pick a scope (all ships, a like-class group or one ship), a target on the right, then a manoeuvre.',
      'Cloaked ships cannot fire: attacking decloaks them with a +25% ambush volley.',
      'Retreat: one round disengaging under fire (−25% incoming), then escape. Hold braces for −20% incoming.',
      'Ram is mutual: both ships take the other’s hull (and shields) as damage.',
      'Hail: if every ship of ours hails and the enemy accepts, a truce ends the battle before any fire.',
    ],
  },
};

export function HelpButton() {
  const entry = HELP[screen.value];
  if (!entry) return null;
  return (
    <button type="button" class={`help-btn${helpOpen.value ? ' active' : ''}`} aria-label={`Help: ${entry.title}`} aria-expanded={helpOpen.value}
      data-testid="help-button" onClick={(e) => { e.stopPropagation(); helpOpen.value = !helpOpen.value; }}>?</button>
  );
}

export function HelpPopover() {
  const entry = HELP[screen.value];
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!helpOpen.value) return;
    const close = (e: Event) => {
      const t = e.target as Element | null;
      if (ref.current && !ref.current.contains(t as Node) && !t?.closest?.('.help-btn')) helpOpen.value = false;
    };
    const esc = (e: KeyboardEvent) => { if (e.key === 'Escape') { e.stopPropagation(); helpOpen.value = false; } };
    window.addEventListener('pointerdown', close, true);
    window.addEventListener('keydown', esc, true);
    return () => { window.removeEventListener('pointerdown', close, true); window.removeEventListener('keydown', esc, true); };
  }, [helpOpen.value]);
  useEffect(() => { helpOpen.value = false; }, [screen.value]);
  if (!helpOpen.value || !entry) return null;
  return (
    <div class="help-popover" role="dialog" aria-label={`Help: ${entry.title}`} ref={ref} data-testid="help-popover">
      <div class="hp-head"><span class="hp-kicker">Help</span><h3>{entry.title}</h3></div>
      <p>{entry.intro}</p>
      <ul>{entry.points.map((p) => <li key={p}>{p}</li>)}</ul>
      <div class="hp-foot">Press <kbd>?</kbd> to toggle · hover any control for details</div>
    </div>
  );
}
