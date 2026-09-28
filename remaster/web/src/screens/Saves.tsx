import { useRef, useState } from 'preact/hooks';
import {
  AUTOSAVE_ID, captureEnvelope, deleteSlot, exportEnvelope, importFile, readSlot, saveIndex, saveToSlot, writeSlot,
} from '../state/saves';
import { back, confirmDialog, loadEnvelope, saveName, toast, view } from '../state/store';
import { Emblem, Empty, Frame, Pill } from '../ui/components';
import { Starfield } from '../ui/Starfield';

export function SavesScreen() {
  const v = view.value;
  const [name, setName] = useState(() => (v ? `${saveName.value || `${v.meta.playerName} campaign`} — turn ${v.meta.turn}` : ''));
  const fileRef = useRef<HTMLInputElement>(null);
  const list = saveIndex.value;

  const saveNew = () => {
    if (!v) return;
    try {
      saveToSlot(name.trim() || `${v.meta.playerName} — turn ${v.meta.turn}`);
      toast('Campaign saved.', 'good');
    } catch (error) {
      toast(error instanceof Error ? error.message : String(error), 'alert');
    }
  };
  const load = (id: string) => {
    const env = readSlot(id);
    if (!env) { toast('That save could not be read.', 'alert'); return; }
    const go = () => loadEnvelope(env);
    if (v) confirmDialog.value = { title: `Load "${env.name}"?`, body: 'Unsaved progress in the current campaign will be lost.', confirm: 'Load', onConfirm: go };
    else go();
  };
  const onImport = async (file: File | undefined) => {
    if (!file) return;
    try {
      const env = await importFile(file);
      const id = `i${Date.now().toString(36)}`;
      writeSlot({ ...env, id, name: env.name || file.name });
      toast(`Imported "${env.name}".`, 'good');
    } catch (error) {
      toast(error instanceof Error ? error.message : String(error), 'alert');
    } finally {
      if (fileRef.current) fileRef.current.value = '';
    }
  };

  return (
    <div class="saves-screen" data-testid="screen-saves">
      <Starfield />
      <div class="saves-wrap">
        <header class="setup-head">
          <Pill tone="ghost" small onClick={() => back()} kbd="Esc">Back</Pill>
          <h1>Save &amp; load</h1>
          <span class="setup-sub">Stored in this browser · export .botfsave files to keep or share them</span>
        </header>
        {v ? (
          <Frame title="Save current campaign" tone="secondary">
            <div class="option-row">
              <input class="text-input grow" value={name} maxLength={80} aria-label="Save name" data-testid="save-name"
                onInput={(e) => setName((e.target as HTMLInputElement).value)}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); saveNew(); } }} />
              <Pill onClick={saveNew} testid="save-new">Save</Pill>
              <Pill tone="ghost" onClick={() => { try { exportEnvelope(captureEnvelope(name || 'campaign', 'export')); } catch (e) { toast(String(e), 'alert'); } }}
                hint="Download the current campaign as a .botfsave file">Export current</Pill>
            </div>
            {v.session.active ? <p class="hint">A battle is in progress: the save captures the open turn and resumes mid-battle.</p> : null}
          </Frame>
        ) : null}
        <Frame title="Saved campaigns" sub={`${list.length} slot${list.length === 1 ? '' : 's'}`}
          actions={<>
            <input ref={fileRef} type="file" accept=".botfsave,application/json" class="visually-hidden" id="import-file" data-testid="import-file"
              onChange={(e) => void onImport((e.target as HTMLInputElement).files?.[0])} />
            <Pill small tone="ghost" onClick={() => fileRef.current?.click()} hint="Import a .botfsave file">Import…</Pill>
          </>}>
          {list.length === 0 ? <Empty>No saved campaigns yet.</Empty> : (
            <ul class="save-list" data-testid="save-list">
              {list.map((s) => (
                <li key={s.id} class="save-row">
                  <Emblem empire={s.player} size={34} />
                  <div class="save-info">
                    <div class="save-name">{s.name}{s.id === AUTOSAVE_ID ? <span class="badge badge-accent">auto</span> : null}{s.kind === 'session' ? <span class="badge badge-alert">mid-battle</span> : null}</div>
                    <div class="save-meta">{s.playerName} · turn {s.turn} · {new Date(s.savedAt).toLocaleString()} · {(s.bytes / 1024).toFixed(0)} KB</div>
                  </div>
                  <div class="save-actions">
                    <Pill small onClick={() => load(s.id)} testid={`load-${s.id}`}>Load</Pill>
                    {v ? <Pill small tone="secondary" onClick={() => {
                      confirmDialog.value = { title: `Overwrite "${s.name}"?`, body: 'The slot will hold the current campaign.', confirm: 'Overwrite',
                        onConfirm: () => { try { saveToSlot(s.name, s.id); toast('Saved.', 'good'); } catch (e) { toast(String(e), 'alert'); } } };
                    }}>Overwrite</Pill> : null}
                    <Pill small tone="ghost" onClick={() => { const env = readSlot(s.id); if (env) exportEnvelope(env); }}>Export</Pill>
                    <Pill small tone="alert" onClick={() => {
                      confirmDialog.value = { title: `Delete "${s.name}"?`, body: 'This cannot be undone.', confirm: 'Delete', tone: 'alert', onConfirm: () => deleteSlot(s.id) };
                    }}>Delete</Pill>
                  </div>
                </li>
              ))}
            </ul>
          )}
        </Frame>
      </div>
    </div>
  );
}
