#!/bin/bash
echo "Test Case 63: weston-resizor"
weston-resizor &
echo "Right-click on the window and select any option in the displayed menu."

echo "3. Run the following arrow key operations."
echo "- up: Window is shrunk longwise."
echo "- down: Windows are expanded longwise."
echo "- left: Window is shrunk crosswise."
echo "- right: Window is expanded crosswise."

echo "4. Place the mouse cursor in the middle of the window and click the left mouse button."
echo "5. Drag (press and hold the left mouse button, then move) the window in the following directions."
echo "- up: Window is shrunk longwise."
echo "- down: Windows are expanded longwise."
echo "- left: Window is shrunk crosswise."
echo "- right: Window is expanded crosswise."
sleep 60 && killall weston-simple-touch

