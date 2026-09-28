import { signal } from '@preact/signals';
import { useEffect, useRef } from 'preact/hooks';
import { core } from '../core/engine';
import type { GameView } from '../core/types';
import {
  focusRequest, goto, mapLayers, selectCell, selectFleet, selection, view,
} from '../state/store';
import { plotCourse, selectedUnit } from '../state/orders';
import { palette } from '../ui/theme';
import { blip } from '../ui/audio';
import { CELL, GalaxyRenderer, knownBounds, type Camera, type Overlay } from './renderer';
import { registerMapHooks } from '../testHooks';

export interface ContextMenuState { cell: number; x: number; y: number }
export const contextMenu = signal<ContextMenuState | null>(null);
export const hoverInfo = signal<{ cell: number; x: number; y: number; eta: number | null; ok: boolean } | null>(null);
export const cameraZoom = signal(1);

import { reducedMotion } from '../state/settings';

export function GalaxyMap() {
  const wrapRef = useRef<HTMLDivElement>(null);
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current!, wrap = wrapRef.current!;
    const renderer = new GalaxyRenderer(canvas);
    const cam: Camera = { cx: 0, cy: 0, zoom: 1 };
    let target: Camera = { ...cam };
    let fitted = false;
    let hoverCell: number | null = null;
    let supplyKey = '', supplyGrid: string | null = null;
    let previewKey = '', preview: Overlay['preview'] = null;
    let drag: { x: number; y: number; cx: number; cy: number; moved: boolean; id: number } | null = null;
    let lastFocusSeq = -1;
    let frame = 0;
    let lastTime = 0;
    const keys = new Set<string>();

    // Width of the chart hidden behind the inspector panel (0 when collapsed or on phones).
    const covered = () => {
      const inspector = wrap.parentElement?.querySelector('.inspector:not(.collapsed)') as HTMLElement | null;
      return inspector && window.innerWidth > 760 ? inspector.getBoundingClientRect().width + 12 : 0;
    };
    const limits = (v: GameView) => {
      const { width, height } = renderer.size;
      const worldW = v.meta.width * CELL, worldH = v.meta.height * CELL;
      const fit = Math.min((width - covered()) / (worldW + CELL * 1.2), height / (worldH + CELL * 1.2));
      return { min: Math.max(0.2, fit * 0.85), max: 3.2, fit };
    };
    const clampCam = (v: GameView, c: Camera) => {
      const { min, max } = limits(v);
      c.zoom = Math.min(max, Math.max(min, c.zoom));
      const worldW = v.meta.width * CELL, worldH = v.meta.height * CELL;
      c.cx = Math.min(worldW + CELL + covered() / c.zoom, Math.max(-CELL, c.cx));
      c.cy = Math.min(worldH + CELL, Math.max(-CELL, c.cy));
    };
    const fitTarget = (v: GameView): Camera => {
      const { fit } = limits(v);
      return { cx: (v.meta.width * CELL) / 2 + covered() / 2 / fit, cy: (v.meta.height * CELL) / 2, zoom: fit };
    };
    const fitView = (v: GameView) => {
      target = fitTarget(v);
      Object.assign(cam, target);
      fitted = true;
    };

    const resize = () => {
      const r = wrap.getBoundingClientRect();
      renderer.resize(r.width, r.height, Math.min(2, window.devicePixelRatio || 1));
      const v = view.value;
      if (v && !fitted) fitView(v);
    };
    const ro = new ResizeObserver(resize);
    ro.observe(wrap);
    resize();

    const overlay = (v: GameView): Overlay => {
      const sel = selection.value;
      const unit = selectedUnit(v);
      // Supply range of the selected own unit, from the core (cached per unit + state version).
      if (unit) {
        const key = `${unit.fleet.id}:${v.meta.version}`;
        if (key !== supplyKey) {
          supplyKey = key;
          try { supplyGrid = core.supply(unit.fleet.id); } catch { supplyGrid = null; }
        }
      } else { supplyKey = ''; supplyGrid = null; }
      // Route preview to the hovered sector.
      if (unit && hoverCell !== null && hoverCell !== unit.fleet.y * v.meta.width + unit.fleet.x) {
        const key = `${unit.fleet.id}:${hoverCell}:${v.meta.version}`;
        if (key !== previewKey) {
          previewKey = key;
          try {
            const r = core.route(unit.fleet.id, hoverCell % v.meta.width, Math.floor(hoverCell / v.meta.width));
            preview = { cells: r.cells, eta: r.eta, ok: r.steps >= 0, cell: hoverCell };
          } catch { preview = null; }
          const prev = hoverInfo.value;
          hoverInfo.value = { cell: hoverCell, x: prev?.x ?? 0, y: prev?.y ?? 0, eta: preview?.ok ? preview.eta : null, ok: !!preview?.ok };
        }
      } else if (preview) { preview = null; previewKey = ''; }
      return {
        selectedCell: sel.cell, selectedFleetId: sel.fleetId, selectedTaskForceId: sel.taskForceId,
        hoverCell, supply: supplyGrid, preview, layers: mapLayers.value, reduceMotion: reducedMotion.value,
        accent: palette(v.meta.player).primary,
      };
    };

    const tick = (time: number) => {
      frame = requestAnimationFrame(tick);
      const v = view.value;
      if (!v) return;
      if (!fitted) fitView(v);
      const fr = focusRequest.value;
      if (fr && fr.seq !== lastFocusSeq) {
        lastFocusSeq = fr.seq;
        if (fr.frame) {
          // Frame the charted region (plus a margin of uncharted space) in the visible part of the chart.
          const b = knownBounds(v);
          if (b) {
            const { width, height } = renderer.size;
            const pad = 1.4;
            const bw = (b.x1 - b.x0 + 1 + pad * 2) * CELL, bh = (b.y1 - b.y0 + 1 + pad * 2) * CELL;
            const { min, max } = limits(v);
            const zoom = Math.min(Math.min(max, 1.7), Math.max(min, Math.min((width - covered()) / bw, height / bh)));
            target = { cx: ((b.x0 + b.x1 + 1) / 2) * CELL + covered() / 2 / zoom, cy: ((b.y0 + b.y1 + 1) / 2) * CELL, zoom };
          } else target = fitTarget(v);
          clampCam(v, target);
          if (reducedMotion.value || !fitted) Object.assign(cam, target);
        } else {
          const zoom = fr.zoom ?? Math.max(target.zoom, 1.2);
          // Centre within the part of the chart not covered by the inspector panel.
          target = { cx: (fr.x + 0.5) * CELL + covered() / 2 / zoom, cy: (fr.y + 0.5) * CELL, zoom };
          clampCam(v, target);
        }
      }
      // keyboard panning
      const pan = 9 / cam.zoom;
      if (keys.has('ArrowLeft') || keys.has('a')) target.cx -= pan;
      if (keys.has('ArrowRight') || keys.has('d')) target.cx += pan;
      if (keys.has('ArrowUp') || keys.has('w')) target.cy -= pan;
      if (keys.has('ArrowDown') || keys.has('s')) target.cy += pan;
      clampCam(v, target);
      // Frame-rate independent glide (0.22 per 60 Hz frame), so slow machines settle as fast.
      const dt = lastTime ? Math.min(250, time - lastTime) : 16.7;
      lastTime = time;
      const k = reducedMotion.value ? 1 : 1 - Math.pow(1 - 0.22, dt / 16.7);
      cam.cx += (target.cx - cam.cx) * k; cam.cy += (target.cy - cam.cy) * k;
      cam.zoom += (target.zoom - cam.zoom) * k;
      if (Math.abs(cameraZoom.value - cam.zoom) > 0.01) cameraZoom.value = cam.zoom;
      renderer.draw(v, cam, overlay(v), time);
    };
    frame = requestAnimationFrame(tick);

    const local = (e: MouseEvent) => {
      const r = canvas.getBoundingClientRect();
      return [e.clientX - r.left, e.clientY - r.top] as const;
    };
    const pick = (sx: number, sy: number) => {
      for (const h of renderer.hits) if ((sx - h.x) ** 2 + (sy - h.y) ** 2 <= h.r * h.r) return h;
      return null;
    };

    const onPointerDown = (e: PointerEvent) => {
      if (e.button !== 0) return;
      wrap.focus({ preventScroll: true });
      contextMenu.value = null;
      drag = { x: e.clientX, y: e.clientY, cx: target.cx, cy: target.cy, moved: false, id: e.pointerId };
      canvas.setPointerCapture(e.pointerId);
    };
    const onPointerMove = (e: PointerEvent) => {
      const v = view.value;
      if (!v) return;
      if (drag && drag.id === e.pointerId) {
        const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
        if (!drag.moved && dx * dx + dy * dy > 16) drag.moved = true;
        if (drag.moved) {
          target.cx = drag.cx - dx / cam.zoom; target.cy = drag.cy - dy / cam.zoom;
          cam.cx = target.cx; cam.cy = target.cy;
          wrap.classList.add('dragging');
        }
      }
      const [sx, sy] = local(e);
      const cell = renderer.cellAt(cam, v, sx, sy);
      if (cell !== hoverCell) {
        hoverCell = cell;
        if (cell === null) hoverInfo.value = null;
        else hoverInfo.value = { cell, x: sx, y: sy, eta: null, ok: true };
      } else if (hoverInfo.value && cell !== null) {
        hoverInfo.value = { ...hoverInfo.value, x: sx, y: sy };
      }
      wrap.style.cursor = pick(sx, sy) ? 'pointer' : drag?.moved ? 'grabbing' : 'crosshair';
    };
    const onPointerUp = (e: PointerEvent) => {
      const v = view.value;
      if (!drag || drag.id !== e.pointerId) return;
      const moved = drag.moved;
      drag = null;
      wrap.classList.remove('dragging');
      if (moved || !v) return;
      const [sx, sy] = local(e);
      const hit = pick(sx, sy);
      if (hit) { selectFleet(hit.id); blip('select'); return; }
      const cell = renderer.cellAt(cam, v, sx, sy);
      const sel = selection.value;
      // Clicking the selected unit's own sector again keeps the unit; otherwise select the sector.
      if (cell !== null && sel.fleetId && sel.cell === cell) return;
      selectCell(cell);
      blip('select');
    };
    const onDblClick = (e: MouseEvent) => {
      const v = view.value;
      if (!v) return;
      const [sx, sy] = local(e);
      if (pick(sx, sy)) return;
      const cell = renderer.cellAt(cam, v, sx, sy);
      if (cell === null) return;
      const system = v.systems.find((s) => s.y * v.meta.width + s.x === cell);
      if (system && system.owner === v.meta.player) { selectCell(cell); goto('system'); return; }
      target = { cx: ((cell % v.meta.width) + 0.5) * CELL, cy: (Math.floor(cell / v.meta.width) + 0.5) * CELL, zoom: Math.max(target.zoom, 1.6) };
    };
    const onContextMenu = (e: MouseEvent) => {
      e.preventDefault();
      const v = view.value;
      if (!v) return;
      const [sx, sy] = local(e);
      const hit = pick(sx, sy);
      const cell = hit ? hit.cell : renderer.cellAt(cam, v, sx, sy);
      if (cell === null) return;
      const sel = selection.value;
      if (!sel.fleetId || (hit && hit.kind === 'fleet')) {
        if (hit && hit.kind === 'fleet') selectFleet(hit.id);
        else if (!sel.fleetId) selectCell(cell);
      }
      // Right-click with a unit selected on a plain reachable sector: order straight away (Shift = menu).
      const unit = selectedUnit(v);
      if (unit && !e.shiftKey && !hit) {
        const system = v.systems.find((s) => s.y * v.meta.width + s.x === cell);
        const hostile = v.contacts.some((c) => c.y * v.meta.width + c.x === cell);
        const here = unit.fleet.y * v.meta.width + unit.fleet.x === cell;
        if (!system && !hostile && !here) {
          try {
            const r = core.route(unit.fleet.id, cell % v.meta.width, Math.floor(cell / v.meta.width));
            if (r.steps > 0) { plotCourse(unit.fleet, cell % v.meta.width, Math.floor(cell / v.meta.width)); return; }
          } catch { /* fall through to the menu */ }
        }
      }
      const r = wrap.getBoundingClientRect();
      contextMenu.value = { cell, x: e.clientX - r.left, y: e.clientY - r.top };
      blip('nav');
    };
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const v = view.value;
      if (!v) return;
      const [sx, sy] = local(e);
      const [wx, wy] = renderer.screenToWorld(cam, sx, sy);
      const factor = Math.exp(-e.deltaY * (e.deltaMode === 1 ? 0.05 : 0.0016));
      const { min, max } = limits(v);
      const zoom = Math.min(max, Math.max(min, target.zoom * factor));
      // keep the world point under the cursor fixed
      const { width, height } = renderer.size;
      target.zoom = zoom;
      target.cx = wx - (sx - width / 2) / zoom;
      target.cy = wy - (sy - height / 2) / zoom;
      cam.zoom = zoom; cam.cx = target.cx; cam.cy = target.cy;
      contextMenu.value = null;
    };
    const typing = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      return !!t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable);
    };
    const onKeyDown = (e: KeyboardEvent) => {
      if (typing(e) || e.ctrlKey || e.metaKey || e.altKey) return;
      const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
      if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'w', 'a', 's', 'd'].includes(key)) {
        if (document.activeElement !== wrap && !wrap.contains(document.activeElement) && document.activeElement !== document.body) return;
        keys.add(key); e.preventDefault();
      } else if (key === '+' || key === '=') { target.zoom *= 1.25; }
      else if (key === '-' || key === '_') { target.zoom /= 1.25; }
      else if (key === 'Home' || key === 'h') {
        const v = view.value;
        const home = v?.colonies[0] && v.systems.find((s) => s.id === v.colonies[0]!.systemId);
        if (home) target = { cx: (home.x + 0.5) * CELL, cy: (home.y + 0.5) * CELL, zoom: 1.6 };
      } else if (key === 'f') { const v = view.value; if (v) target = fitTarget(v); }
    };
    const onKeyUp = (e: KeyboardEvent) => { keys.delete(e.key.length === 1 ? e.key.toLowerCase() : e.key); };
    const onLeave = () => { hoverCell = null; hoverInfo.value = null; };
    const onBlur = () => keys.clear();

    registerMapHooks({
      cellCenter: (cell) => {
        const v = view.value;
        if (!v) return null;
        const [sx, sy] = renderer.worldToScreen(cam, ((cell % v.meta.width) + 0.5) * CELL, (Math.floor(cell / v.meta.width) + 0.5) * CELL);
        const r = canvas.getBoundingClientRect();
        return { x: r.left + sx, y: r.top + sy };
      },
      fleetMarker: (id) => {
        const hit = renderer.hits.find((h) => h.id === id);
        if (!hit) return null;
        const r = canvas.getBoundingClientRect();
        return { x: r.left + hit.x, y: r.top + hit.y };
      },
      // A focus request is applied on the next animation frame; until then the camera target is stale.
      settled: () => (focusRequest.value === null || focusRequest.value.seq === lastFocusSeq)
        && Math.abs(cam.cx - target.cx) < 0.5 && Math.abs(cam.cy - target.cy) < 0.5 && Math.abs(cam.zoom - target.zoom) < 0.002,
    });

    canvas.addEventListener('pointerdown', onPointerDown);
    canvas.addEventListener('pointermove', onPointerMove);
    canvas.addEventListener('pointerup', onPointerUp);
    canvas.addEventListener('pointercancel', onPointerUp);
    canvas.addEventListener('pointerleave', onLeave);
    canvas.addEventListener('dblclick', onDblClick);
    canvas.addEventListener('contextmenu', onContextMenu);
    canvas.addEventListener('wheel', onWheel, { passive: false });
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('blur', onBlur);
    return () => {
      registerMapHooks(undefined);
      cancelAnimationFrame(frame);
      ro.disconnect();
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
    };
  }, []);

  return (
    <div class="galaxy-map" ref={wrapRef} tabIndex={0} aria-label="Galaxy star chart. Drag or arrow keys to pan, wheel to zoom, right-click for orders." data-testid="galaxy-map">
      <canvas ref={canvasRef} />
    </div>
  );
}
