// Expanded ship roster in the Database and build options, and the typed-weapon battle presentation
// (CombatPresentationSource VFX + WebAudio cue layer).
import { mkdirSync } from 'node:fs';
import { expect, test, type Page } from '@playwright/test';
import type { Encounter } from '../src/core/types';
import { newCampaign, SHOTS_DIR, throttleCpu } from './helpers';

async function rowCount(page: Page): Promise<number> {
  return page.locator('.ency-list [role="option"]').count();
}

test('Database: the expanded roster filters by faction, role, era and source', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await newCampaign(page, 1);
  await page.keyboard.press('F8');
  await expect(page.getByTestId('screen-encyclopedia')).toBeVisible();

  // Defaults to the player's faction: legacy and expanded Federation classes together.
  await expect(page.getByTestId('ency-faction')).toHaveValue('Federation');
  const fedAll = await rowCount(page);
  expect(fedAll).toBeGreaterThan(20);

  // Era: Dominion War Federation classes include the Akira, with typed weapon mounts and its asset key.
  await page.getByTestId('ency-era').selectOption('DominionWar');
  const dw = await rowCount(page);
  expect(dw).toBeGreaterThan(3);
  expect(dw).toBeLessThan(fedAll);
  await page.locator('.ency-list [role="option"]', { hasText: 'Akira' }).click();
  const detail = page.getByTestId('ency-detail');
  await expect(detail.getByTestId('roster-tags')).toContainText('Dominion War');
  await expect(detail.getByTestId('weapon-list')).toContainText('Photon Torpedo');
  await expect(detail.getByTestId('weapon-list')).toContainText('weapon.PhotonTorpedo');
  await expect(detail).toContainText('ship.fed.akira');

  // Role narrows further.
  await page.getByTestId('ency-role').selectOption('Strike Cruiser');
  const strike = await rowCount(page);
  expect(strike).toBeGreaterThanOrEqual(1);
  expect(strike).toBeLessThan(dw);
  await expect(page.locator('.ency-list')).toContainText('Akira');
  await page.getByTestId('ency-role').selectOption('');

  // Source across all factions: mod-derived and Kelvin classes are tagged as such.
  await page.getByTestId('ency-era').selectOption('');
  await page.getByTestId('ency-faction').selectOption('');
  await page.getByTestId('ency-source').selectOption('ModDerived');
  const mods = await rowCount(page);
  expect(mods).toBeGreaterThan(3);
  await page.locator('.ency-list [role="option"]').first().click();
  await expect(detail.getByTestId('roster-tags')).toContainText('Mod-derived');
  await page.getByTestId('ency-source').selectOption('Kelvin');
  expect(await rowCount(page)).toBeGreaterThan(0);
  await page.getByTestId('ency-era').selectOption('Kelvin');
  expect(await rowCount(page)).toBeGreaterThan(0);

  // Non-major powers appear as their own factions.
  await page.getByTestId('ency-source').selectOption('');
  await page.getByTestId('ency-era').selectOption('');
  await page.getByTestId('ency-faction').selectOption('Borg');
  expect(await rowCount(page)).toBeGreaterThan(0);

  // Legacy catalogue only.
  await page.getByTestId('ency-faction').selectOption('');
  await page.getByTestId('ency-source').selectOption('Legacy');
  const legacy = await rowCount(page);
  expect(legacy).toBeGreaterThan(50);
  await page.locator('.ency-list [role="option"]').first().click();
  await expect(detail).toContainText('Legacy catalogue');

  // The Remastered build options show buildable roster classes with their era.
  await page.keyboard.press('F2');
  await expect(page.getByTestId('screen-system')).toBeVisible();
  await page.getByTestId('build-options').getByRole('tab', { name: /Starships/ }).click();
  await expect(page.getByTestId('build-options').getByTestId('roster-chip').first()).toBeVisible();
  expect(errors).toEqual([]);
});

test('battle: typed weapon VFX from the presentation source, with audio cues', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await throttleCpu(page);
  await page.goto('/');
  await page.evaluate(() => {
    localStorage.clear();
    localStorage.setItem('botf.tutorial', 'skipped');
    localStorage.setItem('botf.audio', 'on');
  });
  await page.goto('/');
  await page.getByTestId('tactical-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  await page.getByTestId('choice-fight').click();

  const arena = page.getByTestId('combat-arena');
  // Resolve rounds until one exchanges fire (the drill opens out of range), screenshotting mid-volley.
  let shot = false;
  for (let round = 1; round <= 10; round++) {
    await page.getByTestId('resolve-round').click();
    await expect.poll(async () => (await page.evaluate(() => window.__botf!.encounter() as Encounter)).round).toBe(round);
    const e = await page.evaluate(() => window.__botf!.encounter() as Encounter);
    const fire = e.rounds[e.rounds.length - 1]!.salvos.some((s) => s.beam > 0 && !s.ram);
    if (fire && !shot) {
      await page.waitForTimeout(1250);
      mkdirSync(SHOTS_DIR, { recursive: true });
      await page.screenshot({ path: `${SHOTS_DIR}/40-combat-typed-vfx.png` });
      shot = true;
    }
    await page.waitForTimeout(fire ? 1500 : 300);
    const fx = JSON.parse((await arena.getAttribute('data-fx')) ?? '{}') as Record<string, number>;
    if ((fx.beam ?? 0) > 0 && (fx.torpedo ?? 0) > 0) break;
    if (e.finished) break;
  }

  // The animated round drew typed effects: continuous beams and torpedo projectiles.
  await expect.poll(async () => JSON.parse((await arena.getAttribute('data-fx')) ?? '{}'), { timeout: 15000 })
    .toMatchObject({ beam: expect.any(Number), torpedo: expect.any(Number) });
  await expect(arena).toHaveAttribute('data-fx-source', 'derived');
  const fx = JSON.parse((await arena.getAttribute('data-fx'))!) as Record<string, number>;
  // Federation phasers (legacy fit: phaser array) and photon torpedoes.
  expect(fx.PhaserArray ?? 0).toBeGreaterThan(0);
  expect(fx.PhotonTorpedo ?? 0).toBeGreaterThan(0);

  // Cues were requested under the roster weapon keys and the AUDIO-PLAN sfx keys.
  const cues = await page.evaluate(() => window.__botf!.cues());
  expect(cues.requested).toBeGreaterThan(0);
  expect(Object.keys(cues.byKey).some((k) => k.startsWith('weapon.'))).toBeTruthy();
  expect(Object.keys(cues.byKey).some((k) => k.startsWith('sfx.'))).toBeTruthy();

  await page.getByTestId('auto-resolve').click();
  await page.getByTestId('continue-turn').click();
  await expect(page.getByTestId('briefing')).toBeVisible();
  expect(errors).toEqual([]);
});
