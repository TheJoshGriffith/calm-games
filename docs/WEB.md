# Calm games on the web

Every game also runs in the browser, built from the same Qt app with Qt for
WebAssembly, and is hosted on Cloudflare Pages:

| Game | Pages project | Address |
| --- | --- | --- |
| Sudoku | `gwynant-sudoku` | https://sudoku.gwynant.app |
| any game `<name>` | `gwynant-<name>` | https://\<name\>.gwynant.app |

There is no separate web code. `tools/web/` holds the shared loading page,
site assembly and browser smoke test; a game adds only optional extras in
`games/<name>/app/web/`.

## How it is built

`.github/workflows/web.yml` runs on every pull request and every push to main.
Its "Find web games" job lists every `games/<name>/app/CMakeLists.txt` with a
`qt_add_executable` target, and each game gets a "\<Title\> (WebAssembly)" job
(fail-fast off, so one broken game doesn't hide the others):

1. Installs Qt 6.8.3 for Linux (host tools, `QT_HOST_PATH`) and Qt 6.8.3 for
   WebAssembly (`wasm_singlethread`, with Qt Multimedia), plus Emscripten
   3.1.56, the version Qt 6.8 is built against. Both are cached and shared by
   every game's job.
2. Configures with `qt-cmake` (tests and tools off, `ENV` exported so the page
   can pass environment variables) and builds the game's target.
3. `tools/web/make-site.py` assembles `site/`:
   - `<Target>.wasm`, `<Target>.js` and `qtloader.js`;
   - `tools/web/index.html` in place of Qt's generated page: a calm loading
     screen with "Gwynant \<Title\>" and a progress bar, light or dark to
     match the system;
   - `tools/web/_headers` (`application/wasm`, long caching for engine files,
     which the page loads as `?v=<commit>`);
   - icons drawn from the game's iOS app icon;
   - anything in `games/<name>/app/web/site/`, copied last.
4. `tools/web/smoke.mjs` serves the site the way Pages does and opens it in
   headless Chromium. It runs the app's QML smoke test (`?smoke=1`, which fails
   on any QML warning), screenshots the home screen at phone and desktop sizes,
   and checks they are really rendered. Then it runs the game's own checks from
   `games/<name>/app/web/smoke.mjs`, if there are any.
5. Uploads `<name>-web-site` and `<name>-web-screenshots` as workflow
   artifacts, and lists the download sizes in the job summary. The job fails
   if the `.wasm` outgrows Cloudflare Pages' 25 MiB per-file limit.
6. Deploys `site/` to the Pages project `gwynant-<name>` with
   `wrangler pages deploy`, after the smoke test passes. It creates the
   project first if it doesn't exist.
   - A push to main deploys production.
   - A pull request from this repository deploys a preview at
     `https://<branch>.gwynant-<name>.pages.dev`; the link is in the job
     summary.
   - Without the two Cloudflare secrets (fork pull requests) nothing is
     deployed and CI stays green.

### Adding a game

Nothing to do in `web.yml`: a new `games/<name>/app` with a
`qt_add_executable` target is picked up automatically. To work in the browser,
the app should:

- build with single-threaded Qt. `QT_CONFIG(thread)` is false there, so keep
  QtConcurrent or QThread work behind it and do the work in short slices on
  the event loop instead (see `SudokuGame::generateNext`);
- print "smoke test: ok" under `CALM_SMOKE_TEST=1`, like every game's
  `main.cpp`;
- optionally add `games/<name>/app/web/site/` (extra static files such as
  `_redirects`) and `games/<name>/app/web/smoke.mjs` (extra browser checks;
  see Sudoku's).

After its first deploy to main, add its custom domain (step 5 below).

### Differences from the phone apps

- **Threads.** Single-threaded Qt needs no special COOP/COEP headers.
  Sudoku generates puzzles on the main thread instead of with QtConcurrent:
  one at a time, with frames drawn in between, starting shortly after the
  first screen appears. A level the player is waiting for goes first, and a
  Hard puzzle takes a few hundred milliseconds.
- **Sound** works through Qt Multimedia's WebAssembly backend (Web Audio).
  Browsers only allow audio after the first tap or key press, so the very
  first sound may be silent; after that every feedback sound plays. Sudoku's
  smoke test checks that sounds start after tapping digits.
- **Haptics** do nothing (`Platform_generic.cpp`).
- **Settings, saves and statistics** live in Emscripten's IndexedDB-backed
  file system, per browser and per domain. Sudoku's smoke test checks that a
  game survives a reload.

### Building it yourself

```sh
# Qt 6.8.3 for Linux and for WebAssembly (wasm_singlethread), Emscripten 3.1.56
source ~/emsdk/emsdk_env.sh
~/Qt/6.8.3/wasm_singlethread/bin/qt-cmake -S . -B build-wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.8.3/gcc_64 \
    -DQT_WASM_EXTRA_EXPORTED_METHODS=ENV -DCALM_BUILD_TESTS=OFF -DCALM_BUILD_TOOLS=OFF
cmake --build build-wasm --target CalmSudoku
python3 tools/web/make-site.py --build-dir build-wasm --game sudoku --out site
python3 -m http.server -d site 8000      # then open http://localhost:8000
# or, with Playwright installed: node tools/web/smoke.mjs site screens sudoku
```

`python3 -m http.server` ignores `_redirects`; `smoke.mjs`'s own server
applies its rewrites.

## Sudoku share links

`games/sudoku/app/web/site/_redirects` serves `index.html` for
`https://sudoku.gwynant.app/s/<payload>`. It is a rewrite, so the address bar
keeps the link. The page passes the payload to the app as `--share <payload>`.
`main.cpp` only logs it for now; opening a shared board will hook in there.

## Setting up Cloudflare (Josh)

1. **API token.** Cloudflare dashboard > My Profile > API Tokens > Create
   Token > "Create Custom Token", with the permission
   *Account > Cloudflare Pages > Edit* for your account.
2. **Account ID.** Shown under Workers & Pages > Overview, or on the right of
   any domain's Overview page.
3. **GitHub secrets.** Repository Settings > Secrets and variables > Actions:
   `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID`. (Done.)
4. **Pages projects.** Nothing to do: each game's first deploy creates
   `gwynant-<name>` (production branch `main`) as a Direct Upload project.
   If you create one by hand, choose "Upload assets", not a GitHub-connected
   project, which would build the code itself and refuse wrangler uploads.
   To redeploy without a new commit: Actions > Web > Run workflow on main.
5. **Custom domain, once per game** after its first production deploy:
   Workers & Pages > `gwynant-<name>` > Custom domains > Set up a custom
   domain > `<name>.gwynant.app`. Because `gwynant.app` is on Cloudflare, the
   CNAME record and certificate are added for you. Until then each game is at
   `https://gwynant-<name>.pages.dev`.

Nothing in the code depends on the domain names; only these docs mention them.
