#!/bin/bash

# Test Case Steps:
# 1. Configure the pipeline using 'media-ctl'.
# 2. Run simple-dmabuf-v4l with YUYV to YUYV.
# 3. Verify that a video captured image is displayed.

echo "1: Configuring the pipeline using 'media-ctl'"
media-ctl -d /dev/media0 -V "'rcar_csi2 fea80000.csi2':1 [fmt:UYVY2X8/720x480 field:alternate]"
media-ctl -d /dev/media0 -V "'adv748x 4-0070 afe':8 [fmt:UYVY2X8/720x480 field:alternate]"

echo ": Running simple-dmabuf-v4l with YUYV to YUYV"
./simple-dmabuf-v4l /dev/video1 YUYV YUYV &

# Allow some time for the application to display the video captured image
sleep 5  # Adjust sleep time based on the expected time to display the image

echo "3: Verifying that a video captured image is displayed"
# Manual verification is required.
# Examine the display to ensure that a video captured image is shown.

echo "All test cases completed."
