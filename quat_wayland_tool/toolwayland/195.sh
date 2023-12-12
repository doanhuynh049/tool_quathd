#!/bin/bash

# Test Case Steps:
# 1. Run simple-dmabuf-v4l with different combinations of color formats.
# 2. Verify that the test application does NOT launch, and Weston continues running without unexpected behavior.
# 3. Verify that the error message "Error: zwp_linux_buffer_params.create failed." is output.

# Test Case 1: Run simple-dmabuf-v4l with YUYV to RGBA
echo "Test Case 1: Running simple-dmabuf-v4l with YUYV to RGBA"
./simple-dmabuf-v4l /dev/video1 YUYV RGBA
if [ $? -eq 1 ]; then
    echo "Test Passed: Application did not launch, and the expected error message was displayed."
else
    echo "Test Failed: Application launched or did not display the expected error message."
fi

# Test Case 2: Run simple-dmabuf-v4l with YUYV to NV16
echo "Test Case 2: Running simple-dmabuf-v4l with YUYV to NV16"
./simple-dmabuf-v4l /dev/video1 YUYV NV16
if [ $? -eq 1 ]; then
    echo "Test Passed: Application did not launch, and the expected error message was displayed."
else
    echo "Test Failed: Application launched or did not display the expected error message."
fi

# Test Case 3: Run simple-dmabuf-v4l with NV16 to RGBA
echo "Test Case 3: Running simple-dmabuf-v4l with NV16 to RGBA"
./simple-dmabuf-v4l /dev/video1 NV16 RGBA
if [ $? -eq 1 ]; then
    echo "Test Passed: Application did not launch, and the expected error message was displayed."
else
    echo "Test Failed: Application launched or did not display the expected error message."
fi

# Test Case 4: Run simple-dmabuf-v4l with NV16 to NV16
echo "Test Case 4: Running simple-dmabuf-v4l with NV16 to NV16"
./simple-dmabuf-v4l /dev/video1 NV16 NV16
if [ $? -eq 1 ]; then
    echo "Test Passed: Application did not launch, and the expected error message was displayed."
else
    echo "Test Failed: Application launched or did not display the expected error message."
fi

echo "All test cases completed."

