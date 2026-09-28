// Canvas 2D galaxy renderer: nebula + parallax starfield, fog of war, territory, supply range,
// routes, systems and fleet markers. Pure presentation — all numbers come from the view JSON.
import type { ContactView, FleetView, GameView, SystemView } from '../core/types';
import { colorblindPaletteActive, EMBLEMS, palette } from '../ui/theme';

export const CELL = 64;
// Unknown space is veiled, but lightly enough that the nebula fields read through; the
// "uncharted" survey texture, faint grid crosses and the sensor horizon line carry the meaning.
const FOG_UNKNOWN_ALPHA = 150;

/** Bounding box (in cells) of every surveyed or sensed sector, or null when nothing is known. */
export function knownBounds(view: GameView): { x0: number; y0: number; x1: number; y1: number } | null {
  const W = view.meta.width, H = view.meta.height;
  let x0 = W, y0 = H, x1 = -1, y1 = -1;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    if (view.fog[y * W + x] === '0') continue;
    x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y);
  }
  return x1 < 0 ? null : { x0, y0, x1, y1 };
}

export interface Camera { cx: number; cy: number; zoom: number }
export interface HitTarget { kind: 'fleet' | 'contact'; id: number; x: number; y: number; r: number; cell: number }
export interface Overlay {
  selectedCell: number | null;
  selectedFleetId: number | null;
  selectedTaskForceId: number | null;
  hoverCell: number | null;
  supply: string | null;                 // '0'/'1' grid for the selected unit
  preview: { cells: number[]; eta: number; ok: boolean; cell: number } | null;
  layers: { territory: boolean; supply: boolean; routes: boolean; grid: boolean; labels: boolean; trade: boolean };
  reduceMotion: boolean;
  accent: string;
}

interface Star { x: number; y: number; r: number; a: number; phase: number; tint: string }

function mulberry(seed: number): () => number {
  let t = seed >>> 0;
  return () => {
    t += 0x6d2b79f5;
    let r = Math.imul(t ^ (t >>> 15), 1 | t);
    r ^= r + Math.imul(r ^ (r >>> 7), 61 | r);
    return ((r ^ (r >>> 14)) >>> 0) / 4294967296;
  };
}
function hash(n: number): number {
  let h = (n * 2654435761) >>> 0;
  h ^= h >>> 16; h = Math.imul(h, 0x45d9f3b) >>> 0; h ^= h >>> 16;
  return h >>> 0;
}
function hexToRgb(hex: string): [number, number, number] {
  const v = parseInt(hex.slice(1), 16);
  return [(v >> 16) & 255, (v >> 8) & 255, v & 255];
}
function rgba(hex: string, a: number): string {
  const [r, g, b] = hexToRgb(hex);
  return `rgba(${r},${g},${b},${a})`;
}

const STAR_TYPES = [
  { core: '#fff4d6', glow: '#ffcf6b' },   // G
  { core: '#ffe2c0', glow: '#ff9d4d' },   // K
  { core: '#ffd0c4', glow: '#ff6a55' },   // M
  { core: '#f4f7ff', glow: '#bcd2ff' },   // F
  { core: '#e8f1ff', glow: '#7fb0ff' },   // A/B
];

const emblemPaths: Record<number, Path2D> = {};
function emblemPath(empire: number): Path2D {
  return (emblemPaths[empire] ??= new Path2D(EMBLEMS[empire] ?? EMBLEMS[0]));
}

export class GalaxyRenderer {
  private ctx: CanvasRenderingContext2D;
  private width = 0;
  private height = 0;
  private dpr = 1;
  private starLayers: Star[][] = [];
  private nebula: HTMLCanvasElement | null = null;
  private nebulaKey = '';
  private fogCanvas: HTMLCanvasElement | null = null;
  private territoryCanvas: HTMLCanvasElement | null = null;
  private layerKey = '';
  private territoryOwner: Int8Array = new Int8Array(0);
  private unknown: Uint8Array = new Uint8Array(0);
  private frontier: [number, number, number, number][] = [];   // world-space segments of the sensor horizon
  private uncharted: { x: number; y: number }[] = [];            // cells deep in unknown space (labels)
  private surveyPattern: CanvasPattern | null = null;
  hits: HitTarget[] = [];

  constructor(private canvas: HTMLCanvasElement) {
    const ctx = canvas.getContext('2d');
    if (!ctx) throw new Error('Canvas 2D is not available');
    this.ctx = ctx;
    const rand = mulberry(1999);
    const tints = ['#ffffff', '#cfe0ff', '#ffe9c9', '#d7fff4', '#ffd6e6'];
    for (let layer = 0; layer < 3; layer++) {
      const stars: Star[] = [];
      const count = [520, 260, 110][layer]!;
      for (let i = 0; i < count; i++) {
        stars.push({
          x: rand() * 4096, y: rand() * 4096,
          r: [0.5, 0.85, 1.3][layer]! * (0.6 + rand() * 0.8),
          a: [0.35, 0.55, 0.8][layer]! * (0.5 + rand() * 0.5),
          phase: rand() * Math.PI * 2,
          tint: tints[Math.floor(rand() * tints.length)]!,
        });
      }
      this.starLayers.push(stars);
    }
  }

  resize(width: number, height: number, dpr: number): void {
    this.width = width; this.height = height; this.dpr = dpr;
    this.canvas.width = Math.max(1, Math.round(width * dpr));
    this.canvas.height = Math.max(1, Math.round(height * dpr));
  }

  get size(): { width: number; height: number } { return { width: this.width, height: this.height }; }

  worldToScreen(cam: Camera, wx: number, wy: number): [number, number] {
    return [(wx - cam.cx) * cam.zoom + this.width / 2, (wy - cam.cy) * cam.zoom + this.height / 2];
  }
  screenToWorld(cam: Camera, sx: number, sy: number): [number, number] {
    return [(sx - this.width / 2) / cam.zoom + cam.cx, (sy - this.height / 2) / cam.zoom + cam.cy];
  }
  cellAt(cam: Camera, view: GameView, sx: number, sy: number): number | null {
    const [wx, wy] = this.screenToWorld(cam, sx, sy);
    const x = Math.floor(wx / CELL), y = Math.floor(wy / CELL);
    if (x < 0 || y < 0 || x >= view.meta.width || y >= view.meta.height) return null;
    return y * view.meta.width + x;
  }

  private ensureNebula(view: GameView): void {
    const key = `${view.meta.seed}:${view.meta.width}x${view.meta.height}`;
    if (this.nebulaKey === key && this.nebula) return;
    this.nebulaKey = key;
    const W = view.meta.width * CELL, H = view.meta.height * CELL;
    const scale = 0.25;
    const c = document.createElement('canvas');
    c.width = Math.ceil((W + CELL * 12) * scale); c.height = Math.ceil((H + CELL * 12) * scale);
    const g = c.getContext('2d')!;
    const rand = mulberry(Number(String(view.meta.seed).slice(-9)) || 7);
    const hues = ['#3b2a78', '#1f4f8f', '#12605f', '#6a2657', '#2b3f8a', '#48307a'];
    for (let i = 0; i < 26; i++) {
      const x = rand() * c.width, y = rand() * c.height;
      const r = (0.12 + rand() * 0.35) * Math.max(c.width, c.height);
      const hue = hues[Math.floor(rand() * hues.length)]!;
      const grad = g.createRadialGradient(x, y, 0, x, y, r);
      grad.addColorStop(0, rgba(hue, 0.20 + rand() * 0.14));
      grad.addColorStop(0.5, rgba(hue, 0.06));
      grad.addColorStop(1, rgba(hue, 0));
      g.fillStyle = grad;
      g.fillRect(0, 0, c.width, c.height);
    }
    // Filaments
    g.globalCompositeOperation = 'lighter';
    for (let i = 0; i < 14; i++) {
      const hue = hues[Math.floor(rand() * hues.length)]!;
      g.strokeStyle = rgba(hue, 0.05);
      g.lineWidth = 12 + rand() * 30;
      g.beginPath();
      let x = rand() * c.width, y = rand() * c.height;
      g.moveTo(x, y);
      for (let k = 0; k < 4; k++) {
        const nx = x + (rand() - 0.5) * c.width * 0.6, ny = y + (rand() - 0.5) * c.height * 0.6;
        g.quadraticCurveTo((x + nx) / 2 + (rand() - 0.5) * 200, (y + ny) / 2 + (rand() - 0.5) * 200, nx, ny);
        x = nx; y = ny;
      }
      g.stroke();
    }
    this.nebula = c;
  }

  /** Fine diagonal "survey" hatching with sparse specks: the texture of uncharted space. */
  private ensureSurveyPattern(): CanvasPattern | null {
    if (this.surveyPattern) return this.surveyPattern;
    const c = document.createElement('canvas');
    c.width = c.height = 96;
    const g = c.getContext('2d')!;
    g.strokeStyle = 'rgba(150,175,230,0.10)';
    g.lineWidth = 1;
    for (let i = -96; i < 192; i += 12) { g.beginPath(); g.moveTo(i, 0); g.lineTo(i + 96, 96); g.stroke(); }
    const rand = mulberry(42);
    for (let i = 0; i < 26; i++) {
      g.fillStyle = `rgba(190,205,240,${0.08 + rand() * 0.16})`;
      g.fillRect(Math.floor(rand() * 96), Math.floor(rand() * 96), 1, 1);
    }
    this.surveyPattern = this.ctx.createPattern(c, 'repeat');
    return this.surveyPattern;
  }

  private ensureLayers(view: GameView): void {
    const key = `${view.meta.version}:${view.fog}:${colorblindPaletteActive() ? 'cb' : 'std'}`;
    if (key === this.layerKey && this.fogCanvas && this.territoryCanvas) return;
    this.layerKey = key;
    const { width: W, height: H } = view.meta;
    // Unknown cells, the sensor horizon between charted and uncharted space, and label anchors
    // placed deepest inside large uncharted regions.
    const unknown = new Uint8Array(W * H);
    for (let i = 0; i < W * H; i++) unknown[i] = view.fog[i] === '0' ? 1 : 0;
    this.unknown = unknown;
    const frontier: [number, number, number, number][] = [];
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (unknown[y * W + x]) continue;
      if (x > 0 && unknown[y * W + x - 1]) frontier.push([x * CELL, y * CELL, x * CELL, (y + 1) * CELL]);
      if (x < W - 1 && unknown[y * W + x + 1]) frontier.push([(x + 1) * CELL, y * CELL, (x + 1) * CELL, (y + 1) * CELL]);
      if (y > 0 && unknown[(y - 1) * W + x]) frontier.push([x * CELL, y * CELL, (x + 1) * CELL, y * CELL]);
      if (y < H - 1 && unknown[(y + 1) * W + x]) frontier.push([x * CELL, (y + 1) * CELL, (x + 1) * CELL, (y + 1) * CELL]);
    }
    this.frontier = frontier;
    const dist = new Int16Array(W * H).fill(999);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (!unknown[y * W + x]) continue;
      let best = 999;
      for (let yy = 0; yy < H; yy++) for (let xx = 0; xx < W; xx++) {
        if (unknown[yy * W + xx]) continue;
        best = Math.min(best, Math.max(Math.abs(xx - x), Math.abs(yy - y)));
      }
      dist[y * W + x] = best;
    }
    const candidates: { x: number; y: number; d: number }[] = [];
    for (let y = 1; y < H - 1; y++) for (let x = 1; x < W - 1; x++) if (unknown[y * W + x] && dist[y * W + x]! >= 3) candidates.push({ x, y, d: dist[y * W + x]! });
    candidates.sort((a, b) => b.d - a.d || a.y - b.y || a.x - b.x);
    const labels: { x: number; y: number }[] = [];
    for (const c of candidates) {
      if (labels.length >= 4) break;
      if (labels.every((l) => Math.max(Math.abs(l.x - c.x), Math.abs(l.y - c.y)) >= 5)) labels.push({ x: c.x, y: c.y });
    }
    this.uncharted = labels;
    // Fog: one pixel per sector, alpha by knowledge; smoothed when scaled → soft edges.
    const fog = this.fogCanvas ?? document.createElement('canvas');
    fog.width = W + 2; fog.height = H + 2;
    const fg = fog.getContext('2d')!;
    const img = fg.createImageData(W + 2, H + 2);
    for (let y = -1; y <= H; y++) for (let x = -1; x <= W; x++) {
      const inside = x >= 0 && y >= 0 && x < W && y < H;
      const state = inside ? view.fog[y * W + x] : '0';
      const alpha = state === '2' ? 0 : state === '1' ? 78 : FOG_UNKNOWN_ALPHA;
      const i = ((y + 1) * (W + 2) + (x + 1)) * 4;
      img.data[i] = 3; img.data[i + 1] = 6; img.data[i + 2] = 14; img.data[i + 3] = alpha;
    }
    fg.putImageData(img, 0, 0);
    this.fogCanvas = fog;

    // Territory: influence of known colonies (sensor radius 1 strong, 2 faint).
    const owner = new Int8Array(W * H);
    const strength = new Float32Array(W * H);
    for (const s of view.systems) {
      if (!s.owner) continue;
      for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++) {
        const x = s.x + dx, y = s.y + dy;
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        const d = Math.max(Math.abs(dx), Math.abs(dy));
        const v = d <= 1 ? 1 : 0.4;
        const i = y * W + x;
        if (v > strength[i]!) { strength[i] = v; owner[i] = s.owner; }
        else if (v === strength[i] && owner[i] !== s.owner) { if (d <= 1) owner[i] = -1; }
      }
    }
    this.territoryOwner = owner;
    const terr = this.territoryCanvas ?? document.createElement('canvas');
    terr.width = W + 2; terr.height = H + 2;
    const tg = terr.getContext('2d')!;
    const timg = tg.createImageData(W + 2, H + 2);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const o = owner[y * W + x]!;
      if (o <= 0) continue;
      const [r, g, b] = hexToRgb(palette(o).map);
      const i = ((y + 1) * (W + 2) + (x + 1)) * 4;
      timg.data[i] = r; timg.data[i + 1] = g; timg.data[i + 2] = b;
      timg.data[i + 3] = Math.round(255 * (strength[y * W + x]! >= 1 ? 0.26 : 0.1));
    }
    tg.putImageData(timg, 0, 0);
    this.territoryCanvas = terr;
  }

  draw(view: GameView, cam: Camera, overlay: Overlay, time: number): void {
    const { ctx } = this;
    const { width: W, height: H } = view.meta;
    this.ensureNebula(view);
    this.ensureLayers(view);
    ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
    ctx.clearRect(0, 0, this.width, this.height);

    // --- deep space backdrop
    const bg = ctx.createLinearGradient(0, 0, this.width, this.height);
    bg.addColorStop(0, '#03050b'); bg.addColorStop(1, '#070a16');
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, this.width, this.height);

    // --- parallax starfield (three depths)
    const t = overlay.reduceMotion ? 0 : time / 1000;
    const factors = [0.12, 0.25, 0.45];
    this.starLayers.forEach((stars, layer) => {
      const f = factors[layer]!;
      const ox = -cam.cx * cam.zoom * f, oy = -cam.cy * cam.zoom * f;
      for (const s of stars) {
        let x = (s.x + ox) % 4096, y = (s.y + oy) % 4096;
        if (x < 0) x += 4096; if (y < 0) y += 4096;
        if (x > this.width + 4 || y > this.height + 4) continue;
        const tw = layer === 0 ? 1 : 0.75 + 0.25 * Math.sin(t * (1.2 + layer) + s.phase);
        ctx.globalAlpha = s.a * tw;
        ctx.fillStyle = s.tint;
        ctx.beginPath();
        ctx.arc(x, y, s.r, 0, Math.PI * 2);
        ctx.fill();
      }
    });
    ctx.globalAlpha = 1;

    // --- world transform
    const [ox, oy] = this.worldToScreen(cam, 0, 0);
    const z = cam.zoom;
    const worldW = W * CELL, worldH = H * CELL;

    // nebula (slight parallax to feel deep)
    if (this.nebula) {
      const n = this.nebula;
      const px = cam.cx * z * 0.18, py = cam.cy * z * 0.18;
      ctx.globalAlpha = 0.95;
      ctx.drawImage(n, ox - 6 * CELL * z + px * 0.2, oy - 6 * CELL * z + py * 0.2, n.width / 0.25 * z, n.height / 0.25 * z);
      ctx.globalAlpha = 1;
    }

    // galaxy bounds plate
    ctx.fillStyle = 'rgba(8,14,32,0.28)';
    ctx.fillRect(ox, oy, worldW * z, worldH * z);

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    if (overlay.layers.territory && this.territoryCanvas) {
      ctx.drawImage(this.territoryCanvas, ox - CELL * z, oy - CELL * z, (W + 2) * CELL * z, (H + 2) * CELL * z);
      this.drawBorders(view, cam);
    }

    // grid
    if (overlay.layers.grid) {
      ctx.strokeStyle = 'rgba(120,150,210,0.075)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let x = 0; x <= W; x++) { const sx = Math.round(ox + x * CELL * z) + 0.5; ctx.moveTo(sx, oy); ctx.lineTo(sx, oy + worldH * z); }
      for (let y = 0; y <= H; y++) { const sy = Math.round(oy + y * CELL * z) + 0.5; ctx.moveTo(ox, sy); ctx.lineTo(ox + worldW * z, sy); }
      ctx.stroke();
      if (z > 1.1) {
        ctx.fillStyle = 'rgba(150,170,210,0.35)';
        ctx.font = `500 ${Math.min(11, 8 * z)}px "Share Tech Mono", monospace`;
        ctx.textAlign = 'left'; ctx.textBaseline = 'top';
        for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
          if ((x + y) % 2) continue;
          const [sx, sy] = this.worldToScreen(cam, x * CELL + 3, y * CELL + 3);
          if (sx < -40 || sy < -20 || sx > this.width || sy > this.height) continue;
          ctx.fillText(`${x}.${y}`, sx, sy);
        }
      }
    }

    // supply range of the selected unit
    if (overlay.layers.supply && overlay.supply) this.drawSupply(view, cam, overlay.supply, overlay.accent);

    // trade routes
    if (overlay.layers.trade) {
      for (const r of view.tradeRoutes) {
        const a = view.systems.find((s) => s.id === r.from), b = view.systems.find((s) => s.id === r.to);
        if (!a || !b) continue;
        const [x1, y1] = this.worldToScreen(cam, (a.x + 0.5) * CELL, (a.y + 0.5) * CELL);
        const [x2, y2] = this.worldToScreen(cam, (b.x + 0.5) * CELL, (b.y + 0.5) * CELL);
        ctx.strokeStyle = rgba(palette(r.owner).secondary, 0.55);
        ctx.setLineDash([2, 6]); ctx.lineDashOffset = -t * 12;
        ctx.lineWidth = 2;
        ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
        ctx.setLineDash([]);
      }
    }

    // fog of war (soft): the sector grid, plus the same veil over the space around the galaxy
    if (this.fogCanvas) {
      const fx = ox - CELL * z, fy = oy - CELL * z, fw = (W + 2) * CELL * z, fh = (H + 2) * CELL * z;
      ctx.fillStyle = `rgba(3,6,14,${FOG_UNKNOWN_ALPHA / 255})`;
      ctx.beginPath();
      ctx.rect(0, 0, this.width, this.height);
      ctx.rect(fx, fy, fw, fh);
      ctx.fill('evenodd');
      ctx.drawImage(this.fogCanvas, fx, fy, fw, fh);
      this.drawUncharted(view, cam, overlay, t);
    }

    // routes of own units
    if (overlay.layers.routes) {
      const drawnTf = new Set<number>();
      for (const f of view.fleets) {
        if (!f.route.length) continue;
        if (f.taskForceId) { if (drawnTf.has(f.taskForceId)) continue; drawnTf.add(f.taskForceId); }
        const selected = f.id === overlay.selectedFleetId || (!!f.taskForceId && f.taskForceId === overlay.selectedTaskForceId);
        this.drawRoute(view, cam, f.route, f.eta, selected ? overlay.accent : rgba('#cfe0ff', 0.55), selected, t, false);
      }
    }
    if (overlay.preview && overlay.preview.cells.length > 1) {
      this.drawRoute(view, cam, overlay.preview.cells, overlay.preview.eta, overlay.accent, true, t, true);
    }

    // hover & selection
    if (overlay.hoverCell !== null) this.drawCellHighlight(view, cam, overlay.hoverCell, 'rgba(200,220,255,0.10)', null);
    if (overlay.preview && !overlay.preview.ok && overlay.hoverCell !== null) this.drawBlocked(view, cam, overlay.hoverCell);

    // systems
    for (const s of view.systems) this.drawSystem(view, cam, s, overlay, t);

    // fleets
    this.hits = [];
    this.drawFleets(view, cam, overlay, t);

    if (overlay.selectedCell !== null) this.drawSelection(view, cam, overlay.selectedCell, overlay.accent, t);

    // outer frame
    ctx.strokeStyle = 'rgba(140,170,230,0.22)';
    ctx.lineWidth = 1;
    ctx.strokeRect(Math.round(ox) + 0.5, Math.round(oy) + 0.5, Math.round(worldW * z), Math.round(worldH * z));
  }

  /** Uncharted space: survey hatching, faint grid crosses, "uncharted" labels and the sensor horizon. */
  private drawUncharted(view: GameView, cam: Camera, overlay: Overlay, t: number): void {
    const { ctx } = this;
    const { width: W, height: H } = view.meta;
    const z = cam.zoom, s = CELL * z;
    const [ox, oy] = this.worldToScreen(cam, 0, 0);
    const unknownPath = new Path2D();
    let any = false;
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (!this.unknown[y * W + x]) continue;
      const sx = ox + x * s, sy = oy + y * s;
      if (sx > this.width || sy > this.height || sx + s < 0 || sy + s < 0) continue;
      unknownPath.rect(sx, sy, s + 0.5, s + 0.5);
      any = true;
    }
    if (any) {
      ctx.save();
      ctx.clip(unknownPath);
      const pattern = this.ensureSurveyPattern();
      if (pattern) {
        // Anchor the texture to the chart so it pans and zooms with the sectors.
        pattern.setTransform(new DOMMatrix().translateSelf(ox, oy).scaleSelf(Math.max(0.5, z * 0.75)));
        ctx.fillStyle = pattern;
        ctx.fillRect(0, 0, this.width, this.height);
      }
      if (overlay.layers.grid) {
        ctx.strokeStyle = 'rgba(160,185,235,0.20)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        const arm = Math.max(2, Math.min(5, 3.5 * z));
        for (let y = 0; y <= H; y++) for (let x = 0; x <= W; x++) {
          const sx = Math.round(ox + x * s) + 0.5, sy = Math.round(oy + y * s) + 0.5;
          if (sx < -8 || sy < -8 || sx > this.width + 8 || sy > this.height + 8) continue;
          ctx.moveTo(sx - arm, sy); ctx.lineTo(sx + arm, sy); ctx.moveTo(sx, sy - arm); ctx.lineTo(sx, sy + arm);
        }
        ctx.stroke();
      }
      ctx.restore();
      if (z > 0.5) {
        ctx.save();
        const size = Math.round(Math.max(10, Math.min(15, 11 * z)));
        ctx.font = `600 ${size}px "Antonio Variable", Antonio, "Arial Narrow", sans-serif`;
        (ctx as CanvasRenderingContext2D & { letterSpacing?: string }).letterSpacing = `${Math.round(size * 0.45)}px`;
        ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
        for (const l of this.uncharted) {
          const [cx, cy] = this.worldToScreen(cam, (l.x + 0.5) * CELL, (l.y + 0.5) * CELL);
          ctx.fillStyle = 'rgba(170,190,235,0.30)';
          ctx.fillText('UNCHARTED', cx, cy);
          ctx.font = `500 ${Math.max(9, size - 3)}px "Share Tech Mono", monospace`;
          ctx.fillStyle = 'rgba(170,190,235,0.22)';
          ctx.fillText(`SECTOR ${l.x}.${l.y}`, cx, cy + size + 2);
          ctx.font = `600 ${size}px "Antonio Variable", Antonio, "Arial Narrow", sans-serif`;
        }
        ctx.restore();
      }
    }
    // Sensor horizon: where charted space ends.
    if (this.frontier.length) {
      ctx.save();
      ctx.lineCap = 'round';
      const glow = new Path2D(), line = new Path2D();
      for (const [x1, y1, x2, y2] of this.frontier) {
        const [a, b] = this.worldToScreen(cam, x1, y1), [c, d] = this.worldToScreen(cam, x2, y2);
        glow.moveTo(a, b); glow.lineTo(c, d);
        line.moveTo(a, b); line.lineTo(c, d);
      }
      ctx.strokeStyle = rgba(overlay.accent, 0.07); ctx.lineWidth = 7; ctx.stroke(glow);
      ctx.strokeStyle = rgba(overlay.accent, 0.38); ctx.lineWidth = 1.2;
      ctx.setLineDash([5, 7]); ctx.lineDashOffset = -t * 6;
      ctx.stroke(line);
      ctx.restore();
    }
  }

  private drawBorders(view: GameView, cam: Camera): void {
    const { ctx } = this;
    const { width: W, height: H } = view.meta;
    const owner = this.territoryOwner;
    const z = cam.zoom;
    ctx.lineWidth = 1.5;
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const o = owner[y * W + x]!;
      if (o <= 0) continue;
      const [sx, sy] = this.worldToScreen(cam, x * CELL, y * CELL);
      const s = CELL * z;
      ctx.strokeStyle = rgba(palette(o).map, 0.55);
      ctx.beginPath();
      if (x === 0 || owner[y * W + x - 1] !== o) { ctx.moveTo(sx, sy); ctx.lineTo(sx, sy + s); }
      if (x === W - 1 || owner[y * W + x + 1] !== o) { ctx.moveTo(sx + s, sy); ctx.lineTo(sx + s, sy + s); }
      if (y === 0 || owner[(y - 1) * W + x] !== o) { ctx.moveTo(sx, sy); ctx.lineTo(sx + s, sy); }
      if (y === H - 1 || owner[(y + 1) * W + x] !== o) { ctx.moveTo(sx, sy + s); ctx.lineTo(sx + s, sy + s); }
      ctx.stroke();
    }
  }

  private drawSupply(view: GameView, cam: Camera, grid: string, accent: string): void {
    const { ctx } = this;
    const { width: W, height: H } = view.meta;
    const z = cam.zoom, s = CELL * z;
    ctx.fillStyle = rgba(accent, 0.05);
    ctx.strokeStyle = rgba(accent, 0.55);
    ctx.lineWidth = 1.5;
    ctx.setLineDash([6, 4]);
    ctx.beginPath();
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      if (grid[y * W + x] !== '1') continue;
      const [sx, sy] = this.worldToScreen(cam, x * CELL, y * CELL);
      ctx.fillRect(sx, sy, s, s);
      const at = (xx: number, yy: number) => xx >= 0 && yy >= 0 && xx < W && yy < H && grid[yy * W + xx] === '1';
      if (!at(x - 1, y)) { ctx.moveTo(sx, sy); ctx.lineTo(sx, sy + s); }
      if (!at(x + 1, y)) { ctx.moveTo(sx + s, sy); ctx.lineTo(sx + s, sy + s); }
      if (!at(x, y - 1)) { ctx.moveTo(sx, sy); ctx.lineTo(sx + s, sy); }
      if (!at(x, y + 1)) { ctx.moveTo(sx, sy + s); ctx.lineTo(sx + s, sy + s); }
    }
    ctx.stroke();
    ctx.setLineDash([]);
  }

  private drawRoute(view: GameView, cam: Camera, cells: number[], eta: number, color: string, strong: boolean, t: number, ghost: boolean): void {
    const { ctx } = this;
    const W = view.meta.width;
    const pts = cells.map((c) => this.worldToScreen(cam, ((c % W) + 0.5) * CELL, (Math.floor(c / W) + 0.5) * CELL));
    if (pts.length < 2) return;
    ctx.save();
    ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    if (strong) {
      ctx.strokeStyle = rgba(color.startsWith('#') ? color : '#ffffff', 0.18);
      ctx.lineWidth = 7;
      ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); ctx.stroke();
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = strong ? 2.2 : 1.4;
    ctx.setLineDash(ghost ? [3, 5] : [8, 6]);
    ctx.lineDashOffset = -t * 18;
    ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); ctx.stroke();
    ctx.setLineDash([]);
    // waypoint ticks
    ctx.fillStyle = color;
    for (let i = 1; i < pts.length - 1; i++) { ctx.beginPath(); ctx.arc(pts[i]![0], pts[i]![1], strong ? 2.2 : 1.5, 0, Math.PI * 2); ctx.fill(); }
    // arrowhead + ETA badge
    const [x2, y2] = pts[pts.length - 1]!, [x1, y1] = pts[pts.length - 2]!;
    const ang = Math.atan2(y2 - y1, x2 - x1);
    ctx.beginPath();
    ctx.moveTo(x2, y2);
    ctx.lineTo(x2 - 10 * Math.cos(ang - 0.45), y2 - 10 * Math.sin(ang - 0.45));
    ctx.lineTo(x2 - 10 * Math.cos(ang + 0.45), y2 - 10 * Math.sin(ang + 0.45));
    ctx.closePath(); ctx.fill();
    if (eta > 0 && (strong || cam.zoom > 0.9)) {
      const label = `${eta}T`;
      ctx.font = '600 11px "Antonio Variable", Antonio, sans-serif';
      const w = ctx.measureText(label).width + 12;
      const bx = x2 + 10, by = y2 - 22;
      ctx.fillStyle = 'rgba(4,7,14,0.85)';
      roundRect(ctx, bx, by, w, 16, 8); ctx.fill();
      ctx.strokeStyle = color; ctx.lineWidth = 1; roundRect(ctx, bx, by, w, 16, 8); ctx.stroke();
      ctx.fillStyle = '#eaf1ff'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(label, bx + w / 2, by + 8.5);
    }
    ctx.restore();
  }

  private drawBlocked(view: GameView, cam: Camera, cell: number): void {
    const { ctx } = this;
    const W = view.meta.width;
    const [cx, cy] = this.worldToScreen(cam, ((cell % W) + 0.5) * CELL, (Math.floor(cell / W) + 0.5) * CELL);
    const r = 9;
    ctx.strokeStyle = 'rgba(255,90,78,0.9)'; ctx.lineWidth = 2.5;
    ctx.beginPath(); ctx.moveTo(cx - r, cy - r); ctx.lineTo(cx + r, cy + r); ctx.moveTo(cx + r, cy - r); ctx.lineTo(cx - r, cy + r); ctx.stroke();
  }

  private drawCellHighlight(view: GameView, cam: Camera, cell: number, fill: string, stroke: string | null): void {
    const { ctx } = this;
    const W = view.meta.width;
    const [sx, sy] = this.worldToScreen(cam, (cell % W) * CELL, Math.floor(cell / W) * CELL);
    const s = CELL * cam.zoom;
    ctx.fillStyle = fill; ctx.fillRect(sx, sy, s, s);
    if (stroke) { ctx.strokeStyle = stroke; ctx.strokeRect(sx + 0.5, sy + 0.5, s - 1, s - 1); }
  }

  private drawSelection(view: GameView, cam: Camera, cell: number, accent: string, t: number): void {
    const { ctx } = this;
    const W = view.meta.width;
    const [sx, sy] = this.worldToScreen(cam, (cell % W) * CELL, Math.floor(cell / W) * CELL);
    const s = CELL * cam.zoom;
    const pulse = 2 + Math.sin(t * 3) * 1.5;
    const L = Math.max(8, s * 0.26);
    ctx.strokeStyle = accent; ctx.lineWidth = 2.5; ctx.lineCap = 'round';
    const x0 = sx - pulse, y0 = sy - pulse, x1 = sx + s + pulse, y1 = sy + s + pulse;
    ctx.beginPath();
    ctx.moveTo(x0, y0 + L); ctx.lineTo(x0, y0); ctx.lineTo(x0 + L, y0);
    ctx.moveTo(x1 - L, y0); ctx.lineTo(x1, y0); ctx.lineTo(x1, y0 + L);
    ctx.moveTo(x1, y1 - L); ctx.lineTo(x1, y1); ctx.lineTo(x1 - L, y1);
    ctx.moveTo(x0 + L, y1); ctx.lineTo(x0, y1); ctx.lineTo(x0, y1 - L);
    ctx.stroke();
    ctx.lineCap = 'butt';
  }

  private drawSystem(view: GameView, cam: Camera, s: SystemView, overlay: Overlay, t: number): void {
    const { ctx } = this;
    const z = cam.zoom;
    const [cx, cy] = this.worldToScreen(cam, (s.x + 0.5) * CELL, (s.y + 0.5) * CELL);
    if (cx < -80 || cy < -80 || cx > this.width + 80 || cy > this.height + 80) return;
    const type = STAR_TYPES[hash(s.id) % STAR_TYPES.length]!;
    const baseR = (5 + (hash(s.id * 7) % 4)) * Math.min(1.6, Math.max(0.55, z));
    const flicker = overlay.reduceMotion ? 1 : 0.92 + 0.08 * Math.sin(t * 2 + s.id);
    // glow
    const glowR = baseR * 4.2 * flicker;
    const grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, glowR);
    grad.addColorStop(0, rgba(type.glow, 0.55));
    grad.addColorStop(0.35, rgba(type.glow, 0.16));
    grad.addColorStop(1, rgba(type.glow, 0));
    ctx.fillStyle = grad;
    ctx.beginPath(); ctx.arc(cx, cy, glowR, 0, Math.PI * 2); ctx.fill();
    // core + diffraction
    ctx.fillStyle = type.core;
    ctx.beginPath(); ctx.arc(cx, cy, baseR * 0.62, 0, Math.PI * 2); ctx.fill();
    ctx.strokeStyle = rgba(type.core, 0.35); ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(cx - baseR * 2, cy); ctx.lineTo(cx + baseR * 2, cy); ctx.moveTo(cx, cy - baseR * 2); ctx.lineTo(cx, cy + baseR * 2); ctx.stroke();

    // owner ring with emblem
    if (s.owner) {
      const p = palette(s.owner);
      const ringR = baseR * 1.9 + 3;
      ctx.strokeStyle = p.map; ctx.lineWidth = s.owner === view.meta.player ? 2.2 : 1.8;
      ctx.beginPath(); ctx.arc(cx, cy, ringR, 0, Math.PI * 2); ctx.stroke();
      // emblem badge on the ring (shape cue)
      const bx = cx + ringR * 0.72, by = cy - ringR * 0.72, bs = Math.max(9, 11 * Math.min(1.3, z));
      ctx.save();
      ctx.translate(bx - bs / 2, by - bs / 2);
      ctx.scale(bs / 100, bs / 100);
      ctx.fillStyle = '#05070d';
      ctx.beginPath(); ctx.arc(50, 50, 62, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = p.map;
      ctx.fill(emblemPath(s.owner), 'evenodd');
      ctx.restore();
    } else if (s.minor) {
      const r = baseR * 1.9 + 3;
      ctx.strokeStyle = 'rgba(210,220,255,0.75)'; ctx.lineWidth = 1.4;
      ctx.setLineDash([3, 3]);
      ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
      ctx.setLineDash([]);
    }

    // labels
    if (overlay.layers.labels && z > 0.45) {
      const own = s.owner === view.meta.player;
      const fontSize = Math.round(Math.max(10, Math.min(15, 11.5 * z)));
      ctx.font = `${own ? 600 : 500} ${fontSize}px "Antonio Variable", Antonio, "Arial Narrow", sans-serif`;
      ctx.textAlign = 'center'; ctx.textBaseline = 'top';
      const label = s.name.toUpperCase();
      const ly = cy + baseR * 2.2 + 4;
      ctx.lineWidth = 3; ctx.strokeStyle = 'rgba(3,5,11,0.85)';
      ctx.strokeText(label, cx, ly);
      ctx.fillStyle = s.owner ? lighten(palette(s.owner).map) : '#c9d4ea';
      ctx.fillText(label, cx, ly);
      if (s.minor && z > 0.7) {
        ctx.font = `500 ${Math.max(9, fontSize - 3)}px "Inter Variable", Inter, sans-serif`;
        ctx.fillStyle = 'rgba(200,210,235,0.75)';
        ctx.strokeText(s.minor.name, cx, ly + fontSize + 1);
        ctx.fillText(s.minor.name, cx, ly + fontSize + 1);
      }
    }
  }

  private drawFleets(view: GameView, cam: Camera, overlay: Overlay, t: number): void {
    const W = view.meta.width;
    const z = cam.zoom;
    // Group own units per sector (task forces collapse to one marker).
    const ownByCell = new Map<number, FleetView[]>();
    for (const f of view.fleets) {
      const c = f.y * W + f.x;
      (ownByCell.get(c) ?? ownByCell.set(c, []).get(c)!).push(f);
    }
    const foreignByCell = new Map<number, ContactView[]>();
    for (const f of view.contacts) {
      const c = f.y * W + f.x;
      (foreignByCell.get(c) ?? foreignByCell.set(c, []).get(c)!).push(f);
    }
    const size = Math.max(7, Math.min(13, 9.5 * z));
    for (const [cell, fleets] of ownByCell) {
      const x = cell % W, y = Math.floor(cell / W);
      // one marker per unit (task force or ship), laid out left-to-right along the top edge
      const units: { id: number; tf: number; count: number; colony: boolean; ids: number[] }[] = [];
      const seenTf = new Set<number>();
      for (const f of fleets) {
        if (f.taskForceId) {
          if (seenTf.has(f.taskForceId)) continue;
          seenTf.add(f.taskForceId);
          const members = fleets.filter((m) => m.taskForceId === f.taskForceId);
          units.push({ id: f.id, tf: f.taskForceId, count: members.length, colony: false, ids: members.map((m) => m.id) });
        } else units.push({ id: f.id, tf: 0, count: 1, colony: f.colonyShip, ids: [f.id] });
      }
      const max = z < 0.8 ? 1 : 3;
      units.slice(0, max).forEach((u, i) => {
        // Own units stack down the sector's left edge, clear of the star and its owner ring.
        const [sx, sy] = this.worldToScreen(cam, x * CELL + CELL * 0.14, y * CELL + CELL * 0.16 + i * (size * 2.05) / z);
        const selected = u.ids.includes(overlay.selectedFleetId ?? -1) || (!!u.tf && u.tf === overlay.selectedTaskForceId);
        const moving = fleets.some((f) => u.ids.includes(f.id) && f.route.length > 0);
        this.drawMarker(sx, sy, size, view.meta.player, true, u.tf ? u.count : 0, selected, moving, u.colony, t,
          z < 0.8 && units.length > 1 ? units.length : 0);
        this.hits.push({ kind: 'fleet', id: u.id, x: sx, y: sy, r: size + 5, cell });
      });
    }
    for (const [cell, contacts] of foreignByCell) {
      const x = cell % W, y = Math.floor(cell / W);
      const owners = [...new Set(contacts.map((c) => c.owner))];
      owners.forEach((owner, i) => {
        const group = contacts.filter((c) => c.owner === owner);
        // Foreign contacts stack down the sector's right edge (labels sit below the star).
        const [sx, sy] = this.worldToScreen(cam, x * CELL + CELL * 0.86, y * CELL + CELL * 0.16 + i * (size * 2.05) / z);
        const selected = group.some((c) => c.id === overlay.selectedFleetId);
        this.drawMarker(sx, sy, size, owner, false, group.length > 1 ? group.length : 0, selected, false, false, t, 0,
          group.some((c) => c.atWar));
        this.hits.push({ kind: 'contact', id: group[0]!.id, x: sx, y: sy, r: size + 5, cell });
      });
    }
  }

  private drawMarker(x: number, y: number, s: number, owner: number, own: boolean, count: number, selected: boolean,
    moving: boolean, colony: boolean, t: number, stackCount: number, hostile = false): void {
    const { ctx } = this;
    const p = palette(owner);
    ctx.save();
    ctx.translate(x, y);
    if (selected) {
      const pr = s * 1.75 + Math.sin(t * 4) * 1.5;
      ctx.strokeStyle = p.map; ctx.lineWidth = 1.5; ctx.globalAlpha = 0.9;
      ctx.beginPath(); ctx.arc(0, 0, pr, 0, Math.PI * 2); ctx.stroke();
      ctx.globalAlpha = 0.25; ctx.fillStyle = p.map;
      ctx.beginPath(); ctx.arc(0, 0, pr, 0, Math.PI * 2); ctx.fill();
      ctx.globalAlpha = 1;
    }
    if (hostile) {
      ctx.strokeStyle = 'rgba(255,80,70,0.85)'; ctx.lineWidth = 1.4;
      ctx.setLineDash([2, 2]);
      ctx.beginPath(); ctx.arc(0, 0, s * 1.45, 0, Math.PI * 2); ctx.stroke();
      ctx.setLineDash([]);
    }
    // body: own = chevron pointing up, foreign = inverted diamond-chevron; task force = rounded plate
    ctx.fillStyle = 'rgba(3,6,12,0.9)';
    ctx.beginPath(); ctx.arc(0, 0, s * 1.05, 0, Math.PI * 2); ctx.fill();
    ctx.fillStyle = p.map;
    ctx.strokeStyle = 'rgba(255,255,255,0.85)'; ctx.lineWidth = 0.8;
    ctx.beginPath();
    if (count > 0) {
      roundRect(ctx, -s * 0.8, -s * 0.62, s * 1.6, s * 1.24, s * 0.35);
    } else if (own) {
      if (colony) { ctx.arc(0, 0, s * 0.62, 0, Math.PI * 2); }
      else { ctx.moveTo(0, -s * 0.85); ctx.lineTo(s * 0.72, s * 0.6); ctx.lineTo(0, s * 0.25); ctx.lineTo(-s * 0.72, s * 0.6); ctx.closePath(); }
    } else {
      ctx.moveTo(0, s * 0.85); ctx.lineTo(s * 0.75, -s * 0.2); ctx.lineTo(0, -s * 0.72); ctx.lineTo(-s * 0.75, -s * 0.2); ctx.closePath();
    }
    ctx.fill(); ctx.stroke();
    if (count > 0 || stackCount > 0) {
      ctx.fillStyle = p.ink;
      ctx.font = `700 ${Math.round(s * 0.95)}px "Antonio Variable", Antonio, sans-serif`;
      ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(String(count || stackCount), 0, 0.5);
    }
    if (moving) {
      ctx.fillStyle = '#eaf1ff';
      ctx.beginPath(); ctx.arc(s * 0.95, -s * 0.9, 2.2, 0, Math.PI * 2); ctx.fill();
    }
    ctx.restore();
  }
}

function lighten(hex: string): string {
  const [r, g, b] = hexToRgb(hex);
  const m = (v: number) => Math.round(v + (255 - v) * 0.35);
  return `rgb(${m(r)},${m(g)},${m(b)})`;
}

export function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number): void {
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y, x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x, y + h, rr);
  ctx.arcTo(x, y + h, x, y, rr);
  ctx.arcTo(x, y, x + w, y, rr);
  ctx.closePath();
}
