echo "Test Case 74: Clipping (outside valid value)"

weston-simple-shm-tp -pid=0 -scale=2 -pos=0,0,0,0,0,0
weston-simple-shm-tp -pid=0 -scale=2 -pos=-1001,-1001,0,0,0,0
echo "Verify: The crop area that has a zero width and height"
sleep 5

weston-simple-shm-tp -pid=0 -scale=2 -pos=0,0,1200,1200,0,0
weston-simple-shm-tp -pid=0 -scale=2 -pos=0,0,8000,3000,0,0
echo "Verify: The crop area that has a larger width and height than them of the application window"
sleep 5

weston-simple-shm-tp -pid=0 -scale=2 -pos=-1200,-2000,1000,1000,0,0
weston-simple-shm-tp -pid=0 -scale=2 -pos=-1000,-1000,1000,1000,0,0
echo "Verify: The crop area completely outside of the buffer"
echo "Note: If the window contents are not displayed correctly, and even if the application does not start, it can be ignored in this test unless something unexpected occurs to weston."
sleep 60 && killall weston-simple-shm-tp
