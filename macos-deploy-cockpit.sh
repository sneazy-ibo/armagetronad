#!/usr/bin/env bash
#
# Deploy cockpit XMLs from the repo into the installed clients, so layout work
# needs no rebuild: the client re-parses the file when COCKPIT_FILE is set again.
#
# The installed name is derived from the file's own identity, the way the
# resource system does it: <author>/<category>/<name>-<version>.aacockpit.xml.
#
# Usage:
#   ./macos-deploy-cockpit.sh                 # every cockpit in resource/proto
#   ./macos-deploy-cockpit.sh FILE.xml [...]  # just these

set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

INSTALLS=(
    "${SOURCE_DIR}/.local/install/share/games/armagetronad"
    "/Applications/Armagetron Experimental.app/Contents/MacOS/usr/local/share/games/armagetronad"
)

if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    FILES=()
    while IFS= read -r f; do FILES+=("$f"); done \
        < <(find "${SOURCE_DIR}/resource/proto" -name "*.aacockpit.xml" | sort)
fi

python3 - "${INSTALLS[@]}" -- "${FILES[@]}" <<'PY'
import os, re, shutil, sys

args = sys.argv[1:]
installs = args[:args.index("--")]
files = args[args.index("--") + 1:]

deployed = 0
for path in files:
    src = open(path, encoding="latin-1").read()
    m = re.search(r'<Resource[^>]*?name="([^"]*)"[^>]*?version="([^"]*)"[^>]*?author="([^"]*)"[^>]*?category="([^"]*)"', src)
    if not m:
        print(f"skipping {path}: no Resource identity found", file=sys.stderr)
        continue
    name, version, author, category = m.groups()
    rel = os.path.join("resource", "included", author, category, f"{name}-{version}.aacockpit.xml")
    for install in installs:
        if not os.path.isdir(install):
            continue
        dest = os.path.join(install, rel)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(path, dest)
        print(f"deployed {os.path.basename(path)} -> {dest}")
        deployed += 1

print(f"\n{deployed} file(s) deployed.")
PY

echo "In game: Modded Settings -> Reload cockpit (or re-pick a cockpit entry)."
