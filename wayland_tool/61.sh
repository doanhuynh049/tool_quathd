#!/bin/bash
echo "Test Case 61: weston-info"
weston-info
echo " weston information is displayed on the console. (#1)"
sleep 10
killall "weston-info"
