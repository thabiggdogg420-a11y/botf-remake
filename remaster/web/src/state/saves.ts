// Named save slots in localStorage, autosave, and .botfsave export/import.
// Storage access is always guarded: when localStorage is missing or throws (private mode, sandboxed
// or artifact viewers, quota) slots live in memory for this session so saving and continuing still
// work, just without persistence across reloads.
import { signal } from '@preact/signals';
import { core } from '../core/engine';
import { exportSaveText, safeFileName, type ExportOutcome } from '../platform/fileExport';
import { readJson, readText, removeKey, writeJson, writeText } from '../platform/storage';
import { settings } from './settings';

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

// Session-only fallback when the browser refuses to store a slot.
const memorySlots = new Map<string, string>();
let memoryIndex: SaveSummary[] | null = null;

export const saveIndex = signal<SaveSummary[]>(readIndex());
/** True once a slot could only be kept in memory (shown as a hint on the save screen). */
export const storageDegraded = signal(false);

function readIndex(): SaveSummary[] {
  if (memoryIndex) return memoryIndex;
  const list = readJson<SaveSummary[]>(INDEX_KEY, []);
  return Array.isArray(list) ? list : [];
}

function writeIndex(list: SaveSummary[]): void {
  list.sort((a, b) => (a.savedAt < b.savedAt ? 1 : -1));
  if (memoryIndex || !writeJson(INDEX_KEY, list)) memoryIndex = list;
  saveIndex.value = [...list];
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
  const text = JSON.stringify(envelope);
  if (!writeText(SLOT_PREFIX + envelope.id, text)) {
    // Storage full or unavailable: keep the slot for this session and say so.
    memorySlots.set(envelope.id, text);
    if (!memoryIndex) memoryIndex = readIndex();
    storageDegraded.value = true;
  } else memorySlots.delete(envelope.id);
  const { data: _data, briefing: _briefing, ...summary } = envelope;
  writeIndex([...readIndex().filter((s) => s.id !== envelope.id), { ...summary, bytes: text.length }]);
}

export function readSlot(id: string): SaveEnvelope | null {
  try {
    const text = memorySlots.get(id) ?? readText(SLOT_PREFIX + id);
    return text ? parseEnvelope(text) : null;
  } catch {
    return null;
  }
}

export function deleteSlot(id: string): void {
  memorySlots.delete(id);
  removeKey(SLOT_PREFIX + id);
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
    if (!settings.value.autosave) return;
    const v = core.view();
    if (v.meta.scenario === 'drill' || v.meta.scenario === 'diplomacy') return;
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

/** Exports a save through the host viewer's downloads capability or a plain download (see fileExport). */
export function exportEnvelope(envelope: SaveEnvelope): Promise<ExportOutcome> {
  return exportSaveText(`${safeFileName(envelope.name)}-t${envelope.turn}`, JSON.stringify(envelope, null, 1));
}

export async function importFile(file: File): Promise<SaveEnvelope> {
  const text = await file.text();
  return parseEnvelope(text);
}
