// WebAudio cue layer (docs/AUDIO-PLAN.md). Every cue is a short sound synthesised at runtime, so the
// repository and the build contain no recorded audio. Keys match the ship roster weapon keys
// (`weapon.<WeaponType>`, docs/SHIP-ROSTER.md) and the AUDIO-PLAN `sfx.*` categories.
//
// Players may load their own local files for any key (Settings -> Sound). Those are decoded in memory
// for this browser session only: never uploaded, stored or bundled.
import type { WeaponType } from '../core/roster';

export const WEAPON_TYPES: WeaponType[] = [
  'PhaseCannon', 'PhaserBank', 'PhaserArray', 'PulsePhaser', 'Disruptor', 'DisruptorCannon', 'PlasmaCannon', 'PolaronBeam',
  'CuttingBeam', 'EnergyDampener', 'TholianWeb', 'BioEnergy', 'TemporalWeapon', 'SpatialTorpedo', 'PhotonicTorpedo',
  'PhotonTorpedo', 'QuantumTorpedo', 'PlasmaTorpedo', 'TricobaltDevice', 'Torpedo',
];
export const SFX_KEYS = [
  'sfx.ui.beep', 'sfx.alert.red', 'sfx.shield.hit', 'sfx.explosion.small', 'sfx.explosion.large',
  'sfx.cloak', 'sfx.decloak', 'sfx.warp.out', 'sfx.warp.in',
] as const;
export type SfxKey = (typeof SFX_KEYS)[number];
export type WeaponKey = `weapon.${WeaponType}`;
export type CueKey = WeaponKey | SfxKey;
export const CUE_KEYS: CueKey[] = [...WEAPON_TYPES.map((t) => `weapon.${t}` as WeaponKey), ...SFX_KEYS];

/** AUDIO-PLAN section 3 category each weapon key falls back to (for manifests keyed by `wpn.*`). */
export const PLAN_CATEGORY: Record<WeaponKey, string> = {
  'weapon.PhaseCannon': 'wpn.phaser.fed', 'weapon.PhaserBank': 'wpn.phaser.fed', 'weapon.PhaserArray': 'wpn.phaser.fed',
  'weapon.PulsePhaser': 'wpn.pulse.defiant', 'weapon.Disruptor': 'wpn.disruptor.kli', 'weapon.DisruptorCannon': 'wpn.pulse.defiant',
  'weapon.PlasmaCannon': 'wpn.plasma.fer', 'weapon.PolaronBeam': 'wpn.disruptor.rom', 'weapon.CuttingBeam': 'wpn.cutting.borg',
  'weapon.EnergyDampener': 'wpn.plasma.fer', 'weapon.TholianWeb': 'wpn.cutting.borg', 'weapon.BioEnergy': 'wpn.cutting.borg',
  'weapon.TemporalWeapon': 'wpn.cutting.borg', 'weapon.SpatialTorpedo': 'wpn.torpedo.photon', 'weapon.PhotonicTorpedo': 'wpn.torpedo.photon',
  'weapon.PhotonTorpedo': 'wpn.torpedo.photon', 'weapon.QuantumTorpedo': 'wpn.torpedo.quantum', 'weapon.PlasmaTorpedo': 'wpn.torpedo.plasma',
  'weapon.TricobaltDevice': 'wpn.torpedo.quantum', 'weapon.Torpedo': 'wpn.torpedo.photon',
};

type Recipe =
  | { kind: 'beam'; f: number; f2: number; d: number; wave: OscillatorType }
  | { kind: 'pulse'; f: number; d: number; wave: OscillatorType }
  | { kind: 'torpedo'; f: number; d: number }
  | { kind: 'noise'; d: number; lp: number; g?: number }
  | { kind: 'tones'; f: number[]; d: number; wave: OscillatorType }
  | { kind: 'sweep'; f: number; f2: number; d: number; wave: OscillatorType };

const RECIPES: Record<CueKey, Recipe> = {
  'weapon.PhaseCannon': { kind: 'pulse', f: 520, d: 0.1, wave: 'square' },
  'weapon.PhaserBank': { kind: 'beam', f: 880, f2: 760, d: 0.45, wave: 'sawtooth' },
  'weapon.PhaserArray': { kind: 'beam', f: 980, f2: 900, d: 0.55, wave: 'sawtooth' },
  'weapon.PulsePhaser': { kind: 'pulse', f: 700, d: 0.09, wave: 'square' },
  'weapon.Disruptor': { kind: 'beam', f: 420, f2: 300, d: 0.4, wave: 'square' },
  'weapon.DisruptorCannon': { kind: 'pulse', f: 360, d: 0.11, wave: 'square' },
  'weapon.PlasmaCannon': { kind: 'pulse', f: 260, d: 0.14, wave: 'triangle' },
  'weapon.PolaronBeam': { kind: 'beam', f: 620, f2: 820, d: 0.45, wave: 'sawtooth' },
  'weapon.CuttingBeam': { kind: 'beam', f: 180, f2: 160, d: 0.6, wave: 'sawtooth' },
  'weapon.EnergyDampener': { kind: 'sweep', f: 900, f2: 90, d: 0.7, wave: 'sine' },
  'weapon.TholianWeb': { kind: 'sweep', f: 300, f2: 1200, d: 0.6, wave: 'triangle' },
  'weapon.BioEnergy': { kind: 'beam', f: 140, f2: 220, d: 0.7, wave: 'sawtooth' },
  'weapon.TemporalWeapon': { kind: 'sweep', f: 1600, f2: 60, d: 0.8, wave: 'sine' },
  'weapon.SpatialTorpedo': { kind: 'torpedo', f: 500, d: 0.3 },
  'weapon.PhotonicTorpedo': { kind: 'torpedo', f: 460, d: 0.32 },
  'weapon.PhotonTorpedo': { kind: 'torpedo', f: 380, d: 0.38 },
  'weapon.QuantumTorpedo': { kind: 'torpedo', f: 620, d: 0.42 },
  'weapon.PlasmaTorpedo': { kind: 'torpedo', f: 240, d: 0.45 },
  'weapon.TricobaltDevice': { kind: 'torpedo', f: 160, d: 0.6 },
  'weapon.Torpedo': { kind: 'torpedo', f: 400, d: 0.35 },
  'sfx.ui.beep': { kind: 'tones', f: [1200], d: 0.06, wave: 'sine' },
  'sfx.alert.red': { kind: 'tones', f: [660, 520, 660], d: 0.18, wave: 'square' },
  'sfx.shield.hit': { kind: 'sweep', f: 1400, f2: 500, d: 0.18, wave: 'sine' },
  'sfx.explosion.small': { kind: 'noise', d: 0.35, lp: 1800 },
  'sfx.explosion.large': { kind: 'noise', d: 1.2, lp: 700, g: 1.4 },
  'sfx.cloak': { kind: 'sweep', f: 800, f2: 120, d: 0.6, wave: 'sine' },
  'sfx.decloak': { kind: 'sweep', f: 120, f2: 800, d: 0.6, wave: 'sine' },
  'sfx.warp.out': { kind: 'sweep', f: 200, f2: 2400, d: 0.5, wave: 'triangle' },
  'sfx.warp.in': { kind: 'sweep', f: 2400, f2: 200, d: 0.5, wave: 'triangle' },
};

export interface CueOptions { pan?: number; gain?: number }

let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let noise: AudioBuffer | null = null;
const userBuffers = new Map<CueKey, AudioBuffer>();
const lastPlayed = new Map<string, number>();
let voices = 0;
const MAX_VOICES = 24;

/** Called by the settings layer: enabled flag and 0..1 master volume. */
let enabled = false;
let volume = 0.7;
export function configureCues(on: boolean, vol: number): void {
  enabled = on;
  volume = Math.max(0, Math.min(1, vol));
  if (master) master.gain.value = volume;
}

/** Diagnostics for tests: how many cues were requested and actually voiced, by key. */
export const cueStats = { requested: 0, played: 0, byKey: {} as Record<string, number> };

function context(): AudioContext | null {
  if (ctx) return ctx;
  try {
    const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!AC) return null;
    ctx = new AC();
    master = ctx.createGain();
    master.gain.value = volume;
    master.connect(ctx.destination);
  } catch { ctx = null; }
  return ctx;
}

function noiseBuffer(c: AudioContext): AudioBuffer {
  if (noise) return noise;
  noise = c.createBuffer(1, Math.floor(c.sampleRate * 1.5), c.sampleRate);
  const d = noise.getChannelData(0);
  let seed = 1;
  for (let i = 0; i < d.length; i++) { seed = (seed * 16807) % 2147483647; d[i] = (seed / 2147483647) * 2 - 1; }
  return noise;
}

function envelope(c: AudioContext, t0: number, d: number, peak: number): GainNode {
  const g = c.createGain();
  g.gain.setValueAtTime(0.0001, t0);
  g.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak), t0 + Math.min(0.02, d / 4));
  g.gain.exponentialRampToValueAtTime(0.0001, t0 + d);
  return g;
}

function synth(c: AudioContext, out: AudioNode, r: Recipe, t0: number, gain: number): number {
  const osc = (wave: OscillatorType, f: number) => { const o = c.createOscillator(); o.type = wave; o.frequency.setValueAtTime(f, t0); return o; };
  switch (r.kind) {
    case 'beam': case 'sweep': {
      const o = osc(r.wave, r.f);
      o.frequency.exponentialRampToValueAtTime(r.f2, t0 + r.d);
      const g = envelope(c, t0, r.d, 0.06 * gain);
      if (r.kind === 'beam') {   // shimmer: a detuned second voice
        const o2 = osc('sine', r.f * 1.51);
        o2.connect(g); o2.start(t0); o2.stop(t0 + r.d + 0.02);
      }
      o.connect(g).connect(out); o.start(t0); o.stop(t0 + r.d + 0.02);
      return r.d;
    }
    case 'pulse': {
      const o = osc(r.wave, r.f);
      o.frequency.exponentialRampToValueAtTime(r.f * 0.4, t0 + r.d);
      o.connect(envelope(c, t0, r.d, 0.07 * gain)).connect(out); o.start(t0); o.stop(t0 + r.d + 0.02);
      return r.d;
    }
    case 'torpedo': {
      const o = osc('sine', r.f * 2);
      o.frequency.exponentialRampToValueAtTime(r.f * 0.5, t0 + r.d);
      o.connect(envelope(c, t0, r.d, 0.09 * gain)).connect(out); o.start(t0); o.stop(t0 + r.d + 0.02);
      const n = c.createBufferSource(); n.buffer = noiseBuffer(c);
      const f = c.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = r.f * 3; f.Q.value = 2;
      n.connect(f).connect(envelope(c, t0, r.d * 0.6, 0.05 * gain)).connect(out); n.start(t0); n.stop(t0 + r.d);
      return r.d;
    }
    case 'noise': {
      const n = c.createBufferSource(); n.buffer = noiseBuffer(c);
      const f = c.createBiquadFilter(); f.type = 'lowpass'; f.frequency.setValueAtTime(r.lp, t0); f.frequency.exponentialRampToValueAtTime(80, t0 + r.d);
      n.connect(f).connect(envelope(c, t0, r.d, 0.16 * gain * (r.g ?? 1))).connect(out); n.start(t0); n.stop(t0 + r.d + 0.02);
      return r.d;
    }
    case 'tones': {
      r.f.forEach((f, i) => {
        const at = t0 + i * r.d * 0.9;
        const o = osc(r.wave, f);
        o.connect(envelope(c, at, r.d, 0.045 * gain)).connect(out); o.start(at); o.stop(at + r.d + 0.02);
      });
      return r.d * r.f.length;
    }
  }
}

/** Plays a cue now (fire-and-forget). Silent when sound is off, volume is 0 or WebAudio is missing. */
export function playCue(key: CueKey, opts: CueOptions = {}): boolean {
  cueStats.requested++;
  if (!enabled || volume <= 0) return false;
  const now = performance.now();
  // The same key retriggers at most every 45 ms, and at most 24 cues overlap (AUDIO-PLAN section 3).
  if ((lastPlayed.get(key) ?? -1e9) > now - 45 || voices >= MAX_VOICES) return false;
  const c = context();
  if (!c || !master) return false;
  try {
    if (c.state === 'suspended') void c.resume();
    lastPlayed.set(key, now);
    const t0 = c.currentTime + 0.005;
    const pan = c.createStereoPanner ? c.createStereoPanner() : null;
    if (pan) pan.pan.value = Math.max(-1, Math.min(1, opts.pan ?? 0));
    const out: AudioNode = pan ? (pan.connect(master), pan) : master;
    let dur: number;
    const user = userBuffers.get(key);
    if (user) {
      const src = c.createBufferSource(); src.buffer = user;
      const g = c.createGain(); g.gain.value = opts.gain ?? 1;
      src.connect(g).connect(out); src.start(t0);
      dur = user.duration;
    } else {
      dur = synth(c, out, RECIPES[key], t0, opts.gain ?? 1);
    }
    voices++;
    window.setTimeout(() => { voices = Math.max(0, voices - 1); }, dur * 1000 + 30);
    cueStats.played++;
    cueStats.byKey[key] = (cueStats.byKey[key] ?? 0) + 1;
    return true;
  } catch { return false; }
}

export function isCueKey(k: string): k is CueKey { return (CUE_KEYS as string[]).includes(k); }

/**
 * Loads user-supplied local audio files. A file named after a cue key (`weapon.PhotonTorpedo.wav`,
 * `sfx.explosion.large.ogg`) replaces that cue for this session. Returns the keys that were loaded.
 */
export async function loadUserCues(files: Iterable<File>): Promise<{ loaded: CueKey[]; skipped: string[] }> {
  const loaded: CueKey[] = [], skipped: string[] = [];
  const c = context();
  for (const file of files) {
    const key = file.name.replace(/\.[a-z0-9]+$/i, '');
    if (!isCueKey(key) || !c) { skipped.push(file.name); continue; }
    try {
      const buf = await c.decodeAudioData(await file.arrayBuffer());
      userBuffers.set(key, buf);
      loaded.push(key);
    } catch { skipped.push(file.name); }
  }
  return { loaded, skipped };
}
export function userCueKeys(): CueKey[] { return [...userBuffers.keys()]; }
export function clearUserCues(): void { userBuffers.clear(); }
