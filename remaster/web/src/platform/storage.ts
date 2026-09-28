// Guarded localStorage access. Storage can be missing, throw (sandboxed/artifact viewers, private
// mode, quota) or come back empty; the app must keep working, just without persistence.

function store(): Storage | null {
  try { return typeof window !== 'undefined' ? window.localStorage : null; } catch { return null; }
}

export function readText(key: string): string | null {
  try { return store()?.getItem(key) ?? null; } catch { return null; }
}

/** Returns false when the value could not be stored (no storage, quota, sandbox). */
export function writeText(key: string, value: string): boolean {
  try {
    const s = store();
    if (!s) return false;
    s.setItem(key, value);
    return true;
  } catch {
    return false;
  }
}

export function removeKey(key: string): void {
  try { store()?.removeItem(key); } catch { /* ignore */ }
}

export function readJson<T>(key: string, fallback: T): T {
  const text = readText(key);
  if (!text) return fallback;
  try { return JSON.parse(text) as T; } catch { return fallback; }
}

export function writeJson(key: string, value: unknown): boolean {
  try { return writeText(key, JSON.stringify(value)); } catch { return false; }
}

export function storageAvailable(): boolean { return store() !== null; }
