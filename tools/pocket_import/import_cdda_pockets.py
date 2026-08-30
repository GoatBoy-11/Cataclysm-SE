"""Extract CDDA pocket_data for items that still exist in CSE, validated.

Produces:
  cdda_pocket_overlay.json - reviewable pocket_data per matched item
  cdda_pocket_report.txt   - what was kept, dropped, and why
Nothing in CSE is modified; application is a separate, approved step.
"""
import json
import glob
import re
from collections import Counter

CDDA = "D:/Projects/CDDA"
CSE = "D:/Projects/CSE"
OUT = r"C:\Users\Oliver\AppData\Local\Temp\claude\d--Projects-CBN\9f5a2a9e-e00a-4d28-8127-907cc9642349\scratchpad"

SUPPORTED = {
    "pocket_type", "max_contains_volume", "max_contains_weight",
    "max_item_length", "rigid", "watertight", "sealed",
    "spoil_multiplier", "moves", "ammo_restriction", "item_restriction",
}
POCKET_TYPES = {"CONTAINER", "MAGAZINE", "MAGAZINE_WELL", "MOD", "CORPSE", "MIGRATION"}
VOL_RE = re.compile(r"^\d+(\.\d+)?\s*(ml|L)$")
WEIGHT_RE = re.compile(r"^\d+(\.\d+)?\s*(mg|g|kg)$")
LEN_RE = re.compile(r"^\d+(\.\d+)?\s*(mm|cm|m)$")


def entries(root, pattern):
    for f in glob.glob(root + pattern, recursive=True):
        try:
            d = json.load(open(f, encoding="utf-8"))
        except Exception:
            continue
        for e in (d if isinstance(d, list) else [d]):
            if isinstance(e, dict):
                yield e


def main():
    cdda_pocketed = {}
    for e in entries(CDDA, "/data/json/items/**/*.json"):
        if isinstance(e.get("id"), str) and "pocket_data" in e:
            cdda_pocketed[e["id"]] = e

    cse_ids, cse_ammotypes = set(), set()
    for e in entries(CSE, "/data/json/**/*.json"):
        i = e.get("id")
        if isinstance(i, str):
            cse_ids.add(i)
            if e.get("type") == "ammunition_type":
                cse_ammotypes.add(i)

    overlay = {}
    report = []
    dropped_fields = Counter()
    skipped = []

    matched = {i: e for i, e in cdda_pocketed.items()
               if i in cse_ids and len(e["pocket_data"]) >= 2}

    for item_id in sorted(matched):
        pockets_out = []
        notes = []
        for idx, p in enumerate(matched[item_id]["pocket_data"], 1):
            out = {}
            for k, v in p.items():
                if k not in SUPPORTED:
                    dropped_fields[k] += 1
                    continue
                out[k] = v
            ptype = out.get("pocket_type", "CONTAINER")
            if ptype not in POCKET_TYPES:
                notes.append(f"pocket {idx}: unsupported pocket_type {ptype!r}, pocket skipped")
                continue
            for key, rx, unit in (("max_contains_volume", VOL_RE, "volume"),
                                  ("max_contains_weight", WEIGHT_RE, "weight"),
                                  ("max_item_length", LEN_RE, "length")):
                v = out.get(key)
                if v is not None and not (isinstance(v, str) and rx.match(v)):
                    notes.append(f"pocket {idx}: unparseable {unit} {v!r}, field dropped")
                    out.pop(key)
            ir = out.get("item_restriction")
            if ir:
                missing = [x for x in ir if x not in cse_ids]
                if missing:
                    kept = [x for x in ir if x in cse_ids]
                    if kept:
                        out["item_restriction"] = kept
                        notes.append(f"pocket {idx}: dropped unknown items {missing}")
                    else:
                        notes.append(f"pocket {idx}: item_restriction entirely unknown {missing}, pocket skipped")
                        continue
            ar = out.get("ammo_restriction")
            if ar:
                missing = [x for x in ar if x not in cse_ammotypes]
                if missing:
                    kept = {k: v for k, v in ar.items() if k in cse_ammotypes}
                    if kept:
                        out["ammo_restriction"] = kept
                        notes.append(f"pocket {idx}: dropped unknown ammotypes {missing}")
                    else:
                        notes.append(f"pocket {idx}: ammo_restriction entirely unknown {missing}, pocket skipped")
                        continue
            pockets_out.append(out)
        if len(pockets_out) >= 2:
            overlay[item_id] = pockets_out
            if notes:
                report.append(f"{item_id}: KEPT ({len(pockets_out)} pockets)\n  " + "\n  ".join(notes))
            else:
                report.append(f"{item_id}: KEPT ({len(pockets_out)} pockets, clean)")
        else:
            skipped.append(item_id)
            report.append(f"{item_id}: SKIPPED (fewer than 2 usable pockets after validation)\n  " +
                          "\n  ".join(notes))

    with open(OUT + r"\cdda_pocket_overlay.json", "w", encoding="utf-8") as f:
        json.dump(overlay, f, indent=2)

    clean = sum(1 for r in report if r.endswith("clean)"))
    with open(OUT + r"\cdda_pocket_report.txt", "w", encoding="utf-8") as f:
        f.write("CDDA -> CSE pocket import report\n")
        f.write(f"  multi-pocket id matches: {len(matched)}\n")
        f.write(f"  imported cleanly:        {clean}\n")
        f.write(f"  imported with notes:     {len(overlay) - clean}\n")
        f.write(f"  skipped:                 {len(skipped)}\n\n")
        f.write("Dropped (unsupported) fields across all pockets:\n")
        for k, v in dropped_fields.most_common():
            f.write(f"  {k}: {v}\n")
        f.write("\n" + "\n".join(report) + "\n")

    print(f"matches={len(matched)} imported={len(overlay)} (clean={clean}) skipped={len(skipped)}")
    print("top dropped fields:", dropped_fields.most_common(8))


if __name__ == "__main__":
    main()
