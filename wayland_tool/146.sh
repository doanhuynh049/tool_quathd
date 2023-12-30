#!/bin/bash
echo "Test Case 146: linux dmabuf color format(w/ gl-renderer)"
echo "Make sure that a color bar image is not displayed and the following message is output in the terminal window"
./dmabuf-test -f bgra8888
./dmabuf-test -f nv16
./dmabuf-test -f yuyv
./dmabuf-test -f yuv422
# Kill processes after 30s
sleep 30
killall dmabuf-test

