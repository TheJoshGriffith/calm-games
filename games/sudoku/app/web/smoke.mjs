// Sudoku's own web smoke checks, run by tools/web/smoke.mjs after the generic
// ones: a Hard game (puzzle generation on the browser's main thread), sound
// after real taps, the game surviving a reload, and share links.
import { join } from 'node:path';

export default async function sudokuChecks(t) {
  await t.step('sudoku-web-game', async () => {
    const { page, log } = await t.shoot('sudoku-web-game', '/?play=Hard', t.phone, 8000);
    try {
      // Play a few moves with real taps: select a cell, tap a digit on the
      // pad (coordinates for the 430x932 layout). Browsers only let audio
      // start after a user gesture, so this is where sound must begin.
      const before = await page.evaluate(() => window.calmAudio.starts);
      const taps = [[215, 400], [72, 640], [215, 470], [207, 640], [345, 640],
                    [72, 640], [207, 640], [345, 640], [72, 640], [207, 640]];
      for (const [x, y] of taps) {
        await page.mouse.click(x, y);
        await page.waitForTimeout(400);
      }
      const audio = await page.evaluate(() => window.calmAudio);
      console.log(`audio after ${taps.length} taps: ${JSON.stringify(audio)}`);
      if (audio.starts <= before)
        t.fail('sudoku-web-game: no sound started after tapping digits');
      await page.screenshot({ path: join(t.shotDir, 'sudoku-web-game-after-taps.png') });

      // The moves were autosaved; reopening the site should offer to resume.
      await page.waitForTimeout(1000);
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
