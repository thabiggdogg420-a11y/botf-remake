import { AUTOSAVE_ID, readSlot, saveIndex } from '../state/saves';
import { goto, loadEnvelope, meta, startDiplomacyDrill, startDrill } from '../state/store';
import { settingsOpen } from '../ui/SettingsPanel';
import { audioEnabled, setAudio } from '../ui/audio';
import { Emblem, Pill } from '../ui/components';
import { Starfield } from '../ui/Starfield';

export function TitleScreen() {
  const auto = saveIndex.value.find((s) => s.id === AUTOSAVE_ID);
  const latest = saveIndex.value[0];
  const resume = auto ?? latest;
  const m = meta.value;
  return (
    <div class="title-screen" data-testid="screen-title">
      <Starfield />
      <div class="title-nebula" aria-hidden="true" />
      <div class="title-planet" aria-hidden="true" />
      <div class="title-content">
        <div class="title-emblems" aria-hidden="true">
          {[1, 2, 3, 4, 5].map((id) => <Emblem key={id} empire={id} size={26} />)}
        </div>
        <h1 class="title-logo">
          <span class="tl-small">Birth of the Federation</span>
          <span class="tl-big">BOTF<em>Remastered</em></span>
        </h1>
        <p class="title-tag">Five powers. One quadrant. A modern reconstruction of the 1999 grand strategy classic, running on its deterministic simulation core.</p>
        <div class="title-menu">
          {resume ? (
            <Pill tone="primary" wide testid="continue" onClick={() => { const env = readSlot(resume.id); if (env) loadEnvelope(env); }}
              hint={`${resume.name}\nSaved ${new Date(resume.savedAt).toLocaleString()}`}>
              Continue <span class="pill-meta">{resume.playerName} · turn {resume.turn}</span>
            </Pill>
          ) : null}
          <Pill tone={resume ? 'secondary' : 'primary'} wide testid="new-campaign" onClick={() => goto('new')}>New campaign</Pill>
          <Pill tone="secondary" wide testid="load-campaign" onClick={() => goto('saves')} hint="Saved campaigns and .botfsave files">Load campaign</Pill>
          <Pill tone="tertiary" wide testid="tactical-drill" onClick={() => startDrill()} hint="A ready-made skirmish at Sol to practise tactical command">Tactical drill</Pill>
          <Pill tone="tertiary" wide testid="diplomacy-drill" onClick={() => startDiplomacyDrill()} hint="A ready-made diplomatic crisis: incoming proposals, a tribute demand and an ally's call to arms">Diplomacy drill</Pill>
          <div class="title-row">
            <Pill tone="ghost" wide active={audioEnabled.value} onClick={() => setAudio(!audioEnabled.value)}>Sound: {audioEnabled.value ? 'on' : 'off'}</Pill>
            <Pill tone="ghost" wide onClick={() => { settingsOpen.value = true; }} testid="title-settings">Settings</Pill>
          </div>
        </div>
      </div>
      <footer class="title-foot">
        <span>Simulation core {m?.contentVersion ?? '…'} · save schema {m?.saveVersion ?? '…'}</span>
        <span>Unofficial fan reconstruction · not affiliated with any rights holder</span>
      </footer>
    </div>
  );
}
