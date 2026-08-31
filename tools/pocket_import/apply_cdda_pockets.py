"""Apply the reviewed CDDA pocket overlay to CSE base item JSON, in place.

Each matched item definition gains a "pocket_data" key. Touched files are then
reformatted with the repo's own json_formatter so diffs stay canonical.
"""
import json
import glob
import subprocess
from pathlib import Path

CSE = Path(__file__).resolve().parents[2].as_posix()
FORMATTER = CSE + "/out/build/cse-vcpkg/tools/format/RelWithDebInfo/json_formatter.exe"
OVERLAY = "cdda_pocket_overlay.json"

# Only genuine item definitions may gain pocket_data. Matching on id alone once
# injected pockets into an item_group that shared an item's id.
ITEM_TYPES = {
    "ARMOR", "TOOL_ARMOR", "GENERIC", "TOOL", "CONTAINER", "GUN", "GUNMOD",
    "MAGAZINE", "AMMO", "COMESTIBLE", "BOOK", "BIONIC_ITEM", "PET_ARMOR",
    "ENGINE", "WHEEL", "TOOLMOD", "BATTERY",
}


def main():
    overlay = json.load(open(OVERLAY, encoding="utf-8"))
    remaining = set(overlay)
    touched = []

    for f in glob.glob(CSE + "/data/json/**/*.json", recursive=True):
        try:
            data = json.load(open(f, encoding="utf-8"))
        except Exception:
            continue
        if not isinstance(data, list):
            continue
        changed = False
        for e in data:
            if not isinstance(e, dict):
                continue
            i = e.get("id")
            if ( isinstance(i, str) and i in remaining
                    and e.get("type") in ITEM_TYPES ):
                e["pocket_data"] = overlay[i]
                remaining.discard(i)
                changed = True
        if changed:
            with open(f, "w", encoding="utf-8", newline="\n") as out:
                json.dump(data, out, indent=2, ensure_ascii=False)
                out.write("\n")
            touched.append(f)

    for f in touched:
        # The formatter exits 1 when it reformatted the file; only >1 is an error.
        r = subprocess.run([FORMATTER, f], capture_output=True)
        if r.returncode > 1:
            raise RuntimeError(f"formatter failed on {f}: {r.stderr!r}")

    print(f"applied={len(overlay) - len(remaining)} files_touched={len(touched)}")
    if remaining:
        print("NOT FOUND in CSE json:", sorted(remaining))


if __name__ == "__main__":
    main()
