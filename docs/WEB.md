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
2. Configures only that game with `qt-cmake` (`-DCALM_GAMES=<name>`, tests and
   tools off; `ENV`, `FS` and the run-dependency functions exported and
   `-lidbfs.js` linked so the page can pass environment variables and keep the
   home directory in IndexedDB) and builds the game's target.
3. `tools/web/make-site.py` assembles `site/`:
   - `<Target>.wasm.gz` (the app's code, gzipped), `<Target>.js` and
     `qtloader.js`;
   - `tools/web/index.html` in place of Qt's generated page: a calm loading
     screen with "Gwynant \<Title\>" and a progress bar, light or dark to
     match the system;
   - `tools/web/_headers` (long caching for engine files, which the page
     loads as `?v=<commit>`);
   - icons drawn from the game's iOS app icon;
   - anything in `games/<name>/app/web/site/`, copied last.
4. `tools/web/smoke.mjs` serves the site the way Pages does and opens it in
   headless Chromium. It runs the app's QML smoke test (`?smoke=1`, which fails
   on any QML warning), screenshots the home screen at phone and desktop sizes,
   and checks they are really rendered. Then it runs the game's own checks from
   `games/<name>/app/web/smoke.mjs`, if there are any.
5. Uploads `<name>-web-site` and `<name>-web-screenshots` as workflow
   artifacts, and lists the `.wasm` size (raw, gzip as uploaded, brotli) in
   the job summary with a breakdown of what it is made of, from the link map.
   The job warns when any uploaded file passes 15 MiB and fails over
   Cloudflare Pages' 25 MiB per-file limit.
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

### Download size

Each game is one `.wasm` of about 23 MB: Qt's QML engine, Qt Quick, Qt GUI
and Qt Core are about 14 MB of code between them, and the framework's sounds
and fonts are 4.5 MB of data. Cloudflare Pages takes files up to 25 MiB, so:

- **The code is uploaded gzipped.** `make-site.py` writes `<Target>.wasm.gz`
  (about 9.6 MB) and no raw `.wasm`. It is served as a plain
  `application/gzip` file (see `_headers`), which Cloudflare passes through
  untouched. The page unzips it with the browser's `DecompressionStream` and
  compiles it with `WebAssembly.compileStreaming` while it downloads (falling
  back to compiling in one go), and the smoke test checks the streaming path
  is used. Browsers without `DecompressionStream` (before Safari 16.4 or
  Firefox 113) get a "please update your browser" message. Before this, the
  raw file went out with Cloudflare's own brotli at about 10.3 MB.
- **Unused parts of Qt are left out** by `framework/qt/cmake/CalmWeb.cmake`
  (WebAssembly builds only): image format, icon engine and TLS plugins, Qt
  Quick Controls (Calm uses QtQuick.Templates), the DejaVu fallback fonts the
  wasm platform plugin embeds (Calm loads Nunito), and Qt Multimedia's video
  shaders and Qt Quick's curve-renderer shaders. That saves about 2.1 MB raw
  per game. `-DCALM_WEB_TRIM=...` switches the items on and off for
  measuring; a game that needs one of them back (say, JPEG images) should ask
  for it to be removed from the default list.
- The job summary breaks the `.wasm` down by Qt module, plugin and resource
  from the link map (`tools/web/wasm-size-report.py`).

### Building it yourself

```sh
# Qt 6.8.3 for Linux and for WebAssembly (wasm_singlethread), Emscripten 3.1.56
source ~/emsdk/emsdk_env.sh
~/Qt/6.8.3/wasm_singlethread/bin/qt-cmake -S . -B build-wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DQT_HOST_PATH=$HOME/Qt/6.8.3/gcc_64 \
    -DQT_WASM_EXTRA_EXPORTED_METHODS="ENV,FS,addRunDependency,removeRunDependency" \
    -DCMAKE_EXE_LINKER_FLAGS="-lidbfs.js -Wl,--Map=$PWD/link.map" \
    -DCALM_GAMES=sudoku -DCALM_BUILD_TESTS=OFF -DCALM_BUILD_TOOLS=OFF
cmake --build build-wasm --target CalmSudoku
python3 tools/web/make-site.py --build-dir build-wasm --game sudoku --out site
python3 tools/web/wasm-size-report.py build-wasm/games/sudoku/app/CalmSudoku.wasm --map link.map
python3 -m http.server -d site 8000      # then open http://localhost:8000
# or, with Playwright installed: node tools/web/smoke.mjs site screens sudoku
```

`python3 -m http.server` ignores `_redirects`; `smoke.mjs`'s own server
applies its rewrites.

## Sudoku share links

Every Sudoku game can be shared from the share button in the game's header:
as its puzzle code (`H1-3KX9QZ0A`, typed in under "Enter a puzzle code" on
the home screen) or as a link, `https://sudoku.gwynant.app/s/<payload>`, to
the starting board or to the board with the player's digits, marks and
colours (format in `games/sudoku/engine/include/sudoku/Share.h`). Variant
puzzles (`KL1-3KX9QZ0A`) share their code instead of their clues, followed by
`.` and the progress: the app rebuilds the puzzle from the code.

- **Web.** `games/sudoku/app/web/site/_redirects` serves `index.html` for
  `/s/*`. It is a rewrite, so the address bar keeps the link. The page passes
  the payload to the app as `--share <payload>`, and `main.cpp` opens the
  board (`SudokuGame::openShared`). Sharing uses the browser's share sheet
  (`navigator.share`) where there is one, else copies the link and shows
  "Link copied". The smoke test opens a link with progress, checks the board
  is on screen, and shares it on.
- **iOS.** The share button opens the system share sheet. A tapped link
  opens the web app in Safari until Universal Links are set up (below);
  after that, iOS hands it to the app (`QDesktopServices` URL handler in
  `main.cpp`).
- **Desktop.** Sharing copies the link. `CalmSudoku --share <link or payload>`
  opens one.

### Universal Links (not set up yet)

For a tapped link to open the iOS app instead of Safari:

1. **Entitlement.** `com.apple.developer.associated-domains` =
   `applinks:sudoku.gwynant.app` in an entitlements file set as
   `CODE_SIGN_ENTITLEMENTS` on the iOS target.
2. **App ID.** Turn on Associated Domains for `com.thejoshgriffith.calm.sudoku`
   in Certificates, Identifiers & Profiles, so the App Store profile that the
   export makes includes it.
3. **Signing.** The archive is built with `CODE_SIGNING_ALLOWED=NO` (see
   `ios.yml`), and entitlements live in the code signature, so an unsigned
   archive carries none and `-exportArchive` signs the app without the
   entitlement. The archive needs the entitlements in a signature that uses
   no certificate: ad-hoc sign the archived app before exporting, e.g.
   `codesign --force --sign - --entitlements Sudoku.entitlements
   "$RUNNER_TEMP/App.xcarchive/Products/Applications/CalmSudoku.app"`. The export
   then re-signs with the cloud distribution certificate and keeps them.
   Check the result with `codesign -d --entitlements - <app>` on the exported
   `.ipa`.
4. **Site.** Serve `https://sudoku.gwynant.app/.well-known/apple-app-site-association`
   (no redirect, `Content-Type: application/json`, via `_headers`):
   `{"applinks":{"details":[{"appIDs":["<TEAM_ID>.com.thejoshgriffith.calm.sudoku"],"components":[{"/":"/s/*"}]}]}}`.
   The web workflow can write it from the `APPLE_TEAM_ID` secret.

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

Only Sudoku's share links depend on a domain name:
`SudokuGame::ShareUrlBase` is `https://sudoku.gwynant.app/s/`.
