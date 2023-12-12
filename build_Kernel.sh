#!/bin/bash

# Display the number of CPU cores that will be used for parallel compilation
NUM_CORES=$(nproc)
echo "Number of CPU cores available for compilation: $NUM_CORES"

# Set the cross-compiler toolchain (modify this according to your setup)
CROSS_COMPILE=aarch64-linux-gnu-

# Optionally, define CONFIG_FILE with the path to your .config if you want to use it
# CONFIG_FILE=/path/to/your/config/file

# If an existing .config file is needed, uncomment the following lines
# if [ -f "$CONFIG_FILE" ]; then
#     echo "Copying existing .config file..."
#     cp "$CONFIG_FILE" .config || { echo "Failed to copy .config file"; exit 1; }
# fi

# Ensure .scmversion does not interfere with the build
if ! touch .scmversion; then
    echo "Failed to touch .scmversion"
    exit 1
fi

# Configure the kernel for arm64 architecture
echo "Configuring the kernel..."
if ! make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" defconfig; then
    echo "Kernel configuration failed"
    exit 1
fi

# Uncomment the following line if you need to modify the kernel configuration interactively
# make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" menuconfig

# Build the kernel with the specified number of cores
echo "Building the kernel with $NUM_CORES cores..."
if ! make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" -j"$NUM_CORES" LOCALVERSION=-yocto-standard; then
    echo "Kernel build failed"
    exit 1
fi

echo "Kernel build completed successfully. The kernel image is located at 'arch/arm64/boot/Image'."

