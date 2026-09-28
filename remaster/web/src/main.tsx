import { render } from 'preact';
import './styles/fonts.css';
import './styles/main.css';
import { App } from './App';
import { loadCore } from './core/engine';
import { loadMeta } from './state/store';
import { exposeTestHooks } from './testHooks';

const root = document.getElementById('app')!;
root.innerHTML = '<div class="boot"><div class="boot-bar"></div><div class="boot-text">Initialising simulation core…</div></div>';

loadCore()
  .then(() => {
    loadMeta();
    root.innerHTML = '';
    render(<App />, root);
    exposeTestHooks();
  })
  .catch((error: unknown) => {
    root.innerHTML = `<div class="boot"><div class="boot-text">The simulation core failed to load: ${String(error instanceof Error ? error.message : error)}</div></div>`;
  });
