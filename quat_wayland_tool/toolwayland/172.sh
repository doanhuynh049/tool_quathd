#!/bin/bash

# Test Case Steps:
# 1. Run dmabuf-test with different color formats.
# 2. Press the super key + "s" to capture screenshots.
# 3. Verify that a color bar image is displayed in each format without error message.

# Test Case 1: Run dmabuf-test with various color formats
echo "Test Case 1: Running dmabuf-test with different color formats"
formats=("argb8888" "xrgb8888" "bgra8888" "bgrx8888" "nv12" "nv21" "nv16" "nv61" "yuyv" "yvyu" "uyvy" "vyuy" "yuv420" "yuv422" "yuv444" "yvu420" "yvu422" "yvu444")

for format in "${formats[@]}"; do
    echo "Running dmabuf-test -f $format"
    ./dmabuf-test -f $format &
    sleep 10  # Adjust sleep time as needed
    echo " Press the super key + "s" (simulate keypress)"
    sleep 2  # Adjust sleep time as needed
done

# Test Case 2: Verify the display of color bar images
echo "Test Case 2: Verifying color bar images"
# Manual verification is required.
# Examine the display to ensure that a color bar image is shown for each format.
# Check for any error messages.

echo "Test case completed."

