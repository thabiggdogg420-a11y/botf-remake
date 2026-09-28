import { expect, test } from '@playwright/test';
import fs from 'node:fs';
import { centreOn, endTurn, fleetPoint, newCampaign, openContextMenu, SHOTS_DIR, view } from './helpers';

// Visual record of every screen at a laptop and an ultrawide resolution.
for (const [width, height] of [[1280, 720], [2560, 1080]] as const) {
  test.describe(`screens at ${width}x${height}`, () => {
    test.use({ viewport: { width, height } });

    test(`capture ${width}x${height}`, async ({ page }) => {
      test.setTimeout(360_000);   // a long visual tour: every screen, both drills, onboarding and settings
      const dir = `${SHOTS_DIR}/${width}x${height}`;
      fs.mkdirSync(dir, { recursive: true });
      const shot = async (name: string) => {
        await page.waitForTimeout(350);
        await page.screenshot({ path: `${dir}/${name}.png` });
      };
      const errors: string[] = [];
      page.on('pageerror', (e) => errors.push(String(e)));

      await page.goto('/');
      await page.evaluate(() => localStorage.clear());
      await page.goto('/');
      await expect(page.getByTestId('screen-title')).toBeVisible();
      await shot('01-title');
      await page.getByTestId('new-campaign').click();
      await page.getByTestId('faction-3').click();
      await shot('02-new-campaign');
      await page.getByTestId('faction-1').click();

      await newCampaign(page, 1, '424242');
      await shot('03-galaxy-start');

      // Plot the scout, play a few turns to reveal the chart.
      let v = await view(page);
      const W = v.meta.width;
      const scout = v.fleets.find((f) => f.shipClass === 'Scout')!;
      let p = await fleetPoint(page, scout.id);
      await page.mouse.click(p.x, p.y);
      const tx = Math.max(0, scout.x - 4), ty = Math.min(v.meta.height - 1, scout.y + 3);
      await centreOn(page, scout.x - 2, scout.y + 1);
      await openContextMenu(page, ty * W + tx);
      await shot('04-context-menu');
      await page.locator('[data-testid=context-menu] [data-action=move]').click();
      for (let i = 0; i < 5; i++) await endTurn(page);
      v = await view(page);
      const s2 = v.fleets.find((f) => f.id === scout.id)!;
      await centreOn(page, s2.x, s2.y);
      p = await fleetPoint(page, scout.id);
      await page.mouse.click(p.x, p.y);
      const hx = Math.min(W - 1, s2.x + 3), hy = Math.max(0, s2.y - 2);
      const hp = await page.evaluate((c) => window.__botf!.map!.cellCenter(c), hy * W + hx);
      await page.mouse.move(hp!.x, hp!.y);
      await shot('05-galaxy-route-preview');
      await page.keyboard.press('f');
      await page.mouse.move(5, height - 5);
      await shot('06-galaxy-overview');

      await page.getByTestId('end-turn').click();
      await page.getByTestId('confirm-ok').click();
      const briefing = page.getByTestId('briefing');
      if (await briefing.isVisible().catch(() => false)) {
        await shot('07-briefing');
        await page.getByTestId('briefing-close').click();
      }

      const screens: [string, string][] = [
        ['F2', 'system'], ['F3', 'empire'], ['F4', 'research'], ['F5', 'fleets'], ['F6', 'diplomacy'], ['F7', 'intel'], ['F8', 'encyclopedia'],
      ];
      let n = 8;
      for (const [key, name] of screens) {
        await page.keyboard.press(key);
        await expect(page.getByTestId(`screen-${name}`)).toBeVisible();
        await shot(`${String(n++).padStart(2, '0')}-${name}`);
      }
      await page.getByTestId('menu-button').click();
      await shot('15-menu');
      await page.getByTestId('open-saves').click();
      await shot('16-saves');

      // Battle: the tactical drill.
      await page.goto('/');
      await page.getByTestId('tactical-drill').click();
      await page.getByTestId('end-turn').click();
      await shot('17-end-turn-confirm');
      await page.getByTestId('confirm-ok').click();
      await expect(page.getByTestId('encounter-choice')).toBeVisible();
      await shot('18-combat-contact');
      await page.getByTestId('choice-fight').click();
      await page.getByTestId('maneuver-Assault').click();
      for (let i = 0; i < 3; i++) await page.getByTestId('resolve-round').click();
      await page.waitForTimeout(600);
      await shot('19-combat-rounds');
      await page.getByTestId('auto-resolve').click();
      await shot('20-combat-finished');
      await page.getByTestId('continue-turn').click();
      await expect(page.getByTestId('briefing')).toBeVisible();
      await shot('21-battle-briefing');
      await page.getByTestId('briefing').getByText('Replay ›').first().click();
      await page.getByTestId('replay-next').click();
      await page.waitForTimeout(700);
      await shot('22-replay');
      await page.keyboard.press('F6');
      await shot('23-diplomacy-war');
      await page.keyboard.press('F7');
      await shot('24-intel-contact');

      // Diplomacy drill: inbox with deadlines, call to arms, ledger with a dissolved treaty, news.
      await page.goto('/');
      await page.getByTestId('diplomacy-drill').click();
      await page.getByTestId('nav-diplomacy').click();
      await expect(page.getByTestId('diplomacy-inbox')).toBeVisible();
      await shot('25-diplomacy-inbox');
      await page.getByTestId('race-3').click();
      await shot('26-diplomacy-ally-dossier');
      await page.getByTestId('dip-panes').getByText('Treaty ledger').click();
      await shot('27-diplomacy-ledger');
      await page.getByTestId('dip-panes').getByText('Galactic news').click();
      await shot('28-diplomacy-news');

      // Tactical rules 2: intent previews, salvos in flight, a disengaging ship, a hail truce.
      await page.goto('/');
      await page.getByTestId('tactical-drill').click();
      await page.getByTestId('end-turn').click();
      await page.getByTestId('confirm-ok').click();
      await page.getByTestId('choice-fight').click();
      await page.waitForTimeout(400);
      await shot('29-combat-intent');
      await page.getByTestId('ship-5').click();
      await page.getByTestId('maneuver-Retreat').click();
      await page.getByTestId('resolve-round').click();
      await page.waitForTimeout(1250);
      await page.screenshot({ path: `${dir}/30-combat-salvos.png` });
      await page.waitForTimeout(1500);
      await shot('31-combat-disengaging');
      await page.getByTestId('scope-all').click();
      await page.getByTestId('maneuver-Hail').click();
      await page.getByTestId('resolve-round').click();
      await page.waitForTimeout(1100);
      await page.screenshot({ path: `${dir}/32-combat-hail.png` });
      await page.waitForTimeout(1500);
      await shot('33-combat-truce');

      // Onboarding, help and settings.
      await newCampaign(page, 1, '424242', { tutorial: true });
      await shot('34-tutorial-welcome');
      await page.getByTestId('tutorial-next').click();
      await shot('35-tutorial-select-home');
      await page.getByTestId('tutorial-skip').click();
      await page.getByTestId('help-button').click();
      await shot('36-help-galaxy');
      await page.keyboard.press('Escape');
      await page.getByTestId('menu-button').click();
      await page.getByTestId('open-settings').click();
      await shot('37-settings');
      const scale = page.getByTestId('setting-scale');
      await scale.focus();
      for (let i = 0; i < 6; i++) await page.keyboard.press('ArrowRight');
      await page.getByTestId('setting-colorblind').click();
      await page.getByTestId('settings-close').click();
      await page.waitForTimeout(500);
      await shot('38-scale130-colorblind-galaxy');
      await page.keyboard.press('F2');
      await shot('39-scale130-system');
      await page.keyboard.press('F6');
      await shot('40-scale130-diplomacy');
      await page.keyboard.press('F4');
      await shot('41-scale130-research');
      await page.goto('/');
      await page.getByTestId('tactical-drill').click();
      await page.getByTestId('end-turn').click();
      await page.getByTestId('confirm-ok').click();
      await page.getByTestId('choice-fight').click();
      await page.getByTestId('resolve-round').click();
      await page.waitForTimeout(2600);
      await shot('42-scale130-combat');
      expect(errors).toEqual([]);
    });
  });
}
