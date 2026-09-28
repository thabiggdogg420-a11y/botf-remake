import { useEffect, useRef } from 'preact/hooks';
import type { Coord, Encounter, TacticalRound, TacticalShip } from '../core/types';
import { palette } from '../ui/theme';

export interface ShipFrame { pos: Coord | null; hull: number; shields: number; alive: boolean; retreated: boolean; yaw: number }

/** State of every ship after `k` rounds (k = 0: deployment). */
export function frameAt(e: Encounter, k: number): Map<number, ShipFrame> {
  const out = new Map<number, ShipFrame>();
  const first = e.rounds[0];
  for (const s of e.ships) {
    let f: ShipFrame = { pos: s.position, hull: s.hull, shields: s.shields, alive: s.alive, retreated: s.retreated, yaw: s.mine ? 0 : 180 };
    if (k < e.rounds.length || (s.hidden && k === e.rounds.length)) {
      if (k === 0) {
        const st = first?.ships.find((x) => x.id === s.id);
        f = st ? { pos: st.from, hull: st.hullBefore, shields: st.shieldBefore, alive: true, retreated: false, yaw: s.mine ? 0 : 180 }
          : { pos: s.position, hull: s.initialHull, shields: s.initialShields, alive: true, retreated: false, yaw: s.mine ? 0 : 180 };
      } else {
        // last recorded status at or before round k
        for (let r = k - 1; r >= 0; r--) {
          const st = e.rounds[r]!.ships.find((x) => x.id === s.id);
          if (st) {
            f = { pos: st.to, hull: st.hullAfter, shields: st.shieldAfter, alive: !(st.destroyed || isLostBy(e, s.id, r)), retreated: isRetreatedBy(e, s.id, r), yaw: st.yaw };
            break;
          }
        }
      }
    } else if (k === e.rounds.length && e.rounds.length) {
      const st = e.rounds[k - 1]!.ships.find((x) => x.id === s.id);
      if (st) f.yaw = st.yaw;
    }
    out.set(s.id, f);
  }
  return out;
}
function isLostBy(e: Encounter, id: number, r: number) { for (let i = 0; i <= r; i++) if (e.rounds[i]!.lost.includes(id)) return true; return false; }
function isRetreatedBy(e: Encounter, id: number, r: number) { for (let i = 0; i <= r; i++) if (e.rounds[i]!.retreatedIds.includes(id)) return true; return false; }

export function CombatArena({ encounter, round, selected, target, onPick, animateKey }: {
  encounter: Encounter; round: number; selected: number | null; target: number | null; onPick: (id: number) => void; animateKey: string;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const state = useRef({ encounter, round, selected, target, start: performance.now(), animateKey, prevRound: round });
  if (state.current.animateKey !== animateKey || state.current.round !== round) {
    state.current.prevRound = state.current.round;
    state.current.start = performance.now();
  }
  Object.assign(state.current, { encounter, round, selected, target, animateKey });

  useEffect(() => {
    const canvas = ref.current!;
    const ctx = canvas.getContext('2d')!;
    let frame = 0;
    const stars = Array.from({ length: 160 }, () => ({ x: Math.random(), y: Math.random(), r: Math.random() * 1.2 + 0.2, a: Math.random() * 0.6 + 0.2 }));
    const hits: { id: number; x: number; y: number }[] = [];
    const draw = (now: number) => {
      frame = requestAnimationFrame(draw);
      const { encounter: e, round: k, selected: sel, target: tgt, start, prevRound } = state.current;
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      const w = canvas.clientWidth, h = canvas.clientHeight;
      if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) { canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr); }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, w, h);
      const g = ctx.createRadialGradient(w * 0.5, h * 0.45, 0, w * 0.5, h * 0.5, Math.max(w, h) * 0.7);
      g.addColorStop(0, '#0b1430'); g.addColorStop(1, '#03050b');
      ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
      for (const s of stars) { ctx.globalAlpha = s.a; ctx.fillStyle = '#dfe8ff'; ctx.beginPath(); ctx.arc(s.x * w, s.y * h, s.r, 0, 6.283); ctx.fill(); }
      ctx.globalAlpha = 1;
      const pad = 28;
      const cell = Math.min((w - pad * 2) / e.arenaWidth, (h - pad * 2) / e.arenaHeight);
      const ox = (w - cell * e.arenaWidth) / 2, oy = (h - cell * e.arenaHeight) / 2;
      // grid
      ctx.strokeStyle = 'rgba(120,160,230,0.10)'; ctx.lineWidth = 1;
      ctx.beginPath();
      for (let x = 0; x <= e.arenaWidth; x++) { ctx.moveTo(ox + x * cell + 0.5, oy); ctx.lineTo(ox + x * cell + 0.5, oy + e.arenaHeight * cell); }
      for (let y = 0; y <= e.arenaHeight; y++) { ctx.moveTo(ox, oy + y * cell + 0.5); ctx.lineTo(ox + e.arenaWidth * cell, oy + y * cell + 0.5); }
      ctx.stroke();
      ctx.strokeStyle = 'rgba(120,160,230,0.28)'; ctx.strokeRect(ox + 0.5, oy + 0.5, e.arenaWidth * cell, e.arenaHeight * cell);
      const center = (p: Coord) => [ox + (p.x + 0.5) * cell, oy + (p.y + 0.5) * cell] as const;

      // animation between prevRound -> k (only when advancing by one round)
      const animating = k === prevRound + 1 && k > 0;
      const dur = 1100;
      const t = animating ? Math.min(1, (now - start) / dur) : 1;
      const before = frameAt(e, animating ? k - 1 : k);
      const after = frameAt(e, k);
      const roundData: TacticalRound | undefined = animating ? e.rounds[k - 1] : undefined;
      hits.length = 0;
      const pos = (s: TacticalShip): [number, number] | null => {
        const a = before.get(s.id)!, b = after.get(s.id)!;
        if (!b.pos && !a.pos) return null;
        if (!animating || !roundData) return b.pos ? [...center(b.pos)] as [number, number] : null;
        const st = roundData.ships.find((x) => x.id === s.id);
        if (!st) return b.pos ? [...center(b.pos)] as [number, number] : null;
        const p0 = center(st.from), p1 = center(st.via), p2 = center(st.to);
        const e2 = t < 0.5 ? t * 2 : (t - 0.5) * 2;
        const [ax, ay] = t < 0.5 ? p0 : p1, [bx, by] = t < 0.5 ? p1 : p2;
        const ease = e2 * e2 * (3 - 2 * e2);
        return [ax + (bx - ax) * ease, ay + (by - ay) * ease];
      };
      // trails of the last round
      const last = e.rounds[k - 1];
      if (last && !animating) {
        for (const st of last.ships) {
          const ship = e.ships.find((s) => s.id === st.id);
          if (!ship || ship.hidden) continue;
          const [x0, y0] = center(st.from), [x1, y1] = center(st.via), [x2, y2] = center(st.to);
          ctx.strokeStyle = ship.mine ? 'rgba(140,190,255,0.25)' : 'rgba(255,120,110,0.22)';
          ctx.setLineDash([3, 4]); ctx.lineWidth = 1.2;
          ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); ctx.setLineDash([]);
        }
      }
      // ships
      const positions = new Map<number, [number, number]>();
      for (const s of e.ships) {
        const p = pos(s);
        const a = after.get(s.id)!;
        if (!p) continue;
        positions.set(s.id, p);
        const destroyedNow = animating && !a.alive && before.get(s.id)!.alive;
        if (!a.alive && !destroyedNow) {
          ctx.globalAlpha = 0.35; ctx.fillStyle = '#556';
          ctx.beginPath(); ctx.arc(p[0], p[1], cell * 0.12, 0, 6.283); ctx.fill(); ctx.globalAlpha = 1;
          continue;
        }
        if (a.retreated && !animating) continue;
        const pal = palette(s.owner);
        const size = cell * 0.34;
        const yaw = ((a.yaw ?? 0) * Math.PI) / 180;
        ctx.save();
        ctx.translate(p[0], p[1]);
        if (s.id === sel || s.id === tgt) {
          ctx.strokeStyle = s.id === sel ? pal.primary : '#ff5a4e'; ctx.lineWidth = 2;
          const r = size * 1.55 + Math.sin(now / 200) * 1.5;
          ctx.beginPath(); ctx.arc(0, 0, r, 0, 6.283); ctx.stroke();
          if (s.id === tgt) { ctx.beginPath(); for (let i = 0; i < 4; i++) { const an = i * Math.PI / 2; ctx.moveTo(Math.cos(an) * (r + 3), Math.sin(an) * (r + 3)); ctx.lineTo(Math.cos(an) * (r + 9), Math.sin(an) * (r + 9)); } ctx.stroke(); }
        }
        ctx.rotate(yaw);
        const glow = ctx.createRadialGradient(0, 0, 0, 0, 0, size * 1.6);
        glow.addColorStop(0, pal.glow); glow.addColorStop(1, 'rgba(0,0,0,0)');
        ctx.fillStyle = glow; ctx.beginPath(); ctx.arc(0, 0, size * 1.6, 0, 6.283); ctx.fill();
        ctx.fillStyle = pal.map; ctx.strokeStyle = 'rgba(255,255,255,0.85)'; ctx.lineWidth = 1;
        ctx.globalAlpha = s.cloaked ? 0.35 : 1;
        ctx.beginPath();
        ctx.moveTo(size, 0); ctx.lineTo(-size * 0.7, size * 0.62); ctx.lineTo(-size * 0.35, 0); ctx.lineTo(-size * 0.7, -size * 0.62); ctx.closePath();
        ctx.fill(); ctx.stroke();
        ctx.globalAlpha = 1;
        ctx.restore();
        if (destroyedNow && t > 0.6) {
          const q = (t - 0.6) / 0.4;
          ctx.fillStyle = `rgba(255,${180 - q * 120},80,${1 - q})`;
          ctx.beginPath(); ctx.arc(p[0], p[1], size * (1 + q * 2.5), 0, 6.283); ctx.fill();
        }
        // bars
        const bw = cell * 0.8, bx = p[0] - bw / 2, by = p[1] - size - 12;
        const hullNow = animating ? before.get(s.id)!.hull + (a.hull - before.get(s.id)!.hull) * Math.min(1, t * 1.5) : a.hull;
        const shNow = animating ? before.get(s.id)!.shields + (a.shields - before.get(s.id)!.shields) * Math.min(1, t * 1.5) : a.shields;
        if (!s.hidden) {
          ctx.fillStyle = 'rgba(0,0,0,0.6)'; ctx.fillRect(bx, by, bw, 7);
          ctx.fillStyle = '#6fd3ff'; ctx.fillRect(bx, by, bw * Math.max(0, shNow) / Math.max(1, s.maxShields), 3);
          ctx.fillStyle = hullNow / Math.max(1, s.maxHull) > 0.5 ? '#6ee07a' : hullNow / Math.max(1, s.maxHull) > 0.25 ? '#f5c451' : '#ff5a4e';
          ctx.fillRect(bx, by + 4, bw * Math.max(0, hullNow) / Math.max(1, s.maxHull), 3);
        }
        if (cell > 34) {
          ctx.font = '600 10px "Antonio Variable", Antonio, sans-serif';
          ctx.fillStyle = s.mine ? '#cfe0ff' : '#ffc9c4'; ctx.textAlign = 'center'; ctx.textBaseline = 'top';
          ctx.fillText(s.hidden ? 'CLOAKED CONTACT' : s.name.split(' — ')[0]!.toUpperCase(), p[0], p[1] + size + 4);
        }
        hits.push({ id: s.id, x: p[0], y: p[1] });
      }
      // weapons fire during the animated round
      if (roundData && t > 0.35 && t < 0.95) {
        const q = (t - 0.35) / 0.6;
        for (const sv of roundData.salvos) {
          const a = positions.get(sv.from), b = positions.get(sv.to);
          if (!a || !b || sv.beam + sv.torpedo <= 0) continue;
          const shooter = e.ships.find((s) => s.id === sv.from);
          const col = shooter ? palette(shooter.owner).primary : '#fff';
          if (sv.beam > 0) {
            ctx.strokeStyle = col; ctx.globalAlpha = 0.85 * (1 - Math.abs(q - 0.3) * 1.4);
            ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); ctx.globalAlpha = 1;
          }
          if (sv.torpedo > 0) {
            const tx = a[0] + (b[0] - a[0]) * q, ty = a[1] + (b[1] - a[1]) * q;
            ctx.fillStyle = '#ffd27a'; ctx.beginPath(); ctx.arc(tx, ty, 3, 0, 6.283); ctx.fill();
          }
          if (q > 0.7) {
            ctx.strokeStyle = sv.absorbed ? 'rgba(110,210,255,0.8)' : 'rgba(255,160,80,0.8)'; ctx.lineWidth = 2;
            ctx.beginPath(); ctx.arc(b[0], b[1], cell * 0.28 * (q - 0.5), 0, 6.283); ctx.stroke();
          }
        }
      }
    };
    frame = requestAnimationFrame(draw);
    const click = (ev: MouseEvent) => {
      const r = canvas.getBoundingClientRect();
      const x = ev.clientX - r.left, y = ev.clientY - r.top;
      let best: { id: number; d: number } | null = null;
      for (const h of hits) { const d = (h.x - x) ** 2 + (h.y - y) ** 2; if (d < 900 && (!best || d < best.d)) best = { id: h.id, d }; }
      if (best) state.current && onPickRef.current(best.id);
    };
    canvas.addEventListener('click', click);
    return () => { cancelAnimationFrame(frame); canvas.removeEventListener('click', click); };
  }, []);
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;
  return <canvas class="combat-arena" ref={ref} aria-label="Tactical arena" data-testid="combat-arena" />;
}
