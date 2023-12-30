# Test Case 82: OGLES3Navigation3D
echo "Test Case 3: OGLES3Navigation3D"
echo "1. Running OGLES3Navigation3D 100 times with a delay of 5 seconds between runs"
for i in {1..100}; do
    ./OGLES3Navigation3D -vsync=0 &
    sleep 5
    kill -2 $!
    sleep 1
done
