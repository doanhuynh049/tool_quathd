#!/bin/bash
echo "Test Case 110: Check Sprite plane operation - Switching window mode/fullscreen mode"

echo "1. Make sure that weston-simple-egl window is switched between windowed mode and fullscreen mode. (#1)"
weston-simple-egl -o &

echo "2. Make sure that sprite plane is used in the windowed mode and scanout plane is used in fullscreen mode. (#2)"
./busmon_dump -r -d 20

sleep 30 && killall weston-simple-egl busmon_dump

