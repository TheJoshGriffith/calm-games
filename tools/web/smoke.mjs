// Browser smoke test for a Calm game's web build.
//
//   node tools/web/smoke.mjs <site-dir> <screenshot-dir> <game>
//
// Serves the site like Cloudflare Pages would (application/wasm, the site's
// _redirects rewrites), then in headless Chromium:
//   1. runs the app's own QML smoke test (?smoke=1) and expects "smoke test: ok",
//   2. screenshots the home screen at phone and desktop sizes and checks each
//      is really rendered (<game>-web.png, <game>-web-desktop.png),
//   3. runs the game's own checks from games/<game>/app/web/smoke.mjs, if any,
// failing on page errors, aborts or a failed start.
//
// A game hook exports `default async function (t)`, where t has: open(name,
// url, viewport), waitFor(page, log, predicate, ms, what), inspect(png),
// step(name, fn), fail(message), shotDir, phone, base.
import { chromium } from 'playwright';
import { createServer } from 'node:http';
import { readFile, mkdir, access } from 'node:fs/promises';
import { extname, join, normalize, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const siteDir = resolve(process.argv[2] ?? 'site');
const shotDir = resolve(process.argv[3] ?? 'screens');
const game = process.argv[4];
if (!game) {
  console.error('usage: node tools/web/smoke.mjs <site-dir> <screenshot-dir> <game>');
  process.exit(2);
}
await mkdir(shotDir, { recursive: true });

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.svg': 'image/svg+xml',
};

// "/s/*  /index.html  200" style rewrites from the site's _redirects.
const rewrites = [];
try {
  for (const line of (await readFile(join(siteDir, '_redirects'), 'utf8')).split('\n')) {
    const [from, to, status] = line.trim().split(/\s+/);
    if (from && !from.startsWith('#') && status === '200')
      rewrites.push({ prefix: from.replace(/\*$/, ''), wildcard: from.endsWith('*'), to });
  }
} catch {
  // No _redirects: nothing to rewrite.
}

const server = createServer(async (req, res) => {
  let path = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  for (const r of rewrites)
    if (r.wildcard ? path.startsWith(r.prefix) : path === r.prefix) path = r.to;
  if (path === '/') path = '/index.html';
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
const fail = message => failures.push(message);
const fatal = /Aborted|RuntimeError|abort\(|unreachable|out of memory|failed to load|could not start|exited/i;
const phone = { width: 430, height: 932 };

// Counts Web Audio activity, so a test can tell whether sounds really play.
function audioProbe() {
  const probe = window.calmAudio = { contexts: 0, starts: 0, sources: 0, processors: 0, worklets: 0, states: [] };
  const Base = window.AudioContext;
  if (Base) {
    window.AudioContext = class extends Base {
      constructor(...args) {
        super(...args);
        probe.contexts++;
        probe.states.push(this.state);
        this.addEventListener('statechange', () => probe.states.push(this.state));
      }
    };
  }
  const count = (proto, name, key) => {
    const original = proto[name];
    if (original)
      proto[name] = function (...args) { probe[key]++; return original.apply(this, args); };
  };
  count(BaseAudioContext.prototype, 'createBufferSource', 'sources');
  count(BaseAudioContext.prototype, 'createScriptProcessor', 'processors');
  if (window.AudioWorkletNode) {
    const Node = window.AudioWorkletNode;
    window.AudioWorkletNode = class extends Node {
      constructor(...args) { super(...args); probe.worklets++; }
    };
  }
  const start = AudioBufferSourceNode.prototype.start;
  AudioBufferSourceNode.prototype.start = function (...args) {
    probe.starts++;
    return start.apply(this, args);
  };
}

// Checks a screenshot is a real rendered screen (not blank, not just the
// splash) and returns a coarse character map of it for the log.
async function inspect(png) {
  const page = await browser.newPage();
  try {
    return await page.evaluate(async b64 => {
      const img = new Image();
      img.src = 'data:image/png;base64,' + b64;
      await img.decode();
      const canvas = document.createElement('canvas');
      canvas.width = img.width;
      canvas.height = img.height;
      const g = canvas.getContext('2d');
      g.drawImage(img, 0, 0);
      const d = g.getImageData(0, 0, canvas.width, canvas.height).data;
      const lum = j => 0.299 * d[j] + 0.587 * d[j + 1] + 0.114 * d[j + 2];
      const colours = new Set();
      let dark = 0;
      for (let j = 0; j < d.length; j += 4) {
        colours.add(((d[j] >> 4) << 8) | ((d[j + 1] >> 4) << 4) | (d[j + 2] >> 4));
        if (lum(j) < 110) dark++;
      }
      const cols = 54;
      const cw = canvas.width / cols, ch = cw * 2;
      const lines = [];
      for (let r = 0; (r + 1) * ch <= canvas.height; r++) {
        let line = '';
        for (let k = 0; k < cols; k++) {
          let sum = 0, sat = 0, n = 0;
          for (let y = Math.floor(r * ch); y < (r + 1) * ch; y += 2) {
            for (let x = Math.floor(k * cw); x < (k + 1) * cw; x += 2) {
              const j = (y * canvas.width + x) * 4;
              sum += lum(j);
              sat += Math.max(d[j], d[j + 1], d[j + 2]) - Math.min(d[j], d[j + 1], d[j + 2]);
              n++;
            }
          }
          const l = sum / n, s = sat / n;
          line += l >= 250 ? ' ' : l >= 238 ? '.' : l >= 200 ? (s > 25 ? '~' : ':') : l >= 140 ? (s > 40 ? 'o' : '-') : l >= 80 ? '+' : '#';
        }
        lines.push(line);
      }
      return { colours: colours.size, dark: dark / (d.length / 4), map: lines.join('\n') };
    }, png.toString('base64'));
  } finally {
    await page.close();
  }
}

async function open(name, url, viewport = phone) {
  const page = await browser.newPage({ viewport, deviceScaleFactor: 2 });
  await page.addInitScript(audioProbe);
  const log = [];
  page.on('console', msg => {
    const line = `[${msg.type()}] ${msg.text()}`;
    log.push(line);
    console.log(`  ${name} ${line}`);
    if (msg.type() === 'error' && fatal.test(msg.text()))
      fail(`${name}: ${msg.text()}`);
  });
  page.on('pageerror', err => {
    console.log(`  ${name} [pageerror] ${err.stack || err.message}`);
    fail(`${name}: page error: ${err.message}`);
  });
  page.on('requestfailed', r => fail(`${name}: request failed: ${r.url()}`));
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
    fail(`${name}: ${err.message}`);
  }
}

// Loads `url`, waits for the app and settles, then saves and checks a
// screenshot. Returns the open page (caller closes it) and the inspection.
async function shoot(name, url, viewport = phone, settleMs = 4000) {
  const { page, log } = await open(name, url, viewport);
  await waitFor(page, log, s => s === 'running', 120000, `${name} start`);
  await page.waitForTimeout(settleMs); // first frames and the splash fade
  const state = await page.evaluate(() => window.calm.state);
  if (state !== 'running') fail(`${name}: app state is ${state} after start`);
  const canvases = await page.evaluate(() => {
    const count = root => root.querySelectorAll('canvas').length
      + [...root.querySelectorAll('*')].reduce((n, e) => n + (e.shadowRoot ? count(e.shadowRoot) : 0), 0);
    return count(document);
  });
  if (canvases < 1) fail(`${name}: no Qt canvas on the page`);
  const png = await page.screenshot({ path: join(shotDir, `${name}.png`) });
  const look = await inspect(png);
  console.log(`saved ${join(shotDir, `${name}.png`)} (${canvases} canvas, ${look.colours} colours, ` +
              `${(100 * look.dark).toFixed(1)}% dark)\n${look.map}`);
  if (look.colours < 12 || look.dark < 0.003)
    fail(`${name}: the screenshot looks blank (${look.colours} colours)`);
  return { page, log, look };
}

// 1. The app's QML smoke test: exits 1 on any QML warning.
await step('QML smoke test', async () => {
  const { page, log } = await open('smoke', '/?smoke=1');
  try {
    await waitFor(page, log, () => log.some(l => l.includes('smoke test:')), 120000, 'QML smoke test');
    if (!log.some(l => l.includes('smoke test: ok')))
      fail('QML smoke test reported warnings:\n' + log.join('\n'));
  } finally {
    await page.close();
  }
});

// 2. Home screen at phone and desktop sizes.
const home = {};
for (const [name, viewport] of [[`${game}-web`, phone], [`${game}-web-desktop`, { width: 1280, height: 800 }]]) {
  await step(name, async () => {
    const { page, look } = await shoot(name, '/', viewport);
    home[name] = look.map;
    await page.close();
  });
}

// 3. The game's own checks.
const root = resolve(fileURLToPath(import.meta.url), '../../..');
const hook = join(root, 'games', game, 'app', 'web', 'smoke.mjs');
if (await access(hook).then(() => true, () => false)) {
  console.log(`running ${hook}`);
  const { default: run } = await import(pathToFileURL(hook).href);
  await run({ open, waitFor, inspect, step, fail, shoot, shotDir, phone, base, home: home[`${game}-web`] });
}

await browser.close();
server.close();

if (failures.length) {
  console.error('\nweb smoke test FAILED:\n- ' + failures.join('\n- '));
  if (process.env.GITHUB_ACTIONS)
    for (const f of failures) console.log(`::error::${f.split('\n')[0]}`);
  process.exit(1);
}
console.log('\nweb smoke test passed');
