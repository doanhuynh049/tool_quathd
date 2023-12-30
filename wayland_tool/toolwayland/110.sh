#!/bin/bash

# Launch weston-simple-egl in the background
echo "Launching weston-simple-egl in the background."
weston-simple-egl -o &

# Execute key operations to switch between windowed and fullscreen mode
echo "Switching weston-simple-egl window between windowed mode and fullscreen mode."
echo "Press Super key + Shift + F (repeat more than twice)"
# Add instructions for the tester to perform the key operations manually

sleep 10
# Run bus monitor tool to check data transmission
echo "Running bus monitor tool to check data transmission."
./busmon_dump -r -d 20

# Check the results
echo "Checking test results..."
# Add logic to check whether the window is switched between modes, and whether sprite plane is used in windowed mode and scanout plane is used in fullscreen mode

# Display a note about the known issue
echo "Note: The known issue with weston-simple-egl changing from fullscreen to primary mode causing continuous blinking will be addressed in a future release."

# Cleanup: Kill weston-simple-egl process
echo "Cleaning up..."
killall weston-simple-egl

echo "Test completed."

