#!/bin/bash
echo "Test Case 64: weston-simple-touch"
echo "A touchscreen display shall be connected."
weston-simple-touch &
echo "Verify:"
echo "1. weston-simple-touch is launched. (#1)"
echo "2. Touch on the window."
echo "Verify:"
echo "2. Make sure that a red dot is drawn at the touched point. (#2)"
echo "3. Press and hold down the super key, then drag the window with touchscreen."
echo "Verify:"
echo "3. Make sure that the window can be dragged by touchscreen operation. (#3)"
sleep 60 && killall weston-simple-touch 
