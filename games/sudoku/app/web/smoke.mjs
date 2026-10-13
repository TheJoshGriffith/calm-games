// Browser smoke test for the Gwynant Sudoku web build.
//
//   node games/sudoku/app/web/smoke.mjs <site-dir> <screenshot-dir>
//
// Serves the site like Cloudflare Pages would (application/wasm, /s/* -> index),
// then in headless Chromium:
//   1. runs the app's own QML smoke test (?smoke=1) and expects "smoke test: ok",
//   2. opens the home screen and saves screenshots (phone and desktop sizes),
//   3. opens a share link and checks the payload reaches the app,
// failing on page errors, aborts or a failed start.
import { chromium } from 'playwright';
import { createServer } from 'node:http';
import { readFile, mkdir } from 'node:fs/promises';
import { extname, join, normalize, resolve } from 'node:path';

const siteDir = resolve(process.argv[2] ?? 'site');
const shotDir = resolve(process.argv[3] ?? 'screens');
await mkdir(shotDir, { recursive: true });

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.svg': 'image/svg+xml',
};

const server = createServer(async (req, res) => {
  let path = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  if (path === '/' || path.startsWith('/s/')) path = '/index.html';
  const file = normalize(join(siteDir, path));
  if (!file.startsWith(siteDir)) { res.writeHead(403).end(); return; }
  try {
    const body = await readFile(file);
    res.writeHead(200, { 'Content-Type': types[extname(file)] ?? 'application/octet-stream' });
    res.end(body);
  } catch {
    res.writeHead(404).end('not found');
  }
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
const base = `http://127.0.0.1:${server.address().port}`;
console.log(`serving ${siteDir} at ${base}`);

const browser = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
const failures = [];
const fatal = /Aborted|RuntimeError|abort\(|unreachable|out of memory|failed to load|could not start|exited/i;

async function open(name, url, viewport) {
  const page = await browser.newPage({ viewport, deviceScaleFactor: 2 });
  const log = [];
  page.on('console', msg => {
    const line = `[${msg.type()}] ${msg.text()}`;
    log.push(line);
    console.log(`  ${name} ${line}`);
    if (msg.type() === 'error' && fatal.test(msg.text()))
      failures.push(`${name}: ${msg.text()}`);
  });
  page.on('pageerror', err => {
    console.log(`  ${name} [pageerror] ${err.stack || err.message}`);
    failures.push(`${name}: page error: ${err.message}`);
  });
  page.on('requestfailed', r => failures.push(`${name}: request failed: ${r.url()}`));
  await page.goto(base + url);
  return { page, log };
}

async function waitFor(page, log, predicate, timeoutMs, what) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const state = await page.evaluate(() => window.calm && window.calm.state);
    if (state === 'failed') throw new Error(`${what}: the page reported a failed start`);
    if (await predicate(state)) return;
    await page.waitForTimeout(250);
  }
  throw new Error(`${what}: timed out after ${timeoutMs} ms\n${log.slice(-20).join('\n')}`);
}

async function step(name, body) {
  try {
    await body();
  } catch (err) {
    failures.push(`${name}: ${err.message}`);
  }
}

// 1. The app's QML smoke test: exits 1 on any QML warning.
await step('QML smoke test', async () => {
  const { page, log } = await open('smoke', '/?smoke=1', { width: 430, height: 932 });
  try {
    await waitFor(page, log, () => log.some(l => l.includes('smoke test:')), 120000, 'QML smoke test');
    if (!log.some(l => l.includes('smoke test: ok')))
      failures.push('QML smoke test reported warnings:\n' + log.join('\n'));
  } finally {
    await page.close();
  }
});

// 2. Home screen at phone and desktop sizes, and a Hard game, whose puzzle is
//    generated on the browser's main thread.
const phone = { width: 430, height: 932 };
for (const [name, url, viewport, settleMs] of [['sudoku-web', '/', phone, 4000],
                                               ['sudoku-web-desktop', '/', { width: 1280, height: 800 }, 4000],
                                               ['sudoku-web-game', '/?play=Hard', phone, 8000]]) {
  await step(name, async () => {
    const { page, log } = await open(name, url, viewport);
    try {
      await waitFor(page, log, s => s === 'running', 120000, `${name} start`);
      await page.waitForTimeout(settleMs); // first frames, the splash fade, generation
      const state = await page.evaluate(() => window.calm.state);
      if (state !== 'running') failures.push(`${name}: app state is ${state} after start`);
      const canvases = await page.evaluate(() => {
        const count = root => root.querySelectorAll('canvas').length
          + [...root.querySelectorAll('*')].reduce((n, e) => n + (e.shadowRoot ? count(e.shadowRoot) : 0), 0);
        return count(document);
      });
      if (canvases < 1) failures.push(`${name}: no Qt canvas on the page`);
      await page.screenshot({ path: join(shotDir, `${name}.png`) });
      if (process.env.TEMP_PREVIEW)
        await page.screenshot({ path: join(shotDir, `${name}-preview.jpg`), type: 'jpeg', quality: 70, scale: 'css' });
      console.log(`saved ${join(shotDir, `${name}.png`)} (${canvases} canvas)`);
    } finally {
      await page.close();
    }
  });
}

// 3. A share link loads the same app and hands it the payload.
await step('share link', async () => {
  const { page, log } = await open('share', '/s/smoke-payload', { width: 430, height: 932 });
  try {
    await waitFor(page, log, () => log.some(l => l.includes('share link: smoke-payload')), 120000, 'share link');
  } finally {
    await page.close();
  }
});

await browser.close();
server.close();

if (failures.length) {
  console.error('\nweb smoke test FAILED:\n- ' + failures.join('\n- '));
  process.exit(1);
}
console.log('\nweb smoke test passed');
