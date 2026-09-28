// Expanded ship roster (docs/SHIP-ROSTER.md) as the bridge exposes it, plus presentation helpers
// shared by the build options, the Database, the fleet views and the tactical arena.

export type WeaponType =
  | 'PhaseCannon' | 'PhaserBank' | 'PhaserArray' | 'PulsePhaser' | 'Disruptor' | 'DisruptorCannon' | 'PlasmaCannon'
  | 'PolaronBeam' | 'CuttingBeam' | 'EnergyDampener' | 'TholianWeb' | 'BioEnergy' | 'TemporalWeapon' | 'SpatialTorpedo'
  | 'PhotonicTorpedo' | 'PhotonTorpedo' | 'QuantumTorpedo' | 'PlasmaTorpedo' | 'TricobaltDevice' | 'Torpedo';
export type FiringArc = 'Forward' | 'ForwardWide' | 'Broadside' | 'Aft' | 'All';

export interface RosterWeapon {
  type: WeaponType;
  /** Asset / audio key, `weapon.<WeaponType>`. */
  key: string;
  count: number;
  arc: FiringArc;
  torpedo: boolean;
}

export interface RosterDetail {
  assetKey: string;
  faction: string;
  raceId: number;
  minorRaceId: number;
  hullRole: string;
  era: string;
  tier: number;
  sourceKind: 'Canon' | 'ModDerived' | 'Kelvin';
  source: string;
  buildable: boolean;
  buildTurns: number;
  torpedoSharePercent: number;
  sensors: number;
  cloak: boolean;
  weapons: RosterWeapon[];
}

const SPLIT = (s: string) => s.replace(/([a-z])([A-Z0-9])/g, '$1 $2');

export const ERA_ORDER = ['Enterprise', 'Original', 'Movie', 'NextGeneration', 'DominionWar', 'Nemesis', 'Kelvin', 'Future'];
const ERA_LABEL: Record<string, string> = {
  Enterprise: 'Enterprise era', Original: 'Original series', Movie: 'Movie era', NextGeneration: 'Next Generation',
  DominionWar: 'Dominion War', Nemesis: 'Nemesis era', Kelvin: 'Kelvin timeline', Future: 'Future',
};
const SOURCE_LABEL: Record<string, string> = { Canon: 'Canon', ModDerived: 'Mod-derived', Kelvin: 'Kelvin' };
const FACTION_LABEL: Record<string, string> = { Species8472: 'Species 8472', SonA: "Son'a" };
const WEAPON_LABEL: Partial<Record<WeaponType, string>> = { TricobaltDevice: 'Tricobalt device', Torpedo: 'Torpedo launcher' };
const ARC_LABEL: Record<FiringArc, string> = { Forward: 'fore', ForwardWide: 'wide fore', Broadside: 'broadside', Aft: 'aft', All: '360°' };

export const eraLabel = (era: string) => ERA_LABEL[era] ?? SPLIT(era);
export const sourceLabel = (kind: string) => SOURCE_LABEL[kind] ?? kind;
export const roleLabel = (role: string) => SPLIT(role);
export const factionLabel = (f: string) => FACTION_LABEL[f] ?? f;
export const weaponLabel = (t: WeaponType) => WEAPON_LABEL[t] ?? SPLIT(t);
export const arcLabel = (a: FiringArc) => ARC_LABEL[a] ?? a;

export function weaponLine(w: RosterWeapon): string {
  return `${w.count}× ${weaponLabel(w.type)} (${arcLabel(w.arc)})`;
}
export function weaponSummary(ws: RosterWeapon[] | undefined): string {
  return ws && ws.length ? ws.map(weaponLine).join(', ') : '';
}

/** Weapon family used by VFX and audio. */
export type WeaponFamily = 'beam' | 'pulse' | 'torpedo' | 'special';
export function weaponFamily(t: WeaponType): WeaponFamily {
  switch (t) {
    case 'PulsePhaser': case 'DisruptorCannon': case 'PhaseCannon': case 'PlasmaCannon': return 'pulse';
    case 'SpatialTorpedo': case 'PhotonicTorpedo': case 'PhotonTorpedo': case 'QuantumTorpedo': case 'PlasmaTorpedo':
    case 'TricobaltDevice': case 'Torpedo': return 'torpedo';
    case 'EnergyDampener': case 'TholianWeb': case 'BioEnergy': case 'TemporalWeapon': return 'special';
    default: return 'beam';
  }
}

/** Mounts assumed for legacy catalogue ships (ids < 1000), which have no typed mounts: the classic faction fit. */
const LEGACY_FIT: Record<number, [WeaponType, WeaponType]> = {
  1: ['PhaserArray', 'PhotonTorpedo'],       // Federation
  2: ['Disruptor', 'PhotonTorpedo'],         // Klingon
  3: ['Disruptor', 'PlasmaTorpedo'],         // Romulan
  4: ['PhaserBank', 'PhotonTorpedo'],        // Cardassian (spiral-wave disruptor presented as a beam)
  5: ['PlasmaCannon', 'PhotonTorpedo'],      // Ferengi
};
export function legacyWeapons(owner: number): RosterWeapon[] {
  const [beam, torp] = LEGACY_FIT[owner] ?? ['PhaserBank', 'Torpedo'];
  return [
    { type: beam, key: `weapon.${beam}`, count: 4, arc: 'All', torpedo: false },
    { type: torp, key: `weapon.${torp}`, count: 2, arc: 'Forward', torpedo: true },
  ];
}
