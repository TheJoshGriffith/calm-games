#!/usr/bin/env python3
"""Upload one game's store screenshots to App Store Connect.

Takes the images tools/store_screenshots.py makes (<dir>/<game>/<device>/
<nn>-<shot>-<theme>.png) and puts them on the game's App Store version that
is still being prepared, in every localization it has, replacing the
screenshots already there. The App Store shows at most ten per device, so
each set is every light shot followed by the dark ones until there are ten.

    ASC_KEY_ID=... ASC_ISSUER_ID=... ASC_KEY_P8="$(cat AuthKey.p8)" \\
        python3 tools/upload_screenshots.py --game sudoku --dir store-screenshots

--dry-run prints the sets without touching App Store Connect. Needs
requests and PyJWT with cryptography.
"""

import argparse
import hashlib
import os
import pathlib
import sys
import time

API = "https://api.appstoreconnect.apple.com/v1"

# Our device folder: App Store Connect's display type for its size.
DISPLAY_TYPES = {
    "iphone-6.9": "APP_IPHONE_67",
    "ipad-13": "APP_IPAD_PRO_3GEN_129",
}

MAX_PER_SET = 10

# Version states whose screenshots can still be changed.
EDITABLE = {"PREPARE_FOR_SUBMISSION", "DEVELOPER_REJECTED", "REJECTED", "METADATA_REJECTED", "INVALID_BINARY"}


def pick(folder):
    """The set for one device: light shots first, then dark, at most ten."""
    files = sorted(folder.glob("*.png"))
    light = [f for f in files if f.stem.endswith("-light")]
    dark = [f for f in files if f.stem.endswith("-dark")]
    return (light + dark)[:MAX_PER_SET]


class Connect:
    def __init__(self):
        import jwt
        import requests
        self.requests = requests
        self.session = requests.Session()
        self.key_id = os.environ["ASC_KEY_ID"]
        self.issuer = os.environ["ASC_ISSUER_ID"]
        self.key = os.environ["ASC_KEY_P8"]
        self.jwt = jwt
        self.expires = 0

    def headers(self):
        now = int(time.time())
        if now > self.expires - 60:
            self.expires = now + 15 * 60
            token = self.jwt.encode({"iss": self.issuer, "iat": now, "exp": self.expires, "aud": "appstoreconnect-v1"},
                                    self.key, algorithm="ES256", headers={"kid": self.key_id, "typ": "JWT"})
            self.session.headers["Authorization"] = f"Bearer {token}"

    def call(self, method, path, **kwargs):
        self.headers()
        response = self.session.request(method, path if path.startswith("http") else API + path, **kwargs)
        if response.status_code >= 400:
            raise RuntimeError(f"{method} {path}: {response.status_code} {response.text}")
        return response.json() if response.content else None

    def all(self, path, **params):
        """Every item of a list, across pages."""
        items, url = [], path
        while url:
            page = self.call("GET", url, params=params if url == path else None)
            items += page["data"]
            url = page.get("links", {}).get("next")
        return items


def upload(asc, set_id, image):
    data = image.read_bytes()
    shot = asc.call("POST", "/appScreenshots", json={"data": {
        "type": "appScreenshots",
        "attributes": {"fileName": image.name, "fileSize": len(data)},
        "relationships": {"appScreenshotSet": {"data": {"type": "appScreenshotSets", "id": set_id}}},
    }})["data"]
    for part in shot["attributes"]["uploadOperations"]:
        chunk = data[part["offset"]:part["offset"] + part["length"]]
        headers = {h["name"]: h["value"] for h in part["requestHeaders"]}
        # A plain request: the upload URL must not get the API token.
        response = asc.requests.request(part["method"], part["url"], data=chunk, headers=headers, timeout=120)
        response.raise_for_status()
    asc.call("PATCH", f"/appScreenshots/{shot['id']}", json={"data": {
        "type": "appScreenshots", "id": shot["id"],
        "attributes": {"uploaded": True, "sourceFileChecksum": hashlib.md5(data).hexdigest()},
    }})
    return shot["id"]


def wait_until_processed(asc, ids):
    deadline = time.monotonic() + 600
    pending = set(ids)
    while pending:
        for shot_id in list(pending):
            state = asc.call("GET", f"/appScreenshots/{shot_id}")["data"]["attributes"]["assetDeliveryState"]
            if state["state"] == "COMPLETE":
                pending.discard(shot_id)
            elif state["state"] == "FAILED":
                raise RuntimeError(f"screenshot {shot_id} failed: {state.get('errors')}")
        if pending:
            if time.monotonic() > deadline:
                raise RuntimeError(f"{len(pending)} screenshot(s) still processing after 10 minutes")
            time.sleep(5)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", required=True)
    parser.add_argument("--dir", default="store-screenshots", help="where store_screenshots.py put the images")
    parser.add_argument("--dry-run", action="store_true")
    opts = parser.parse_args()

    sets = {}
    for device, display_type in DISPLAY_TYPES.items():
        images = pick(pathlib.Path(opts.dir, opts.game, device))
        if not images:
            sys.exit(f"no {device} screenshots for {opts.game} in {opts.dir}")
        sets[display_type] = images
        print(f"{display_type}: {', '.join(i.name for i in images)}")
    if opts.dry_run:
        return

    asc = Connect()
    bundle = f"com.thejoshgriffith.calm.{opts.game}"
    apps = asc.all("/apps", **{"filter[bundleId]": bundle})
    if not apps:
        sys.exit(f"no App Store Connect app for {bundle}")
    versions = [v for v in asc.all(f"/apps/{apps[0]['id']}/appStoreVersions", **{"filter[platform]": "IOS"})
                if v["attributes"]["appStoreState"] in EDITABLE]
    if not versions:
        sys.exit(f"{bundle} has no App Store version open for changes")
    version = versions[0]
    print(f"{bundle} {version['attributes']['versionString']} ({version['attributes']['appStoreState']})")

    for localization in asc.all(f"/appStoreVersions/{version['id']}/appStoreVersionLocalizations"):
        locale = localization["attributes"]["locale"]
        existing = {s["attributes"]["screenshotDisplayType"]: s["id"]
                    for s in asc.all(f"/appStoreVersionLocalizations/{localization['id']}/appScreenshotSets")}
        for display_type, images in sets.items():
            set_id = existing.get(display_type)
            if set_id:
                for old in asc.all(f"/appScreenshotSets/{set_id}/appScreenshots"):
                    asc.call("DELETE", f"/appScreenshots/{old['id']}")
            else:
                set_id = asc.call("POST", "/appScreenshotSets", json={"data": {
                    "type": "appScreenshotSets",
                    "attributes": {"screenshotDisplayType": display_type},
                    "relationships": {"appStoreVersionLocalization": {
                        "data": {"type": "appStoreVersionLocalizations", "id": localization["id"]}}},
                }})["data"]["id"]
            ids = [upload(asc, set_id, image) for image in images]
            wait_until_processed(asc, ids)
            asc.call("PATCH", f"/appScreenshotSets/{set_id}/relationships/appScreenshots",
                     json={"data": [{"type": "appScreenshots", "id": i} for i in ids]})
            print(f"{locale} {display_type}: {len(ids)} uploaded", flush=True)


if __name__ == "__main__":
    main()
