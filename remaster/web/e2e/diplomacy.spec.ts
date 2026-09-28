import { expect, test, type Page } from '@playwright/test';
import type { GameView } from '../src/core/types';
import { endTurn, throttleCpu, view } from './helpers';

// The diplomacy drill (bridge newDiplomacyDrill) sets up, through the core's diplomacy engine:
// a Ferengi trade-pact proposal and a Klingon tribute demand awaiting our answer, the Klingons
// betraying a pact with our Romulan allies (which calls us to arms), and an eliminated Cardassia.
async function startDrill(page: Page): Promise<void> {
  await throttleCpu(page);
  await page.goto('/');
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('botf.tutorial', 'skipped'); });
  await page.goto('/');
  await page.getByTestId('diplomacy-drill').click();
  await expect(page.getByTestId('screen-galaxy')).toBeVisible();
  await page.getByTestId('nav-diplomacy').click();
  await expect(page.getByTestId('diplomacy-inbox')).toBeVisible();
}

const pending = async (page: Page) => (await view(page)).pending;

test('diplomacy: incoming AI proposals accept/reject, call to arms honoured, ledger and news', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  await startDrill(page);
  const inbox = page.getByTestId('diplomacy-inbox');

  // Eliminated empires are greyed out.
  await expect(page.getByTestId('race-4')).toHaveClass(/eliminated/);
  await expect(page.getByTestId('race-3')).toContainText('ALLY');

  // Ferengi trade pact: accept (with deadline and consequences shown).
  const trade = inbox.locator('[data-testid^=proposal-]', { hasText: 'Trade Pact' });
  await expect(trade).toContainText(/answer by turn \d+ · 2 turns left/);
  await expect(trade).toContainText('Accept:');
  await trade.locator('[data-testid^=accept-]').click();
  await expect(trade).toContainText('Acceptance ordered');
  // Klingon tribute: reject, undo, reject again.
  const tribute = inbox.locator('[data-testid^=proposal-]', { hasText: 'Tribute demand' });
  await expect(tribute).toContainText('¢150');
  await tribute.locator('[data-testid^=reject-]').click();
  await expect(tribute).toContainText('Rejection ordered');
  await tribute.locator('[data-testid^=undo-]').click();
  await expect(tribute.locator('[data-testid^=reject-]')).toBeVisible();
  await tribute.locator('[data-testid^=reject-]').click();
  await expect.poll(async () => (await pending(page)).filter((p) => p.type === 'RespondDiplomaticProposal').length).toBe(2);

  // Call to arms from the Romulans against the Klingons: refuse, reconsider, then honour.
  const call = page.getByTestId('call-2');
  await expect(call).toContainText('call to arms');
  await expect(call).toContainText('Refuse:');
  await page.getByTestId('refuse-call-2').click();
  await expect(call).toContainText('You will let this call lapse');
  await page.getByTestId('reconsider-call-2').click();
  await page.getByTestId('honor-call-2').click();
  await expect(page.getByTestId('confirm-dialog')).toContainText('+10 trust');
  await page.getByTestId('confirm-ok').click();
  await expect(call).toContainText('War declaration ordered');
  await expect.poll(async () => (await pending(page)).some((p) => p.type === 'DeclareWar' && p.targetEmpire === 2)).toBeTruthy();

  // Treaty ledger shows the treaty dissolved when Cardassia was eliminated; news shows the betrayal.
  await page.getByTestId('dip-panes').getByText('Treaty ledger').click();
  await expect(page.getByTestId('treaty-ledger')).toContainText('Dissolved');
  await page.getByTestId('dip-panes').getByText('Galactic news').click();
  await expect(page.getByTestId('galactic-news')).toContainText('Treaty Betrayed');

  // Resolve the turn: the pact is signed, the tribute refused and we are at war with the Klingons.
  const turn = (await view(page)).meta.turn;
  await endTurn(page);
  await expect.poll(async () => (await view(page)).meta.turn).toBe(turn + 1);
  const v = await view(page);
  expect(v.empires.find((e) => e.empire === 2)?.atWar).toBeTruthy();
  await page.getByTestId('nav-diplomacy').click();
  await page.getByTestId('dip-panes').getByText('Treaty ledger').click();
  await expect(page.getByTestId('treaty-ledger')).toContainText('Trade Pact');
  await expect(page.getByTestId('race-2')).toContainText('WAR');
  expect(errors).toEqual([]);
});

test('diplomacy: a refused call to arms lapses and costs the ally’s trust', async ({ page }) => {
  await startDrill(page);
  const before = (await view(page)).empires.find((e) => e.empire === 3)!.trust!;
  await page.getByTestId('refuse-call-2').click();
  await expect(page.getByTestId('call-2')).toContainText('You will let this call lapse');
  for (let i = 0; i < 4; i++) {
    await endTurn(page);
    const v = await view(page);
    if (!v.alerts.some((a) => a.kind === 'callToArms')) break;
  }
  await page.getByTestId('nav-diplomacy').click();
  await expect(page.getByTestId('call-2')).toHaveCount(0);
  const after = (await view(page)).empires.find((e) => e.empire === 3)!;
  expect(after.trust!).toBeLessThan(before);
  await page.getByTestId('race-3').click();
  await page.getByTestId('dossier').getByRole('tab', { name: /History/ }).click();
  await expect(page.getByTestId('dossier')).toContainText('Alliance Refused');
});

test('briefing links incoming proposals to the diplomacy inbox', async ({ page }) => {
  await startDrill(page);
  // Ending the first turn unanswered: the briefing reminds us of the open call to arms.
  await page.getByTestId('end-turn').click();
  await page.getByTestId('confirm-ok').click();
  await page.waitForFunction(() => (window.__botf!.view() as GameView).meta.turn >= 2);
  const briefing = page.getByTestId('briefing');
  await expect(briefing).toBeVisible();
  const answer = briefing.locator('.kind-callToArms, .kind-proposal').first();
  await expect(answer).toContainText(/Answer|Respond/);
  await answer.getByTestId('briefing-event').click();
  await expect(page.getByTestId('screen-diplomacy')).toBeVisible();
  await expect(page.getByTestId('diplomacy-inbox')).toBeVisible();
});
