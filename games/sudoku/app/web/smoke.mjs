// Sudoku's own web smoke checks, run by tools/web/smoke.mjs after the generic
// ones: a Hard game (puzzle generation on the browser's main thread), sound
// after real taps, the game surviving a reload, and share links.
import { join } from 'node:path';

export default async function sudokuChecks(t) {
  await t.step('sudoku-web-game', async () => {
    const { page, log } = await t.shoot('sudoku-web-game', '/?play=Hard', t.phone, 8000);
    try {
      // Play moves with real taps: select a cell, tap a digit on the pad
      // (coordinates for the 430x932 layout). Browsers only let audio start
      // after a user gesture, so this is where sound must begin. The puzzle
      // is random, so walk along two rows until a tap lands on an empty cell.
      const before = await page.evaluate(() => window.calmAudio.starts);
      const columns = [38, 82, 126, 170, 214, 258, 302, 346, 390];
      const cells = [...columns.map(x => [x, 400]), ...columns.map(x => [x, 310])];
      const digits = [[72, 640], [207, 640], [345, 640]];
      let taps = 0;
      for (const [i, [x, y]] of cells.entries()) {
        await page.mouse.click(x, y);
        const [dx, dy] = digits[i % digits.length];
        await page.mouse.click(dx, dy);
        taps += 2;
        await page.waitForTimeout(400);
        if (await page.evaluate(() => window.calmAudio.starts) > before) break;
      }
      const audio = await page.evaluate(() => window.calmAudio);
      console.log(`audio after ${taps} taps: ${JSON.stringify(audio)}`);
      if (audio.starts <= before)
        t.fail('sudoku-web-game: no sound started after tapping digits');
      await page.screenshot({ path: join(t.shotDir, 'sudoku-web-game-after-taps.png') });

      // The moves were autosaved; once the page has written them to
      // IndexedDB (it checks every second), reopening should offer to resume.
      await t.waitFor(page, log, () => page.evaluate(() => !window.calm.unsaved()), 15000, 'saving to IndexedDB');
      const files = () => page.evaluate(() => ({
        storage: window.calm.storage,
        events: window.calm.events,
        files: window.calm.files ? window.calm.files() : [],
      }));
      console.log(`before reload: ${JSON.stringify(await files())}`);
      await page.goto(t.base + '/');
      await t.waitFor(page, log, s => s === 'running', 120000, 'reload');
      await page.waitForTimeout(4000);
      console.log(`after reload: ${JSON.stringify(await files())}`);
      const reopened = await t.inspect(await page.screenshot({ path: join(t.shotDir, 'sudoku-web-reopened.png') }));
      console.log(`home screen after reopening:\n${reopened.map}`);
      // A first visit has no game to continue; after playing, the home
      // screen gains a Continue card, so it must look different.
      if (t.home && reopened.map === t.home)
        t.fail('reopened site shows no saved game (saves did not persist)');
    } finally {
      await page.close();
    }
  });

  // A share link (/s/<payload>, rewritten to index.html by _redirects) loads
  // the same app and hands it the payload.
  await t.step('share link', async () => {
    const { page, log } = await t.open('share', '/s/smoke-payload');
    try {
      await t.waitFor(page, log, () => log.some(l => l.includes('share link: smoke-payload')), 120000, 'share link');
    } finally {
      await page.close();
    }
  });
}
