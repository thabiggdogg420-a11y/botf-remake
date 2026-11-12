// Typed facade over the WebAssembly simulation core. Every call is synchronous once loaded.
import createBotfCore from '../generated/botf-core.mjs';
import type {
  BattleReport, Briefing, ColonyDetail, Command, DiplomacyView, Encounter, Encyclopedia, Forecast, GameView, IntelView,
  Meta, ResearchView, SaveBlob, StationDetail, TurnResult, WorkforcePreview,
} from './types';

export class CoreError extends Error {
  constructor(public readonly method: string, message: string) {
    super(message);
    this.name = 'CoreError';
  }
}

type RawCall = (method: string, args: string) => string;
let raw: RawCall | null = null;

export async function loadCore(): Promise<void> {
  if (raw) return;
  const module = await createBotfCore();
  raw = module.cwrap('botf_call', 'string', ['string', 'string']) as RawCall;
}

function call<T>(method: string, args: object = {}): T {
  if (!raw) throw new CoreError(method, 'Simulation core is not loaded');
  const text = raw(method, JSON.stringify(args));
  const reply = JSON.parse(text) as { ok: boolean; result?: T; error?: string };
  if (!reply.ok) throw new CoreError(method, reply.error ?? 'Unknown error');
  return reply.result as T;
}

export interface OrderOptions { cancelIds?: number[] }


// The core lists every star from turn 1; one the player has not explored arrives as {id, x, y, name: designation,
// explored: false}. The browser client's map draws known systems only, so uncharted entries are dropped here.
function charted(v: GameView): GameView {
  return v && Array.isArray(v.systems) ? { ...v, systems: v.systems.filter((s) => (s as { explored?: boolean }).explored !== false) } : v;
}

export const core = {
  meta: () => call<Meta>('meta'),
  encyclopedia: () => call<Encyclopedia>('encyclopedia'),
  newCampaign: (options: {
    faction: number; empires: number; seed: number; era: number; ruleset: 'reference' | 'prototype';
    width: number; height: number; systems: number; events: boolean; aiAggression: boolean; preset?: string;
    shape?: 'irregular' | 'spiral' | 'ring' | 'elliptical'; minorDensity?: 'none' | 'few' | 'some' | 'many'; difficulty?: 'simple' | 'easy' | 'normal' | 'hard' | 'expert' | 'impossible';
    startSystems?: number; planetsMin?: number; planetsMax?: number; starTypes?: boolean;
  }) => charted(call<GameView>('newCampaign', options)),
  newBattleDrill: (station = false) => charted(call<GameView>('newBattleDrill', { station })),
  newFrontierDrill: () => charted(call<GameView>('newFrontierDrill')),
  station: (stationId: number) => call<StationDetail>('station', { stationId }),
  newDiplomacyDrill: () => charted(call<GameView>('newDiplomacyDrill')),
  headlessFingerprint: () => call<string | number>('headlessFingerprint'),
  view: () => charted(call<GameView>('view')),
  submit: (command: Command, options: OrderOptions = {}) => call<{ id: number }>('submit', { command, ...options }),
  dryRun: (command: Command | null, options: OrderOptions = {}) =>
    call<{ ok: boolean; error: string }>('dryRun', command ? { command, ...options } : { ...options }),
  dryRunMany: (items: { command?: Command; cancelIds?: number[] }[]) => call<string[]>('dryRunMany', { items }),
  cancel: (ids: number[]) => call<boolean>('cancel', { ids }),
  route: (fleetId: number, x: number, y: number) =>
    call<{ cells: number[]; steps: number; speed: number; eta: number; supplied: boolean }>('route', { fleetId, x, y }),
  supply: (fleetId: number) => call<string>('supply', { fleetId }),
  colony: (systemId: number) => call<ColonyDetail>('colony', { systemId }),
  workforcePreview: (systemId: number, food: number, industry: number, research: number) =>
    call<WorkforcePreview>('workforcePreview', { systemId, food, industry, research }),
  research: () => call<ResearchView>('research'),
  forecast: () => call<Forecast>('forecast'),
  diplomacy: () => call<DiplomacyView>('diplomacy'),
  intel: () => call<IntelView>('intel'),
  endTurn: () => call<TurnResult>('endTurn'),
  briefing: () => call<Briefing | null>('briefing'),
  tactical: () => call<Encounter | null>('tactical'),
  tacticalOrder: (order: { fleetId?: number; groupId?: number; targetFleetId?: number; maneuver: string; all?: boolean }) =>
    call<number>('tacticalOrder', order),
  tacticalRound: () => call<Encounter>('tacticalRound'),
  tacticalAuto: () => call<Encounter>('tacticalAuto'),
  replays: () => call<{ index: number; turn: number; x: number; y: number; rounds: number; winner: number }[]>('replays'),
  replay: (index: number) => call<Encounter>('replay', { index }),
  battleReport: (index: number) => call<BattleReport>('battleReport', { index }),
  save: () => call<SaveBlob>('save'),
  load: (data: string, scenario = 'campaign', briefing = '') => charted(call<GameView>('load', { data, scenario, briefing })),
  fingerprint: () => call<string | number>('fingerprint'),
};
