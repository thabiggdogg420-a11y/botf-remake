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

/** Opens a drill battle with sound on, then resolves rounds until the arena has drawn the wanted effects. */
async function fightUntil(page: Page, drill: string, shotName: string, done: (fx: Record<string, number>) => boolean): Promise<Record<string, number>> {
  await throttleCpu(page);
  await page.goto('/');
  await page.evaluate(() => {
    localStorage.clear();
    localStorage.setItem('botf.tutorial', 'skipped');
    localStorage.setItem('botf.audio', 'on');
  });
  await page.goto('/');
  await page.getByTestId(drill).click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  const choice = page.getByTestId('choice-fight');
  if (await choice.isVisible().catch(() => false)) await choice.click();

  const arena = page.getByTestId('combat-arena');
  const fxNow = async () => JSON.parse((await arena.getAttribute('data-fx')) ?? '{}') as Record<string, number>;
  // Resolve rounds until fire is exchanged (drills open out of range), screenshotting mid-volley.
  let shot = false;
  for (let round = 1; round <= 12; round++) {
    await page.getByTestId('resolve-round').click();
    await expect.poll(async () => (await page.evaluate(() => window.__botf!.encounter() as Encounter)).round).toBe(round);
    const e = await page.evaluate(() => window.__botf!.encounter() as Encounter);
    const last = e.rounds[e.rounds.length - 1]!;
    const fire = last.salvos.some((s) => s.beam + s.torpedo > 0 && !s.ram) || (last.events ?? []).some((ev) => ev.kind === 'Fire');
    if (fire && !shot) {
      await page.waitForTimeout(1250);
      mkdirSync(SHOTS_DIR, { recursive: true });
      await page.screenshot({ path: `${SHOTS_DIR}/${shotName}.png` });
      shot = true;
    }
    await page.waitForTimeout(fire ? 1500 : 300);
    if (done(await fxNow()) || e.finished) break;
  }
  await expect.poll(async () => done(await fxNow()), { timeout: 15000 }).toBeTruthy();
  return fxNow();
}

async function finishBattle(page: Page): Promise<void> {
  const e = await page.evaluate(() => window.__botf!.encounter() as Encounter);
  if (!e.finished) await page.getByTestId('auto-resolve').click();
  await page.getByTestId('continue-turn').click();
  await expect(page.getByTestId('briefing')).toBeVisible();
}

test('battle (rules 2): typed weapon VFX derived from round results, with audio cues', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  const fx = await fightUntil(page, 'tactical-drill', '40-combat-typed-vfx', (f) => (f.beam ?? 0) > 0 && (f.torpedo ?? 0) > 0);
  const arena = page.getByTestId('combat-arena');
  // Rules 2 rounds carry no event stream: the derived source draws continuous beams and torpedo projectiles.
  await expect(arena).toHaveAttribute('data-fx-source', 'derived');
  // Federation phasers (legacy fit: phaser array) and photon torpedoes.
  expect(fx.PhaserArray ?? 0).toBeGreaterThan(0);
  expect(fx.PhotonTorpedo ?? 0).toBeGreaterThan(0);

  // Cues were requested under the roster weapon keys and the AUDIO-PLAN sfx keys.
  const cues = await page.evaluate(() => window.__botf!.cues());
  expect(cues.requested).toBeGreaterThan(0);
  expect(Object.keys(cues.byKey).some((k) => k.startsWith('weapon.'))).toBeTruthy();
  expect(Object.keys(cues.byKey).some((k) => k.startsWith('sfx.'))).toBeTruthy();
  await finishBattle(page);
  expect(errors).toEqual([]);
});

test('battle (rules 3): VFX and cues come from the core combat event stream', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  const fx = await fightUntil(page, 'starbase-drill', '41-combat-event-stream', (f) => (f.beam ?? 0) + (f.pulse ?? 0) + (f.torpedo ?? 0) > 0);
  const e = await page.evaluate(() => window.__botf!.encounter() as Encounter);
  expect(e.rules).toBe(3);
  expect(e.rounds.some((r) => (r.events ?? []).some((ev) => ev.kind === 'Fire' && ev.key?.startsWith('weapon.')))).toBeTruthy();
  await expect(page.getByTestId('combat-arena')).toHaveAttribute('data-fx-source', 'events');
  // Effects are keyed by the event's weapon type.
  const fired = new Set(e.rounds.flatMap((r) => (r.events ?? []).filter((ev) => ev.kind === 'Fire').map((ev) => ev.weapon!)));
  expect(Object.keys(fx).some((k) => fired.has(k))).toBeTruthy();
  const cues = await page.evaluate(() => window.__botf!.cues());
  expect(Object.keys(cues.byKey).some((k) => fired.has(k.replace('weapon.', '')))).toBeTruthy();
  await finishBattle(page);
  expect(errors).toEqual([]);
});
