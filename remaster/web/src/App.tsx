import { useEffect } from 'preact/hooks';
import {
  back, briefing, briefingOpen, busy, confirmDialog, encounter, endTurnWarnings, eventsOpen, GAME_SCREENS, goto, menuOpen, outcomeDismissed,
  quitToTitle, requestEndTurn, screen, selectFleet, selection, view, victoryOpen, type Screen, toast, saveName,
} from './state/store';
import { EventsPanel, OutcomeScreen, pendingEventCount, VictoryPanel } from './ui/Remaster';
import { saveToSlot } from './state/saves';
import { audioEnabled, setAudio } from './ui/audio';
import { ConfirmLayer, Emblem, fmt, Pill, signed, TooltipLayer, Toasts, tip } from './ui/components';
import { palette } from './ui/theme';
import { focusMap } from './state/store';
import { TitleScreen } from './screens/Title';
import { NewCampaignScreen } from './screens/NewCampaign';
import { SavesScreen } from './screens/Saves';
import { GalaxyScreen } from './screens/Galaxy';
import { SystemScreen } from './screens/System';
import { EmpireScreen } from './screens/Empire';
import { ResearchScreen } from './screens/Research';
import { FleetsScreen } from './screens/Fleets';
import { DiplomacyScreen } from './screens/Diplomacy';
import { IntelScreen } from './screens/Intel';
import { EncyclopediaScreen } from './screens/Encyclopedia';
import { CombatScreen } from './screens/Combat';
import { BriefingPanel } from './screens/Briefing';
import { settings } from './state/settings';
import { HelpButton, helpOpen, HelpPopover } from './ui/Help';
import { SettingsModal, settingsOpen } from './ui/SettingsPanel';
import { TutorialOverlay } from './ui/Tutorial';

const TEXT_INPUTS = new Set(['text', 'number', 'search', 'email', 'password', 'url', 'tel', '']);
/** True while the user is entering text: shortcuts must not fire. Sliders/checkboxes do not count. */
function isTyping(e: KeyboardEvent): boolean {
  const t = e.target as HTMLElement | null;
  if (!t) return false;
  if (t.tagName === 'INPUT') return TEXT_INPUTS.has(((t as HTMLInputElement).getAttribute('type') ?? '').toLowerCase());
  return t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable;
}
function onFormControl(e: KeyboardEvent): boolean {
  const t = e.target as HTMLElement | null;
  return !!t && ['INPUT', 'BUTTON', 'SELECT', 'TEXTAREA'].includes(t.tagName);
}

export function App() {
  const current = screen.value;
  const v = view.value;
  const empire = v ? v.meta.player : 1;
  // Reading the settings re-renders the tree when the owner palette or scale changes.
  const prefs = settings.value;

  useEffect(() => {
    document.documentElement.dataset.faction = palette(empire).key;
  }, [empire]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (isTyping(e)) return;
      if (confirmDialog.value) {
        if (e.key === 'Escape') { confirmDialog.value = null; e.preventDefault(); }
        return;
      }
      if (settingsOpen.value) {
        if (e.key === 'Escape') { settingsOpen.value = false; e.preventDefault(); }
        return;
      }
      if (eventsOpen.value || victoryOpen.value) {
        if (e.key === 'Escape') { eventsOpen.value = false; victoryOpen.value = false; e.preventDefault(); }
        return;
      }
      const inGame = !!view.value && !['title', 'new', 'saves'].includes(screen.value);
      if (e.key === '?' && inGame && !e.ctrlKey && !e.metaKey) { e.preventDefault(); helpOpen.value = !helpOpen.value; return; }
      if (e.key === 'Escape' && helpOpen.value) { helpOpen.value = false; return; }
      const fkey = /^F([1-8])$/.exec(e.key);
      if (fkey && inGame) {
        e.preventDefault();
        const target = GAME_SCREENS[Number(fkey[1]) - 1];
        if (target) goto(target.id);
        return;
      }
      if (e.key === 'Escape') {
        if (briefingOpen.value) { briefingOpen.value = false; return; }
        if (menuOpen.value) { menuOpen.value = false; return; }
        if (inGame && screen.value === 'galaxy') {
          if (selection.value.fleetId) { selection.value = { ...selection.value, fleetId: null, taskForceId: null }; return; }
          menuOpen.value = true;
          return;
        }
        if (screen.value === 'combat' && encounter.value && !encounter.value.finished) return;
        back();
        return;
      }
      if (!inGame || e.ctrlKey || e.metaKey || e.altKey) return;
      if (e.key === 'Enter') {
        if (onFormControl(e)) return;
        e.preventDefault();
        if (briefingOpen.value) { briefingOpen.value = false; return; }
        if (screen.value === 'combat') return;
        requestEndTurn();
        return;
      }
      if (e.key === 'Tab' && screen.value === 'galaxy' && !onFormControl(e)) {
        const vv = view.value;
        if (!vv || !vv.fleets.length) return;
        e.preventDefault();
        // Cycle units: each task force once (by its lead) and each ungrouped ship.
        const units = vv.fleets.filter((f) => !f.taskForceId || vv.taskForces.find((t) => t.id === f.taskForceId)?.fleetIds[0] === f.id);
        const cur = selection.value.fleetId;
        const idx = units.findIndex((f) => f.id === cur);
        const next = units[(idx + (e.shiftKey ? units.length - 1 : 1)) % units.length] ?? units[0]!;
        selectFleet(next.id);
        focusMap(next.x, next.y);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  const outside = current === 'title' || current === 'new' || current === 'saves';
  return (
    <div class={`app ${outside ? 'app-outside' : 'app-ingame'}${prefs.reducedMotion ? ' reduce-motion' : ''}`} data-scale={prefs.uiScale}>
      {outside || !v ? (
        <main class="outside" id="main">
          {current === 'title' ? <TitleScreen /> : current === 'new' ? <NewCampaignScreen /> : <SavesScreen />}
        </main>
      ) : (
        <div class="shell">
          <div class="shell-elbow" aria-hidden="true"><Emblem empire={empire} size={34} /></div>
          <TopBar />
          <Rail />
          <main class="stage" id="main">
            <ScreenView id={current} />
          </main>
          {briefingOpen.value && briefing.value ? <BriefingPanel /> : null}
          {menuOpen.value ? <GameMenu /> : null}
          <EventsPanel />
          <VictoryPanel />
          <OutcomeScreen />
          <HelpPopover />
          <TutorialOverlay />
        </div>
      )}
      <SettingsModal />
      <TooltipLayer />
      <ConfirmLayer />
      <Toasts />
    </div>
  );
}

function ScreenView({ id }: { id: Screen }) {
  switch (id) {
    case 'galaxy': return <GalaxyScreen />;
    case 'system': return <SystemScreen />;
    case 'empire': return <EmpireScreen />;
    case 'research': return <ResearchScreen />;
    case 'fleets': return <FleetsScreen />;
    case 'diplomacy': return <DiplomacyScreen />;
    case 'intel': return <IntelScreen />;
    case 'encyclopedia': return <EncyclopediaScreen />;
    case 'combat': return <CombatScreen />;
    default: return <GalaxyScreen />;
  }
}

function stardate(turn: number): string {
  return (41000 + turn * 25.7).toFixed(1);
}

function TopBar() {
  const v = view.value!;
  const e = v.empire;
  const income = e.income;
  const pendingBattle = v.session.battlePending && !v.session.battleFinished;
  const warnings = endTurnWarnings(v);
  const alertCount = v.alerts.length;
  return (
    <header class="topbar">
      <div class="tb-empire">
        <div class="tb-name">{v.meta.playerName}</div>
        <div class="tb-date" {...tip(`Turn ${v.meta.turn}. ${v.meta.eraName} era · ${v.meta.ruleset === 'reference' ? 'reference rules' : 'prototype rules'} · seed ${v.meta.seed}`)}>
          <span>TURN {v.meta.turn}</span><span class="sep">/</span><span>STARDATE {stardate(v.meta.turn)}</span>
        </div>
      </div>
      <div class="tb-stats">
        <div class="tb-stat" data-testid="credits" tabIndex={0} {...tip(
          `Treasury ${fmt(e.credits)}${e.draftCredits !== e.credits ? ` (${fmt(e.draftCredits)} after this turn's purchases)` : ''}\n` +
          `Expected change at turn end: ${signed(income.net)}\n  Colonies (income − structure upkeep) ${signed(income.colonies)}\n  Trade routes ${signed(income.trade)}\n  Ship maintenance −${fmt(income.ships)}` +
          (income.spend ? `\nOrders already commit ${fmt(income.spend)} credits.` : '') + '\nEvents and battles can change the result.', 'Credits')}>
          <span class="tb-icon ic-credits" aria-hidden="true" />
          <span class="tb-val">{fmt(e.draftCredits)}</span>
          <span class={`tb-delta ${income.net < 0 ? 'neg' : 'pos'}`}>{signed(income.net)}</span>
        </div>
        <div class="tb-stat" tabIndex={0} {...tip(`Research output ${e.researchOutput} per turn, split across the six fields by your allocation.\nOpen Research (F4) to rebalance.`, 'Research')}>
          <span class="tb-icon ic-research" aria-hidden="true" />
          <span class="tb-val">{fmt(e.researchOutput)}</span><span class="tb-unit">/turn</span>
        </div>
        <div class="tb-stat hide-sm" tabIndex={0} {...tip(`Industry ${e.industry} per turn across all colonies; food ${e.food} for ${e.population} citizens.`, 'Production')}>
          <span class="tb-icon ic-industry" aria-hidden="true" />
          <span class="tb-val">{fmt(e.industry)}</span><span class="tb-unit">ind</span>
        </div>
        <div class="tb-stat hide-sm" tabIndex={0} {...tip(`${v.colonies.length} colonies · ${fmt(e.population)} population · ${v.fleets.length} ships`, 'Empire')}>
          <span class="tb-icon ic-pop" aria-hidden="true" />
          <span class="tb-val">{fmt(e.population)}</span><span class="tb-unit">pop</span>
        </div>
        <button type="button" class={`tb-stat tb-alerts ${alertCount ? 'has' : ''}`} onClick={() => goto('galaxy')}
          {...tip(alertCount ? v.alerts.map((a) => `• ${a.text}`).join('\n') : 'No alerts', 'Alerts')}>
          <span class="tb-icon ic-alert" aria-hidden="true" /><span class="tb-val">{alertCount}</span>
        </button>
      </div>
      <div class="tb-actions">
        <HelpButton />
        {briefing.value ? <Pill small tone="ghost" onClick={() => { briefingOpen.value = true; }} hint="Reopen the last turn briefing">Briefing</Pill> : null}
        {v.events?.length ? (
          <Pill small tone={pendingEventCount(v) ? 'warn' : 'ghost'} onClick={() => { eventsOpen.value = true; }} testid="open-events"
            hint={pendingEventCount(v) ? `${pendingEventCount(v)} galactic event${pendingEventCount(v) === 1 ? '' : 's'} await your decision` : 'Active galactic events'}>
            Events{pendingEventCount(v) ? ` · ${pendingEventCount(v)}` : ''}
          </Pill>
        ) : null}
        <Pill small tone="ghost" onClick={() => { victoryOpen.value = true; }} testid="open-victory"
          hint={v.victory?.conditions.map((c) => `${c.name}: ${c.enabled ? `${c.percent}%` : 'off'}`).join('\n')}>Victory</Pill>
        <Pill small tone="tertiary" onClick={() => { menuOpen.value = !menuOpen.value; }} kbd="Esc" testid="menu-button">Menu</Pill>
        <button type="button" class={`end-turn${busy.value ? ' busy' : ''}${pendingBattle ? ' battle' : ''}`} data-testid="end-turn"
          disabled={busy.value || v.meta.outcome !== 'ongoing'}
          onClick={() => requestEndTurn()}
          {...tip(pendingBattle ? 'A battle awaits your orders.' : warnings.length ? warnings.join('\n') : 'All systems ready.', pendingBattle ? 'Battle in progress' : `End turn ${v.meta.turn}`)}>
          <span class="et-label">{pendingBattle ? 'Battle stations' : v.session.active ? 'Continue turn' : 'End turn'}</span>
          <span class="et-sub">{v.pending.length} orders · <kbd>Enter</kbd></span>
        </button>
      </div>
    </header>
  );
}

function Rail() {
  const v = view.value!;
  const current = screen.value;
  const pendingBattle = v.session.battlePending;
  return (
    <nav class="rail" aria-label="Screens">
      {GAME_SCREENS.map((s, i) => (
        <button type="button" key={s.id} class={`rail-btn rb-${i % 4}${current === s.id ? ' active' : ''}`} data-testid={`nav-${s.id}`}
          aria-current={current === s.id ? 'page' : undefined} onClick={() => goto(s.id)} {...tip(`${s.hint} (${s.key})`, s.label)}>
          <span class="rb-key">{s.key}</span>
          <span class="rb-label">{s.label}</span>
        </button>
      ))}
      {pendingBattle || v.battles.length ? (
        <button type="button" class={`rail-btn rb-combat${current === 'combat' ? ' active' : ''}${pendingBattle ? ' pulse' : ''}`} data-testid="nav-combat"
          onClick={() => goto('combat')} {...tip(pendingBattle ? 'A battle awaits your orders' : 'Battle reports and replays', 'Combat')}>
          <span class="rb-key">!</span><span class="rb-label">Combat</span>
        </button>
      ) : null}
      <div class="rail-fill" aria-hidden="true"><span>{`${v.meta.turn}`.padStart(3, '0')}-{String(v.meta.seed).slice(-4)}</span></div>
    </nav>
  );
}

function GameMenu() {
  const v = view.value!;
  const quickSave = () => {
    try {
      const name = saveName.value || `${v.meta.playerName} campaign`;
      saveToSlot(`${name} — turn ${v.meta.turn}`);
      toast('Campaign saved.', 'good');
      menuOpen.value = false;
    } catch (error) {
      toast(error instanceof Error ? error.message : String(error), 'alert');
    }
  };
  return (
    <div class="modal-backdrop" onClick={(e) => { if (e.target === e.currentTarget) menuOpen.value = false; }}>
      <div class="modal game-menu" role="dialog" aria-label="Game menu" data-testid="game-menu">
        <div class="gm-head"><Emblem empire={v.meta.player} size={40} /><div><div class="gm-title">{v.meta.playerName}</div><div class="gm-sub">Turn {v.meta.turn} · {v.meta.eraName} era</div></div></div>
        <div class="gm-actions">
          <Pill wide onClick={() => { menuOpen.value = false; }}>Resume</Pill>
          <Pill wide tone="secondary" onClick={quickSave} testid="quick-save">Quick save</Pill>
          <Pill wide tone="secondary" onClick={() => { menuOpen.value = false; goto('saves'); }} testid="open-saves">Save / load…</Pill>
          {v.meta.outcome !== 'ongoing' ? (
            <Pill wide tone="secondary" onClick={() => { menuOpen.value = false; outcomeDismissed.value = false; }}>
              {v.meta.outcome === 'victory' ? 'Victory summary' : 'Defeat summary'}
            </Pill>
          ) : null}
          <Pill wide tone="ghost" onClick={() => { menuOpen.value = false; victoryOpen.value = true; }} testid="menu-victory">Victory progress…</Pill>
          <Pill wide tone="ghost" onClick={() => { menuOpen.value = false; settingsOpen.value = true; }} testid="open-settings"
            hint="Interface scale, reduced motion, colour-blind palette, sound, autosave, tutorial">Settings…</Pill>
          <Pill wide tone="ghost" active={audioEnabled.value} onClick={() => setAudio(!audioEnabled.value)} hint="Synthesised interface sounds">
            Sound {audioEnabled.value ? 'on' : 'off'}
          </Pill>
          <Pill wide tone="alert" onClick={() => {
            menuOpen.value = false;
            confirmDialog.value = {
              title: 'Leave this campaign?', body: 'Unsaved progress since the last save or autosave will be lost.', confirm: 'Quit to title', tone: 'alert',
              onConfirm: () => quitToTitle(),
            };
          }}>Quit to title</Pill>
        </div>
        <div class="gm-keys">
          <div><kbd>F1</kbd>–<kbd>F8</kbd> screens</div><div><kbd>Enter</kbd> end turn</div><div><kbd>Tab</kbd> next ship</div>
          <div><kbd>Esc</kbd> back / menu</div><div><kbd>Right-click</kbd> orders</div><div><kbd>Wheel</kbd> zoom</div>
        </div>
      </div>
    </div>
  );
}
