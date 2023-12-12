#!/bin/bash

# Test Case 1: OGLES3Texturing
echo "Test Case 1: OGLES3Texturing"
echo "1. Running OGLES3Texturing 100 times with a delay of 3 seconds between runs"
for i in {1..100}; do
    echo "Iteration: $i"
    ./OGLES3Texturing -vsync=0 &
    sleep 3
    kill -2 $!
    sleep 1
done

# Test Case 3: OGLES3Navigation3D
echo "Test Case 2: OGLES3Navigation3D"
echo "1. Running OGLES3Navigation3D 100 times with a delay of 5 seconds between runs"
for i in {1..100}; do
    echo "Iteration: $i"
    ./OGLES3Navigation3D -vsync=0 &
    sleep 5
    kill -2 $!
    sleep 1
done

# Test Case 3: OGLES3Water
echo "Test Case 3: OGLES3Water"
echo "1. Running OGLES3Water 100 times with a delay of 3 seconds between runs"
for i in {1..100}; do
    echo "Iteration: $i"
    ./OGLES3Water -vsync=0 &
    sleep 3
    kill -2 $!
    sleep 1
done

echo "All test cases completed."

