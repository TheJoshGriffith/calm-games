# Gwynant Sudoku on the web

Gwynant Sudoku lives at **https://sudoku.gwynant.app**, served by the
Cloudflare Pages project `gwynant-sudoku` (production branch `main`).

The same Qt app runs in the browser, built with Qt for WebAssembly. There is
no separate web code: `games/sudoku/app/web/` only holds the page that loads
the app, its icons and the Cloudflare Pages settings.

## How it is built

`.github/workflows/web.yml` runs on every pull request and every push to main:

1. Installs Qt 6.8.3 for Linux (host tools, `QT_HOST_PATH`) and Qt 6.8.3 for
   WebAssembly (`wasm_singlethread`, with Qt Multimedia), plus Emscripten
   3.1.56, the version Qt 6.8 is built against.
2. Configures with `qt-cmake` (tests and tools off) and builds `CalmSudoku`.
3. `games/sudoku/app/web/make-site.sh` assembles `site/`: `CalmSudoku.wasm`,
   `CalmSudoku.js`, `qtloader.js`, our `index.html` (instead of Qt's generated
   page), icons, `_headers` and `_redirects`.
4. `games/sudoku/app/web/smoke.mjs` serves the site and opens it in headless
   Chromium: it runs the app's QML smoke test (`?smoke=1`, fails on any QML
   warning), screenshots the home screen at phone and desktop sizes and a
   Hard game (so puzzle generation is exercised), checks each screenshot is
   really rendered, and checks a share link reaches the app.
5. Uploads `site/` and the screenshots as workflow artifacts
   (`sudoku-web-site`, `sudoku-web-screenshots`), and the job summary lists
   the download sizes. The build fails if `CalmSudoku.wasm` outgrows
   Cloudflare Pages' 25 MiB per-file limit (it is about 22 MB; Cloudflare
   serves it brotli-compressed, about 6.5 MB).
6. On main only, and only once the two Cloudflare secrets exist, deploys
   `site/` to the Cloudflare Pages project `gwynant-sudoku`. Until then the
   deploy steps are skipped and CI stays green.

The single-threaded build needs no special headers (multithreaded Qt for
WebAssembly would need COOP/COEP). It does mean puzzles are generated on the
main thread, one at a time between frames, instead of on a worker thread.

### Building it yourself

```sh
# Qt 6.8.3 for Linux and for WebAssembly (wasm_singlethread), Emscripten 3.1.56
source ~/emsdk/emsdk_env.sh
~/Qt/6.8.3/wasm_singlethread/bin/qt-cmake -S . -B build-wasm -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.8.3/gcc_64 \
    -DCALM_BUILD_TESTS=OFF -DCALM_BUILD_TOOLS=OFF
cmake --build build-wasm --target CalmSudoku
games/sudoku/app/web/make-site.sh build-wasm site
python3 -m http.server -d site 8000      # then open http://localhost:8000
```

`python3 -m http.server` does not apply `_redirects`, so share links
(`/s/...`) only work on Cloudflare or under `smoke.mjs`'s own server.

## Share links

`_redirects` serves `index.html` for `https://sudoku.gwynant.app/s/<payload>`
(a rewrite, so the address bar keeps the link), and the page passes the
payload to the app as `--share <payload>`. `main.cpp` currently only logs it;
opening a shared board will hook in there.

## Setting up Cloudflare (Josh)

1. **API token.** Cloudflare dashboard > My Profile > API Tokens > Create
   Token > "Create Custom Token", with the permission
   *Account > Cloudflare Pages > Edit* for your account. Copy the token.
2. **Account ID.** Shown on the right of any domain's Overview page, or under
   Workers & Pages > Overview.
3. **GitHub secrets.** In the repository: Settings > Secrets and variables >
   Actions > New repository secret, add `CLOUDFLARE_API_TOKEN` and
   `CLOUDFLARE_ACCOUNT_ID`.
4. **The Pages project** `gwynant-sudoku`, production branch `main`. The
   workflow creates it on its first deploy if it doesn't exist. To create it
   by hand: Workers & Pages > Create > Pages > "Upload assets" (Direct
   Upload). Don't connect it to the GitHub repository: a repository-connected
   project builds the code itself and won't take uploads from wrangler. Once
   the secrets are in, re-run the Web workflow on main (Actions > Web > Run
   workflow) or push to main to deploy. It then also serves at
   `https://gwynant-sudoku.pages.dev`.
5. **Custom domain.** Workers & Pages > gwynant-sudoku > Custom domains >
   Set up a custom domain > `sudoku.gwynant.app`. Because `gwynant.app` is on
   Cloudflare, the CNAME record and certificate are added for you.

Nothing in the code depends on the domain name; only these docs mention it.
