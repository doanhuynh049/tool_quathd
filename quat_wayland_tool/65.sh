#!/bin/bash
echo "Test Case 65: weston-stacking"
weston-stacking &
echo "Verify:"
# 1. Make sure that weston-stacking is launched. (#1)
echo "2. Make sure that each key operation is done correctly."
echo "- f key"
echo "Verify:"
echo "- Make sure that switching fullscreen (no title)/normal window is correct. (#2)"

echo "- m key"
echo "Verify:"
echo "- Make sure that switching maximize window (with title)/normal window is correct. (#3)"

echo "- n key"
echo "Verify:"
echo "- Make sure that displaying a child window is correct. (#4)"

echo "- p key"
echo "Verify:"
echo "- Make sure that displaying a popup window is correct. (#5)"
echo "[ Note ] The following problem is an OSS Issue. We will check to resolve or not this problem on the next Wayland/Weston version up. Therefore, currently, the tester ignores this problem in the Test."
echo "- The detail of the 1st phenomenon: when pressing p key, App is terminated with an error log: zxdg_shell_v6@17: error 2: xdg_popup was not created on the topmost popup toytoolkit warning: 2 windows exist."

echo "- t key"
echo "Verify:"
echo "- Make sure that displaying a child window with a green background is correct. (#7)"
echo "[ Note ] The detail of the 2nd phenomenon: p key: Display a black rectangle on pushing, wipe out on releasing. -> the black rectangle is not displayed."

sleep 60
killall weston-stacking
