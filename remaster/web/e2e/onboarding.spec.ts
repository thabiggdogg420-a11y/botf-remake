import { expect, test, type Page } from '@playwright/test';
import { cellPoint, centreOn, fleetPoint, newCampaign, openContextMenu, view } from './helpers';

const step = (page: Page) => page.getByTestId('tutorial');

async function expectStep(page: Page, id: string): Promise<void> {
  await expect(step(page)).toHaveAttribute('data-step', id, { timeout: 10_000 });
}

test('First Contact tutorial: select Sol, manage, queue, move the scout, end turn, read the briefing', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await newCampaign(page, 1, '424242', { tutorial: true });
  await expectStep(page, 'welcome');
  await page.getByTestId('tutorial-next').click();

  // 1. Select the home system on the chart (the step clears the initial selection first).
  await expectStep(page, 'select-home');
  let v = await view(page);
  const home = v.systems.find((s) => s.id === v.colonies[0]!.systemId)!;
  await centreOn(page, home.x, home.y);
  const p = await cellPoint(page, home.y * v.meta.width + home.x);
  await page.mouse.click(p.x, p.y);
  await expectStep(page, 'manage');

  // 2. Manage the system, 3. queue construction.
  await page.getByTestId('open-system').click();
  await expect(page.getByTestId('screen-system')).toBeVisible();
  await expectStep(page, 'queue');
  await page.locator('[data-testid=build-options] button.choice:not(.disabled)').first().click();
  await expectStep(page, 'scout');

  // 4. Back to the chart; select the scout and plot a course through the context menu.
  await page.keyboard.press('F1');
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  v = await view(page);
  const scout = v.fleets.find((f) => f.shipClass === 'Scout')!;
  await centreOn(page, scout.x, scout.y);
  const sp = await fleetPoint(page, scout.id);
  await page.mouse.click(sp.x, sp.y);
  await expect(page.getByTestId('unit-card')).toContainText(scout.name.split(' — ')[0]!);
  const W = v.meta.width;
  const tx = Math.max(0, scout.x - 3), ty = Math.min(v.meta.height - 1, scout.y + 2);
  await centreOn(page, tx, ty);
  await openContextMenu(page, ty * W + tx);
  await page.locator('[data-testid=context-menu] [data-action=move]').click();
  await expectStep(page, 'end-turn');

  // 5. End the turn, 6. acknowledge the briefing.
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect.poll(async () => (await view(page)).meta.turn).toBe(1);
  const briefing = page.getByTestId('briefing');
  if (await briefing.isVisible().catch(() => false)) {
    await expectStep(page, 'briefing');
    await page.getByTestId('briefing-close').click();
  }
  await expectStep(page, 'finish');
  await page.getByTestId('tutorial-next').click();
  await expect(step(page)).toHaveCount(0);
  expect(await page.evaluate(() => localStorage.getItem('botf.tutorial'))).toBe('done');

  // A finished tutorial does not come back for the next campaign.
  await page.goto('/');
  await page.getByTestId('new-campaign').click();
  await page.getByTestId('start-campaign').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await expect(step(page)).toHaveCount(0);
  expect(errors).toEqual([]);
});

test('tutorial can be skipped (remembered) and replayed from Settings; help explains each screen', async ({ page }) => {
  await newCampaign(page, 1, '777', { tutorial: true });
  await expectStep(page, 'welcome');
  await page.getByTestId('tutorial-skip').click();
  await expect(step(page)).toHaveCount(0);
  expect(await page.evaluate(() => localStorage.getItem('botf.tutorial'))).toBe('skipped');

  // Contextual help popovers.
  await page.getByTestId('help-button').click();
  await expect(page.getByTestId('help-popover')).toContainText('Star chart');
  await page.keyboard.press('Escape');
  await expect(page.getByTestId('help-popover')).toHaveCount(0);
  await page.keyboard.press('F6');
  await page.keyboard.press('Shift+Slash');
  await expect(page.getByTestId('help-popover')).toContainText('Diplomacy');
  await page.keyboard.press('Shift+Slash');
  await expect(page.getByTestId('help-popover')).toHaveCount(0);

  // Replay from Settings.
  await page.getByTestId('menu-button').click();
  await page.getByTestId('open-settings').click();
  await page.getByTestId('replay-tutorial').click();
  await expectStep(page, 'welcome');
});
