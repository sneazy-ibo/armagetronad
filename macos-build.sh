#!/usr/bin/env bash
#
# Build Armagetron Advanced (trunk) natively on macOS, via autotools.
#
# Usage:
#   ./macos-build.sh                 # release build, install to default prefix
#   ./macos-build.sh --bundle        # also produce a double-clickable .app
#   ./macos-build.sh --debug         # -O0 build
#   ./macos-build.sh --clean         # wipe the build dir first
#
# Options:
#   --build-dir DIR   out-of-tree build directory
#   --prefix DIR      install prefix
#   --bundle-path P   where to write the .app (default <prefix>/<name>.app)
#   --name NAME       app name, e.g. "Armagetron Experimental"
#   --log FILE        bundle: append console output to FILE
#   --bundle-id ID    bundle identifier (default: derived from the name)
#   --jobs N          parallel build jobs
#   --debug           -O0 -g instead of -O2
#   --self-contained  bundle: also copy linked dylibs into the app
#   -h, --help        this text
#
# Needs Xcode command line tools, Homebrew, and:
#   brew install pkg-config autoconf automake ftgl glew protobuf boost \
#                sdl2 sdl2_image sdl2_mixer libpng
#
# Flags that look redundant but are not, on macOS:
#   --with-boost and CPPFLAGS=-I<brew>/include   configure does not find
#       Homebrew's boost or GLEW headers by itself, and missing GLEW silently
#       disables the effects.
#   -ffp-contract=off   FMA contraction changes float results vs. x86, which
#       desynchronises physics (cycles teleport on the grid).

set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${HOME}/Developer/armagetronad-build/macos"
PREFIX="${HOME}/Developer/arma-trunk-install"
BUNDLE_PATH=""          # derived from --prefix unless given explicitly
APP_NAME="Armagetron Advanced"
BUNDLE_ID=""            # derived from APP_NAME unless given explicitly
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
OPT_FLAGS="-O2"
BUNDLE=0
SELF_CONTAINED=0
LOG_PATH=""
CLEAN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir)      BUILD_DIR="$2"; shift 2 ;;
        --prefix)         PREFIX="$2"; shift 2 ;;
        --bundle-path)    BUNDLE_PATH="$2"; BUNDLE=1; shift 2 ;;
        --name)           APP_NAME="$2"; shift 2 ;;
        --bundle-id)      BUNDLE_ID="$2"; shift 2 ;;
        --jobs)           JOBS="$2"; shift 2 ;;
        --debug)          OPT_FLAGS="-O0 -g"; shift ;;
        --bundle)         BUNDLE=1; shift ;;
        --self-contained) BUNDLE=1; SELF_CONTAINED=1; shift ;;
        --log)            LOG_PATH="$2"; shift 2 ;;
        --clean)          CLEAN=1; shift ;;
        -h|--help)        awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
    esac
done

# Resolved after parsing, so --prefix and --name affect both.
[ -n "$BUNDLE_PATH" ] || BUNDLE_PATH="$PREFIX/${APP_NAME}.app"
if [ -z "$BUNDLE_ID" ]; then
    suffix="$(printf '%s' "$APP_NAME" | sed -e 's/^Armagetron[[:space:]]*//' -e 's/[^A-Za-z0-9]/-/g' | tr '[:upper:]' '[:lower:]')"
    [ -n "$suffix" ] || suffix="trunk"
    BUNDLE_ID="org.armagetronad.${suffix}"
fi

say()  { printf '\033[1m==> %s\033[0m\n' "$*"; }
fail() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# dependencies
command -v brew >/dev/null || fail "Homebrew is required (https://brew.sh)"
BREW_PREFIX="$(brew --prefix)"
command -v xcodebuild >/dev/null || fail "Xcode command line tools are required"

MISSING=()
for formula in pkg-config autoconf automake ftgl glew protobuf boost sdl2 sdl2_image sdl2_mixer libpng; do
    brew list --versions "$formula" >/dev/null 2>&1 || MISSING+=("$formula")
done
if [ ${#MISSING[@]} -gt 0 ]; then
    fail "missing Homebrew packages: ${MISSING[*]}
install them with:
  brew install ${MISSING[*]}"
fi

# A stale Homebrew link (seen with sdl2_image) only shows up at launch as a
# dyld failure, so check before building.
BROKEN_LIBS=0
for lib in "$BREW_PREFIX"/lib/libSDL2_image-*.dylib "$BREW_PREFIX"/lib/libSDL2_mixer-*.dylib; do
    [ -e "$lib" ] || continue
    while read -r dep; do
        case "$dep" in
            "$BREW_PREFIX"/*|/usr/lib/*|/System/*) ;;
            *) continue ;;
        esac
        [ -e "$dep" ] || { echo "  broken link: $(basename "$lib") -> $dep"; BROKEN_LIBS=1; }
    done < <(otool -L "$lib" | tail -n +2 | awk '{print $1}')
done
if [ "$BROKEN_LIBS" = 1 ]; then
    say "Homebrew library links are stale — repairing"
    brew reinstall sdl2_image sdl2_mixer
fi

# configure
cd "$SOURCE_DIR"
if [ ! -x ./configure ] || [ ! -f Makefile.in ]; then
    say "Bootstrapping (autoconf/automake)"
    ./bootstrap.sh >/dev/null
fi

if [ "$CLEAN" = 1 ]; then
    say "Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

say "Configuring ($OPT_FLAGS) in $BUILD_DIR"
"$SOURCE_DIR/configure" \
    --with-boost="$BREW_PREFIX" \
    CPPFLAGS="-I$BREW_PREFIX/include" \
    LDFLAGS="-Wl,-headerpad_max_install_names" \
    CXXFLAGS="$OPT_FLAGS -ffp-contract=off" \
    CFLAGS="$OPT_FLAGS -ffp-contract=off" \
    > configure.log 2>&1 \
    || { tail -20 configure.log; fail "configure failed (see $BUILD_DIR/configure.log)"; }

# build
say "Building with $JOBS jobs"
make -j"$JOBS" > build.log 2>&1 \
    || { grep -n "error:" build.log | head -20; fail "build failed (see $BUILD_DIR/build.log)"; }

BIN="$BUILD_DIR/src/armagetronad_main"
[ -x "$BIN" ] || fail "expected binary not found: $BIN"

# install
say "Installing to $PREFIX"
make install prefix="$PREFIX" > install.log 2>&1 \
    || { tail -20 install.log; fail "install failed (see $BUILD_DIR/install.log)"; }

VERSION="$("$PREFIX/bin/armagetronad" --version 2>&1 | sed -n 's/.*version \([^ ]*\)$/\1/p' | sed 's/\.$//' | head -1)"
[ -n "$VERSION" ] || VERSION="unknown"
say "Installed $PREFIX/bin/armagetronad (version $VERSION)"

# bundle
if [ "$BUNDLE" = 1 ]; then
    say "Building app bundle: $BUNDLE_PATH"
    rm -rf "$BUNDLE_PATH"
    mkdir -p "$BUNDLE_PATH/Contents/Resources"

    # Keep the prefix inside Contents/MacOS: binreloc finds the data relative
    # to the executable, which keeps the bundle relocatable.
    make install prefix=/usr/local DESTDIR="$BUNDLE_PATH/Contents/MacOS" > /dev/null 2>&1 \
        || fail "bundle install step failed"

    cp "$SOURCE_DIR/desktop/os-x/armagetronad.icns" "$BUNDLE_PATH/Contents/Resources/armagetronad.icns"

    # Optional launcher: Finder and the Dock discard console output, which makes
    # runtime problems invisible.
    BUNDLE_EXEC="usr/local/bin/armagetronad"
    if [ -n "$LOG_PATH" ]; then
        mkdir -p "$(dirname "$LOG_PATH")"
        cat > "$BUNDLE_PATH/Contents/MacOS/launch" <<LAUNCH
#!/bin/sh
exec "$BUNDLE_PATH/Contents/MacOS/usr/local/bin/armagetronad" "\$@" >> "$LOG_PATH" 2>&1
LAUNCH
        chmod +x "$BUNDLE_PATH/Contents/MacOS/launch"
        BUNDLE_EXEC="launch"
        say "Console output will be appended to $LOG_PATH"
    fi

    cat > "$BUNDLE_PATH/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>     <string>en</string>
    <key>CFBundleExecutable</key>            <string>${BUNDLE_EXEC}</string>
    <key>CFBundleIconFile</key>              <string>armagetronad.icns</string>
    <key>CFBundleIdentifier</key>            <string>${BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key> <string>6.0</string>
    <key>CFBundleName</key>                  <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>           <string>${APP_NAME}</string>
    <key>CFBundlePackageType</key>           <string>APPL</string>
    <key>CFBundleShortVersionString</key>    <string>${VERSION}</string>
    <key>CFBundleSignature</key>             <string>Arma</string>
    <key>NSHighResolutionCapable</key>       <true/>
    <key>NSHumanReadableCopyright</key>      <string>The Armagetron Advanced Developer Team</string>
</dict>
</plist>
PLIST

    if [ "$SELF_CONTAINED" = 1 ]; then
        if command -v dylibbundler >/dev/null; then
            say "Copying third-party libraries into the bundle"
            # Contents/Frameworks, where SDL insists on finding itself (it shows a
            # modal error otherwise). The executable sits four directories deeper
            # in Contents/MacOS/usr/local/bin, so the load path has to climb out
            # of that to reach it.
            dylibbundler -od -b -x "$BUNDLE_PATH/Contents/MacOS/usr/local/bin/armagetronad" \
                -d "$BUNDLE_PATH/Contents/Frameworks" \
                -p @executable_path/../../../../Frameworks > /dev/null 2>&1 \
                || say "warning: dylibbundler failed; bundle still links Homebrew libraries"

            # Homebrew's SDL2 is sdl2-compat, which loads SDL3 at runtime: it is
            # not a linked dependency, so dylibbundler cannot see it. It looks
            # next to the executable first, and without it every start pops up a
            # modal error dialog.
            if [ -e "$BREW_PREFIX/lib/libSDL3.dylib" ]; then
                cp -f "$BREW_PREFIX/lib/libSDL3.dylib" "$BUNDLE_PATH/Contents/MacOS/usr/local/bin/libSDL3.dylib"
            else
                say "warning: libSDL3.dylib not found; the bundle needs an SDL2 that does not dlopen SDL3"
            fi
        else
            say "warning: dylibbundler not installed (brew install dylibbundler) — skipping self-contained step"
        fi
    fi
    say "Bundle ready: $BUNDLE_PATH"
fi

# smoke test
if "$PREFIX/bin/armagetronad" --version >/dev/null 2>&1; then
    say "Smoke test passed"
else
    fail "the built binary does not run (check for broken Homebrew links: brew reinstall sdl2_image)"
fi
