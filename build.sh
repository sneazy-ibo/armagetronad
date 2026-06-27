#!/usr/bin/env bash
# macOS build wrapper (the root Makefile is the autotools/Linux build; macOS uses Xcode).
# Default: terse — prints only compiler errors and the final BUILD result.
#   ./build.sh        terse
#   ./build.sh -v     full xcodebuild log
set -o pipefail

PROJECT="MacOS/Armagetron Advanced.xcodeproj"
SCHEME="Armagetron Advanced"
DEST='platform=macOS'

if [ "$1" = "-v" ]; then
    exec xcodebuild -project "$PROJECT" -scheme "$SCHEME" -destination "$DEST" build
fi

xcodebuild -project "$PROJECT" -scheme "$SCHEME" -destination "$DEST" build 2>&1 \
    | grep -E "error:|\*\* BUILD" || true
