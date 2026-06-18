#!/bin/bash
# Feedback loop: build -> run -> get feedback -> return results

PROJ="/Users/tom11w/Projects/Cloned/armagetronad/MacOS/Armagetron Advanced.xcodeproj"
APP="/Users/tom11w/Library/Developer/Xcode/DerivedData/Armagetron_Advanced-hhaufrsnrxtyzmdrfytvnayhozio/Build/Products/Debug/Armagetron Advanced.app/Contents/MacOS/Armagetron Advanced"
LOGDIR="/tmp/armagetron_debug"
mkdir -p "$LOGDIR"

echo "Building..."
BUILD_LOG="$LOGDIR/build.log"
xcodebuild -project "$PROJ" -scheme "Armagetron Advanced" -configuration Debug > "$BUILD_LOG" 2>&1
BUILD_EXIT=$?

if [ $BUILD_EXIT -ne 0 ]; then
    # Show build error dialog with last error line
    LAST_ERR=$(grep "error:" "$BUILD_LOG" | tail -1 | sed 's/^[[:space:]]*//')
    osascript -e "
set result to button returned of (display dialog \"BUILD FAILED\" buttons [\"Show Error\", \"Copy Log\"] default button 1 with title \"Build Result\")
if result is \"Copy Log\" then
    set the clipboard to (do shell script \"cat $BUILD_LOG\")
end if
"
    echo "BUILD FAILED: $LAST_ERR"
    exit 1
fi

echo "Build succeeded."

# Kill any running instance
pkill -f "Armagetron Advanced" 2>/dev/null
sleep 0.5

echo "Running app (5s timeout)..."
RUN_LOG="$LOGDIR/run.log"
timeout 5 "$APP" > "$RUN_LOG" 2>&1 || true
sleep 0.5

# Kill any remaining instance
pkill -f "Armagetron Advanced" 2>/dev/null

echo "RUNLOG: $RUN_LOG"
# Show feedback dialog
FEEDBACK=$(osascript -e "
set result to button returned of (display dialog \"What did you see?\" buttons [\"Black\", \"Blue\", \"Other\"] default button 1 with title \"Feedback\" with icon caution)
if result is \"Other\" then
    set otherText to text returned of (display dialog \"Describe what you saw:\" default answer \"\" with title \"Details\")
    return \"Other: \" & otherText
else
    return result
end if
")

echo "FEEDBACK: $FEEDBACK"
