// Contextual actions for a sector, built from the view and validated by dry-running the exact
// command through the core, so every disabled reason is the core's own message.
import { core } from '../core/engine';
import type { Command, FleetView, GameView } from '../core/types';
import {
  cancelOrders, dryRun, focusMap, goto, issue, selectFleet, selectSystem, selection, unitOrderIds, view,
} from './store';
import { diplomacyTarget, intelTarget } from './navigation';

export type ActionTone = 'primary' | 'secondary' | 'tertiary' | 'alert' | 'good' | 'ghost' | 'warn';
export interface Action {
  id: string;
  label: string;
  detail?: string;
  tone?: ActionTone;
  disabled?: string;   // reason when unavailable
  group: 'orders' | 'units' | 'system' | 'view';
  run: () => void;
}

export function selectedUnit(v: GameView): { fleet: FleetView; taskForceId: number } | null {
  const sel = selection.value;
  if (!sel.fleetId) return null;
  const fleet = v.fleets.find((f) => f.id === sel.fleetId);
  if (!fleet) return null;
  return { fleet, taskForceId: fleet.taskForceId || 0 };
}

export function unitName(v: GameView, fleet: FleetView): string {
  if (fleet.taskForceId) {
    const tf = v.taskForces.find((t) => t.id === fleet.taskForceId);
    if (tf) return `${tf.name} (${tf.fleetIds.length} ships)`;
  }
  return `${fleet.name} #${fleet.id}`;
}

function moveCommand(fleet: FleetView, x: number, y: number, targetFleetId = 0): Command {
  if (fleet.taskForceId) {
    return { type: 'SetTaskForceOrder', taskForceId: fleet.taskForceId, targetX: x, targetY: y, stance: fleet.stance, targetFleetId };
  }
  return targetFleetId ? { type: 'AttackFleet', fleetId: fleet.id, targetFleetId } : { type: 'MoveFleet', fleetId: fleet.id, targetX: x, targetY: y };
}

export function replaceIds(fleet: FleetView): number[] {
  return unitOrderIds(fleet.id, fleet.taskForceId || null);
}

export function plotCourse(fleet: FleetView, x: number, y: number): boolean {
  const v = view.value!;
  const ok = issue(moveCommand(fleet, x, y), { cancelIds: replaceIds(fleet) });
  if (ok) {
    const system = v.systems.find((s) => s.x === x && s.y === y);
    const updated = view.value?.fleets.find((f) => f.id === fleet.id);
    const eta = updated?.eta ?? 0;
    import('./store').then((m) => m.toast(`${unitName(v, fleet)}: course set for ${system ? system.name : `sector ${x}.${y}`}${eta ? ` — ${eta} turn${eta === 1 ? '' : 's'}` : ''}.`, 'good', 2800));
  }
  return ok;
}

export function sectorLabel(v: GameView, cell: number): string {
  const x = cell % v.meta.width, y = Math.floor(cell / v.meta.width);
  const system = v.systems.find((s) => s.x === x && s.y === y);
  if (system) return system.name;
  return v.fog[cell] === '0' ? `Uncharted sector ${x}.${y}` : `Sector ${x}.${y}`;
}

export function cellActions(cell: number): Action[] {
  const v = view.value;
  if (!v) return [];
  const W = v.meta.width;
  const x = cell % W, y = Math.floor(cell / W);
  const system = v.systems.find((s) => s.x === x && s.y === y) ?? null;
  const unit = selectedUnit(v);
  const actions: Action[] = [];
  const battleLock = v.session.active ? 'Finish the battle in progress first' : undefined;

  if (unit) {
    const { fleet } = unit;
    const here = fleet.x === x && fleet.y === y;
    const cancelIds = replaceIds(fleet);
    const name = unitName(v, fleet);
    if (!here) {
      let label = 'Set course here';
      let reason: string | undefined = battleLock;
      try {
        const route = core.route(fleet.id, x, y);
        if (route.steps < 0) reason ??= route.supplied ? 'No supplied path to this sector' : 'Outside this unit’s supply range';
        else label = `Set course here — ${route.eta} turn${route.eta === 1 ? '' : 's'}`;
      } catch (e) { reason ??= e instanceof Error ? e.message : String(e); }
      const command = moveCommand(fleet, x, y);
      if (!reason) reason = dryRun(command, cancelIds) || undefined;
      actions.push({ id: 'move', label, detail: `${name} moves when the turn ends`, tone: 'primary', disabled: reason, group: 'orders', run: () => plotCourse(fleet, x, y) });
    }
    for (const contact of v.contacts.filter((c) => c.x === x && c.y === y).slice(0, 3)) {
      const command = moveCommand(fleet, x, y, contact.id);
      const reason = battleLock ?? (dryRun(command, cancelIds) || undefined);
      actions.push({
        id: `attack-${contact.id}`, label: `Attack ${contact.name}`, detail: `${contact.ownerName} #${contact.id} — the battle is fought at turn end`,
        tone: 'alert', disabled: reason, group: 'orders',
        run: () => { issue(command, { cancelIds, success: `${name} will engage ${contact.name}.` }); },
      });
    }
    if (here) {
      if (fleet.colonyShip) {
        const command: Command = { type: 'Colonize', fleetId: fleet.id };
        actions.push({
          id: 'colonize', label: system ? `Colonize ${system.name}` : 'Colonize', detail: 'The colony ship is consumed to found a colony', tone: 'good',
          disabled: battleLock ?? (dryRun(command, cancelIds) || undefined), group: 'orders',
          run: () => { issue(command, { cancelIds, success: `Colony ship ordered to settle ${system?.name ?? 'here'}.` }); },
        });
        if (system && v.meta.campaignExpansion) {
          for (const planet of system.planets) {
            if (planet.terraformState !== 'Untouched' && planet.terraformState !== 'In Progress' && planet.terraformState !== 'InProgress') continue;
            const t: Command = { type: 'TerraformPlanet', fleetId: fleet.id, systemId: system.id, planetIndex: planet.index };
            actions.push({
              id: `terraform-${planet.index}`, label: `Terraform ${planet.name}`,
              detail: `${planet.className}: capacity ${planet.baseCapacity} → ${planet.maxCapacity} (${planet.terraformWork}/${planet.terraformNeeded} work)`,
              tone: 'secondary', disabled: battleLock ?? (dryRun(t, cancelIds) || undefined), group: 'orders',
              run: () => { issue(t, { cancelIds, success: `Terraforming ${planet.name} begins at turn end.` }); },
            });
          }
        }
      }
      if (fleet.shipClass === 'Transport' && system) {
        const command: Command = { type: 'Invade', fleetId: fleet.id, systemId: system.id };
        actions.push({
          id: 'invade', label: `Invade ${system.name}`, detail: 'Ground assault resolves at turn end', tone: 'alert',
          disabled: battleLock ?? (dryRun(command, cancelIds) || undefined), group: 'orders',
          run: () => { issue(command, { cancelIds, success: `Invasion of ${system.name} ordered.` }); },
        });
      }
      if (fleet.targetX !== fleet.x || fleet.targetY !== fleet.y || cancelIds.length) {
        actions.push({
          id: 'hold', label: 'Hold position', detail: 'Cancel this unit’s orders for the turn', tone: 'tertiary', group: 'orders',
          disabled: battleLock,
          run: () => {
            if (cancelIds.length) cancelOrders(cancelIds, 'Orders cancelled.');
            const refreshed = view.value?.fleets.find((f) => f.id === fleet.id);
            if (refreshed && (refreshed.targetX !== refreshed.x || refreshed.targetY !== refreshed.y)) {
              issue(moveCommand(refreshed, refreshed.x, refreshed.y), { cancelIds: replaceIds(refreshed), success: 'Holding position.' });
            }
          },
        });
      }
    }
  }

  // Units in this sector
  const units = v.fleets.filter((f) => f.x === x && f.y === y);
  const seen = new Set<number>();
  for (const f of units) {
    if (f.taskForceId) { if (seen.has(f.taskForceId)) continue; seen.add(f.taskForceId); }
    if (unit && (f.id === unit.fleet.id || (f.taskForceId && f.taskForceId === unit.taskForceId))) continue;
    if (actions.filter((a) => a.group === 'units').length >= 5) break;
    actions.push({ id: `select-${f.id}`, label: `Select ${unitName(v, f)}`, group: 'units', tone: 'secondary', run: () => selectFleet(f.id) });
  }
  for (const c of v.contacts.filter((c) => c.x === x && c.y === y).slice(0, 3)) {
    if (unit) continue;
    actions.push({ id: `inspect-${c.id}`, label: `Inspect ${c.name}`, detail: `${c.ownerName} #${c.id}`, group: 'units', tone: 'tertiary', run: () => selectFleet(c.id) });
  }

  if (system) {
    const own = system.owner === v.meta.player;
    if (own) actions.push({ id: 'open', label: `Manage ${system.name}`, detail: 'Open the system screen (double-click)', tone: 'primary', group: 'system', run: () => { selectSystem(system.id); goto('system'); } });
    const dip = diplomacyTarget(v, system);
    if (dip) actions.push({ id: 'diplomacy', label: `Diplomacy: ${dip.name}`, group: 'system', tone: 'secondary', run: () => dip.open() });
    const intel = intelTarget(v, system);
    if (intel) actions.push({ id: 'intel', label: `Intelligence on ${intel.name}`, group: 'system', tone: 'tertiary', run: () => intel.open() });
  }
  actions.push({ id: 'centre', label: 'Centre view here', group: 'view', tone: 'ghost', run: () => focusMap(x, y) });
  if (unit) actions.push({ id: 'deselect', label: 'Deselect unit', group: 'view', tone: 'ghost', run: () => { selection.value = { ...selection.value, fleetId: null, taskForceId: null }; } });
  return actions;
}
