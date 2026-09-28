// Save-file export that works both in a plain browser and inside the claude.ai Artifact viewer.
//
// Inside the viewer, plain <a download>/blob downloads are blocked; the host offers a `downloads`
// capability through `window.claude.use('downloads')` instead. Its allowlist accepts `.txt` (not
// `.botfsave`), so exports made through it are named `<name>.botfsave.txt`.
//
//  * No `window.claude` / `use` (plain browser, file://): the anchor download, unchanged.
//  * `use()` resolves null (no viewer answered; can take ~10 s): anchor download.
//  * `save()` rejects with {code:'declined'}: the viewer's user said no — a quiet no-op.
//  * {code:'unavailable' | 'not_granted' | anything else}: anchor download.
//
// `use()` is only ever called from an explicit export click, never at load.

export type ExportOutcome = 'saved' | 'declined' | 'downloaded';

interface DownloadsCapability { save(file: { filename: string; data: string }): Promise<unknown> }
interface ClaudeRuntime { use?: (capability: string) => Promise<DownloadsCapability | null | undefined> }

function runtime(): ClaudeRuntime | null {
  try {
    const claude = (window as unknown as { claude?: ClaudeRuntime }).claude;
    return claude && typeof claude.use === 'function' ? claude : null;
  } catch {
    return null;
  }
}

/** True when an export click will first ask the host viewer (so the UI can show "Preparing file…"). */
export function hostDownloadsPossible(): boolean { return runtime() !== null; }

/** File-system friendly base name. */
export function safeFileName(name: string, fallback = 'campaign'): string {
  return name.replace(/[^\w\- ]+/g, '').trim().replace(/\s+/g, '-').slice(0, 60) || fallback;
}

/** The classic download: a temporary object URL clicked through an anchor. */
export function anchorDownload(filename: string, text: string, mime = 'application/json'): void {
  const blob = new Blob([text], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.rel = 'noopener';
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}

function errorCode(error: unknown): string {
  if (error && typeof error === 'object' && 'code' in error) return String((error as { code: unknown }).code);
  return '';
}

/**
 * Exports `text` as a save file. `baseName` has no extension: the anchor path writes
 * `<baseName>.botfsave`, the host path `<baseName>.botfsave.txt`.
 */
export async function exportSaveText(baseName: string, text: string): Promise<ExportOutcome> {
  const claude = runtime();
  if (claude) {
    let downloads: DownloadsCapability | null | undefined = null;
    try { downloads = await claude.use!('downloads'); } catch { downloads = null; }
    if (downloads && typeof downloads.save === 'function') {
      try {
        await downloads.save({ filename: `${baseName}.botfsave.txt`, data: text });
        return 'saved';
      } catch (error) {
        if (errorCode(error) === 'declined') return 'declined';
        // 'unavailable', 'not_granted' or unknown: fall back to the anchor download below.
      }
    }
  }
  anchorDownload(`${baseName}.botfsave`, text);
  return 'downloaded';
}

/** File input `accept` for imports: native saves plus the viewer's `.botfsave.txt` exports. */
export const SAVE_FILE_ACCEPT = '.botfsave,.txt,text/plain,application/json';
