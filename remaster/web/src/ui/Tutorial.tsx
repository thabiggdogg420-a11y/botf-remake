// Coach-mark overlay for the First Contact tutorial: a spotlight ring around the step's target and
// a card with the instructions. Non-blocking: the player performs each step on the real interface.
import { useEffect, useState } from 'preact/hooks';
import { goto, screen } from '../state/store';
import { uiScale } from '../state/settings';
import { nextTutorialStep, skipTutorial, STEPS, tutorialStep } from '../state/tutorial';
import { Pill } from './components';

interface Box { left: number; top: number; width: number; height: number }

export function TutorialOverlay() {
  const i = tutorialStep.value;
  const step = i === null ? null : STEPS[i];
  const [box, setBox] = useState<Box | null>(null);
  const current = screen.value;
  useEffect(() => {
    if (!step?.target) { setBox(null); return; }
    let frame = 0;
    const measure = () => {
      const el = document.querySelector(step.target!);
      const k = uiScale.value;
      if (el) {
        const r = el.getBoundingClientRect();
        const next = { left: r.left / k, top: r.top / k, width: r.width / k, height: r.height / k };
        setBox((b) => (b && Math.abs(b.left - next.left) < 0.5 && Math.abs(b.top - next.top) < 0.5 && Math.abs(b.width - next.width) < 0.5 && Math.abs(b.height - next.height) < 0.5 ? b : next));
      } else setBox(null);
      frame = window.setTimeout(measure, 250);
    };
    measure();
    return () => window.clearTimeout(frame);
  }, [step?.id, current]);
  if (!step || i === null) return null;
  const vw = window.innerWidth / uiScale.value, vh = window.innerHeight / uiScale.value;
  // Card placement: beside a small target, or top-centre for large areas (the star chart) and none.
  const cardW = Math.min(400, vw - 32);
  let style: Record<string, string>;
  const big = box && box.width * box.height > vw * vh * 0.3;
  if (box && !big) {
    const below = box.top + box.height + 14 + 190 < vh;
    const left = Math.min(Math.max(16, box.left + box.width / 2 - cardW / 2), vw - cardW - 16);
    style = below ? { left: `${left}px`, top: `${box.top + box.height + 14}px` } : { left: `${left}px`, top: `${Math.max(16, box.top - 14 - 190)}px` };
  } else {
    style = { left: `${Math.max(16, vw / 2 - cardW / 2)}px`, top: `${Math.min(96, vh * 0.14)}px` };
  }
  const offScreen = step.screen && step.screen !== current;
  return (
    <div class="tutorial-layer" aria-live="polite">
      {box && !big ? <div class="tutorial-spot" style={{ left: `${box.left - 6}px`, top: `${box.top - 6}px`, width: `${box.width + 12}px`, height: `${box.height + 12}px` }} /> : null}
      {box && big ? <div class="tutorial-spot area" style={{ left: `${box.left + 4}px`, top: `${box.top + 4}px`, width: `${box.width - 8}px`, height: `${box.height - 8}px` }} /> : null}
      <div class="tutorial-card" role="dialog" aria-label={`Tutorial: ${step.title}`} data-testid="tutorial" data-step={step.id} style={{ ...style, width: `${cardW}px` }}>
        <div class="tc-head">
          <span class="tc-kicker">First contact · {i + 1}/{STEPS.length}</span>
          <button type="button" class="tc-skip" onClick={skipTutorial} data-testid="tutorial-skip">Skip tutorial</button>
        </div>
        <h3>{step.title}</h3>
        <p>{step.body}</p>
        <div class="tc-progress" aria-hidden="true">{STEPS.map((s, n) => <i key={s.id} class={n < i ? 'done' : n === i ? 'now' : ''} />)}</div>
        <div class="tc-actions">
          {offScreen ? <Pill small tone="ghost" onClick={() => goto(step.screen!)}>Go to {step.screen}</Pill> : null}
          {!step.done ? <Pill small onClick={nextTutorialStep} testid="tutorial-next">{i + 1 === STEPS.length ? 'Finish' : 'Next'}</Pill>
            : <span class="tc-wait">Waiting for you…</span>}
        </div>
      </div>
    </div>
  );
}
