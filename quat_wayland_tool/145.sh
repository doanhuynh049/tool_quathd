#!/bin/bash
echo "Test Case 145: linux dmabuf color format (w/ gl-renderer)"
echo "1. Make sure that a color bar image is displayed in each format without error message."
./dmabuf-test -f argb8888
./dmabuf-test -f abgr8888
./dmabuf-test -f xrgb8888
./dmabuf-test -f xbgr8888
./dmabuf-test -f nv12
./dmabuf-test -f yuv420
./dmabuf-test -f yvu420

# Kill processes after 30s
sleep 30
killall dmabuf-test

