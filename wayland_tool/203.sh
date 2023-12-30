#!/bin/bash
echo "Test Case 203: weston-editor (compose key support)"
echo -n "<question> <minus> <greater> : U2192" > /home/root/.XCompose
echo "2. Make sure that the '→' is displayed in the 'Entry' area. (#1)"
weston-editor &
sleep 60
killall weston-editor

