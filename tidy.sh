#!/usr/bin/env bash
# Run clang-tidy on macOS.
#
# The compile DB lives in .tidy/ (NOT the repo root) so clangd keeps using the
# xcode-build-server flags for the editor — a root compile_commands.json makes
# clangd resolve generated headers (e.g. nTrueVersion.h) wrongly. See docs/sharp-edges.md.
#
#   ./tidy.sh --refresh                 regenerate the DB from the latest Xcode build log
#   ./tidy.sh src/foo/bar.cpp [...]     tidy one or more files
#   ./tidy.sh --refresh src/foo/bar.cpp refresh then tidy
set -euo pipefail

TIDY="${CLANG_TIDY:-/opt/homebrew/opt/llvm/bin/clang-tidy}"
DB=.tidy/compile_commands.json

if [ "${1:-}" = "--refresh" ]; then
    shift
    DD=$(sed -n 's/.*"build_root"[[:space:]]*:[[:space:]]*"\(.*\)".*/\1/p' buildServer.json)
    [ -n "$DD" ] || { echo "could not read build_root from buildServer.json"; exit 1; }
    mkdir -p .tidy
    xcode-build-server parse -s "$DD" -o "$DB"
    # xcode-build-server's output needs cleanup before clang-tidy (brew clang) can use it:
    #  - the universal build emits several entries per file (arm64/x86_64 x SDKs);
    #  - each "command" joins TWO clang invocations with ';' (an SDK-stat-cache build
    #    plus the real compile) -> "expected exactly one compiler job";
    #  - commands carry Apple-clang-only flags (e.g. -ivfsstatcache) brew clang rejects.
    # Normalise to one arm64 entry per real source file: the single real compile
    # command, with the incompatible flags stripped.
    python3 - "$DB" <<'PY'
import json, sys, shlex
p = sys.argv[1]
db = json.load(open(p))
# apple-clang-only / build-only flags brew clang-tidy rejects; each takes a following arg
DROP_WITH_ARG = {"-ivfsstatcache", "-index-store-path", "-index-unit-output-path",
                 "-o", "-serialize-diagnostics"}
# flags to drop that take no following arg. -gmodules stores debug info in Apple
# .pcm modules brew clang can't read ("unknown module format").
DROP_NO_ARG = {"-gmodules"}
best = {}
for e in db:
    f = e["file"]
    if not f.endswith((".cpp", ".cc", ".cxx", ".c", ".mm", ".m")):
        continue
    cmd = e.get("command") or " ".join(e.get("arguments", []))
    base = f.rsplit("/", 1)[-1]
    segs = [s for s in cmd.split(";") if s.strip()]
    seg = next((s for s in segs if base in s), segs[-1] if segs else cmd)
    # inline any @response-file so its include paths pass through the same filter
    expanded = []
    for t in shlex.split(seg):
        if t.startswith("@"):
            try:
                with open(t[1:]) as fh:
                    expanded.extend(shlex.split(fh.read()))
            except OSError:
                expanded.append(t)
        else:
            expanded.append(t)
    toks, skip = [], False
    for t in expanded:
        if skip:
            skip = False; continue
        if t in DROP_WITH_ARG:
            skip = True; continue
        if t in DROP_NO_ARG:
            continue
        toks.append(t)
    arm = "arm64" in seg
    entry = {"directory": e["directory"], "arguments": toks, "file": f}
    if f not in best or (arm and not best[f][1]):
        best[f] = (entry, arm)
out = [v[0] for v in best.values()]
json.dump(out, open(p, "w"), indent=1)
print(f"normalised -> {len(out)} entries (one real compile each)")
PY
    [ $# -eq 0 ] && exit 0
fi

[ -f "$DB" ] || { echo "no $DB — run: ./tidy.sh --refresh"; exit 1; }
exec "$TIDY" -p .tidy "$@"
