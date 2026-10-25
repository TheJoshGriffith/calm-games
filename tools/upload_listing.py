#!/usr/bin/env python3
"""Fill in a game's App Store listing from store/<game>.json.

Sets, on the App Store version still being prepared and its app info:
name, subtitle and privacy policy URL; description, keywords, promotional
text, support and marketing URLs; copyright; category (Games plus two
subcategories); the age rating questionnaire (none of everything, so 4+);
the content rights declaration; and a free price. Store text is checked
against Apple's limits first.

    ASC_KEY_ID=... ASC_ISSUER_ID=... ASC_KEY_P8="$(cat AuthKey.p8)" \\
        python3 tools/upload_listing.py --game sudoku

--dry-run only checks the listing. App Privacy ("Data Not Collected"),
availability and the App Review contact can't be set through the API, so
they stay in App Store Connect. Needs requests and PyJWT with cryptography.
"""

import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from upload_screenshots import EDITABLE, Connect  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[1]

LIMITS = {"name": 30, "subtitle": 30, "promotionalText": 170, "description": 4000, "keywords": 100}
PLAY_LIMITS = {"title": 30, "shortDescription": 80, "fullDescription": 4000}

# The age rating answers for a calm game: nothing to declare.
AGE_RATING_NONE = [
    "alcoholTobaccoOrDrugUseOrReferences", "contests", "gamblingSimulated", "gunsOrOtherWeapons",
    "horrorOrFearThemes", "matureOrSuggestiveThemes", "medicalOrTreatmentInformation",
    "profanityOrCrudeHumor", "sexualContentGraphicAndNudity", "sexualContentOrNudity",
    "violenceCartoonOrFantasy", "violenceRealistic", "violenceRealisticProlongedGraphicOrSadistic",
]
AGE_RATING_FALSE = [
    "advertising", "ageAssurance", "gambling", "healthOrWellnessTopics", "lootBox", "messagingAndChat",
    "parentalControls", "unrestrictedWebAccess", "userGeneratedContent",
]


def check(listing):
    problems = [f"{key} is {len(listing[key])} characters, over {limit}"
                for key, limit in LIMITS.items() if len(listing[key]) > limit]
    problems += [f"play.{key} is {len(listing['play'][key])} characters, over {limit}"
                 for key, limit in PLAY_LIMITS.items() if len(listing["play"][key]) > limit]
    if " ," in listing["keywords"] or ", " in listing["keywords"]:
        problems.append("keywords: no spaces around commas (they count towards the 100)")
    return problems


def one(items, what):
    if not items:
        raise RuntimeError(f"no {what}")
    return items[0]


def state(item):
    attributes = item["attributes"]
    return attributes.get("appStoreState") or attributes.get("state")


def localization(asc, path, kind, parent, locale):
    """The localization for `locale` under `path`, created if missing."""
    for item in asc.all(path):
        if item["attributes"]["locale"] == locale:
            return item["id"]
    relationship = "appInfo" if kind == "appInfoLocalizations" else "appStoreVersion"
    parent_type = "appInfos" if kind == "appInfoLocalizations" else "appStoreVersions"
    return asc.call("POST", f"/{kind}", json={"data": {
        "type": kind, "attributes": {"locale": locale},
        "relationships": {relationship: {"data": {"type": parent_type, "id": parent}}},
    }})["data"]["id"]


def patch(asc, kind, item_id, attributes=None, relationships=None):
    data = {"type": kind, "id": item_id}
    if attributes:
        data["attributes"] = attributes
    if relationships:
        data["relationships"] = relationships
    asc.call("PATCH", f"/{kind}/{item_id}", json={"data": data})


def set_free(asc, app_id):
    points = asc.all(f"/apps/{app_id}/appPricePoints", **{"filter[territory]": "USA", "limit": 200})
    free = one([p for p in points if float(p["attributes"]["customerPrice"]) == 0], "free price point")
    asc.call("POST", "/appPriceSchedules", json={
        "data": {"type": "appPriceSchedules", "relationships": {
            "app": {"data": {"type": "apps", "id": app_id}},
            "baseTerritory": {"data": {"type": "territories", "id": "USA"}},
            "manualPrices": {"data": [{"type": "appPrices", "id": "${free}"}]},
        }},
        "included": [{"type": "appPrices", "id": "${free}", "attributes": {"startDate": None},
                      "relationships": {"appPricePoint": {"data": {"type": "appPricePoints", "id": free["id"]}}}}],
    })


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", required=True)
    parser.add_argument("--dry-run", action="store_true")
    opts = parser.parse_args()

    listing = json.loads((ROOT / "store" / f"{opts.game}.json").read_text())
    problems = check(listing)
    for problem in problems:
        print(f"store/{opts.game}.json: {problem}", file=sys.stderr)
    if problems:
        return 1
    print(f"{listing['name']}: {listing['subtitle']} ({listing['locale']})")
    if opts.dry_run:
        return 0

    asc = Connect()
    locale = listing["locale"]
    bundle = f"com.thejoshgriffith.calm.{opts.game}"
    app = one(asc.all("/apps", **{"filter[bundleId]": bundle}), f"App Store Connect app for {bundle}")
    warnings = []

    def step(name, action):
        try:
            action()
            print(f"  {name}", flush=True)
        except RuntimeError as error:
            warnings.append(f"{bundle}: {name}: {error}")

    # App info: name, subtitle, privacy policy, category, age rating.
    info = one([i for i in asc.all(f"/apps/{app['id']}/appInfos") if state(i) in EDITABLE],
               "app info open for changes")
    info_loc = localization(asc, f"/appInfos/{info['id']}/appInfoLocalizations", "appInfoLocalizations",
                            info["id"], locale)
    step("name, subtitle, privacy policy", lambda: patch(asc, "appInfoLocalizations", info_loc, {
        "name": listing["name"], "subtitle": listing["subtitle"], "privacyPolicyUrl": listing["privacyPolicyUrl"]}))

    category = {"primaryCategory": {"data": {"type": "appCategories", "id": listing["primaryCategory"]}}}
    for slot, sub in zip(["primarySubcategoryOne", "primarySubcategoryTwo"], listing["primarySubcategories"]):
        category[slot] = {"data": {"type": "appCategories", "id": sub}}
    step("category", lambda: patch(asc, "appInfos", info["id"], relationships=category))

    def age_rating():
        declaration = asc.call("GET", f"/appInfos/{info['id']}/ageRatingDeclaration")["data"]
        known = declaration["attributes"]
        answers = {k: "NONE" for k in AGE_RATING_NONE if k in known}
        answers.update({k: False for k in AGE_RATING_FALSE if k in known})
        patch(asc, "ageRatingDeclarations", declaration["id"], answers)
    step("age rating", age_rating)

    step("content rights", lambda: patch(asc, "apps", app["id"],
                                         {"contentRightsDeclaration": "DOES_NOT_USE_THIRD_PARTY_CONTENT"}))
    step("free price", lambda: set_free(asc, app["id"]))

    # The version: copyright and the product page text.
    version = one([v for v in asc.all(f"/apps/{app['id']}/appStoreVersions", **{"filter[platform]": "IOS"})
                   if state(v) in EDITABLE], "App Store version open for changes")
    step(f"copyright on {version['attributes']['versionString']}",
         lambda: patch(asc, "appStoreVersions", version["id"], {"copyright": listing["copyright"]}))
    version_loc = localization(asc, f"/appStoreVersions/{version['id']}/appStoreVersionLocalizations",
                               "appStoreVersionLocalizations", version["id"], locale)
    step("description, keywords, promotional text, URLs", lambda: patch(
        asc, "appStoreVersionLocalizations", version_loc,
        {key: listing[key] for key in ["description", "keywords", "promotionalText", "supportUrl", "marketingUrl"]}))

    for warning in warnings:
        print(f"::warning::{warning}")
    return 1 if warnings else 0


if __name__ == "__main__":
    sys.exit(main())
