import { expect, type Page } from '@playwright/test';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import type { GameView } from '../src/core/types';

export const SHOTS_DIR = process.env.BOTF_SHOTS_DIR
  ?? join(tmpdir(), 'botf-web-shots');

export async function view(page: Page): Promise<GameView> {
  return page.evaluate(() => window.__botf!.view() as GameView);
}

export async function settle(page: Page): Promise<void> {
  await page.waitForFunction(() => window.__botf?.map?.settled() ?? true, null, { timeout: 8000 });
}

export async function cellPoint(page: Page, cell: number): Promise<{ x: number; y: number }> {
  await settle(page);
  const p = await page.evaluate((c) => window.__botf!.map!.cellCenter(c), cell);
  expect(p).not.toBeNull();
  return p!;
}

export async function fleetPoint(page: Page, id: number): Promise<{ x: number; y: number }> {
  await settle(page);
  await page.waitForFunction((fid) => !!window.__botf?.map?.fleetMarker(fid), id, { timeout: 5000 });
  return (await page.evaluate((fid) => window.__botf!.map!.fleetMarker(fid), id))!;
}

/** Opens the sector context menu (Shift+right-click always shows the menu). */
export async function openContextMenu(page: Page, cell: number): Promise<void> {
  const p = await cellPoint(page, cell);
  await page.mouse.move(p.x, p.y);
  await page.keyboard.down('Shift');
  await page.mouse.click(p.x, p.y, { button: 'right' });
  await page.keyboard.up('Shift');
  await expect(page.getByTestId('context-menu')).toBeVisible();
}

/** Centres the chart on a sector so it is clear of overlays before clicking it. */
export async function centreOn(page: Page, x: number, y: number): Promise<void> {
  // Uses the same focus signal the UI uses (alerts, briefing links, Tab cycling).
  await page.evaluate(({ x: cx, y: cy }) => window.__botf!.focus(cx, cy, 1.4), { x, y });
  await settle(page);
}

/** BOTF_CPU_THROTTLE=N slows Chromium's CPU N times, reproducing timing on slow CI runners. */
export async function throttleCpu(page: Page): Promise<void> {
  const rate = Number(process.env.BOTF_CPU_THROTTLE ?? 0);
  if (rate > 1) {
    const cdp = await page.context().newCDPSession(page);
    await cdp.send('Emulation.setCPUThrottlingRate', { rate });
  }
}

export async function newCampaign(page: Page, faction = 1, seed = '424242', options: { tutorial?: boolean } = {}): Promise<void> {
  await throttleCpu(page);
  await page.goto('/');
  // A fresh browser profile, with the First Contact tutorial already dismissed unless a test wants it.
  await page.evaluate((keepTutorial) => {
    localStorage.clear();
    if (!keepTutorial) localStorage.setItem('botf.tutorial', 'skipped');
  }, !!options.tutorial);
  await page.goto('/');
  await page.getByTestId('new-campaign').click();
  await expect(page.getByTestId('screen-new')).toBeVisible();
  await page.getByTestId(`faction-${faction}`).click();
  await page.getByTestId('seed-input').fill(seed);
  await page.getByTestId('start-campaign').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await settle(page);
}

/** Ends the turn through the UI (confirming the end-turn dialog) and dismisses the briefing. */
export async function endTurn(page: Page): Promise<void> {
  const before = (await view(page)).meta.turn;
  await page.getByTestId('end-turn').click();
  const ok = page.getByTestId('confirm-ok');
  if (await ok.isVisible().catch(() => false)) await ok.click();
  else {
    await ok.waitFor({ state: 'visible', timeout: 1500 }).then(() => ok.click()).catch(() => undefined);
  }
  await page.waitForFunction((t) => {
    const v = window.__botf!.view() as GameView;
    return v.meta.turn > t || v.session.battlePending;
  }, before, { timeout: 15000 });
  const v = await view(page);
  if (v.session.battlePending) {
    await expect(page.getByTestId('screen-combat')).toBeVisible();
    const choice = page.getByTestId('choice-auto');
    if (await choice.isVisible().catch(() => false)) await choice.click();
    else await page.getByTestId('auto-resolve').click();
    await page.getByTestId('continue-turn').click();
    await page.waitForFunction((t) => (window.__botf!.view() as GameView).meta.turn > t, before);
  }
  const briefing = page.getByTestId('briefing');
  if (await briefing.isVisible().catch(() => false)) await page.getByTestId('briefing-close').click();
}
