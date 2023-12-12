#!/bin/bash

# Define color formats
color_formats=("bgra8888" "nv16" "yuyv" "yuv422")

# Define corresponding indices for reference
indices=("1" "2" "3" "4")

# Define timeout duration
timeout_duration=5

# Loop through color formats
for i in "${!color_formats[@]}"; do
    format=${color_formats[$i]}
    index=${indices[$i]}

    # Run dmabuf-test with the current color format in the background and limit execution time
    echo "Running dmabuf-test with color format: $format"
    output=$(timeout $timeout_duration ./dmabuf-test -f $format 2>&1)  # Redirect stderr to stdout and limit execution time

    sleep 1

    # Check if the output is empty (indicating timeout)
    if [ -z "$output" ]; then
        echo "[$index] $format: Skipped (no output within $timeout_duration seconds)."
    elif [[ "$output" == *"Error: zwp_linux_buffer_params.create failed."* ]]; then
        echo "[$index] $format: Color bar image not displayed, expected error message found."
    else
        echo "[$index] $format: Error: Unexpected output or missing error message."
    fi

    # Add a newline for better readability
    echo ""
done

