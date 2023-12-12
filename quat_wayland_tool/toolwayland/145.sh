#!/bin/bash

# Define color formats
color_formats=("argb8888" "abgr8888" "xrgb8888" "xbgr8888" "nv12" "yuv420" "yvu420")

# Define corresponding indices for reference
indices=("1" "2" "3" "4" "5" "6" "7")

# Loop through color formats
for i in "${!color_formats[@]}"; do
    format=${color_formats[$i]}
    index=${indices[$i]}

    # Run dmabuf-test with the current color format
    echo "Running dmabuf-test with color format: $format"
    ./dmabuf-test -f $format

    # Check for error messages
    if [ $? -eq 0 ]; then
        echo "[$index] $format: Color bar image displayed without error."
    else
        echo "[$index] $format: Error encountered while displaying color bar image."
    fi

    # Add a newline for better readability
    echo ""
done

