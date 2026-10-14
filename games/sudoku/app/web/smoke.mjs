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
  // the same app, which opens the shared board: Hard puzzle H1-00000004 with
  // its top three rows filled in, corner marks, and the bottom three rows
  // coloured yellow and teal. Sharing it on gives the starting board's link.
  const progress = '1fWVYFhCiCIMgaxy5RCD4Gpet__8AAAAACnYNYGuXxNgAAkkkkkkwDAYADwEBAYDBgYGBgAAAAAAAAA2222xkK0ZCtGQrQ';
  const start = '1cGVYFhCiCIMgaxy5RCD4Gpes';
  await t.step('share link', async () => {
    const { page, log, look } = await t.shoot('sudoku-web-shared', `/s/${progress}`, t.phone, 4000);
    try {
      if (!log.some(l => l.includes('share link: opened')))
        t.fail(`share link: the app did not open the shared board\n${log.slice(-20).join('\n')}`);
      // The board, not the home screen, and with the shared colours on it
      // (light saturated cells are '~' in the map).
      const tinted = map => (map.match(/~/g) || []).length;
      console.log(`tinted cells: shared ${tinted(look.map)}, home ${t.home ? tinted(t.home) : '?'}`);
      if (t.home && (look.map === t.home || tinted(look.map) < tinted(t.home) + 40))
        t.fail('share link: the shared board (with its colours) is not on screen');

      // Share it on: the header's share button, then "Share this puzzle"
      // (coordinates for the 430x932 layout). Headless Chromium has no share
      // sheet, so the link goes to the clipboard with a "Link copied" note.
      await page.mouse.click(348, 34);
      await page.waitForTimeout(800);
      await page.screenshot({ path: join(t.shotDir, 'sudoku-web-share-sheet.png') });
      await page.mouse.click(215, 779);
      await page.waitForTimeout(600);
      await page.screenshot({ path: join(t.shotDir, 'sudoku-web-link-copied.png') });
      const shared = await page.evaluate(() => window.calm.shared);
      console.log(`shared: ${JSON.stringify(shared)}`);
      const expected = `Try this Hard Sudoku: https://sudoku.gwynant.app/s/${start}`;
      if (!shared.includes(expected))
        t.fail(`share link: sharing the opened board gave ${JSON.stringify(shared)}, not "${expected}"`);
    } finally {
      await page.close();
    }
  });

  // A link with no puzzle in it is turned away and the app carries on.
  // A variant link names its puzzle by code: the app rebuilds the Killer
  // puzzle (on the browser's main thread) and opens it with its cages.
  await t.step('variant share link', async () => {
    const { page, log } = await t.shoot('sudoku-web-killer', '/s/KL1-00000004', t.phone, 6000);
    try {
      if (!log.some(l => l.includes('share link: opened KL1-00000004')))
        t.fail(`variant share link: the app did not open it\n${log.slice(-20).join('\n')}`);
    } finally {
      await page.close();
    }
  });

  await t.step('bad share link', async () => {
    const { page, log } = await t.open('bad-share', '/s/not-a-board');
    try {
      await t.waitFor(page, log, () => log.some(l => l.includes('share link: rejected not-a-board')), 120000, 'bad share link');
      await page.waitForTimeout(1000);
      const state = await page.evaluate(() => window.calm.state);
      if (state !== 'running')
        t.fail(`bad share link: app state is ${state}`);
    } finally {
      await page.close();
    }
  });
}
