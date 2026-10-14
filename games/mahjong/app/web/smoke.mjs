// Mahjong's own web smoke checks, run by tools/web/smoke.mjs after the
// generic ones: a share link (/s/<deal code>, rewritten to index.html by
// _redirects) opens that deal, and a link with no deal is turned away.

export default async function mahjongChecks(t) {
  await t.step('share link', async () => {
    const { page, log, look } = await t.shoot('mahjong-web-shared', '/s/turtle-k1', t.phone, 4000);
    try {
      if (!log.some(l => l.includes('share link: opened')))
        t.fail(`share link: the app did not open the shared deal\n${log.slice(-20).join('\n')}`);
      if (t.home && look.map === t.home)
        t.fail('share link: the shared deal is not on screen');
    } finally {
      await page.close();
    }
  });

  await t.step('bad share link', async () => {
    const { page, log } = await t.open('bad-share', '/s/not-a-deal!');
    try {
      await t.waitFor(page, log, () => log.some(l => l.includes('share link: rejected')), 120000, 'bad share link');
      await page.waitForTimeout(1000);
      const state = await page.evaluate(() => window.calm.state);
      if (state !== 'running')
        t.fail(`bad share link: app state is ${state}`);
    } finally {
      await page.close();
    }
  });
}
