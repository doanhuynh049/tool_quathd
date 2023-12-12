#!/bin/bash

# Launch weston-simple-shm on the VGA screen in the background
weston-simple-shm &
WESTON_PID=$!

# Provide instructions for manual check
echo "Please check the VGA screen for any unexpected output. Press ENTER once you have confirmed there is no unexpected output."
read -r

# Wait for at least 10 minutes
echo "Waiting for 10 minutes. Please continue to observe the VGA screen and console output for any unexpected output."
for i in {20..1}; do
    echo "Waiting for $i more minute(s). Please continue to observe the VGA screen and console output for any unexpected output."
    sleep 30
done
# Provide instructions for the second manual check
echo "Please check the VGA screen and console output again for any unexpected output. Press ENTER once you have confirmed there is no unexpected output."
read -r

# Check if weston-simple-shm is still running
if ps -p $WESTON_PID > /dev/null 2>&1; then
    echo "weston-simple-shm is still running. No unexpected console output. (#2)"
else
    echo "weston-simple-shm has been terminated unexpectedly."
    exit 1
fi

# Close weston-simple-shm
kill $WESTON_PID
echo "weston-simple-shm has been closed."

# End of script
echo "GPU display test completed."

