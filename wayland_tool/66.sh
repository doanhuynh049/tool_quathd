echo "Test Case 66: weston-transformed"
weston-transformed -w 1280 -h 720 &
sleep 60 && killall weston-transformed
