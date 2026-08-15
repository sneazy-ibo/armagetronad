# std-library migration — scope

Goal (from TASKS): replace custom `tString`→`std::string` and the intrusive
`tJUST_CONTROLLED_PTR`/`tControlledPTR`→`std::shared_ptr`, so we can turn on
warnings-as-errors / sanitizers / clang-tidy that gave false positives on the
custom types.

## Step 0 — measure first (do this before any migration)

We are about to justify a large refactor by "the safeguards false-positive on the
custom types." **Nobody has written down which diagnostic fires on which type.**
That's the cheap thing to find out first (this user's prime directive):

1. Flip the suppressions off (a sanitizer build, or clang-tidy with the custom
   checks on, or `-Werror` with the warning set we actually want).
2. Record the exact diagnostics and which type triggers each.

Likely outcome, from reading the code: **the noise comes from `tString`'s
`tArray<char>` storage and `operator const char*`, not from the smart pointers.**
The intrusive pointers are *custom* but not *unsafe*. If that holds, the migration
shrinks dramatically (see "Recommendation"). If it doesn't, we have the real list
instead of a guess. Either way we stop guessing.

## The two halves are independent — split them

`tString` and the smart pointers don't depend on each other. Treat them as two
projects; don't entangle them.

## Scale (measured, `src/`)

| Type | Mentions | Files |
|------|----------|-------|
| `tString` | 1765 | 99 |
| `tColoredString` | 285 | — |
| `tJUST_CONTROLLED_PTR` | 151 | — |
| `tControlledPTR` | 57 | — |
| `tCONTROLLED_PTR` (macro) | 28 | — |
| `tCheckedPTR`/`Const` | 32 | — |
| manual `AddRef`/`Release` | 89 sites | — |

Top `tString` files: `ePlayer.cpp` (231), `tString.cpp` (113),
`tDirectories.cpp` (104), `nNetwork.cpp` (66), `nServerInfo.cpp` (62),
`nKrawall.cpp` (54), `tLocale.cpp` (48), `tConfiguration.cpp` (45).

## Part A — `tString` → `std::string` (tractable; the value is here)

### Don't rewrite 99 files. Reparent one class.

`tString : public tArray<char>` and `tColoredString : public tString`. The unsafe
storage everyone wants gone is the `tArray<char>` base. So:

**Reparent `tString` onto `std::string`** (or hold one as a member) and
reimplement its ~25 extra methods (`StrPos`, `SubStr`, `Truncate`, `ReadLine`,
`toInt`, `NetFilter`, `StripWhitespace`, `Len`/`SetLen`, …) as thin wrappers over
`std::string`. **All 1765 call sites keep compiling unchanged.** You get the
sanitizer-clean storage — which is the actual goal — without touching callers.
Retiring the custom method names callsite-by-callsite becomes *optional* cleanup
afterward, not a prerequisite.

`tColoredString` is **not** "a `std::string`" — it carries colour-code behaviour
(`RemoveTrailingColor`, `RemoveColors`, `SetPos` counts visible length). It stays a
subclass; only its storage changes with the base. Don't try to delete it.

### The one trap that will bite: `Len()` is `strlen + 1`

`tString` stores the trailing `'\0'` inside the array, so `Len()` returns
`size() + 1` (empty string → `Len()==1`). Callers bake this in
(`for(i=Len()-1; …)`, `SetLen(n)` to mean an `n-1`-char string). The shim's `Len()`
**must** return `size()+1` and `SetLen(n)` resize to `n-1` content + NUL — do **not**
naively map `Len()→size()`. Keep the existing `tString_test` (add one if none) and
assert this off-by-one explicitly; it's the whole-codebase landmine.

`operator const char*()` can stay during the transition (return `.c_str()`); it's
not UB. Decide later whether to force `.c_str()` at callsites for clarity.

### Landability

One reparent commit (the class + its `.cpp`), build, run. Behaviour-preserving by
construction. No per-file march required to get the safeguard win — that's the
point of doing it at the class boundary.

## Part B — intrusive smart pointers → `std::shared_ptr` (risky; probably skip)

### Why it is *not* a drop-in

These are **intrusive** refcounts: the count lives in the object via
`tReferencable<T>` (13 subclasses: `nMessage`, `eWall`, `eGrid`, `eFace`, `ePoint`,
`eHalfEdge`, `eVoter`, `uBind`, `nObserver`, `nBandwidth*`, `eTempEdge`,
`eReferencableGameObject`). `std::shared_ptr` is **non-intrusive** — count in an
external control block. They are not interchangeable:

- 89 manual `AddRef`/`Release` sites assume the count is on the object.
- `tStackObject<T>` lets the *same* type live on the stack (refcount sentinel) —
  illegal under `shared_ptr` ownership.
- Objects self-register in global intrusive lists and are constructed via
  `tNEW`/bare `new` in mixed ways; `shared_ptr` needs `make_shared` /
  `enable_shared_from_this` at **every** construction + every `this`-based AddRef.
- **`nMessage` is one of these** and sits on the frozen-wire, single-threaded,
  `tRecorder`-deterministic net path. Changing its lifecycle model is exactly the
  kind of change CLAUDE.md flags as high-risk.

`std` has no intrusive smart pointer, and **no new deps** (no `boost::intrusive_ptr`).

### Recommendation: don't migrate this for the safeguard goal

`tReferencable` defaults to `tNonMutex` (no-op lock) — matching the
single-threaded-by-design net core. It is custom but **not unsafe**. If Step 0
confirms the sanitizer noise is `tString`, the lazy-correct move is:
**keep the intrusive pointers, make the base class clean** (e.g. confirm no UB
under ASan/UBSan, fix anything real). That deletes ~325 callsite edits and the
`nMessage` lifecycle risk for, likely, zero safeguard cost.

Only migrate specific *leaf* refcounted classes (no network, no `tStackObject`
use, simple construction) if Step 0 proves they actually trip a check we want —
one class at a time, with a self-check.

## Recommended order

1. **Step 0** — turn the safeguards on, record the real diagnostics. (hours)
2. **Part A reparent** — `tString`/`tColoredString` onto `std::string`, preserve
   `Len()==size()+1`, build, run. The safeguard win. (the bulk; 1–2 sessions)
3. Turn on the now-justified safeguards for the migrated `tools/` unit; fix real
   hits. (this is where warnings-as-errors first pays off)
4. **Part B** — only if Step 0 demands it, and only leaf classes. Default: leave
   intrusive pointers as-is and note why.

First concrete action: Step 0. Don't write migration code until the diagnostic
list is real.
