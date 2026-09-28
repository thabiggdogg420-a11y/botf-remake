// Shapes of the JSON produced by bridge/WebBridge.cpp. Kept deliberately structural: the bridge
// derives these from the core's public structs, so new fields simply appear.
import type { RosterDetail, RosterWeapon } from './roster';
export type { RosterDetail, RosterWeapon } from './roster';

export type Severity = 'info' | 'good' | 'warn' | 'alert';

export interface Meta {
  contentVersion: string;
  saveVersion: number;
  empires: { id: number; name: string; home: string; race: number }[];
  eras: { id: number; name: string; techLevel: number; prototypeTechLevel: number }[];
  researchFields: string[];
  maxTechLevel: number;
  maneuvers: { key: string; name: string }[];
  stances: string[];
  governors: { key: string; name: string }[];
  treaties: { key: string; name: string }[];
  durations: { value: number; name: string }[];
  tones: { key: string; name: string }[];
  departments: { key: string; name: string }[];
  structures: { key: string; name: string; industryCost: number; energyCost: number; upkeep: number; isShip: boolean }[];
}

export interface Economy {
  food: number; foodDeficit: number; industry: number; research: number;
  energyUsed: number; energyAvailable: number; inactiveFactories: number; inactiveLabs: number;
  income: number; upkeep: number; net: number; shipyardPowered: boolean;
  poweredBatteries: number; inactiveBatteries: number;
}
export interface Growth { enabled: boolean; min: number; max: number; upChance: number }

export interface Tech {
  field: number; name: string; level: number; progress: number; allocation: number; committedAllocation: number;
  current: string; next: string | null; cost: number;
}

export interface Planet {
  index: number; name: string; class: string; className: string; terraformState: string;
  terraformWork: number; terraformNeeded: number; baseCapacity: number; maxCapacity: number; capacity: number;
}

export interface SystemView {
  id: number; x: number; y: number; name: string; visible: boolean; owner: number;
  capacity: number; maxCapacity: number; planets: Planet[];
  minor?: {
    name: string; memberOf: number; attitude?: number; friendship?: boolean; unknown?: boolean;
    temperament?: string; bonus?: string; techLevel?: number; population?: number;
  };
}

export interface Governor {
  workforce: string; queue: string; override: boolean;
  workforceExplanation: string; queueExplanation: string; warning: string;
}

export interface ColonyBrief {
  systemId: number; name: string; population: number; capacity: number; morale: number;
  workers: { food: number; industry: number; research: number; unassigned: number };
  structures: { farms: number; factories: number; laboratories: number; shipyards: number; powerPlants: number; fusionPlants: number; defenseBatteries: number };
  energy: number; shipyardEnabled: boolean; economy: Economy; growth: Growth;
  queueLength: number; building?: { name: string; work: number; cost: number; eta: number };
  queueBlocker: string; governor: Governor;
}

export interface FleetView {
  id: number; owner: number; name: string; shipClass: string; className: string; referenceShipId: number;
  designClass?: string; designName?: string; roster?: RosterDetail | null;
  x: number; y: number; targetX: number; targetY: number; committedTargetX?: number; committedTargetY?: number;
  hull: number; maxHull: number; shields: number; maxShields: number; attack: number; range: number; speed: number;
  maintenance: number; colonyShip: boolean; colonize: boolean; taskForceId: number; stance: string; attackTarget: number;
  retreat: boolean; invadeSystemId: number; terraforming: boolean; terraformSystemId: number; terraformPlanetIndex: number;
  route: number[]; eta: number; routeBlocked: boolean; hasOrders: boolean;
  buildingOutpost?: boolean; outpostProgress?: number;
}

export interface ContactView {
  id: number; owner: number; ownerName: string; name: string; className: string; shipClass: string;
  x: number; y: number; hull: number; maxHull: number; shields: number; maxShields: number; attack: number;
  taskForce: boolean; atWar: boolean;
}

export interface TaskForceView {
  id: number; name: string; fleetIds: number[]; stance: string; targetX: number; targetY: number; attackTarget: number;
  speed: number; attack: number; hull: number; shields: number;
}

export interface RelationMemory {
  theirGrievances: number; ourGrievances: number; theirBetrayals: number;
  lastWarTurn?: number; lastPeaceTurn?: number; ourRefusedProposals?: number; theirRefusedProposals?: number; lastProposalFromThem?: number;
}
export interface RelationView {
  empire: number; name: string; contact: boolean; eliminated: boolean;
  atWar?: boolean; tradeTreaty?: boolean; trust?: number; warStartTurn?: number;
  personality?: string; allied?: boolean; sensorSharing?: boolean; treatyForbidsAttack?: boolean; memory?: RelationMemory;
}

export interface PendingCommand {
  id: number; type: string; summary: string; systemId?: number; fleetId?: number; taskForceId?: number;
  targetFleetId?: number; targetSystemId?: number; targetEmpire?: number; targetX?: number; targetY?: number;
  building?: string; referenceId?: number; proposalId?: number; proposalAccept?: boolean; memberFleetIds?: number[]; stance?: string;
}

/** Minor-race roster profile (MinorRaces.h). */
export interface MinorProfile {
  raceId: number; raceName: string; homeworld: string; temperament: string; bonus: string; bonusKind: string; bonusValue: number;
  description: string; signatureShip?: string;
}
export interface MemberBonuses {
  members: number; research: number; industry: number; food: number; credits: number; morale: number; groundDefense: number;
  minors: { systemId: number; name: string; bonus: string }[];
}

export interface Alert { kind: string; severity: Severity; text: string; systemId?: number; fleetId?: number; empire?: number; eventId?: number }

// ---- Schema-13 gameplay layer ----------------------------------------------------------------
export interface RulesView {
  preset: 'classic' | 'remastered'; presetName: string;
  stations: boolean; galaxyFeatures: boolean; curatedEvents: boolean; alternativeVictories: boolean;
}
export interface StationStatsView {
  maxHull: number; maxShields: number; beam: number; torpedo: number;
  supplyRadius: number; sensorRadius: number; upkeep: number; repairPercent: number;
}
export interface StationView extends StationStatsView {
  id: number; owner: number; ownerName: string; mine: boolean; name: string; type: 'Outpost' | 'Starbase'; typeName: string;
  x: number; y: number; hull: number; shields: number; builtTurn: number;
  upgrade?: { fundingSystemId: number; funding: string; work: number; cost: number };
}
export interface StationDetail {
  station: StationView; starbase: StationStatsView; upgradeCredits: number; upgradeIndustry: number;
  funding: { systemId: number; name: string; distance: number; industry: number; blocker: string }[];
  upgradeDrafted: boolean; ownShipsHere: number; contactsHere: number;
}
export type FeatureType = 'Nebula' | 'Wormhole' | 'BlackHole' | 'NeutronStar' | 'IonStorm' | 'SubspaceRift';
export interface FeatureView {
  id: number; type: FeatureType; name: string; x: number; y: number;
  pairId?: number; exit?: { x: number; y: number }; clusterId?: number;
  driftX?: number; driftY?: number; cells?: number[]; expiresTurn?: number;
  surveyed: boolean; effect: string; surveyReward: string; passable: boolean;
}
export interface EventChoice { index: number; label: string; effect: string; isDefault: boolean; blocker?: string }
export interface GalacticEventView {
  id: number; kind: string; name: string; empire: number; systemId?: number; system?: string; x?: number; y?: number;
  startTurn: number; endTurn: number; deadline: number; choice: number; choiceApplied: boolean; magnitude: number;
  description: string; drafted: number; draftIds: number[]; awaitingChoice: boolean; defaultChoice: number; choices: EventChoice[];
}
export interface TemplateView {
  id: number; name: string; isDefault: boolean;
  items: { product: string; referenceId: number; name: string; isShip: boolean }[];
}
export interface VictoryCondition { condition: 'Conquest' | 'Domination' | 'Scientific' | 'Diplomatic'; name: string; enabled: boolean; achieved: boolean; percent: number; explanation: string }
export interface VictoryView {
  conditions: VictoryCondition[]; alternative: boolean; threshold: number;
  megaprojectName: string; megaprojectCost: number; megaprojectCredits: number; megaprojectWorkPerTurn: number;
  megaproject?: { systemId: number; system: string; work: number; startTurn: number };
  rivalProjects: { owner: number; ownerName: string; work: number; startTurn: number }[];
}

export interface BattleSummary {
  index: number; turn: number; x: number; y: number; first: number; second: number; winner: number;
  rounds: number; lost: number; involved: boolean;
}

export interface GameView {
  meta: {
    turn: number; width: number; height: number; seed: number | string; player: number; playerName: string;
    ruleset: string; era: number; eraName: string; contentVersion: string; saveVersion: number;
    outcome: 'ongoing' | 'victory' | 'defeat'; winner: number; advancedRules: boolean; campaignExpansion: boolean;
    modernEngines: boolean; populationEnabled: boolean; scenario: string; version: number; empireCount: number;
    rules: RulesView;
  };
  empire: {
    id: number; name: string; credits: number; draftCredits: number; research: number; researchReserve: number;
    intelligence: number; researchOutput: number; industry: number; food: number; population: number;
    income: { colonies: number; trade: number; ships: number; spend: number; net: number };
    memberBonuses?: MemberBonuses;
    techs: Tech[];
  };
  fog: string;
  systems: SystemView[];
  colonies: ColonyBrief[];
  fleets: FleetView[];
  contacts: ContactView[];
  taskForces: TaskForceView[];
  empires: RelationView[];
  tradeRoutes: { owner: number; from: number; to: number }[];
  pending: PendingCommand[];
  battles: BattleSummary[];
  session: { active: boolean; battlePending: boolean; battleFinished: boolean; replays: number };
  alerts: Alert[];
  stations: StationView[];
  outpostTurns: number;
  stationUpgradeCost: { credits: number; industry: number };
  features: FeatureView[];
  events: GalacticEventView[];
  templates: TemplateView[];
  victory: VictoryView;
}

export interface ConstructionChoice {
  product: string; referenceId: number; name: string; cost: number; upkeep: number; blocker: string; isShip: boolean;
  turns: number; energyCost?: number;
  designClass?: string; designName?: string; hull?: number; shields?: number; attack?: number; range?: number;
  speed?: number; role?: number; maintenance?: number; description?: string; roster?: RosterDetail | null;
}
export interface QueueItem {
  index: number; product: string; referenceId: number; name: string; isShip: boolean; work: number; cost: number;
  pinned: boolean; eta: number;
}
export interface ColonyDetail {
  colony: ColonyBrief;
  committedEconomy: Economy;
  committedWorkers: { food: number; industry: number; research: number };
  capacityLimits: { food: number; industry: number; research: number };
  baseCapacity: number; maxCapacity: number;
  planets: Planet[];
  choices: ConstructionChoice[];
  queue: QueueItem[];
  queueBlocker: string; overflow: number; rushCost: number; buyBlocker: string; shipyardToggleBlocker: string;
  scrap: { building: string; name: string; count: number; energyCost: number; upkeep: number; blocker: string }[];
  megaproject: {
    enabled: boolean; name: string; here: boolean; systemId?: number; system?: string; work?: number;
    cost: number; credits: number; workPerTurn: number; startBlocker: string; cancelBlocker: string;
  };
  templates: { id: number; name: string; blocker: string }[];
  queueExplanation: string;
  fundsStation?: { stationId: number; name: string; x: number; y: number; work: number; cost: number };
}
export interface WorkforcePreview {
  error: string; economy?: Economy; growth?: Growth;
  workers?: { food: number; industry: number; research: number }; cancelIds?: number[];
}

export interface ResearchLevel {
  level: number; name: string; cost: number; description?: string;
  ships: { id: number; name: string; cost: number; requires: number[]; othersMet: boolean }[];
  structures: { id: number; name: string; cost: number; energy: number }[];
}
export interface ResearchField {
  field: number; name: string; level: number; progress: number; allocation: number; committedAllocation: number;
  current: string; levels: ResearchLevel[];
}
export interface ResearchView {
  fields: ResearchField[]; output: number; reserve: number;
  blockers: { product: string; name: string; blocker: string }[];
}

export interface Forecast {
  error: string; credits?: number; creditsAfter?: number; research?: number; fleetUpkeep?: number; tradeIncome?: number;
  fields?: { field: number; gained: number; levelsGained: number }[];
  colonies?: { systemId: number; completed: string; population: number }[];
}

export interface Participant { id: number; minor: boolean; name: string }
export interface DiplomaticEventView { turn: number; type: string; typeKey: number; value: number; text: string; a: Participant; b: Participant }
export interface ProposalView {
  id: number; incoming: boolean; proposer: Participant; target: Participant; type: string; typeName: string;
  duration: string; tone: string; creditOffer: number; creditDemand: number; submittedTurn: number; dueTurn: number;
  status: string; responseDrafted: boolean; kind: 'treaty' | 'gift' | 'tribute' | 'war'; reason: string;
}
export interface CallToArmsView {
  defender: number; defenderName: string; aggressor: number; aggressorName: string; issuedTurn: number; dueTurn: number;
  honored: boolean; honorDrafted: boolean; boundByTreaty: boolean; honorBlocker: string;
}
export interface DiplomacyView {
  modern: boolean; advancedRules: boolean; turn: number;
  empires: RelationView[];
  minors: {
    systemId: number; name: string; system: string; memberOf: number; attitude: number; friendship: boolean; offerInFlight: boolean;
    race?: MinorProfile; managed?: boolean; techLevel?: number; population?: number; progress?: number; progressNeeded?: number;
  }[];
  eligibility: { target: number; minor: boolean; treaties: { type: string; name: string; ok: boolean; reason: string }[] }[];
  proposals: ProposalView[];
  treaties: { id: number; counterparty: Participant; type: string; typeName: string; duration: string; turnsRemaining: number; startTurn: number }[];
  events: DiplomaticEventView[];
  news: DiplomaticEventView[];
  standing: { betrayals: number; lastBetrayalTurn: number };
  callsToArms: CallToArmsView[];
  treatyHistory: { id: number; counterparty: Participant; type: string; typeName: string; startTurn: number; status: string; statusKey: number }[];
  victory?: { bloc: number; galaxy: number; achieved: boolean; allies: number[]; memberMinors: string[] };
  majorOffers: { id: number; proposer: number; target: number; dueTurn: number; type: string }[];
  minorOffers: { id: number; systemId: number; system: string; dueTurn: number; type: string }[];
  dossiers: { turn: number; target: number; knownColonies: number; estimatedCredits: number }[];
}

export interface IntelBudget { security: number; rivals: { id: number; espionage: number; sabotage: number }[] }
export interface IntelView {
  modern: boolean; reserve: number; output: number; securityStockpile: number; defenseStrengthMilli?: number;
  budget: IntelBudget; committedBudget: IntelBudget;
  rivals: {
    id: number; name: string; contact: boolean; atWar: boolean; eliminated: boolean; focus?: string; focusName?: string;
    espionageStockpile?: number; sabotageStockpile?: number; departmentWeights?: number[]; attackStrengthMilli?: number;
    visibleColonies: { systemId: number; name: string }[];
  }[];
  reports: { turn: number; target: number; targetName: string; success: boolean; knownColonies: number; estimatedCredits: number; systemId: number; system: string }[];
  operations: {
    turn: number; mine: boolean; attacker: number; attackerName: string; defender: number; defenderName: string;
    area: string; department: string; outcome: string; success: boolean; creditsStolen: number; researchStolen: number;
    sabotageSeverity: number; detected: boolean; details: string;
  }[];
}

export interface Coord { x: number; y: number }
export interface TacticalShip {
  id: number; owner: number; ownerName: string; mine: boolean; name: string; shipClass: string; referenceShipId: number;
  hidden: boolean; hull: number; maxHull: number; initialHull: number; shields: number; maxShields: number; initialShields: number;
  beam: number; torpedo: number; speed: number; position: Coord | null; alive: boolean; active: boolean; retreated: boolean;
  canRetreat: boolean; canCloak: boolean; cloaked: boolean; groupId: number; group?: string; disengaging: boolean;
  /** Outposts and starbases fight as immobile combatants (ids >= 2000000). */
  immobile?: boolean; station?: boolean; stationType?: 'Outpost' | 'Starbase'; stationName?: string;
  maneuver?: string; target?: number; ordered?: boolean;
  /** Expanded-roster classes only: typed weapon mounts and the local asset key. */
  weapons?: RosterWeapon[]; assetKey?: string;
  /** What the ship executes next round and who decided it (player order, standing order, tactical AI). */
  next?: { maneuver: string; target: number; source: 'order' | 'standing' | 'ai' };
}
export interface RoundShip {
  id: number; owner: number; hullBefore: number; hullAfter: number; shieldBefore: number; shieldAfter: number;
  beam: number; torpedo: number; taken: number; from: Coord; via: Coord; to: Coord; maneuver: string; target: number;
  destroyed: boolean; retreated: boolean; decloaked: boolean; hailed: boolean; ai: boolean; disengaging: boolean; yaw: number;
}
export interface Salvo { from: number; to: number; beam: number; torpedo: number; absorbed: boolean; ram: boolean; ambush: boolean }
export interface TacticalRound {
  index: number; ended: boolean; winner: number; truce: boolean; ships: RoundShip[];
  salvos: Salvo[];
  lost: number[]; retreatedIds: number[];
}
export interface HailPreview { atWar: boolean; trust: number; ourStrength: number; theirStrength: number; accepts: boolean }
export interface Encounter {
  arenaWidth: number; arenaHeight: number; round: number; maxRounds: number; finished: boolean; winner: number;
  sectorX: number; sectorY: number; turn: number; controller: number; location: string; rules: number; foe: number;
  hail: HailPreview;
  ships: TacticalShip[]; rounds: TacticalRound[];
}

export interface BriefingEvent {
  category: string; severity: Severity; text: string; systemId?: number; fleetId?: number; x?: number; y?: number; battle?: number;
  /** Diplomacy: proposal | tribute | gift | callToArms | betrayal | callRefused | callHonored | eliminated | secession | war | peace | news … */
  kind?: string; proposalId?: number; dueTurn?: number; stationId?: number; eventId?: number;
}
export interface Briefing {
  turn: number; creditsBefore: number; creditsAfter: number; researchGained: number; fleetUpkeep: number; tradeIncome: number;
  ordersApplied: number; stateHash: string | number; outcome: 'ongoing' | 'victory' | 'defeat';
  colonies: { systemId: number; name: string; completed: string; economy: Economy }[];
  events: BriefingEvent[];
  replays: number;
}
export interface TurnResult { status: 'battle' | 'complete'; turn: number; briefing?: Briefing | null }

export interface BattleReport {
  turn: number; x: number; y: number; first: number; second: number; winner: number;
  participants: { id: number; owner: number; name: string; hull: number; shields: number }[];
  rounds: { index: number; damage: { id: number; hullBefore: number; hullAfter: number; shieldBefore: number; shieldAfter: number }[]; lost: number[]; retreated: number[] }[];
  lost: number[];
}

export interface SaveBlob { kind: 'game' | 'session'; data: string; turn: number; player: number; fingerprint: string | number; scenario: string }

export interface Encyclopedia {
  races: { id: number; name: string }[];
  technologies: { id: number; name: string; field: number; level: number; cost: number; description: string }[];
  ships: {
    id: number; race: number; industryCost: number; troops: number; designClass: string; designName: string; hull: number;
    shields: number; attack: number; range: number; speed: number; role: number; maintenance: number; description: string; requires: number[];
    /** Expanded roster detail (ids >= 1000); null for legacy catalogue records. */
    roster: RosterDetail | null;
  }[];
  buildings: {
    id: number; name: string; field: number; level: number; industryCost: number; energyCost: number; populationCost: number;
    moraleBonus: number; excludedEmpireMask: number; bonusType: number; description: string; bonusValues: number[];
  }[];
  minorRaces: (MinorProfile & { techLevel: number; population: number; affinity: number[] })[];
}

/** A command as the bridge accepts it (see commandFromJson in WebBridge.cpp). */
export type Command = { type: string } & Record<string, unknown>;
