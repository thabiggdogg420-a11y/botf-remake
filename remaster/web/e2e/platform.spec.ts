import { expect, test, type Page } from '@playwright/test';
import { newCampaign } from './helpers';

// --- Settings -----------------------------------------------------------------------------------
test('settings: scale, reduced motion, colour-blind palette, sound and autosave persist', async ({ page }) => {
  await page.goto('/');
  await page.evaluate(() => localStorage.clear());
  await page.goto('/');
  await page.getByTestId('title-settings').click();
  const panel = page.getByTestId('settings');
  await expect(panel).toBeVisible();
  const scale = page.getByTestId('setting-scale');
  await scale.focus();
  await page.keyboard.press('ArrowRight');
  await page.keyboard.press('ArrowRight');
  await expect(page.getByTestId('setting-scale-value')).toHaveText('110%');
  for (const id of ['setting-motion', 'setting-colorblind', 'setting-sound']) {
    const before = await page.getByTestId(id).getAttribute('aria-checked');
    await page.getByTestId(id).click();
    await expect(page.getByTestId(id)).toHaveAttribute('aria-checked', before === 'true' ? 'false' : 'true');
  }
  await page.getByTestId('setting-autosave').click();
  await expect(page.getByTestId('setting-autosave')).toHaveAttribute('aria-checked', 'false');
  await page.getByTestId('settings-close').click();

  await page.reload();
  const root = await page.evaluate(() => ({
    scale: getComputedStyle(document.documentElement).getPropertyValue('--ui-scale').trim(),
    motion: document.documentElement.dataset.motion, palette: document.documentElement.dataset.palette,
    zoom: getComputedStyle(document.querySelector('.app')!).zoom,
  }));
  expect(root).toMatchObject({ scale: '1.1', palette: 'colorblind' });
  expect(Number(root.zoom)).toBeCloseTo(1.1, 2);
  await page.getByTestId('title-settings').click();
  await expect(page.getByTestId('setting-scale-value')).toHaveText('110%');
  await expect(page.getByTestId('setting-autosave')).toHaveAttribute('aria-checked', 'false');
  await expect(page.getByTestId('setting-colorblind')).toHaveAttribute('aria-checked', 'true');
  await expect(page.getByTestId('setting-sound')).toHaveAttribute('aria-checked', 'true');
  const motionNow = await page.getByTestId('setting-motion').getAttribute('aria-checked');
  expect(root.motion).toBe(motionNow === 'true' ? 'reduced' : 'full');
  await page.getByTestId('settings-close').click();

  // The scaled interface still hit-tests the star chart correctly (canvases are counter-zoomed).
  await page.evaluate(() => localStorage.setItem('botf.tutorial', 'skipped'));
  await page.getByTestId('new-campaign').click();
  await page.getByTestId('start-campaign').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.waitForFunction(() => window.__botf?.map?.settled() ?? false);
  // The counter-zoomed star chart still fills the (zoomed) stage exactly.
  const sizes = await page.evaluate(() => {
    const m = document.querySelector('.galaxy-map')!.getBoundingClientRect(), s = document.querySelector('.stage')!.getBoundingClientRect();
    return { dw: Math.abs(m.width - s.width), dh: Math.abs(m.height - s.height) };
  });
  expect(sizes.dw).toBeLessThan(4);
  expect(sizes.dh).toBeLessThan(4);
  const v = await page.evaluate(() => window.__botf!.view() as import('../src/core/types').GameView);
  const scout = v.fleets.find((f) => f.shipClass === 'Scout')!;
  await page.waitForFunction((id) => !!window.__botf?.map?.fleetMarker(id), scout.id);
  const p = (await page.evaluate((id) => window.__botf!.map!.fleetMarker(id), scout.id))!;
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('unit-card')).toContainText(scout.name.split(' — ')[0]!);
  // Autosave was switched off: ending a turn leaves no autosave slot.
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await page.waitForFunction(() => (window.__botf!.view() as { meta: { turn: number } }).meta.turn >= 1);
  expect(await page.evaluate(() => localStorage.getItem('botf.save.autosave'))).toBeNull();
});

test('the app keeps working when localStorage throws (saves live in memory)', async ({ page }) => {
  await page.addInitScript(() => {
    const deny = () => { throw new DOMException('denied', 'SecurityError'); };
    Storage.prototype.setItem = deny;
    Storage.prototype.getItem = deny;
    Storage.prototype.removeItem = deny;
  });
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto('/');
  await page.getByTestId('new-campaign').click();
  await page.getByTestId('start-campaign').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('menu-button').click();
  await page.getByTestId('open-saves').click();
  await page.getByTestId('save-name').fill('Memory slot');
  await page.getByTestId('save-new').click();
  await expect(page.getByTestId('save-list')).toContainText('Memory slot');
  await expect(page.getByTestId('storage-degraded')).toBeVisible();
  expect(errors).toEqual([]);
});

// --- Save export: plain browser vs. the claude.ai Artifact viewer's downloads capability ----------
async function openSaves(page: Page): Promise<void> {
  await newCampaign(page, 1, '31337');
  await page.getByTestId('menu-button').click();
  await page.getByTestId('open-saves').click();
  await expect(page.getByTestId('screen-saves')).toBeVisible();
}

/** Installs a fake `window.claude.use('downloads')` whose save() resolves or rejects with `code`. */
async function fakeViewer(page: Page, mode: 'ok' | 'declined' | 'unavailable' | 'not_granted' | 'null'): Promise<void> {
  await page.addInitScript((m) => {
    const w = window as unknown as { claude: unknown; __saved: unknown[] };
    w.__saved = [];
    w.claude = {
      use: async (name: string) => {
        if (name !== 'downloads' || m === 'null') return null;
        return {
          save: async (file: { filename: string; data: string }) => {
            if (m !== 'ok') throw { code: m };
            w.__saved.push({ filename: file.filename, size: file.data.length });
          },
        };
      },
    };
  }, mode);
}

test('export without a host viewer uses a plain .botfsave download', async ({ page }) => {
  await openSaves(page);
  const download = page.waitForEvent('download');
  await page.getByTestId('export-current').click();
  expect((await download).suggestedFilename()).toMatch(/\.botfsave$/);
});

test('export inside the artifact viewer saves .botfsave.txt through the downloads capability', async ({ page }) => {
  await fakeViewer(page, 'ok');
  await openSaves(page);
  let downloaded = false;
  page.on('download', () => { downloaded = true; });
  await page.getByTestId('export-current').click();
  await expect.poll(() => page.evaluate(() => (window as unknown as { __saved: { filename: string }[] }).__saved.length)).toBe(1);
  const saved = await page.evaluate(() => (window as unknown as { __saved: { filename: string; size: number }[] }).__saved[0]!);
  expect(saved.filename).toMatch(/\.botfsave\.txt$/);
  expect(saved.size).toBeGreaterThan(100);
  expect(downloaded).toBe(false);
});

for (const code of ['unavailable', 'not_granted', 'null'] as const) {
  test(`export falls back to the anchor download when the viewer answers ${code}`, async ({ page }) => {
    await fakeViewer(page, code);
    await openSaves(page);
    const download = page.waitForEvent('download');
    await page.getByTestId('export-current').click();
    expect((await download).suggestedFilename()).toMatch(/\.botfsave$/);
  });
}

test('a declined viewer export is a quiet no-op', async ({ page }) => {
  await fakeViewer(page, 'declined');
  await openSaves(page);
  let downloaded = false;
  page.on('download', () => { downloaded = true; });
  await page.getByTestId('export-current').click();
  await expect(page.getByTestId('export-current')).toContainText('Export current');
  await page.waitForTimeout(500);
  expect(downloaded).toBe(false);
});

test('import accepts .botfsave.txt files exported by the viewer', async ({ page }) => {
  await openSaves(page);
  expect(await page.getByTestId('import-file').getAttribute('accept')).toContain('.txt');
  const text = await page.evaluate(() => {
    // A real envelope of the current campaign, as the viewer would have stored it.
    const raw = localStorage.getItem('botf.save.autosave');
    return raw ?? '';
  });
  expect(text.length).toBeGreaterThan(100);
  await page.getByTestId('import-file').setInputFiles({ name: 'campaign-t0.botfsave.txt', mimeType: 'text/plain', buffer: Buffer.from(text) });
  await expect(page.getByTestId('save-list').locator('.save-row')).toHaveCount(2);
});
