# Building Armagetron Advanced (trunk) on macOS

Native macOS builds of `trunk` with autotools + Homebrew, the same path upstream
uses for macOS. There is no Xcode project: nothing lists the source files, so
upstream additions are picked up without touching a project file.

## Quick start

```
brew install pkg-config autoconf automake ftgl glew protobuf boost \
             sdl2 sdl2_image sdl2_mixer libpng
./macos-build.sh              # release (-O2), installs to ~/Developer/arma-trunk-install
./macos-build.sh --bundle     # ...and builds a double-clickable .app
./macos-build.sh --help       # all options, and the macOS-specific flags
```

To install alongside a stable 0.2.9 install as a separate app:

```
./macos-build.sh --bundle --name "Armagetron Experimental" \
                 --bundle-path "/Applications/Armagetron Experimental.app"
```

`--name` derives the bundle identifier, so the two installs get their own Dock
icon. The profile directory is still shared
(`~/Library/Application Support/Armagetron Advanced`): 0.2.9 keeps
`var/user.cfg`, 0.4 writes `config/user_3_1_utf8.cfg`.

The build is out-of-tree in `~/Developer/armagetronad-build/macos`, so switching
branches or merging upstream never leaves stale objects in the source tree.

## Running

```
~/Developer/arma-trunk-install/bin/armagetronad
```

Run the installed binary rather than `src/armagetronad_main` from the build tree:
the game finds its data relative to the executable. For a throwaway profile:

```
~/Developer/arma-trunk-install/bin/armagetronad \
  --userdatadir ~/Developer/arma-trunk-profile \
  --userconfigdir ~/Developer/arma-trunk-profile/config \
  --vardir ~/Developer/arma-trunk-profile/var
```

## Updating from upstream

```
git fetch upstream trunk
git merge upstream/trunk
./macos-build.sh --clean
```

Autotools needs no help with new source files. What breaks is a new dependency
(`macos-build.sh` fails early and names the missing package) or one of the
Homebrew quirks documented in `--help`.

## Continuous integration

`.github/workflows/`:

- **`mac-build.yml`** on `trunk-mac` and `trunk-mac-modded`: builds and bundles
  every push, uploads the `.app`, and reports a broken build.
- **`upstream-sync.yml`** daily: merges `upstream/trunk` into `trunk-mac`, merges
  that into `trunk-mac-modded`, builds the result, and pushes only if it builds.

Scheduled workflows only run from the repository's default branch, so
`upstream-sync.yml` has to exist there. Both jobs use `macos-14` (Apple Silicon)
runners, which are free for public repositories.

## Benchmarking

`--playback file.aarec --benchmark` renders every recorded frame and prints the
average fps at exit. The game stops presenting when its window is not frontmost
while `--benchmark` still counts those frames, so keep the window in front (or
run windowed with `KEEP_WINDOW_ACTIVE 1`, the default) or the numbers mean
nothing.
