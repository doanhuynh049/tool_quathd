echo "Test Case 73: Display Configuration with weston-simple-shm-tp"
weston-simple-shm-tp -pid=0 -scale=1 -pos=0,0,0,0,2960,1050
echo "Make sure that the scaling with the specified value is performed correctly."
sleep 60 && killall 
