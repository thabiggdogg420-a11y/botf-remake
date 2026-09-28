import { expect, test } from '@playwright/test';
import type { GameView } from '../src/core/types';
import { centreOn, endTurn, fleetPoint, newCampaign, openContextMenu, view } from './helpers';

const cheb = (a: { x: number; y: number }, b: { x: number; y: number }) => Math.max(Math.abs(a.x - b.x), Math.abs(a.y - b.y));

test('campaign: scout, colonize, build, staff, research, turns, screens, save/reload', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await newCampaign(page, 1);
  let v = await view(page);
  const W = v.meta.width, H = v.meta.height;
  const home = v.systems.find((s) => s.id === v.colonies[0]!.systemId)!;

  // --- Move the scout through the right-click context menu --------------------------------
  const scout = v.fleets.find((f) => f.shipClass === 'Scout')!;
  const tx = Math.max(0, Math.min(W - 1, scout.x + (scout.x < W / 2 ? 3 : -3)));
  const ty = Math.max(0, Math.min(H - 1, scout.y + (scout.y < H / 2 ? 2 : -2)));
  let p = await fleetPoint(page, scout.id);
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('unit-card')).toBeVisible();
  await centreOn(page, Math.round((scout.x + tx) / 2), Math.round((scout.y + ty) / 2));
  await openContextMenu(page, ty * W + tx);
  await page.locator('[data-testid=context-menu] [data-action=move]').click();
  v = await view(page);
  expect(v.pending.some((o) => o.type === 'MoveFleet' && o.fleetId === scout.id)).toBeTruthy();
  expect(v.fleets.find((f) => f.id === scout.id)!.eta).toBeGreaterThan(0);

  // --- Send the colony ship to the nearest free, habitable, surveyed system ------------------
  const ship = v.fleets.find((f) => f.colonyShip)!;
  const target = v.systems
    .filter((s) => s.owner === 0 && !s.minor && s.planets.some((pl) => pl.capacity > 0))
    .sort((a, b) => cheb(a, home) - cheb(b, home))[0]!;
  expect(target, 'a colonizable system is surveyed at start').toBeTruthy();
  await centreOn(page, home.x, home.y);
  p = await fleetPoint(page, ship.id);
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('unit-card')).toContainText(/colony/i);
  await centreOn(page, target.x, target.y);
  await openContextMenu(page, target.y * W + target.x);
  await page.locator('[data-testid=context-menu] [data-action=move]').click();

  // --- Queue construction on the System screen -------------------------------------------
  await page.keyboard.press('F2');
  await expect(page.getByTestId('screen-system')).toBeVisible();
  const queueBefore = await page.locator('li.queue-item').count();
  await page.locator('[data-testid=build-options] button.choice:not(.disabled)').first().click();
  await expect(page.locator('li.queue-item')).toHaveCount(queueBefore + 1);

  // --- Change the workforce with the live forecast slider --------------------------------
  const slider = page.getByTestId('wf-research');
  await slider.focus();
  await page.keyboard.press('ArrowLeft');
  await expect.poll(async () => (await view(page)).pending.some((o) => o.type === 'AssignWorkers')).toBeTruthy();

  // --- Research allocation ----------------------------------------------------------------
  await page.keyboard.press('F4');
  await expect(page.getByTestId('screen-research')).toBeVisible();
  const alloc = page.getByTestId('alloc-5');
  await alloc.focus();
  for (let i = 0; i < 4; i++) await page.keyboard.press('ArrowRight');
  await page.getByTestId('apply-research').click();
  await expect.poll(async () => (await view(page)).pending.some((o) => o.type === 'AllocateResearch')).toBeTruthy();

  // --- End turns until the colony ship arrives, then colonize ---------------------------
  await page.keyboard.press('F1');
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  for (let i = 0; i < 10; i++) {
    v = await view(page);
    const s = v.fleets.find((f) => f.id === ship.id)!;
    if (s.x === target.x && s.y === target.y) break;
    await endTurn(page);
  }
  v = await view(page);
  expect(v.fleets.find((f) => f.id === ship.id)).toMatchObject({ x: target.x, y: target.y });
  await centreOn(page, target.x, target.y);
  p = await fleetPoint(page, ship.id);
  await page.mouse.click(p.x, p.y);
  await openContextMenu(page, target.y * W + target.x);
  const colonize = page.locator('[data-testid=context-menu] [data-action=colonize]');
  await expect(colonize).not.toHaveAttribute('aria-disabled', 'true');
  await colonize.click();
  const coloniesBefore = (await view(page)).colonies.length;
  await endTurn(page);
  await expect.poll(async () => (await view(page)).colonies.length).toBe(coloniesBefore + 1);

  // --- A few more turns -------------------------------------------------------------------
  for (let i = 0; i < 3; i++) await endTurn(page);
  v = await view(page);
  expect(v.meta.turn).toBeGreaterThanOrEqual(5);

  // --- Every screen opens ------------------------------------------------------------------
  const screens: [string, string][] = [
    ['F1', 'screen-galaxy'], ['F2', 'screen-system'], ['F3', 'screen-empire'], ['F4', 'screen-research'],
    ['F5', 'screen-fleets'], ['F6', 'screen-diplomacy'], ['F7', 'screen-intel'], ['F8', 'screen-encyclopedia'],
  ];
  for (const [key, id] of screens) {
    await page.keyboard.press(key);
    await expect(page.getByTestId(id)).toBeVisible();
  }
  await page.getByTestId('nav-empire').click();
  await expect(page.getByTestId('colony-table')).toContainText(target.name);
  await page.getByTestId('nav-encyclopedia').click();
  await page.getByTestId('ency-search').fill('Galaxy');
  await expect(page.getByTestId('ency-detail')).toBeVisible();

  // --- Save, reload the page, continue ----------------------------------------------------
  await page.getByTestId('nav-galaxy').click();
  const fingerprint = await page.evaluate(() => window.__botf!.fingerprint());
  const turn = (await view(page)).meta.turn;
  await page.getByTestId('menu-button').click();
  await page.getByTestId('quick-save').click();
  await page.reload();
  await expect(page.getByTestId('screen-title')).toBeVisible();
  await page.getByTestId('continue').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  expect((await view(page)).meta.turn).toBe(turn);
  expect(await page.evaluate(() => window.__botf!.fingerprint())).toBe(fingerprint);

  // Named save through the save/load screen
  await page.getByTestId('menu-button').click();
  await page.getByTestId('open-saves').click();
  await page.getByTestId('save-name').fill('E2E checkpoint');
  await page.getByTestId('save-new').click();
  await expect(page.getByTestId('save-list')).toContainText('E2E checkpoint');
  expect(errors).toEqual([]);
});

test('tactical drill: command a battle round by round, then auto-resolve and replay', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await page.goto('/');
  await page.getByTestId('tactical-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  await expect(page.getByTestId('encounter-choice')).toBeVisible();
  // Strategic orders are locked while the battle is pending.
  let v = await page.evaluate(() => window.__botf!.view() as GameView);
  expect(v.session.battlePending).toBeTruthy();
  await page.getByTestId('choice-fight').click();
  await page.getByTestId('maneuver-Charge').click();
  await page.getByTestId('resolve-round').click();
  await expect(page.getByTestId('combat-log')).not.toContainText('deploy');
  await page.getByTestId('maneuver-Assault').click();
  await page.getByTestId('resolve-round').click();
  await page.getByTestId('auto-resolve').click();
  await expect(page.getByTestId('continue-turn')).toBeVisible();
  await page.getByTestId('continue-turn').click();
  await expect(page.getByTestId('briefing')).toBeVisible();
  await expect(page.getByTestId('briefing')).toContainText(/Battle at/);
  await page.getByTestId('briefing').getByText('Replay ›').first().click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  await expect(page.getByTestId('replay')).toBeVisible();
  await page.getByTestId('replay-next').click();
  await page.getByTestId('replay-next').click();
  await expect(page.getByTestId('replay')).toContainText('round 2');
  v = await page.evaluate(() => window.__botf!.view() as GameView);
  expect(v.meta.turn).toBe(1);
  expect(v.battles.length).toBeGreaterThan(0);
  expect(errors).toEqual([]);
});
