// Tactical arena (Canvas 2D). Pure presentation of the core's encounter record:
//  * the camera frames the part of the 16x12 arena the battle actually uses;
//  * ships sharing (or crowding) a cell are laid out in a readable formation, each with its own
//    hull/shield rings, name and next-order chip, so nothing stacks;
//  * each resolved round animates movement, then every salvo in turn: beams, torpedo tracers,
//    mutual ram impacts, decloak ambushes, hails, disengaging ships, withdrawals and losses;
//  * between rounds, intent lines preview what every ship (including AI-controlled ones) will do.
import { useEffect, useMemo, useRef } from 'preact/hooks';
import type { Coord, Encounter, RoundShip, Salvo, TacticalRound, TacticalShip } from '../core/types';
import { reducedMotion } from '../state/settings';
import { palette } from '../ui/theme';

export interface ShipFrame {
  pos: Coord | null; hull: number; shields: number; alive: boolean; retreated: boolean; yaw: number;
  cloaked: boolean; disengaging: boolean;
}

function statusIn(r: TacticalRound | undefined, id: number): RoundShip | undefined {
  return r?.ships.find((x) => x.id === id);
}

/** State of every ship after `k` rounds (k = 0: deployment). */
export function frameAt(e: Encounter, k: number): Map<number, ShipFrame> {
  const out = new Map<number, ShipFrame>();
  const first = e.rounds[0];
  for (const s of e.ships) {
    const defaultYaw = s.mine ? 0 : 180;
    // Cloak-capable ships deploy cloaked and stay so until a round records their decloak.
    let decloakedBy = false;
    for (let r = 0; r < Math.min(k, e.rounds.length); r++) if (statusIn(e.rounds[r], s.id)?.decloaked) decloakedBy = true;
    const cloaked = s.hidden || (s.canCloak && !decloakedBy && (s.cloaked || k < e.rounds.length));
    let f: ShipFrame = { pos: s.position, hull: s.hull, shields: s.shields, alive: s.alive, retreated: s.retreated, yaw: defaultYaw, cloaked, disengaging: s.disengaging && !e.finished };
    if (k < e.rounds.length || (s.hidden && k === e.rounds.length)) {
      if (k === 0) {
        const st = statusIn(first, s.id);
        f = st ? { pos: st.from, hull: st.hullBefore, shields: st.shieldBefore, alive: true, retreated: false, yaw: defaultYaw, cloaked, disengaging: false }
          : { pos: s.position, hull: s.initialHull, shields: s.initialShields, alive: true, retreated: false, yaw: defaultYaw, cloaked, disengaging: false };
      } else {
        for (let r = k - 1; r >= 0; r--) {
          const st = statusIn(e.rounds[r], s.id);
          if (st) {
            f = { pos: st.to, hull: st.hullAfter, shields: st.shieldAfter, alive: !(st.destroyed || isLostBy(e, s.id, r)),
              retreated: isRetreatedBy(e, s.id, r), yaw: st.yaw, cloaked, disengaging: r === k - 1 && st.disengaging };
            break;
          }
        }
      }
    } else if (k === e.rounds.length && e.rounds.length) {
      const st = statusIn(e.rounds[k - 1], s.id);
      if (st) f.yaw = st.yaw;
    }
    out.set(s.id, f);
  }
  return out;
}
function isLostBy(e: Encounter, id: number, r: number) { for (let i = 0; i <= r; i++) if (e.rounds[i]!.lost.includes(id)) return true; return false; }
function isRetreatedBy(e: Encounter, id: number, r: number) { for (let i = 0; i <= r; i++) if (e.rounds[i]!.retreatedIds.includes(id)) return true; return false; }

const MANEUVER_LABEL: Record<string, string> = {
  Engage: 'ENGAGE', Charge: 'CHARGE', Circle: 'CIRCLE', Hold: 'HOLD', Retreat: 'RETREAT', Assault: 'ASSAULT', Harry: 'HARRY',
  Flyby: 'FLYBY', Strafe: 'STRAFE', Evade: 'EVADE', Ram: 'RAM', Hail: 'HAIL',
};

interface View { x0: number; y0: number; cols: number; rows: number }

/** The arena region worth showing: every recorded position plus a margin, shaped to the canvas. */
function frameRegion(e: Encounter, aspect: number): View {
  let x0 = e.arenaWidth, y0 = e.arenaHeight, x1 = -1, y1 = -1;
  const add = (p: Coord | null | undefined) => {
    if (!p) return;
    x0 = Math.min(x0, p.x); y0 = Math.min(y0, p.y); x1 = Math.max(x1, p.x); y1 = Math.max(y1, p.y);
  };
  for (const s of e.ships) if (!s.hidden) add(s.position);
  for (const r of e.rounds) for (const st of r.ships) { add(st.from); add(st.via); add(st.to); }
  if (x1 < 0) return { x0: 0, y0: 0, cols: e.arenaWidth, rows: e.arenaHeight };
  x0 -= 1; y0 -= 1.2; x1 += 2; y1 += 2.2;
  let cols = Math.max(8, x1 - x0), rows = Math.max(5, y1 - y0);
  if (cols / rows < aspect) { const want = rows * aspect; x0 -= (want - cols) / 2; cols = want; }
  else { const want = cols / aspect; y0 -= (want - rows) / 2; rows = want; }
  cols = Math.min(cols, e.arenaWidth + 0.6); rows = Math.min(rows, e.arenaHeight + 0.6);
  x0 = Math.min(Math.max(-0.3, x0), e.arenaWidth + 0.3 - cols);
  y0 = Math.min(Math.max(-0.3, y0), e.arenaHeight + 0.3 - rows);
  return { x0, y0, cols, rows };
}

function shipScale(s: TacticalShip): number {
  if (s.station) return s.stationType === 'Starbase' ? 1.55 : 1.3;
  const cls = s.shipClass === 'Scout' ? 0.8 : s.shipClass === 'Transport' || s.shipClass === 'ColonyShip' ? 0.86 : 1;
  return cls * Math.min(1.3, Math.max(0.85, 0.7 + Math.sqrt(Math.max(1, s.maxHull)) / 22));
}

function rgba(hex: string, a: number): string {
  const v = parseInt(hex.slice(1), 16);
  return `rgba(${(v >> 16) & 255},${(v >> 8) & 255},${v & 255},${a})`;
}

const ease = (x: number) => x * x * (3 - 2 * x);
const clamp01 = (x: number) => Math.max(0, Math.min(1, x));

export function CombatArena({ encounter, round, selected, target, onPick, animateKey, intent = false }: {
  encounter: Encounter; round: number; selected: number | null; target: number | null; onPick: (id: number) => void; animateKey: string;
  /** Draw next-round intent lines and order chips (live battle between rounds). */
  intent?: boolean;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const state = useRef({ encounter, round, selected, target, start: performance.now(), animateKey, prevRound: round, intent, hover: null as number | null });
  if (state.current.animateKey !== animateKey || state.current.round !== round) {
    state.current.prevRound = state.current.animateKey === animateKey ? state.current.round : round;
    state.current.start = performance.now();
  }
  Object.assign(state.current, { encounter, round, selected, target, animateKey, intent });
  const onPickRef = useRef(onPick);
  onPickRef.current = onPick;
  const stars = useMemo(() => {
    let seed = 7;
    const rand = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
    return Array.from({ length: 220 }, () => ({ x: rand(), y: rand(), r: rand() * 1.2 + 0.2, a: rand() * 0.55 + 0.15 }));
  }, []);

  useEffect(() => {
    const canvas = ref.current!;
    const ctx = canvas.getContext('2d')!;
    let frame = 0;
    let hits: { id: number; x: number; y: number; r: number }[] = [];
    let viewKey = '';
    let view: View = { x0: 0, y0: 0, cols: 16, rows: 12 };
    const draw = (now: number) => {
      frame = requestAnimationFrame(draw);
      const st = state.current;
      const { encounter: e, round: k, selected: sel, target: tgt, start, prevRound } = st;
      const dpr = Math.min(2, window.devicePixelRatio || 1);
      const w = canvas.clientWidth, h = canvas.clientHeight;
      if (!w || !h) return;
      if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) { canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr); }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

      // --- backdrop
      const g = ctx.createRadialGradient(w * 0.5, h * 0.45, 0, w * 0.5, h * 0.5, Math.max(w, h) * 0.75);
      g.addColorStop(0, '#0c1633'); g.addColorStop(0.6, '#060a18'); g.addColorStop(1, '#02040a');
      ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
      const neb = ctx.createRadialGradient(w * 0.78, h * 0.25, 0, w * 0.78, h * 0.25, w * 0.45);
      neb.addColorStop(0, 'rgba(90,60,160,0.16)'); neb.addColorStop(1, 'rgba(90,60,160,0)');
      ctx.fillStyle = neb; ctx.fillRect(0, 0, w, h);
      for (const s of stars) { ctx.globalAlpha = s.a; ctx.fillStyle = '#dfe8ff'; ctx.beginPath(); ctx.arc(s.x * w, s.y * h, s.r, 0, 6.283); ctx.fill(); }
      ctx.globalAlpha = 1;

      // --- framing (stable for the whole encounter so nothing jumps between rounds)
      const pad = 10;
      const key = `${e.turn}:${e.sectorX}.${e.sectorY}:${e.rounds.length}:${Math.round(w)}x${Math.round(h)}`;
      if (key !== viewKey) { viewKey = key; view = frameRegion(e, (w - pad * 2) / (h - pad * 2)); }
      const cell = Math.min((w - pad * 2) / view.cols, (h - pad * 2) / view.rows);
      const ox = (w - cell * view.cols) / 2 - view.x0 * cell, oy = (h - cell * view.rows) / 2 - view.y0 * cell;
      const center = (p: Coord): [number, number] => [ox + (p.x + 0.5) * cell, oy + (p.y + 0.5) * cell];

      // grid + arena bounds + withdrawal edges
      ctx.strokeStyle = 'rgba(120,160,230,0.09)'; ctx.lineWidth = 1;
      ctx.beginPath();
      for (let x = 0; x <= e.arenaWidth; x++) { const sx = Math.round(ox + x * cell) + 0.5; ctx.moveTo(sx, oy); ctx.lineTo(sx, oy + e.arenaHeight * cell); }
      for (let y = 0; y <= e.arenaHeight; y++) { const sy = Math.round(oy + y * cell) + 0.5; ctx.moveTo(ox, sy); ctx.lineTo(ox + e.arenaWidth * cell, sy); }
      ctx.stroke();
      const firstOwner = e.ships[0]?.owner ?? 0;
      const secondOwner = e.ships.find((s) => s.owner !== firstOwner)?.owner ?? 0;
      for (const [owner, edgeX] of [[firstOwner, 0], [secondOwner, e.arenaWidth - 1]] as const) {
        if (!owner) continue;
        const col = palette(owner).map;
        const x = ox + edgeX * cell;
        const band = ctx.createLinearGradient(edgeX === 0 ? x : x + cell, 0, edgeX === 0 ? x + cell : x, 0);
        band.addColorStop(0, rgba(col, 0.16)); band.addColorStop(1, rgba(col, 0));
        ctx.fillStyle = band; ctx.fillRect(x, oy, cell, e.arenaHeight * cell);
        if (cell > 26) {
          ctx.save();
          ctx.translate(edgeX === 0 ? x + 9 : x + cell - 9, oy + (e.arenaHeight * cell) / 2);
          ctx.rotate(edgeX === 0 ? -Math.PI / 2 : Math.PI / 2);
          ctx.font = '600 9px "Antonio Variable", Antonio, sans-serif';
          ctx.fillStyle = rgba(col, 0.55); ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
          (ctx as CanvasRenderingContext2D & { letterSpacing?: string }).letterSpacing = '3px';
          ctx.fillText('WITHDRAWAL EDGE', 0, 0);
          ctx.restore();
        }
      }
      ctx.strokeStyle = 'rgba(120,160,230,0.26)'; ctx.strokeRect(ox + 0.5, oy + 0.5, e.arenaWidth * cell, e.arenaHeight * cell);

      // --- timeline of the animated round
      const motion = !reducedMotion.value;
      const animating = motion && k === prevRound + 1 && k > 0;
      const dur = 2300;
      const t = animating ? Math.min(1, (now - start) / dur) : 1;
      const tMove = clamp01(t / 0.38);
      const before = frameAt(e, animating ? k - 1 : k);
      const after = frameAt(e, k);
      const roundData: TacticalRound | undefined = animating ? e.rounds[k - 1] : undefined;
      const lastRound: TacticalRound | undefined = k > 0 ? e.rounds[k - 1] : undefined;
      const salvos = (roundData?.salvos ?? []).filter((sv) => sv.beam + sv.torpedo > 0 || sv.ram);
      const salvoWindow = (i: number): [number, number] => {
        const n = Math.max(1, salvos.length);
        const s0 = 0.4 + (0.42 * i) / n;
        return [s0, Math.min(0.97, s0 + Math.max(0.16, 0.42 / n + 0.08))];
      };

      // --- raw positions (movement phase interpolates from -> via -> to)
      const rawPos = new Map<number, [number, number]>();
      for (const s of e.ships) {
        const a = before.get(s.id)!, b = after.get(s.id)!;
        if (s.hidden) continue;
        if (!b.pos && !a.pos) continue;
        const stt = roundData ? statusIn(roundData, s.id) : undefined;
        if (!animating || !stt) { if (b.pos) rawPos.set(s.id, center(b.pos)); continue; }
        const p0 = center(stt.from), p1 = center(stt.via), p2 = center(stt.to);
        const e2 = ease(tMove < 0.5 ? tMove * 2 : (tMove - 0.5) * 2);
        const [ax, ay] = tMove < 0.5 ? p0 : p1, [bx, by] = tMove < 0.5 ? p1 : p2;
        rawPos.set(s.id, [ax + (bx - ax) * e2, ay + (by - ay) * e2]);
      }
      const baseR = Math.max(10, Math.min(24, cell * 0.3));
      const radius = (s: TacticalShip) => baseR * shipScale(s);
      const shown = (s: TacticalShip) => {
        const a = after.get(s.id)!, b = before.get(s.id)!;
        if (!rawPos.has(s.id)) return false;
        if (a.alive && !a.retreated) return true;
        return animating && (b.alive && !b.retreated);   // dies or leaves during this animation
      };

      // --- formation layout: co-located ships spread out, then a few relaxation passes
      const pos = new Map<number, [number, number]>();
      const groups = new Map<string, number[]>();
      for (const s of e.ships) {
        if (!shown(s)) continue;
        const p = rawPos.get(s.id)!;
        const gk = `${Math.round((p[0] - ox) / cell - 0.5)},${Math.round((p[1] - oy) / cell - 0.5)}`;
        (groups.get(gk) ?? groups.set(gk, []).get(gk)!).push(s.id);
      }
      for (const ids of groups.values()) {
        const n = ids.length;
        ids.forEach((id, i) => {
          const p = rawPos.get(id)!;
          if (n === 1) { pos.set(id, [p[0], p[1]]); return; }
          const ship = e.ships.find((s) => s.id === id)!;
          const dir = ship.mine ? 1 : -1;
          const spacing = baseR * 2.6;
          if (n === 2) {
            // Pair: stacked vertically with a slight stagger.
            pos.set(id, [p[0] + dir * (i ? -0.15 : 0.15) * spacing, p[1] + (i ? 0.5 : -0.5) * spacing]);
            return;
          }
          // Wedge: lead ship forward, wingmen staggered back and to alternating sides.
          const rank = Math.ceil(i / 2), side = i === 0 ? 0 : i % 2 ? -1 : 1;
          const depth = Math.ceil((n - 1) / 2);
          pos.set(id, [p[0] + dir * (depth / 2 - rank) * spacing * 0.6, p[1] + side * rank * spacing * 0.62]);
        });
      }
      // Each ship claims a box: its condition rings above, its name and order chip below. Boxes are
      // separated along the axis of least overlap so neither glyphs nor labels ever stack.
      const labelsOn = cell >= 22;
      ctx.font = '600 10.5px "Antonio Variable", Antonio, "Arial Narrow", sans-serif';
      const boxes = new Map<number, { hw: number; up: number; down: number }>();
      for (const id of pos.keys()) {
        const s = e.ships.find((x) => x.id === id)!;
        const R = radius(s);
        const labelW = labelsOn ? ctx.measureText(s.name.split(' — ')[0]!.toUpperCase()).width : 0;
        boxes.set(id, { hw: Math.max(R * 1.75, labelW / 2 + 4), up: R * 1.75, down: R * 1.7 + (labelsOn ? 30 : 4) });
      }
      const ids = [...pos.keys()];
      for (let iter = 0; iter < 14; iter++) {
        let moved = false;
        for (let i = 0; i < ids.length; i++) for (let j = i + 1; j < ids.length; j++) {
          const a = pos.get(ids[i]!)!, b = pos.get(ids[j]!)!;
          const ba = boxes.get(ids[i]!)!, bb = boxes.get(ids[j]!)!;
          const dx = b[0] - a[0], dy = b[1] - a[1];
          const ox = ba.hw + bb.hw - Math.abs(dx);
          const oy = (dy >= 0 ? ba.down + bb.up : ba.up + bb.down) - Math.abs(dy);
          if (ox <= 0 || oy <= 0) continue;
          moved = true;
          if (oy <= ox * 1.2) {
            const dir = dy === 0 ? (i % 2 ? -1 : 1) : Math.sign(dy);
            a[1] -= (dir * oy) / 2; b[1] += (dir * oy) / 2;
          } else {
            const dir = dx === 0 ? (i % 2 ? -1 : 1) : Math.sign(dx);
            a[0] -= (dir * ox) / 2; b[0] += (dir * ox) / 2;
          }
        }
        if (!moved) break;
      }
      for (const [id, p] of pos) {
        const bx = boxes.get(id)!;
        p[0] = Math.min(w - bx.hw, Math.max(bx.hw, p[0]));
        p[1] = Math.min(h - bx.down, Math.max(bx.up, p[1]));
      }

      // --- ram lunge offset for rammers during their salvo
      const lunge = new Map<number, [number, number]>();
      salvos.forEach((sv, i) => {
        if (!sv.ram) return;
        const rammer = roundData?.ships.find((x) => x.id === sv.from && x.maneuver === 'Ram');
        if (!rammer) return;
        const [s0, s1] = salvoWindow(i);
        const q = clamp01((t - s0) / (s1 - s0));
        const a = pos.get(sv.from), b = pos.get(sv.to);
        if (!a || !b) return;
        const f = Math.sin(Math.min(1, q * 1.4) * Math.PI) * 0.6;
        lunge.set(sv.from, [(b[0] - a[0]) * f, (b[1] - a[1]) * f]);
      });
      const at = (id: number): [number, number] | null => {
        const p = pos.get(id);
        if (!p) return null;
        const l = lunge.get(id);
        return l ? [p[0] + l[0], p[1] + l[1]] : p;
      };

      // --- trails of the last round (static)
      if (lastRound && !animating) {
        for (const stt of lastRound.ships) {
          const ship = e.ships.find((s) => s.id === stt.id);
          if (!ship || ship.hidden) continue;
          const [x0, y0] = center(stt.from), [x1, y1] = center(stt.via), [x2, y2] = center(stt.to);
          if (x0 === x2 && y0 === y2) continue;
          ctx.strokeStyle = rgba(palette(ship.owner).map, 0.22);
          ctx.setLineDash([3, 5]); ctx.lineWidth = 1.2;
          ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); ctx.setLineDash([]);
        }
      }

      // --- intent lines: what every ship will do next round (player orders, standing orders, AI)
      if (st.intent && !animating && !e.finished) {
        for (const s of e.ships) {
          if (!s.next || !s.active || s.hidden) continue;
          const a = at(s.id);
          if (!a) continue;
          const tid = s.next.target;
          const b = tid ? at(tid) : null;
          const col = palette(s.owner).map;
          ctx.save();
          if (b) {
            ctx.strokeStyle = rgba(col, s.id === sel || tid === tgt ? 0.55 : 0.2);
            ctx.lineWidth = s.id === sel ? 1.8 : 1.1;
            ctx.setLineDash(s.next.source === 'ai' ? [2, 5] : [7, 5]);
            ctx.lineDashOffset = motion ? -now / 60 : 0;
            const ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
            const ra = radius(s) * 1.6, rb = baseR * 1.6;
            const sx = a[0] + Math.cos(ang) * ra, sy = a[1] + Math.sin(ang) * ra, ex = b[0] - Math.cos(ang) * rb, ey = b[1] - Math.sin(ang) * rb;
            ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(ex, ey); ctx.stroke();
            ctx.setLineDash([]);
            ctx.fillStyle = ctx.strokeStyle;
            ctx.beginPath(); ctx.moveTo(ex, ey); ctx.lineTo(ex - 7 * Math.cos(ang - 0.4), ey - 7 * Math.sin(ang - 0.4)); ctx.lineTo(ex - 7 * Math.cos(ang + 0.4), ey - 7 * Math.sin(ang + 0.4)); ctx.fill();
          } else if (s.next.maneuver === 'Retreat') {
            const dir = s.owner === firstOwner ? -1 : 1;
            ctx.strokeStyle = rgba(col, 0.45); ctx.lineWidth = 1.4; ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(a[0] + dir * radius(s) * 1.6, a[1]); ctx.lineTo(a[0] + dir * radius(s) * 3.6, a[1]); ctx.stroke();
          }
          ctx.restore();
        }
      }

      // --- ships
      hits = [];
      const floatTexts: { x: number; y: number; text: string; color: string; q: number }[] = [];
      for (const s of e.ships) {
        if (!shown(s)) {
          // Wreck marker where a ship was lost.
          const a = after.get(s.id)!;
          if (!a.alive && a.pos && !s.hidden) {
            const [x, y] = center(a.pos);
            ctx.globalAlpha = 0.3; ctx.strokeStyle = '#8a93a8'; ctx.lineWidth = 1.2;
            ctx.beginPath(); ctx.moveTo(x - 5, y - 5); ctx.lineTo(x + 5, y + 5); ctx.moveTo(x + 5, y - 5); ctx.lineTo(x - 5, y + 5); ctx.stroke();
            ctx.globalAlpha = 1;
          }
          continue;
        }
        const p = at(s.id)!;
        const a = after.get(s.id)!, b = before.get(s.id)!;
        const pal = palette(s.owner);
        const R = radius(s);
        const stt = roundData ? statusIn(roundData, s.id) : undefined;
        const destroyedNow = animating && !a.alive && b.alive;
        const retreatingNow = animating && a.retreated && !b.retreated;
        let alpha = 1;
        if (destroyedNow) alpha = t < 0.86 ? 1 : Math.max(0, 1 - (t - 0.86) / 0.1);
        let [px, py] = p;
        if (retreatingNow) {
          // Warp out toward the ship's own withdrawal edge before this round's fire.
          const q = clamp01((t - 0.2) / 0.3);
          const dir = s.owner === firstOwner ? -1 : 1;
          px += dir * q * q * cell * 5; alpha = 1 - q;
          ctx.strokeStyle = rgba(pal.map, 0.5 * (1 - q)); ctx.lineWidth = 2;
          ctx.beginPath(); ctx.moveTo(p[0], py); ctx.lineTo(px, py); ctx.stroke();
        }
        const cloaked = animating ? (b.cloaked && !(stt?.decloaked && t > 0.4)) : a.cloaked;
        const hullNow = animating ? b.hull + (a.hull - b.hull) * clamp01((t - 0.45) / 0.5) : a.hull;
        const shNow = animating ? b.shields + (a.shields - b.shields) * clamp01((t - 0.45) / 0.5) : a.shields;
        ctx.save();
        ctx.globalAlpha = alpha;
        ctx.translate(px, py);
        // selection / target / hover rings
        if (s.id === sel) {
          ctx.strokeStyle = pal.primary; ctx.lineWidth = 2.2;
          ctx.setLineDash([R * 0.9, R * 0.35]); ctx.lineDashOffset = motion ? -now / 40 : 0;
          ctx.beginPath(); ctx.arc(0, 0, R * 1.85, 0, 6.283); ctx.stroke(); ctx.setLineDash([]);
        }
        if (s.id === tgt) {
          ctx.strokeStyle = '#ff5a4e'; ctx.lineWidth = 2;
          const rr = R * 1.85 + (motion ? Math.sin(now / 220) * 1.5 : 0);
          for (let i = 0; i < 4; i++) {
            const an = i * Math.PI / 2 + Math.PI / 4;
            ctx.beginPath(); ctx.arc(0, 0, rr, an - 0.35, an + 0.35); ctx.stroke();
            ctx.beginPath(); ctx.moveTo(Math.cos(an) * (rr + 2), Math.sin(an) * (rr + 2)); ctx.lineTo(Math.cos(an) * (rr + 8), Math.sin(an) * (rr + 8)); ctx.stroke();
          }
        }
        if (s.id === st.hover && s.id !== sel && s.id !== tgt) {
          ctx.strokeStyle = 'rgba(230,238,255,0.55)'; ctx.lineWidth = 1;
          ctx.beginPath(); ctx.arc(0, 0, R * 1.75, 0, 6.283); ctx.stroke();
        }
        // condition rings: shields outside, hull inside
        if (!cloaked || s.mine) {
          const hullFrac = Math.max(0, hullNow) / Math.max(1, s.maxHull), shFrac = Math.max(0, shNow) / Math.max(1, s.maxShields);
          ctx.lineWidth = 2.6; ctx.lineCap = 'round';
          ctx.strokeStyle = 'rgba(255,255,255,0.08)'; ctx.beginPath(); ctx.arc(0, 0, R * 1.32, 0, 6.283); ctx.stroke();
          ctx.strokeStyle = hullFrac > 0.5 ? '#6ee07a' : hullFrac > 0.25 ? '#f5c451' : '#ff5a4e';
          if (hullFrac > 0) { ctx.beginPath(); ctx.arc(0, 0, R * 1.32, -Math.PI / 2, -Math.PI / 2 + hullFrac * 6.283); ctx.stroke(); }
          if (s.maxShields > 0) {
            ctx.lineWidth = 1.6;
            ctx.strokeStyle = 'rgba(111,211,255,0.14)'; ctx.beginPath(); ctx.arc(0, 0, R * 1.55, 0, 6.283); ctx.stroke();
            ctx.strokeStyle = '#6fd3ff';
            if (shFrac > 0) { ctx.beginPath(); ctx.arc(0, 0, R * 1.55, -Math.PI / 2, -Math.PI / 2 + shFrac * 6.283); ctx.stroke(); }
          }
          ctx.lineCap = 'butt';
        }
        // hull glyph
        // Stations never turn to face anything: a slow docking-ring spin instead of the ship's heading.
        const yaw = s.station ? (motion ? now / 6000 : 0) : (((animating && stt ? stt.yaw : a.yaw) ?? 0) * Math.PI) / 180;
        ctx.rotate(yaw);
        const glow = ctx.createRadialGradient(0, 0, 0, 0, 0, R * 1.5);
        glow.addColorStop(0, pal.glow); glow.addColorStop(1, 'rgba(0,0,0,0)');
        ctx.fillStyle = glow; ctx.beginPath(); ctx.arc(0, 0, R * 1.5, 0, 6.283); ctx.fill();
        ctx.globalAlpha = alpha * (cloaked ? 0.32 : 1);
        ctx.fillStyle = pal.map; ctx.strokeStyle = 'rgba(255,255,255,0.9)'; ctx.lineWidth = 1.2;
        ctx.beginPath();
        if (s.station) {
          // Immobile base: an armoured hexagon (outpost) or a ring with docking spokes (starbase).
          if (s.stationType === 'Starbase') {
            ctx.arc(0, 0, R * 0.95, 0, 6.283); ctx.moveTo(R * 0.42, 0); ctx.arc(0, 0, R * 0.42, 0, 6.283, true);
            ctx.fill('evenodd'); ctx.stroke();
            ctx.beginPath();
            for (let i = 0; i < 4; i++) {
              const an = (i * Math.PI) / 2;
              ctx.moveTo(Math.cos(an) * R * 0.42, Math.sin(an) * R * 0.42); ctx.lineTo(Math.cos(an) * R * 1.18, Math.sin(an) * R * 1.18);
            }
            ctx.lineWidth = 2.2; ctx.strokeStyle = pal.map; ctx.stroke();
            ctx.lineWidth = 1.2; ctx.strokeStyle = 'rgba(255,255,255,0.9)';
            ctx.beginPath(); ctx.arc(0, 0, R * 0.22, 0, 6.283);
          } else {
            for (let i = 0; i < 6; i++) {
              const an = (i * Math.PI) / 3;
              if (i) ctx.lineTo(Math.cos(an) * R * 0.9, Math.sin(an) * R * 0.9); else ctx.moveTo(R * 0.9, 0);
            }
            ctx.closePath();
          }
        } else if (s.shipClass === 'Transport' || s.shipClass === 'ColonyShip') {
          ctx.ellipse(0, 0, R * 0.95, R * 0.5, 0, 0, 6.283);
        } else if (s.shipClass === 'Scout') {
          ctx.moveTo(R, 0); ctx.lineTo(-R * 0.75, R * 0.45); ctx.lineTo(-R * 0.45, 0); ctx.lineTo(-R * 0.75, -R * 0.45); ctx.closePath();
        } else {
          ctx.moveTo(R, 0); ctx.lineTo(R * 0.15, R * 0.3); ctx.lineTo(-R * 0.55, R * 0.75); ctx.lineTo(-R * 0.35, R * 0.15);
          ctx.lineTo(-R * 0.8, 0); ctx.lineTo(-R * 0.35, -R * 0.15); ctx.lineTo(-R * 0.55, -R * 0.75); ctx.lineTo(R * 0.15, -R * 0.3); ctx.closePath();
        }
        ctx.fill(); ctx.stroke();
        if (cloaked) {
          // Cloak shimmer
          ctx.globalAlpha = alpha * (0.35 + (motion ? 0.25 * Math.sin(now / 180 + s.id) : 0.1));
          ctx.strokeStyle = '#bfe6ff'; ctx.setLineDash([2, 3]); ctx.lineWidth = 1;
          ctx.beginPath(); ctx.arc(0, 0, R * 1.1, 0, 6.283); ctx.stroke(); ctx.setLineDash([]);
        }
        ctx.restore();
        ctx.globalAlpha = 1;

        // disengaging: dashed halo + arrow toward the withdrawal edge
        const disengaging = animating ? !!stt?.disengaging && t > 0.2 : a.disengaging;
        if (disengaging && !retreatingNow) {
          const dir = s.owner === firstOwner ? -1 : 1;
          ctx.strokeStyle = '#f5c451'; ctx.lineWidth = 1.5; ctx.setLineDash([4, 3]);
          ctx.beginPath(); ctx.arc(px, py, R * 1.95, 0, 6.283); ctx.stroke(); ctx.setLineDash([]);
          ctx.fillStyle = '#f5c451';
          const ax = px + dir * R * 2.35;
          ctx.beginPath(); ctx.moveTo(ax + dir * 7, py); ctx.lineTo(ax, py - 5); ctx.lineTo(ax, py + 5); ctx.fill();
        }
        // hail: comm rings
        const hailing = animating ? !!stt?.hailed && t < 0.75 : false;
        if (hailing) {
          for (let i = 0; i < 3; i++) {
            const q = ((t * 2.2 + i / 3) % 1);
            ctx.strokeStyle = `rgba(160,220,255,${0.6 * (1 - q)})`; ctx.lineWidth = 1.4;
            ctx.beginPath(); ctx.arc(px, py, R * (1.3 + q * 2.2), -0.9, 0.9); ctx.stroke();
            ctx.beginPath(); ctx.arc(px, py, R * (1.3 + q * 2.2), Math.PI - 0.9, Math.PI + 0.9); ctx.stroke();
          }
        }
        // decloak shimmer burst
        if (animating && stt?.decloaked && t > 0.36 && t < 0.7) {
          const q = (t - 0.36) / 0.34;
          ctx.strokeStyle = `rgba(190,235,255,${0.8 * (1 - q)})`; ctx.lineWidth = 2;
          ctx.beginPath(); ctx.arc(px, py, R * (1 + q * 1.6), 0, 6.283); ctx.stroke();
        }
        // explosion
        if (destroyedNow && t > 0.8) {
          const q = clamp01((t - 0.8) / 0.2);
          const fire = ctx.createRadialGradient(px, py, 0, px, py, R * (1 + q * 3));
          fire.addColorStop(0, `rgba(255,245,200,${1 - q})`); fire.addColorStop(0.4, `rgba(255,150,60,${0.8 * (1 - q)})`); fire.addColorStop(1, 'rgba(255,60,30,0)');
          ctx.fillStyle = fire; ctx.beginPath(); ctx.arc(px, py, R * (1 + q * 3), 0, 6.283); ctx.fill();
          ctx.fillStyle = `rgba(255,200,120,${1 - q})`;
          for (let i = 0; i < 10; i++) {
            const an = (i / 10) * 6.283 + s.id;
            const d = R * (0.8 + q * 3.2) * (0.6 + ((i * 37 + s.id) % 10) / 20);
            ctx.fillRect(px + Math.cos(an) * d, py + Math.sin(an) * d, 2, 2);
          }
        }

        // labels: name, then state / next order chip
        const showLabel = labelsOn || s.id === sel || s.id === tgt || s.id === st.hover;
        if (showLabel && alpha > 0.2) {
          const ly = py + R * 1.65 + 3;
          ctx.font = '600 10.5px "Antonio Variable", Antonio, "Arial Narrow", sans-serif';
          ctx.textAlign = 'center'; ctx.textBaseline = 'top';
          const name = s.hidden ? 'CLOAKED CONTACT' : s.name.split(' — ')[0]!.toUpperCase();
          ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(3,5,11,0.85)';
          ctx.strokeText(name, px, ly);
          ctx.fillStyle = s.mine ? '#d6e4ff' : '#ffd0cb';
          ctx.fillText(name, px, ly);
          let chip = '';
          let chipColor = 'rgba(200,215,245,0.85)';
          if (s.station && !disengaging && !(st.intent && !animating && !e.finished && s.next)) { chip = `${(s.stationType ?? 'station').toUpperCase()} · IMMOBILE`; chipColor = '#cfe0ff'; }
          if (disengaging) { chip = 'DISENGAGING'; chipColor = '#f5c451'; }
          else if (cloaked && !s.mine) { chip = 'CLOAKED'; chipColor = '#bfe6ff'; }
          else if (st.intent && !animating && !e.finished && s.next) {
            chip = `${s.next.source === 'ai' ? 'AI · ' : s.next.source === 'standing' ? '↻ ' : ''}${MANEUVER_LABEL[s.next.maneuver] ?? s.next.maneuver}`;
            chipColor = s.next.source === 'ai' ? 'rgba(200,215,245,0.7)' : pal.primary;
          } else if (cloaked) { chip = 'CLOAKED'; chipColor = '#bfe6ff'; }
          if (chip) {
            ctx.font = '500 9.5px "Share Tech Mono", monospace';
            const cw = ctx.measureText(chip).width + 10;
            ctx.fillStyle = 'rgba(4,7,14,0.82)';
            ctx.beginPath(); ctx.roundRect?.(px - cw / 2, ly + 13, cw, 13, 6); ctx.fill();
            ctx.strokeStyle = chipColor; ctx.lineWidth = 0.8; ctx.stroke();
            ctx.fillStyle = chipColor; ctx.fillText(chip, px, ly + 15);
          }
        }
        if (retreatingNow && t > 0.2 && t < 0.9) floatTexts.push({ x: p[0], y: py - R * 2, text: 'WITHDRAWN', color: '#f5c451', q: clamp01((t - 0.2) / 0.7) });
        if (hailing) floatTexts.push({ x: px, y: py - R * 2.1, text: 'HAILING', color: '#a0dcff', q: clamp01(t / 0.75) });
        hits.push({ id: s.id, x: px, y: py, r: Math.max(16, R * 1.6) });
      }

      // --- weapons fire, salvo by salvo
      if (roundData) {
        salvos.forEach((sv, i) => {
          const [s0, s1] = salvoWindow(i);
          if (t < s0 || t > s1 + 0.12) return;
          const q = clamp01((t - s0) / (s1 - s0));
          const a = at(sv.from), b = at(sv.to);
          if (!a || !b) return;
          const shooter = e.ships.find((s) => s.id === sv.from);
          const col = shooter ? palette(shooter.owner).primary : '#ffffff';
          const tgtStatus = statusIn(roundData, sv.to);
          const shielded = (tgtStatus?.shieldBefore ?? 0) > 0;
          if (sv.ram) {
            // Mutual ram: the impact is drawn once, when the rammer arrives.
            if (sv.from !== roundData.ships.find((x) => x.id === sv.from && x.maneuver === 'Ram')?.id) {
              if (q > 0.55) floatTexts.push({ x: b[0], y: b[1] - baseR * 1.6, text: `−${sv.beam} RECOIL`, color: '#ffb36b', q });
              return;
            }
            if (q > 0.5) {
              const f = clamp01((q - 0.5) / 0.5);
              const burst = ctx.createRadialGradient(b[0], b[1], 0, b[0], b[1], baseR * (1.5 + f * 2.5));
              burst.addColorStop(0, `rgba(255,255,230,${1 - f})`); burst.addColorStop(0.5, `rgba(255,170,80,${0.7 * (1 - f)})`); burst.addColorStop(1, 'rgba(255,80,40,0)');
              ctx.fillStyle = burst; ctx.beginPath(); ctx.arc(b[0], b[1], baseR * (1.5 + f * 2.5), 0, 6.283); ctx.fill();
              floatTexts.push({ x: b[0], y: b[1] - baseR * 2.3, text: `RAM −${sv.beam}`, color: '#ff7a5c', q: f });
            }
            return;
          }
          if (sv.ambush && q < 0.6) floatTexts.push({ x: a[0], y: a[1] - baseR * 2.2, text: 'AMBUSH +25%', color: '#bfe6ff', q: q / 0.6 });
          if (sv.beam > 0) {
            const flick = motion ? 0.75 + 0.25 * Math.sin(now / 25 + i) : 1;
            const fade = q < 0.15 ? q / 0.15 : q > 0.7 ? Math.max(0, 1 - (q - 0.7) / 0.3) : 1;
            ctx.save();
            ctx.lineCap = 'round';
            ctx.strokeStyle = rgba(col.startsWith('#') ? col : '#ffffff', 0.25 * fade); ctx.lineWidth = 7;
            ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
            ctx.globalAlpha = fade * flick;
            ctx.strokeStyle = col; ctx.lineWidth = 2.4;
            ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
            ctx.strokeStyle = '#ffffff'; ctx.lineWidth = 0.9;
            ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke();
            ctx.restore();
          }
          if (sv.torpedo > 0) {
            // Tracer on a slight arc with a fading tail.
            const tq = clamp01(q / 0.75);
            const mx = (a[0] + b[0]) / 2 + (b[1] - a[1]) * 0.18, my = (a[1] + b[1]) / 2 - (b[0] - a[0]) * 0.18;
            const pt = (u: number): [number, number] => [(1 - u) * (1 - u) * a[0] + 2 * (1 - u) * u * mx + u * u * b[0], (1 - u) * (1 - u) * a[1] + 2 * (1 - u) * u * my + u * u * b[1]];
            if (tq < 1) {
              for (let j = 0; j < 8; j++) {
                const u = tq - j * 0.025;
                if (u < 0) break;
                const [x, y] = pt(u);
                ctx.fillStyle = `rgba(255,${200 - j * 12},${120 - j * 10},${0.9 - j * 0.11})`;
                ctx.beginPath(); ctx.arc(x, y, 3.2 - j * 0.3, 0, 6.283); ctx.fill();
              }
            }
          }
          // impact: shield bubble or hull sparks, and the damage number
          if (q > 0.62) {
            const f = clamp01((q - 0.62) / 0.38);
            const tr = baseR * 1.7;
            if (shielded) {
              const ang = Math.atan2(a[1] - b[1], a[0] - b[0]);
              ctx.strokeStyle = `rgba(111,211,255,${0.9 * (1 - f)})`; ctx.lineWidth = 3;
              ctx.beginPath(); ctx.arc(b[0], b[1], tr, ang - 0.9, ang + 0.9); ctx.stroke();
            } else {
              ctx.fillStyle = `rgba(255,190,110,${1 - f})`;
              for (let j = 0; j < 6; j++) { const an = j * 1.047 + i; ctx.fillRect(b[0] + Math.cos(an) * f * tr, b[1] + Math.sin(an) * f * tr, 2, 2); }
            }
            floatTexts.push({ x: b[0], y: b[1] - baseR * 1.9, text: `−${sv.beam + sv.torpedo}`, color: shielded ? '#8fdcff' : '#ffb36b', q: f });
          }
        });
        if (roundData.truce && t > 0.15) {
          const q = clamp01((t - 0.15) / 0.3);
          ctx.fillStyle = `rgba(4,8,18,${0.55 * q})`; ctx.fillRect(0, h / 2 - 26, w, 52);
          ctx.font = '600 20px "Antonio Variable", Antonio, sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
          ctx.fillStyle = `rgba(160,220,255,${q})`;
          ctx.fillText('TRUCE — BOTH SIDES STAND DOWN', w / 2, h / 2);
        }
      } else if (lastRound && !motion) {
        // Reduced motion: the last round's fire as static lines with damage figures.
        for (const sv of lastRound.salvos) {
          if (!(sv.beam + sv.torpedo > 0 || sv.ram)) continue;
          const a = at(sv.from), b = at(sv.to);
          if (!a || !b) continue;
          const shooter = e.ships.find((s) => s.id === sv.from);
          ctx.strokeStyle = rgba(shooter ? palette(shooter.owner).map : '#ffffff', 0.35); ctx.lineWidth = 1.2;
          ctx.setLineDash(sv.ram ? [2, 2] : []);
          ctx.beginPath(); ctx.moveTo(a[0], a[1]); ctx.lineTo(b[0], b[1]); ctx.stroke(); ctx.setLineDash([]);
        }
      }

      // floating texts on top
      ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      for (const ft of floatTexts) {
        ctx.globalAlpha = Math.max(0, 1 - ft.q * 0.9);
        ctx.font = '700 12px "Antonio Variable", Antonio, sans-serif';
        ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(3,5,11,0.9)';
        ctx.strokeText(ft.text, ft.x, ft.y - ft.q * 14);
        ctx.fillStyle = ft.color;
        ctx.fillText(ft.text, ft.x, ft.y - ft.q * 14);
      }
      ctx.globalAlpha = 1;
    };
    frame = requestAnimationFrame(draw);
    const local = (ev: MouseEvent): [number, number] => {
      const r = canvas.getBoundingClientRect();
      // Screen pixels -> canvas CSS pixels (the interface may be zoomed).
      const sx = canvas.clientWidth ? r.width / canvas.clientWidth : 1, sy = canvas.clientHeight ? r.height / canvas.clientHeight : 1;
      return [(ev.clientX - r.left) / sx, (ev.clientY - r.top) / sy];
    };
    const pick = (x: number, y: number) => {
      let best: { id: number; d: number } | null = null;
      for (const h of hits) { const d = (h.x - x) ** 2 + (h.y - y) ** 2; if (d < h.r * h.r * 1.3 && (!best || d < best.d)) best = { id: h.id, d }; }
      return best?.id ?? null;
    };
    const click = (ev: MouseEvent) => { const [x, y] = local(ev); const id = pick(x, y); if (id !== null) onPickRef.current(id); };
    const move = (ev: MouseEvent) => {
      const [x, y] = local(ev);
      const id = pick(x, y);
      state.current.hover = id;
      canvas.style.cursor = id !== null ? 'pointer' : 'default';
    };
    const leave = () => { state.current.hover = null; };
    canvas.addEventListener('click', click);
    canvas.addEventListener('pointermove', move);
    canvas.addEventListener('pointerleave', leave);
    return () => {
      cancelAnimationFrame(frame);
      canvas.removeEventListener('click', click);
      canvas.removeEventListener('pointermove', move);
      canvas.removeEventListener('pointerleave', leave);
    };
  }, []);
  return <canvas class="combat-arena" ref={ref} aria-label="Tactical arena" data-testid="combat-arena" />;
}

/** Plain-language log of one resolved round (also used by replays). */
export function roundLog(e: Encounter, r: TacticalRound): { text: string; kind: string }[] {
  const ship = (id: number) => e.ships.find((x) => x.id === id);
  const name = (id: number) => { const s = ship(id); return s ? `${s.hidden ? 'Cloaked contact' : s.name.split(' — ')[0]} #${id}` : `#${id}`; };
  const lines: { text: string; kind: string }[] = [];
  if (r.truce) lines.push({ text: 'Every ship on one side hailed and the other side agreed: a truce ends the engagement before any fire.', kind: 'truce' });
  for (const st of r.ships) {
    if (st.hailed) lines.push({ text: `${name(st.id)} hails the enemy${st.ai ? ' (AI)' : ''}.`, kind: 'hail' });
    if (st.decloaked) {
      const fired = r.salvos.some((sv) => sv.from === st.id && sv.ambush && sv.beam + sv.torpedo > 0);
      lines.push({ text: `${name(st.id)} decloaks to attack${fired ? ' — its volley strikes with +25% ambush damage.' : ', but no target is in range: the ambush volley is lost.'}`, kind: 'cloak' });
    }
    if (st.disengaging) lines.push({ text: `${name(st.id)} is disengaging: weapons offline, −25% incoming; it escapes next round if still retreating.`, kind: 'retreat' });
  }
  const seenRam = new Set<number>();
  for (const sv of r.salvos) {
    if (sv.ram) {
      const rammer = r.ships.find((x) => x.id === sv.from && x.maneuver === 'Ram');
      if (rammer && !seenRam.has(sv.from)) {
        seenRam.add(sv.from);
        const recoil = r.salvos.find((x) => x.ram && x.from === sv.to && x.to === sv.from);
        lines.push({ text: `${name(sv.from)} rams ${name(sv.to)}: ${sv.beam} impact damage, ${recoil?.beam ?? 0} recoil to the rammer.`, kind: 'ram' });
      }
      continue;
    }
    if (sv.beam + sv.torpedo <= 0) continue;
    const st = r.ships.find((x) => x.id === sv.from);
    lines.push({
      text: `${name(sv.from)}${st ? ` — ${st.maneuver}${st.ai ? ' (AI)' : ''}` : ''}: ${sv.beam} beam + ${sv.torpedo} torpedo at ${name(sv.to)}${sv.ambush ? ' (ambush)' : ''}`,
      kind: sv.ambush ? 'cloak' : 'fire',
    });
  }
  for (const id of r.lost) lines.push({ text: `${name(id)} destroyed.`, kind: 'lost' });
  for (const id of r.retreatedIds) lines.push({ text: `${name(id)} withdrew from the battle.`, kind: 'retreat' });
  if (r.ended && !r.truce) lines.push({ text: r.winner ? `Battle over: ${e.ships.find((s) => s.owner === r.winner)?.ownerName ?? 'victor'} hold the field.` : 'Battle over: no victor.', kind: 'end' });
  if (!lines.length) lines.push({ text: 'Ships manoeuvre for position.', kind: 'move' });
  return lines;
}

/** Salvo helper for tests/UI: the ram pair of a round, if any. */
export function ramSalvos(r: TacticalRound): Salvo[] { return r.salvos.filter((s) => s.ram); }
