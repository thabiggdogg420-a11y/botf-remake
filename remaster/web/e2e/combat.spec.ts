import { expect, test, type Page } from '@playwright/test';
import type { Encounter, GameView } from '../src/core/types';
import { throttleCpu } from './helpers';

async function encounterOf(page: Page): Promise<Encounter> {
  return page.evaluate(() => window.__botf!.encounter() as Encounter);
}

async function openDrillBattle(page: Page): Promise<void> {
  await throttleCpu(page);
  await page.goto('/');
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('botf.tutorial', 'skipped'); });
  await page.goto('/');
  await page.getByTestId('tactical-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  await expect(page.getByTestId('encounter-choice')).toBeVisible();
}

test('tactical drill: AI previews, retreat disengages then withdraws, hail truce ends the battle', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await openDrillBattle(page);

  // The contact dialog explains the cloaked contacts and previews the hail outcome from the core.
  const choice = page.getByTestId('encounter-choice');
  await expect(choice).toContainText('cloaked contact');
  let e = await encounterOf(page);
  await expect(page.getByTestId('hail-outlook')).toContainText(e.hail.accepts ? 'would accept a truce' : 'would refuse a truce');
  await page.getByTestId('choice-fight').click();

  // Every active ship of ours previews its next order and who decided it.
  const mine = e.ships.filter((s) => s.mine && s.active);
  for (const s of mine) await expect(page.getByTestId(`next-${s.id}`)).toBeVisible();
  await expect(page.getByTestId(`ship-${mine[0]!.id}`)).toContainText(/AI|standing|ordered/i);

  // Retreat one ship: the first round leaves it disengaging under fire, the second it withdraws.
  const runner = mine.find((s) => s.canRetreat && s.shipClass === 'Destroyer') ?? mine.find((s) => s.canRetreat)!;
  await page.getByTestId(`ship-${runner.id}`).click();
  await expect(page.getByTestId('scope-ship')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('maneuver-Retreat').click();
  await expect.poll(async () => (await encounterOf(page)).ships.find((s) => s.id === runner.id)?.next?.maneuver).toBe('Retreat');
  await page.getByTestId('resolve-round').click();
  await expect.poll(async () => (await encounterOf(page)).round).toBe(1);
  e = await encounterOf(page);
  const afterFirst = e.ships.find((s) => s.id === runner.id)!;
  if (afterFirst.alive) {
    expect(afterFirst.disengaging).toBeTruthy();
    await expect(page.getByTestId(`ship-${runner.id}`)).toContainText('Disengaging');
    await expect(page.getByTestId('combat-log')).toContainText('disengaging');
    await page.getByTestId('resolve-round').click();
    await expect.poll(async () => (await encounterOf(page)).round).toBe(2);
    e = await encounterOf(page);
    const afterSecond = e.ships.find((s) => s.id === runner.id)!;
    if (afterSecond.alive) {
      expect(afterSecond.retreated).toBeTruthy();
      await expect(page.getByTestId(`ship-${runner.id}`)).toContainText('WITHDRAWN');
      await expect(page.getByTestId('combat-log')).toContainText('withdrew');
    }
  }

  // Hail with every ship: the truce ends the battle when the enemy accepts (Encounter::aiAcceptsHail).
  e = await encounterOf(page);
  if (!e.finished) {
    const accepts = e.hail.accepts;
    await page.getByTestId('scope-all').click();
    await page.getByTestId('maneuver-Hail').click();
    await page.getByTestId('resolve-round').click();
    await expect.poll(async () => (await encounterOf(page)).round).toBe(e.round + 1);
    const after = await encounterOf(page);
    if (accepts) {
      expect(after.finished).toBeTruthy();
      expect(after.rounds[after.rounds.length - 1]!.truce).toBeTruthy();
      await expect(page.getByTestId('combat-log')).toContainText('truce');
      await expect(page.locator('.combat-head h1')).toContainText('Truce');
    } else {
      await expect(page.getByTestId('combat-log')).toContainText('hails');
    }
  }
  const final = await encounterOf(page);
  if (!final.finished) await page.getByTestId('auto-resolve').click();
  await page.getByTestId('continue-turn').click();
  await expect(page.getByTestId('briefing')).toBeVisible();
  const v = await page.evaluate(() => window.__botf!.view() as GameView);
  expect(v.meta.turn).toBe(1);
  expect(errors).toEqual([]);
});

test('tactical drill: group orders and the replay of cloak ambushes', async ({ page }) => {
  await openDrillBattle(page);
  await page.getByTestId('choice-fight').click();
  const e = await encounterOf(page);
  const cruiser = e.ships.find((s) => s.mine && s.active && s.group === 'Fast Attack') ?? e.ships.find((s) => s.mine && s.active)!;
  await page.getByTestId(`ship-${cruiser.id}`).click();
  await page.getByTestId('scope-group').click();
  await page.getByTestId('maneuver-Charge').click();
  const members = e.ships.filter((s) => s.mine && s.groupId === cruiser.groupId);
  await expect.poll(async () => (await encounterOf(page)).ships.filter((s) => members.some((m) => m.id === s.id) && s.next?.maneuver === 'Charge' && s.next.source === 'order').length)
    .toBe(members.length);
  await page.getByTestId('resolve-round').click();
  // Klingon ships start cloaked and decloak to attack with an ambush volley.
  await expect(page.getByTestId('combat-log')).toContainText(/decloaks|ambush/);
  await page.getByTestId('auto-resolve').click();
  await page.getByTestId('continue-turn').click();
  await expect(page.getByTestId('briefing')).toBeVisible();
  await page.getByTestId('briefing').getByText('Replay ›').first().click();
  await expect(page.getByTestId('replay')).toBeVisible();
  await page.getByTestId('replay-next').click();
  await expect(page.getByTestId('replay')).toContainText(/decloaks|ambush/);
});
