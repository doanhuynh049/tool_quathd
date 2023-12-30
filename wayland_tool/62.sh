#!/bin/bash
echo "Test Case 62: weston-multi-resource"
export WAYLAND_DEBUG=1
weston-multi-resource -p5:14 -p7:14 -k9:14 -p12:14
echo "VerifyThe following messages are outputted in WAYLAND_DEBUG log."
echo "Create the first pointer device after 5 seconds."
echo "Create the second pointer device after 2 seconds of 2-1."
echo "Create the keyboard device after 2 seconds of 2-2."
echo "Create the third pointer device after 3 seconds of 2-3."
echo "Release all in the end."
sleep 10 && killall "weston-multi-resource"
