// Combat presentation layer: what the tactical arena animates for one resolved round.
//
// The arena never reads salvos directly any more; it asks a CombatPresentationSource for a
// CombatRoundPresentation (typed weapon shots with timing, impacts, losses, withdrawals and ship
// motion). Today the only source is `roundResultSource`, which *derives* that from the round result
// the bridge already returns (Salvo beam/torpedo totals + the shooter's typed roster mounts). When the
// core grows a combat event stream, a second source can map those events 1:1 onto the same shapes
// and the renderer and audio cue layer stay unchanged.
import type { Coord, Encounter, RoundShip, Salvo, TacticalRound, TacticalShip } from '../core/types';
import { legacyWeapons, weaponFamily, type RosterWeapon, type WeaponFamily, type WeaponType } from '../core/roster';

/** One visible weapon discharge: a beam, one bolt of a pulse volley, or one torpedo. */
export interface CombatShot {
  /** Stable within a round (used to fire each audio cue once). */
  id: string;
  from: number;
  to: number;
  weapon: WeaponType | 'Ram';
  /** Audio / asset key: `weapon.<WeaponType>` (roster keys), or `sfx.explosion.small` for a ram. */
  key: string;
  family: WeaponFamily | 'ram';
  /** Core colour of the effect (CSS hex). */
  color: string;
  damage: number;
  /** Normalised round time (0..1) the shot leaves the shooter and reaches the target. */
  start: number;
  impact: number;
  /** Impact lands on raised shields (bubble flash) rather than bare hull (sparks). */
  shieldHit: boolean;
  ambush: boolean;
  /** Lateral offset of the muzzle (-1..1) so parallel bolts/torpedoes fan out. */
  spread: number;
}

export interface CombatMotion {
  id: number;
  from: Coord; via: Coord; to: Coord;
  /** Heading in degrees before and after the round. */
  yawFrom: number; yawTo: number;
}

export interface CombatRoundPresentation {
  round: number;
  /** Movement phase occupies [0, moveEnd] of the round timeline. */
  moveEnd: number;
  motions: CombatMotion[];
  shots: CombatShot[];
  destroyed: { id: number; at: number }[];
  withdrawn: { id: number; at: number }[];
  decloaked: { id: number; at: number }[];
  truce: boolean;
}

/**
 * Adapter between a combat record and the arena/audio presentation. Implementations must be pure:
 * the same encounter and round index always yield the same presentation (replays depend on it).
 */
export interface CombatPresentationSource {
  /** 'derived' = reconstructed from round results; 'events' = authored by a core event stream. */
  readonly kind: 'derived' | 'events';
  round(encounter: Encounter, roundIndex: number): CombatRoundPresentation | null;
}

const OWNER_FACTION: Record<number, string> = { 1: 'Federation', 2: 'Klingon', 3: 'Romulan', 4: 'Cardassian', 5: 'Ferengi' };

/** Beam colour per faction (phaser orange, Klingon/Romulan disruptor greens, Cardassian spiral-wave yellow…). */
const BEAM_BY_FACTION: Record<string, string> = {
  Federation: '#ff9a3c', Klingon: '#57ff4a', Romulan: '#3cffb4', Cardassian: '#ffd84a', Ferengi: '#ffb84a',
  Dominion: '#c77dff', Borg: '#6aff7a', Breen: '#9ad7ff', Tholian: '#ff7a4a', Gorn: '#ffe06a', Orion: '#7aff9a',
  Hirogen: '#ff8a5a', Kazon: '#ffae5a', Vidiian: '#9aff6a', Suliban: '#ffb0e0', Xindi: '#ffd06a', Species8472: '#f0ff7a',
  Krenim: '#ff6a6a', Malon: '#ffc05a', SonA: '#ff9ad0', Maquis: '#ff9a3c', Reman: '#3cffb4', Devore: '#9ab8ff',
  Andorian: '#6ad0ff', Vulcan: '#ff9a3c', Bajoran: '#ffcc6a', Nausicaan: '#ff8a4a', Talarian: '#ffa06a',
};
/** Projectile colours: photon red-orange, quantum blue, plasma green. */
const PROJECTILE: Partial<Record<WeaponType, string>> = {
  PhotonTorpedo: '#ff5a2a', QuantumTorpedo: '#4aa8ff', PlasmaTorpedo: '#4aff7a', PhotonicTorpedo: '#ffb45a',
  SpatialTorpedo: '#ffe07a', TricobaltDevice: '#e8f4ff', Torpedo: '#ff7a4a',
};
const SPECIAL: Partial<Record<WeaponType, string>> = {
  EnergyDampener: '#9ab8ff', TholianWeb: '#ff9a4a', BioEnergy: '#f0ff7a', TemporalWeapon: '#ff4a8a', CuttingBeam: '#8aff6a',
  PolaronBeam: '#c77dff', PlasmaCannon: '#ffcc4a',
};

export function shipFaction(s: Pick<TacticalShip, 'owner'> & { faction?: string }): string {
  return s.faction ?? OWNER_FACTION[s.owner] ?? 'Federation';
}

export function weaponColor(type: WeaponType, faction: string): string {
  return PROJECTILE[type] ?? SPECIAL[type] ?? BEAM_BY_FACTION[faction] ?? '#ffffff';
}

/** Typed mounts a ship fires with: its roster mounts, or the classic faction fit for legacy hulls. */
export function mountsOf(s: TacticalShip): RosterWeapon[] {
  return s.weapons && s.weapons.length ? s.weapons : legacyWeapons(s.owner);
}

function factionOf(s: TacticalShip): string {
  const key = s.assetKey?.split('.')[1];
  const byKey: Record<string, string> = { fed: 'Federation', kli: 'Klingon', rom: 'Romulan', car: 'Cardassian', fer: 'Ferengi' };
  return (key && byKey[key]) ?? OWNER_FACTION[s.owner] ?? 'Federation';
}

/** Splits `total` across mounts by weight (count), integers summing exactly to total. */
function split(total: number, mounts: RosterWeapon[]): number[] {
  const w = mounts.reduce((a, m) => a + m.count, 0) || 1;
  const out = mounts.map((m) => Math.floor((total * m.count) / w));
  let rest = total - out.reduce((a, b) => a + b, 0);
  for (let i = 0; rest > 0; i = (i + 1) % out.length, rest--) out[i]! += 1;
  return out;
}

const MOVE_END = 0.38;
function salvoWindow(i: number, n: number): [number, number] {
  const count = Math.max(1, n);
  const s0 = 0.4 + (0.42 * i) / count;
  return [s0, Math.min(0.97, s0 + Math.max(0.16, 0.42 / count + 0.08))];
}

function shotsForSalvo(e: Encounter, r: TacticalRound, sv: Salvo, i: number, n: number): CombatShot[] {
  const [s0, s1] = salvoWindow(i, n);
  const shooter = e.ships.find((s) => s.id === sv.from);
  const target: RoundShip | undefined = r.ships.find((x) => x.id === sv.to);
  const shieldHit = !sv.absorbed ? (target?.shieldBefore ?? 0) > 0 : true;
  const base = `${r.index}:${i}`;
  if (sv.ram) {
    const rammer = r.ships.some((x) => x.id === sv.from && x.maneuver === 'Ram');
    if (!rammer) return [];
    return [{ id: `${base}:ram`, from: sv.from, to: sv.to, weapon: 'Ram', key: 'sfx.explosion.small', family: 'ram', color: '#ffb36b',
      damage: sv.beam, start: s0, impact: s0 + (s1 - s0) * 0.55, shieldHit: false, ambush: false, spread: 0 }];
  }
  if (!shooter) return [];
  const faction = factionOf(shooter);
  const mounts = mountsOf(shooter);
  const shots: CombatShot[] = [];
  const span = s1 - s0;
  for (const [total, torpedo] of [[sv.beam, false], [sv.torpedo, true]] as const) {
    if (total <= 0) continue;
    // A ship with no mount of that kind still shows the damage it dealt, as its faction's default.
    const own = mounts.filter((m) => m.torpedo === torpedo);
    const list = own.length ? own : legacyWeapons(shooter.owner).filter((m) => m.torpedo === torpedo);
    const parts = split(total, list);
    list.forEach((m, mi) => {
      const dmg = parts[mi]!;
      if (dmg <= 0) return;
      const family = weaponFamily(m.type);
      const color = weaponColor(m.type, faction);
      const key = m.key || `weapon.${m.type}`;
      const offset = (mi / Math.max(1, list.length)) * span * 0.18;
      if (family === 'torpedo') {
        const n2 = Math.min(3, Math.max(1, m.count));
        for (let k = 0; k < n2; k++) {
          const start = s0 + offset + k * span * 0.07;
          shots.push({ id: `${base}:${mi}:t${k}`, from: sv.from, to: sv.to, weapon: m.type, key, family, color,
            damage: Math.round(dmg / n2), start, impact: Math.min(0.99, start + span * 0.6), shieldHit, ambush: sv.ambush,
            spread: n2 === 1 ? 0 : (k / (n2 - 1)) * 2 - 1 });
        }
      } else if (family === 'pulse') {
        const n2 = Math.min(4, Math.max(2, m.count));
        for (let k = 0; k < n2; k++) {
          const start = s0 + offset + k * span * 0.08;
          shots.push({ id: `${base}:${mi}:p${k}`, from: sv.from, to: sv.to, weapon: m.type, key, family, color,
            damage: Math.round(dmg / n2), start, impact: Math.min(0.99, start + span * 0.22), shieldHit, ambush: sv.ambush,
            spread: (k % 2 ? 1 : -1) * 0.5 });
        }
      } else {
        // Continuous beam (phaser, disruptor, polaron, cutting beam, special effects).
        const start = s0 + offset;
        shots.push({ id: `${base}:${mi}:b`, from: sv.from, to: sv.to, weapon: m.type, key, family, color, damage: dmg,
          start, impact: Math.min(0.99, start + span * 0.12), shieldHit, ambush: sv.ambush, spread: 0 });
      }
    });
  }
  return shots;
}

function lostIn(e: Encounter, id: number, upTo: number): boolean {
  for (let i = 0; i <= upTo; i++) if (e.rounds[i]!.lost.includes(id)) return true;
  return false;
}

/** Derives presentation from the round results the bridge returns today (salvo totals + roster mounts). */
export const roundResultSource: CombatPresentationSource = {
  kind: 'derived',
  round(e, roundIndex) {
    const r = e.rounds[roundIndex];
    if (!r) return null;
    const salvos = r.salvos.filter((sv) => sv.beam + sv.torpedo > 0 || sv.ram);
    const shots = salvos.flatMap((sv, i) => shotsForSalvo(e, r, sv, i, salvos.length));
    const prev = roundIndex > 0 ? e.rounds[roundIndex - 1] : undefined;
    const motions: CombatMotion[] = r.ships.map((st) => {
      const before = prev?.ships.find((x) => x.id === st.id);
      const ship = e.ships.find((s) => s.id === st.id);
      return { id: st.id, from: st.from, via: st.via, to: st.to, yawFrom: before?.yaw ?? (ship?.mine ? 0 : 180), yawTo: st.yaw };
    });
    const lastImpact = (id: number) => Math.max(0.8, ...shots.filter((s) => s.to === id).map((s) => s.impact));
    return {
      round: roundIndex,
      moveEnd: MOVE_END,
      motions,
      shots,
      destroyed: r.lost.filter((id) => !(roundIndex > 0 && lostIn(e, id, roundIndex - 1))).map((id) => ({ id, at: Math.min(0.9, lastImpact(id)) })),
      withdrawn: r.retreatedIds.map((id) => ({ id, at: 0.2 })),
      decloaked: r.ships.filter((s) => s.decloaked).map((s) => ({ id: s.id, at: 0.36 })),
      truce: r.truce,
    };
  },
};

/** The active source. Swap for an event-stream source once the core provides one. */
let active: CombatPresentationSource = roundResultSource;
export function presentationSource(): CombatPresentationSource { return active; }
export function setPresentationSource(source: CombatPresentationSource): void { active = source; }
