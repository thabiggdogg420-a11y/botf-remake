import { useEffect, useRef } from 'preact/hooks';

/** Slow-drifting parallax starfield for the title and setup screens. */
export function Starfield({ accent = '#6fa8ff' }: { accent?: string }) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const canvas = ref.current!;
    const ctx = canvas.getContext('2d')!;
    const reduce = window.matchMedia?.('(prefers-reduced-motion: reduce)').matches;
    let w = 0, h = 0, dpr = 1, frame = 0;
    const stars = Array.from({ length: 420 }, (_, i) => ({
      x: Math.random(), y: Math.random(), z: 0.15 + ((i * 7919) % 100) / 115, p: Math.random() * 6.28,
    }));
    const resize = () => {
      dpr = Math.min(2, window.devicePixelRatio || 1);
      w = canvas.clientWidth; h = canvas.clientHeight;
      canvas.width = w * dpr; canvas.height = h * dpr;
    };
    resize();
    window.addEventListener('resize', resize);
    const draw = (t: number) => {
      frame = requestAnimationFrame(draw);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, w, h);
      const time = reduce ? 0 : t / 1000;
      for (const s of stars) {
        const x = ((s.x + time * 0.004 * s.z) % 1) * w;
        const y = s.y * h;
        const a = (0.35 + 0.5 * s.z) * (0.75 + 0.25 * Math.sin(time * 1.5 + s.p));
        ctx.globalAlpha = a;
        ctx.fillStyle = s.z > 0.9 ? accent : '#dfe8ff';
        ctx.beginPath(); ctx.arc(x, y, 0.4 + s.z * 1.1, 0, 6.283); ctx.fill();
      }
      ctx.globalAlpha = 1;
    };
    frame = requestAnimationFrame(draw);
    return () => { cancelAnimationFrame(frame); window.removeEventListener('resize', resize); };
  }, [accent]);
  return <canvas class="starfield" ref={ref} aria-hidden="true" />;
}
