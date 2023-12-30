#!/bin/bash
echo "Test case 60: weston-image"
cd /temp/picture && weston-image picture1.jpg &  
weston-image picture1.jpg & 
weston-image picture1.jpg &
echo "Make sure the following key operation."
echo "- '+' key (#2)"
echo "- '-' key (#3)"
echo "- '1' key (#4)"
echo "Make sure that the pic file is displayed correctly. (#1)"
echo "Make sure that the displayed position is changed for the mouse operation. (#6)"
sleep 60
killall "weston-image"
