import type { ComponentChildren, JSX } from 'preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { EMBLEMS, palette } from './theme';
import { confirmDialog, toasts } from '../state/store';
import { uiScale } from '../state/settings';

// ---------------------------------------------------------------------------------------------
// Emblem: owner glyph (shape = non-colour cue; fill = faction colour)
// ---------------------------------------------------------------------------------------------
export function Emblem({ empire, size = 22, mono = false, title }: { empire: number; size?: number; mono?: boolean; title?: string }) {
  const p = palette(empire);
  return (
    <svg class="emblem" width={size} height={size} viewBox="0 0 100 100" role="img" aria-label={title ?? p.key}>
      {title ? <title>{title}</title> : null}
      <path d={EMBLEMS[empire] ?? EMBLEMS[0]} fill={mono ? 'currentColor' : p.map} fill-rule="evenodd" />
    </svg>
  );
}

// ---------------------------------------------------------------------------------------------
// Tooltip helpers: any element with data-tip gets a rich tooltip (see TooltipLayer).
// ---------------------------------------------------------------------------------------------
export function tip(text: string | undefined | null, title?: string): Record<string, string> {
  if (!text) return {};
  return title ? { 'data-tip': text, 'data-tip-title': title } : { 'data-tip': text };
}

export function TooltipLayer() {
  const [state, setState] = useState<{ text: string; title?: string; x: number; y: number; below: boolean } | null>(null);
  const current = useRef<Element | null>(null);
  useEffect(() => {
    const show = (target: Element) => {
      const text = target.getAttribute('data-tip');
      if (!text) return;
      // Rects are in screen pixels; the tooltip is laid out inside the zoomed interface.
      const k = uiScale.value;
      const r = target.getBoundingClientRect();
      const below = r.top < 120;
      setState({ text, title: target.getAttribute('data-tip-title') ?? undefined, x: (r.left + r.width / 2) / k, y: (below ? r.bottom + 8 : r.top - 8) / k, below });
    };
    const over = (e: PointerEvent) => {
      const el = (e.target as Element | null)?.closest?.('[data-tip]') ?? null;
      if (el === current.current) return;
      current.current = el;
      if (el) show(el); else setState(null);
    };
    const hide = () => { current.current = null; setState(null); };
    const focusIn = (e: FocusEvent) => {
      const el = (e.target as Element | null)?.closest?.('[data-tip]') ?? null;
      current.current = el;
      if (el) show(el);
    };
    document.addEventListener('pointerover', over);
    document.addEventListener('pointerdown', hide, true);
    document.addEventListener('focusin', focusIn);
    document.addEventListener('focusout', hide);
    window.addEventListener('wheel', hide, { passive: true });
    return () => {
      document.removeEventListener('pointerover', over);
      document.removeEventListener('pointerdown', hide, true);
      document.removeEventListener('focusin', focusIn);
      document.removeEventListener('focusout', hide);
      window.removeEventListener('wheel', hide);
    };
  }, []);
  const ref = useRef<HTMLDivElement>(null);
  const [shift, setShift] = useState(0);
  useEffect(() => {
    if (!ref.current || !state) return;
    const r = ref.current.getBoundingClientRect();
    // Measure against the unshifted position (the rendered one includes the previous tooltip's shift).
    const pad = 8, k = uiScale.value;
    const left0 = r.left - shift * k, right0 = r.right - shift * k;
    if (left0 < pad) setShift((pad - left0) / k);
    else if (right0 > window.innerWidth - pad) setShift((window.innerWidth - pad - right0) / k);
    else setShift(0);
  }, [state]);
  if (!state) return null;
  return (
    <div ref={ref} class={`tooltip ${state.below ? 'below' : 'above'}`} role="tooltip"
      style={{ left: `${state.x + shift}px`, top: `${state.y}px` }}>
      {state.title ? <div class="tooltip-title">{state.title}</div> : null}
      {state.text.split('\n').map((line, i) => <div key={i} class={line.startsWith('  ') ? 'tooltip-indent' : ''}>{line}</div>)}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Buttons
// ---------------------------------------------------------------------------------------------
type Tone = 'primary' | 'secondary' | 'tertiary' | 'alert' | 'good' | 'ghost' | 'warn';
export function Pill(props: {
  children: ComponentChildren; onClick?: (e: MouseEvent) => void; disabled?: boolean; reason?: string; tone?: Tone;
  active?: boolean; small?: boolean; kbd?: string; title?: string; hint?: string; wide?: boolean; testid?: string;
  type?: 'button' | 'submit'; class?: string; ariaLabel?: string;
}) {
  const { children, onClick, disabled, reason, tone = 'primary', active, small, kbd, hint, wide, testid, ariaLabel } = props;
  const tipText = disabled && reason ? `Unavailable: ${reason}` : hint;
  return (
    <button
      type={props.type ?? 'button'}
      class={`pill pill-${tone}${active ? ' active' : ''}${small ? ' small' : ''}${wide ? ' wide' : ''}${disabled ? ' disabled' : ''} ${props.class ?? ''}`}
      aria-disabled={disabled ? 'true' : undefined}
      aria-pressed={active === undefined ? undefined : active}
      aria-label={ariaLabel}
      data-testid={testid}
      onClick={(e) => { if (disabled) { e.preventDefault(); return; } onClick?.(e as unknown as MouseEvent); }}
      {...tip(tipText, props.title)}
    >
      <span class="pill-label">{children}</span>
      {kbd ? <kbd>{kbd}</kbd> : null}
    </button>
  );
}

export function IconButton({ label, onClick, children, disabled, reason, testid }: {
  label: string; onClick: () => void; children: ComponentChildren; disabled?: boolean; reason?: string; testid?: string;
}) {
  return (
    <button type="button" class={`icon-btn${disabled ? ' disabled' : ''}`} aria-label={label} data-testid={testid}
      aria-disabled={disabled ? 'true' : undefined}
      onClick={() => { if (!disabled) onClick(); }} {...tip(disabled && reason ? `Unavailable: ${reason}` : label)}>
      {children}
    </button>
  );
}

// ---------------------------------------------------------------------------------------------
// LCARS panel frame
// ---------------------------------------------------------------------------------------------
export function Frame({ title, sub, children, actions, tone = 'primary', class: cls, flush, testid, id }: {
  title: ComponentChildren; sub?: ComponentChildren; children: ComponentChildren; actions?: ComponentChildren;
  tone?: 'primary' | 'secondary' | 'tertiary' | 'alert'; class?: string; flush?: boolean; testid?: string; id?: string;
}) {
  return (
    <section class={`frame frame-${tone} ${cls ?? ''}`} data-testid={testid} id={id}>
      <header class="frame-head">
        <span class="frame-elbow" aria-hidden="true" />
        <h2 class="frame-title">{title}</h2>
        {sub ? <span class="frame-sub">{sub}</span> : null}
        <span class="frame-rule" aria-hidden="true" />
        {actions ? <div class="frame-actions">{actions}</div> : null}
      </header>
      <div class={`frame-body${flush ? ' flush' : ''}`}>{children}</div>
    </section>
  );
}

export function Section({ title, children, aside }: { title: ComponentChildren; children: ComponentChildren; aside?: ComponentChildren }) {
  return (
    <div class="section">
      <div class="section-head"><h3>{title}</h3>{aside ? <div class="section-aside">{aside}</div> : null}</div>
      {children}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Readouts
// ---------------------------------------------------------------------------------------------
export function fmt(n: number | undefined | null): string {
  if (n === undefined || n === null || Number.isNaN(n)) return '—';
  return Math.round(n).toLocaleString('en-US');
}
export function signed(n: number): string {
  return `${n > 0 ? '+' : n < 0 ? '−' : '±'}${fmt(Math.abs(n))}`;
}

export function Stat({ label, value, tipText, tipTitle, tone, sub, icon, testid }: {
  label: string; value: ComponentChildren; tipText?: string; tipTitle?: string; tone?: 'good' | 'warn' | 'alert' | 'accent' | 'dim';
  sub?: ComponentChildren; icon?: ComponentChildren; testid?: string;
}) {
  return (
    <div class={`stat${tone ? ` tone-${tone}` : ''}`} {...tip(tipText, tipTitle ?? label)} tabIndex={tipText ? 0 : undefined} data-testid={testid}>
      <div class="stat-label">{icon}{label}</div>
      <div class="stat-value">{value}</div>
      {sub ? <div class="stat-sub">{sub}</div> : null}
    </div>
  );
}

export function Meter({ value, max, tone = 'accent', segments = 0, height = 6, label, ghost }: {
  value: number; max: number; tone?: 'accent' | 'good' | 'warn' | 'alert' | 'secondary' | 'shield' | 'hull'; segments?: number; height?: number;
  label?: string; ghost?: number;
}) {
  const pct = max > 0 ? Math.max(0, Math.min(1, value / max)) : 0;
  const ghostPct = ghost !== undefined && max > 0 ? Math.max(0, Math.min(1, ghost / max)) : null;
  return (
    <div class={`meter meter-${tone}${segments ? ' segmented' : ''}`} style={{ height: `${height}px`, '--segments': segments || 1 } as JSX.CSSProperties}
      role="meter" aria-valuemin={0} aria-valuemax={max} aria-valuenow={value} aria-label={label}>
      {ghostPct !== null ? <div class="meter-ghost" style={{ width: `${ghostPct * 100}%` }} /> : null}
      <div class="meter-fill" style={{ width: `${pct * 100}%` }} />
    </div>
  );
}

export function Badge({ children, tone = 'dim', tipText }: { children: ComponentChildren; tone?: string; tipText?: string }) {
  return <span class={`badge badge-${tone}`} {...tip(tipText)}>{children}</span>;
}

export function Tabs<T extends string>({ value, options, onChange, testid }: {
  value: T; options: { id: T; label: ComponentChildren; hint?: string; count?: number }[]; onChange: (v: T) => void; testid?: string;
}) {
  return (
    <div class="tabs" role="tablist" data-testid={testid}>
      {options.map((o) => (
        <button type="button" role="tab" aria-selected={o.id === value} class={`tab${o.id === value ? ' active' : ''}`}
          onClick={() => onChange(o.id)} {...tip(o.hint)} key={o.id}>
          {o.label}{o.count !== undefined ? <span class="tab-count">{o.count}</span> : null}
        </button>
      ))}
    </div>
  );
}

export function Empty({ children }: { children: ComponentChildren }) {
  return <div class="empty">{children}</div>;
}

// ---------------------------------------------------------------------------------------------
// Global overlays
// ---------------------------------------------------------------------------------------------
export function Toasts() {
  return (
    <div class="toasts" role="status" aria-live="polite">
      {toasts.value.map((t) => <div key={t.id} class={`toast toast-${t.tone}`}>{t.text}</div>)}
    </div>
  );
}

export function ConfirmLayer() {
  const d = confirmDialog.value;
  const ref = useRef<HTMLButtonElement>(null);
  useEffect(() => { ref.current?.focus(); }, [d]);
  if (!d) return null;
  const close = () => { confirmDialog.value = null; };
  return (
    <div class="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) close(); }}>
      <div class="modal" role="dialog" aria-modal="true" aria-label={d.title} data-testid="confirm-dialog"
        onKeyDown={(e) => { if (e.key === 'Escape') { e.stopPropagation(); close(); } }}>
        <Frame title={d.title} tone={d.tone === 'alert' ? 'alert' : 'primary'}>
          <p class="modal-body">{d.body}</p>
          {d.lines?.length ? <ul class="modal-lines">{d.lines.map((l) => <li key={l}>{l}</li>)}</ul> : null}
          <div class="modal-actions">
            <Pill tone="ghost" onClick={close}>{d.cancel ?? 'Cancel'}</Pill>
            <button ref={ref} type="button" class={`pill pill-${d.tone === 'alert' ? 'alert' : 'primary'} big`} data-testid="confirm-ok"
              onClick={() => { close(); d.onConfirm(); }}>
              <span class="pill-label">{d.confirm}</span><kbd>Enter</kbd>
            </button>
          </div>
        </Frame>
      </div>
    </div>
  );
}

export function Slider({ value, min = 0, max, step = 1, onInput, onChange, label, disabled, testid }: {
  value: number; min?: number; max: number; step?: number; onInput: (v: number) => void; onChange?: (v: number) => void;
  label: string; disabled?: boolean; testid?: string;
}) {
  const pct = max > min ? ((value - min) / (max - min)) * 100 : 0;
  return (
    <input class="slider" type="range" min={min} max={max} step={step} value={value} aria-label={label} disabled={disabled}
      data-testid={testid}
      style={{ '--pct': `${pct}%` } as JSX.CSSProperties}
      onInput={(e) => onInput(Number((e.target as HTMLInputElement).value))}
      onChange={(e) => onChange?.(Number((e.target as HTMLInputElement).value))} />
  );
}
