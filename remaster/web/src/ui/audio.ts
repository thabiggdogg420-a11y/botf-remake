// Optional synthesized UI sounds (WebAudio). Off by default; toggled from the menu.
import { signal } from '@preact/signals';

function readPref(): boolean {
  try { return window.localStorage.getItem('botf.audio') === 'on'; } catch { return false; }
}
export const audioEnabled = signal<boolean>(typeof window !== 'undefined' ? readPref() : false);

export function setAudio(on: boolean): void {
  audioEnabled.value = on;
  try { window.localStorage.setItem('botf.audio', on ? 'on' : 'off'); } catch { /* ignore */ }
  if (on) blip('confirm');
}

let ctx: AudioContext | null = null;
type Kind = 'nav' | 'confirm' | 'cancel' | 'error' | 'endturn' | 'start' | 'select' | 'fire';

const TONES: Record<Kind, { f: number[]; d: number; type: OscillatorType; g: number }> = {
  nav: { f: [880, 1320], d: 0.07, type: 'sine', g: 0.05 },
  select: { f: [1200], d: 0.05, type: 'sine', g: 0.04 },
  confirm: { f: [660, 990], d: 0.09, type: 'triangle', g: 0.05 },
  cancel: { f: [700, 470], d: 0.09, type: 'triangle', g: 0.05 },
  error: { f: [220, 180], d: 0.16, type: 'square', g: 0.035 },
  endturn: { f: [523, 659, 784], d: 0.1, type: 'sine', g: 0.05 },
  start: { f: [392, 523, 659, 1046], d: 0.11, type: 'sine', g: 0.05 },
  fire: { f: [1400, 300], d: 0.12, type: 'sawtooth', g: 0.025 },
};

export function blip(kind: Kind): void {
  if (!audioEnabled.value) return;
  try {
    ctx ??= new AudioContext();
    const tone = TONES[kind];
    const start = ctx.currentTime;
    tone.f.forEach((freq, i) => {
      const osc = ctx!.createOscillator();
      const gain = ctx!.createGain();
      osc.type = tone.type;
      osc.frequency.value = freq;
      const t0 = start + i * tone.d * 0.8;
      gain.gain.setValueAtTime(0, t0);
      gain.gain.linearRampToValueAtTime(tone.g, t0 + 0.01);
      gain.gain.exponentialRampToValueAtTime(0.0001, t0 + tone.d);
      osc.connect(gain).connect(ctx!.destination);
      osc.start(t0);
      osc.stop(t0 + tone.d + 0.02);
    });
  } catch {
    /* audio is decorative */
  }
}
