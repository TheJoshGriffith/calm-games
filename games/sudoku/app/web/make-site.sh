#!/usr/bin/env bash
# Assembles the static web site for Gwynant Sudoku from a Qt for WebAssembly
# build:  games/sudoku/app/web/make-site.sh <wasm-build-dir> <site-dir> [build-id]
#
# The site is plain static files (Cloudflare Pages, or python3 -m http.server).
set -euo pipefail

build_dir=${1:?usage: make-site.sh <wasm-build-dir> <site-dir> [build-id]}
site_dir=${2:?usage: make-site.sh <wasm-build-dir> <site-dir> [build-id]}
build_id=${3:-$(date +%s)}
web_dir=$(cd "$(dirname "$0")" && pwd)
app_dir="$build_dir/games/sudoku/app"

for f in CalmSudoku.js CalmSudoku.wasm qtloader.js; do
    if [ ! -f "$app_dir/$f" ]; then
        echo "missing $app_dir/$f (is this a Qt for WebAssembly build?)" >&2
        exit 1
    fi
done

rm -rf "$site_dir"
mkdir -p "$site_dir"
cp "$app_dir/CalmSudoku.js" "$app_dir/CalmSudoku.wasm" "$app_dir/qtloader.js" "$site_dir/"
cp "$web_dir/_headers" "$web_dir/_redirects" "$web_dir"/*.png "$web_dir/favicon.ico" "$site_dir/"
# Our branded page replaces Qt's generated CalmSudoku.html. The build id makes
# fresh URLs for the engine files on every deploy, so they can be cached hard.
sed "s/__BUILD__/$build_id/g" "$web_dir/index.html" > "$site_dir/index.html"

ls -l "$site_dir"
