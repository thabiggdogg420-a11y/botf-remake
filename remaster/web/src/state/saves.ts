// Named save slots in localStorage, autosave, and .botfsave export/import.
import { signal } from '@preact/signals';
import { core } from '../core/engine';

export interface SaveEnvelope {
  format: 'botf-web-save';
  version: 1;
  id: string;
  name: string;
  savedAt: string;
  turn: number;
  player: number;
  playerName: string;
  scenario: string;
  kind: 'game' | 'session';
  fingerprint: string;
  contentVersion: string;
  data: string;
  briefing?: string;
}
export type SaveSummary = Omit<SaveEnvelope, 'data' | 'briefing'> & { bytes: number };

const INDEX_KEY = 'botf.saves.index';
const SLOT_PREFIX = 'botf.save.';
export const AUTOSAVE_ID = 'autosave';

export const saveIndex = signal<SaveSummary[]>(readIndex());

function storage(): Storage | null {
  try { return window.localStorage; } catch { return null; }
}

function readIndex(): SaveSummary[] {
  try {
    const raw = storage()?.getItem(INDEX_KEY);
    const list = raw ? (JSON.parse(raw) as SaveSummary[]) : [];
    return Array.isArray(list) ? list : [];
  } catch {
    return [];
  }
}

function writeIndex(list: SaveSummary[]): void {
  list.sort((a, b) => (a.savedAt < b.savedAt ? 1 : -1));
  storage()?.setItem(INDEX_KEY, JSON.stringify(list));
  saveIndex.value = list;
}

// Lazy import avoids a static cycle with the store (store imports saves for autosave).
async function storeModule() { return import('./store'); }

export function captureEnvelope(name: string, id: string): SaveEnvelope {
  const blob = core.save();
  const v = core.view();
  // The briefing is UI chrome, not campaign state; kept alongside so a reload shows the last report.
  const briefing = core.briefing();
  return {
    format: 'botf-web-save',
    version: 1,
    id,
    name,
    savedAt: new Date().toISOString(),
    turn: blob.turn,
    player: blob.player,
    playerName: v.meta.playerName,
    scenario: blob.scenario,
    kind: blob.kind,
    fingerprint: String(blob.fingerprint),
    contentVersion: v.meta.contentVersion,
    data: blob.data,
    briefing: briefing ? JSON.stringify(briefing) : undefined,
  };
}

export function writeSlot(envelope: SaveEnvelope): void {
  const store = storage();
  if (!store) throw new Error('Browser storage is unavailable (private mode?). Use Export instead.');
  const text = JSON.stringify(envelope);
  try {
    store.setItem(SLOT_PREFIX + envelope.id, text);
  } catch {
    throw new Error('Browser storage is full. Delete old saves or export this one to a file.');
  }
  const { data: _data, briefing: _briefing, ...summary } = envelope;
  writeIndex([...readIndex().filter((s) => s.id !== envelope.id), { ...summary, bytes: text.length }]);
}

export function readSlot(id: string): SaveEnvelope | null {
  try {
    const text = storage()?.getItem(SLOT_PREFIX + id);
    return text ? parseEnvelope(text) : null;
  } catch {
    return null;
  }
}

export function deleteSlot(id: string): void {
  storage()?.removeItem(SLOT_PREFIX + id);
  writeIndex(readIndex().filter((s) => s.id !== id));
}

export function saveToSlot(name: string, id?: string): SaveEnvelope {
  const slotId = id ?? `s${Date.now().toString(36)}`;
  const envelope = captureEnvelope(name, slotId);
  writeSlot(envelope);
  return envelope;
}

export function autosave(): void {
  try {
    const v = core.view();
    if (v.meta.scenario === 'drill') return;
    saveToSlot(`Autosave — ${v.meta.playerName}, turn ${v.meta.turn}`, AUTOSAVE_ID);
  } catch (error) {
    void storeModule().then((m) => m.toast(`Autosave failed: ${error instanceof Error ? error.message : String(error)}`, 'warn'));
  }
}

export function parseEnvelope(text: string): SaveEnvelope {
  const parsed = JSON.parse(text) as Partial<SaveEnvelope>;
  if (parsed.format !== 'botf-web-save' || typeof parsed.data !== 'string')
    throw new Error('Not a BOTF Remastered save file.');
  return parsed as SaveEnvelope;
}

export function exportEnvelope(envelope: SaveEnvelope): void {
  const blob = new Blob([JSON.stringify(envelope, null, 1)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  const safe = envelope.name.replace(/[^\w\- ]+/g, '').trim().replace(/\s+/g, '-').slice(0, 60) || 'campaign';
  a.href = url;
  a.download = `${safe}-t${envelope.turn}.botfsave`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}

export async function importFile(file: File): Promise<SaveEnvelope> {
  const text = await file.text();
  return parseEnvelope(text);
}
