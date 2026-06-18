#!/bin/bash
open "/Users/tom11w/Library/Developer/Xcode/DerivedData/Armagetron_Advanced-hhaufrsnrxtyzmdrfytvnayhozio/Build/Products/Debug/Armagetron Advanced.app"
sleep 5
osascript -e '
tell application "System Events"
    set procList to every process whose background only is false
    repeat with p in procList
        if name of p is "Armagetron Advanced" then
            set winCount to count of every window of p
            log "Found Armagetron Advanced with " & winCount & " windows"
        end if
    end repeat
end tell
' 2>&1
