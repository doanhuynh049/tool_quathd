#!/bin/bash

# Test Case 1: Launch weston-simple-egl and check bus monitor
echo "Test Case 1: Launch weston-simple-egl and check bus monitor"
weston-simple-egl -o &
sleep 2
./busmon_dump -r -d 20

# Test Case 2: Launch weston-simple-shm and check bus monitor
echo "Test Case 2: Launch weston-simple-shm and check bus monitor"
weston-simple-shm &
sleep 2
./busmon_dump -r -d 20

# Test Case 3: Bring weston-simple-egl to front and check bus monitor
echo "Test Case 3: Bring weston-simple-egl to front and check bus monitor"
# Focus weston-simple-egl window and move mouse cursor out of the window
# You may need to adjust this part based on your desktop environment or window manager
# For example, you can use xdotool to simulate mouse movements:
# xdotool search --name "weston-simple-egl" windowactivate mousemove 100 100
./busmon_dump -r -d 20

echo "All test cases completed."

