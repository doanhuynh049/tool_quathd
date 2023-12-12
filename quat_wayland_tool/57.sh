#!/bin/bash
echo "Test case 57: weston fullscreen"
weston-fullscreen -w 0 -h 0 &
echo "make sure that Surface size: 0, 0 is displayed when press F key"
sleep 2

weston-fullscreen -w 1 -h 1 &
echo "make sure that Surface size: 1, 1 is displayed when press F key"
sleep 2

weston-fullscreen -w 1920 -h 1080 & 
echo "- weston-fullscreen -w 6145 -h 1081 (#5)"
weston-fullscreen -w 6145 -h 1081 &
sleep 10s && killall  weston-fullscreen

