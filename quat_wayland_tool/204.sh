echo "Test Case: weston-confine"
echo "1. Make sure that weston-confine object is displayed. (#1)"
weston-confine &
echo "2. Make sure that some lines are drawn as the mouse pointer is moved. (#2)"
echo "4. Make sure that the mouse pointer cannot move out of the window. (#3)"
sleep 30
killall weston-confine
