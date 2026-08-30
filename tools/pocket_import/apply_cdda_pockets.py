"""Apply the reviewed CDDA pocket overlay to CSE base item JSON, in place.

Each matched item definition gains a "pocket_data" key. Touched files are then
reformatted with the repo's own json_formatter so diffs stay canonical.
"""
import json
import glob
import subprocess

CSE = "D:/Projects/CSE"
FORMATTER = CSE + "/out/build/cse-vcpkg/tools/format/RelWithDebInfo/json_formatter.exe"
OVERLAY = (r"C:\Users\Oliver\AppData\Local\Temp\claude\d--Projects-CBN"
           r"\9f5a2a9e-e00a-4d28-8127-907cc9642349\scratchpad\cdda_pocket_overlay.json")

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
            if ( isinstance(i, str) and i in remaining and "pocket_data" not in e
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
        subprocess.run([FORMATTER, f], check=True, capture_output=True)

    print(f"applied={len(overlay) - len(remaining)} files_touched={len(touched)}")
    if remaining:
        print("NOT FOUND in CSE json:", sorted(remaining))


if __name__ == "__main__":
    main()
