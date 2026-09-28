// Schema-13 gameplay layer in the browser: outposts and starbases, anomalies, galactic events,
// victory progress, build templates, the Classic/Remastered choice and station combat.
import { expect, test, type Page } from '@playwright/test';
import fs from 'node:fs';
import type { Encounter, GameView } from '../src/core/types';
import { centreOn, endTurn, settle, SHOTS_DIR, throttleCpu, view } from './helpers';

const dir = `${SHOTS_DIR}/remaster`;

async function shot(page: Page, name: string): Promise<void> {
  fs.mkdirSync(dir, { recursive: true });
  await page.waitForTimeout(300);
  await page.screenshot({ path: `${dir}/${name}.png` });
}

async function freshTitle(page: Page): Promise<void> {
  await throttleCpu(page);
  await page.goto('/');
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('botf.tutorial', 'skipped'); });
  await page.goto('/');
  await expect(page.getByTestId('screen-title')).toBeVisible();
}

async function stationPoint(page: Page, id: number): Promise<{ x: number; y: number }> {
  await settle(page);
  await page.waitForFunction((sid) => !!window.__botf?.map?.stationMarker(sid), id, { timeout: 5000 });
  return (await page.evaluate((sid) => window.__botf!.map!.stationMarker(sid), id))!;
}

test('frontier drill: resolve an event, build an outpost, fund a starbase, chronicle briefing, victory panel', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await freshTitle(page);
  await page.getByTestId('frontier-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();

  let v = await view(page);
  expect(v.meta.rules.preset).toBe('remastered');
  expect(v.features.length).toBeGreaterThan(0);

  // --- Galactic events: the cards open with deadlines; choose an option.
  const panel = page.getByTestId('events-panel');
  await expect(panel).toBeVisible();
  const event = v.events.find((e) => e.awaitingChoice)!;
  const card = page.getByTestId(`event-card-${event.id}`);
  await expect(card).toContainText(event.name);
  await expect(card).toContainText(`turn ${event.deadline}`);
  await shot(page, '01-events-panel');
  const option = event.choices.find((c) => !c.isDefault)!;
  await page.getByTestId(`event-${event.id}-option-${option.index}`).click();
  await expect(page.getByTestId(`event-${event.id}-option-${option.index}`)).toHaveAttribute('aria-pressed', 'true');
  await expect.poll(async () => (await view(page)).events.find((e) => e.id === event.id)?.drafted).toBe(option.index);
  await page.getByTestId('events-close').click();
  await expect(panel).toBeHidden();

  // --- Build an outpost: select the transport from the sector card and give the order.
  v = await view(page);
  const transport = v.fleets.find((f) => f.shipClass === 'Transport')!;
  await centreOn(page, transport.x, transport.y);
  await page.getByTestId(`unit-row-${transport.id}`).click();
  await expect(page.getByTestId('unit-card')).toBeVisible();
  const build = page.getByTestId('action-outpost');
  await expect(build).toBeVisible();
  await expect(build).not.toHaveAttribute('aria-disabled', 'true');
  await build.click();
  await expect.poll(async () => (await view(page)).fleets.find((f) => f.id === transport.id)?.buildingOutpost).toBe(true);
  await expect(page.getByTestId('unit-orders')).toContainText(/assembl\w* an outpost/i);

  // --- Station inspector: the existing outpost, its coverage and a starbase upgrade.
  const outpost = v.stations.find((s) => s.mine && s.type === 'Outpost')!;
  await centreOn(page, outpost.x, outpost.y);
  const sp = await stationPoint(page, outpost.id);
  await page.mouse.click(sp.x, sp.y);
  const stationCard = page.getByTestId('station-card');
  await expect(stationCard).toBeVisible();
  await expect(stationCard).toContainText(outpost.name);
  await expect(page.getByTestId('station-upgrade')).toBeVisible();
  await shot(page, '02-station-inspector');
  const home = v.colonies[0]!.systemId;
  await page.getByTestId(`fund-${home}`).click();
  await expect.poll(async () => !!(await view(page)).stations.find((s) => s.id === outpost.id)?.upgrade).toBe(true);
  await expect(stationCard).toContainText('Starbase upgrade');

  // --- Anomaly tooltips: hovering a charted anomaly explains its effect.
  const feature = v.features.find((f) => f.type === 'Wormhole') ?? v.features[0]!;
  await centreOn(page, feature.x, feature.y);
  const fp = await page.evaluate((c) => window.__botf!.map!.cellCenter(c), feature.y * v.meta.width + feature.x);
  await page.mouse.move(fp!.x + 3, fp!.y + 3);
  await expect(page.locator('.hover-tip')).toContainText(feature.name);
  await expect(page.locator('.hover-tip')).toContainText(feature.effect.slice(0, 20));
  await shot(page, '03-anomaly-tooltip');

  // --- End turns until the outpost is assembled; the chronicle briefing files it under Stations.
  const before = v.stations.filter((s) => s.mine).length;
  let sawStations = false;
  for (let i = 0; i < v.outpostTurns; i++) {
    const t0 = (await view(page)).meta.turn;
    await page.getByTestId('end-turn').click();
    await page.getByTestId('confirm-ok').click();
    await page.waitForFunction((t) => (window.__botf!.view() as GameView).meta.turn > t, t0);
    const briefing = page.getByTestId('briefing');
    await briefing.waitFor({ state: 'visible', timeout: 3000 }).catch(() => undefined);
    if (!(await briefing.isVisible().catch(() => false))) continue;
    const stations = briefing.locator('.brief-group.cat-stations [data-testid=briefing-event]');
    if (await stations.count()) sawStations = true;
    if (i === v.outpostTurns - 1 && await stations.count()) {
      await shot(page, '04-chronicle-briefing');
      // Chronicle entries are clickable: a station entry selects the station on the chart.
      await stations.last().click();
      await expect(briefing).toBeHidden();
    } else await page.getByTestId('briefing-close').click();
  }
  v = await view(page);
  expect(v.stations.filter((s) => s.mine).length).toBeGreaterThan(before);
  expect(v.fleets.some((f) => f.id === transport.id)).toBeFalsy();
  expect(sawStations).toBeTruthy();
  await shot(page, '05-outpost-built');

  // --- Victory panel: four conditions with percent and explanation.
  await page.getByTestId('open-victory').click();
  const victory = page.getByTestId('victory-panel');
  await expect(victory).toBeVisible();
  for (const c of v.victory.conditions) {
    const row = page.getByTestId(`victory-${c.condition}`);
    await expect(row).toContainText(c.name);
    await expect(row).toContainText(c.explanation.slice(0, 24));
  }
  await expect(page.getByTestId('victory-Scientific')).toContainText('%');
  await shot(page, '06-victory-panel');
  await page.keyboard.press('Escape');
  await expect(victory).toBeHidden();

  // --- End-of-campaign screen (previewed through the test hook).
  await page.evaluate(() => window.__botf!.previewOutcome('victory'));
  await expect(page.getByTestId('outcome-screen')).toBeVisible();
  await expect(page.getByTestId('outcome-title')).toHaveText(/victory/i);
  await shot(page, '07-outcome-victory');
  await page.getByTestId('outcome-dismiss').click();
  await page.evaluate(() => window.__botf!.previewOutcome('defeat'));
  await expect(page.getByTestId('outcome-title')).toHaveText(/defeat/i);
  await shot(page, '08-outcome-defeat');
  await page.getByTestId('outcome-dismiss').click();
  await expect(page.getByTestId('outcome-screen')).toBeHidden();

  // --- The System screen offers the megaproject (blocked early, with the core's reason).
  await page.getByTestId('nav-system').click();
  await expect(page.getByTestId('megaproject')).toBeVisible();
  await expect(page.getByTestId('megaproject-start')).toHaveAttribute('aria-disabled', 'true');
  await shot(page, '09-system-megaproject');

  // --- Minor-race roster: contacted minors show their profile; the database lists every race.
  await page.getByTestId('nav-diplomacy').click();
  const minorRow = page.locator('.race-row.minor').first();
  if (await minorRow.count()) {
    await minorRow.click();
    await expect(page.getByTestId('minor-profile')).toBeVisible();
    await shot(page, '13-minor-dossier');
  }
  await page.getByTestId('nav-empire').click();
  await expect(page.getByTestId('member-bonuses')).toBeVisible();
  await page.getByTestId('nav-encyclopedia').click();
  await page.getByRole('tab', { name: 'Minor races' }).click();
  await expect(page.getByTestId('minor-entry')).toBeVisible();
  await expect(page.getByTestId('minor-entry')).toContainText('Join bonus');
  await shot(page, '14-database-minor-races');
  expect(errors).toEqual([]);
});

test('classic ruleset and build templates: define, apply, set default, delete', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await freshTitle(page);
  await page.getByTestId('new-campaign').click();
  await expect(page.getByTestId('preset-remastered')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('preset-classic').click();
  await expect(page.getByTestId('preset-classic')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('faction-1').click();
  await page.getByTestId('seed-input').fill('424242');
  await page.getByTestId('start-campaign').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  let v = await view(page);
  expect(v.meta.rules.preset).toBe('classic');
  expect(v.features.length).toBe(0);

  await page.getByTestId('nav-system').click();
  await expect(page.getByTestId('templates')).toBeVisible();
  await page.getByTestId('template-name').fill('Core world');
  await page.getByTestId('template-save').click();
  await expect.poll(async () => (await view(page)).templates.length).toBe(1);
  v = await view(page);
  const t = v.templates[0]!;
  expect(t.name).toBe('Core world');
  await expect(page.getByTestId(`template-${t.id}`)).toContainText('Core world');

  const queued = v.colonies[0]!.queueLength;
  await page.getByTestId(`template-apply-${t.id}`).click();
  await expect.poll(async () => (await view(page)).colonies[0]!.queueLength).toBeGreaterThan(queued);

  await page.getByTestId(`template-default-${t.id}`).click();
  await expect.poll(async () => (await view(page)).templates[0]?.isDefault).toBe(true);
  await expect(page.getByTestId(`template-${t.id}`)).toContainText('default');
  await shot(page, '10-templates');

  await page.getByTestId(`template-delete-${t.id}`).click();
  await page.getByTestId('confirm-ok').click();
  await expect.poll(async () => (await view(page)).templates.length).toBe(0);
  await endTurn(page);
  expect(errors).toEqual([]);
});

test('starbase drill: the station fights as an immobile base', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await freshTitle(page);
  await page.getByTestId('starbase-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  const v = await view(page);
  const base = v.stations.find((s) => s.mine)!;
  expect(base.type).toBe('Starbase');
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await expect(page.getByTestId('screen-combat')).toBeVisible();
  const choice = page.getByTestId('choice-fight');
  if (await choice.isVisible().catch(() => false)) await choice.click();
  const e = await page.evaluate(() => window.__botf!.encounter() as Encounter);
  const station = e.ships.find((s) => s.station)!;
  expect(station.id).toBeGreaterThanOrEqual(2000000);
  expect(station.immobile).toBeTruthy();
  await expect(page.getByTestId(`ship-${station.id}`)).toContainText('immobile');
  await page.getByTestId(`ship-${station.id}`).click();
  await expect(page.getByTestId('maneuver-Retreat')).toHaveAttribute('aria-disabled', 'true');
  await shot(page, '11-starbase-combat');
  const at = station.position;
  await page.getByTestId('resolve-round').click();
  await expect.poll(async () => (await page.evaluate(() => window.__botf!.encounter() as Encounter)).round).toBe(1);
  const after = (await page.evaluate(() => window.__botf!.encounter() as Encounter)).ships.find((s) => s.id === station.id)!;
  if (after.alive) expect(after.position).toEqual(at);
  await page.waitForTimeout(1200);
  await shot(page, '12-starbase-round');
  expect(errors).toEqual([]);
});
