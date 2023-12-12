#!/bin/bash

# Set up the working environment
setup_environment() {
    export WORK=$(pwd)
    export MMNGR_LIB_COMMIT="0322548e54b45a064c9cecea29018ef50cdb8423"
    export MMNGR_DRV_COMMIT="2439802426474136312bd10bc4c143fbf1c84850"
    source /opt/poky/3.1.11/environment-setup-aarch64-poky-linux
    export KERNELSRC=/home/quathd/rvc/project/linux-bsp-rcar3
    export INCSHARED=$SDKTARGETSYSROOT/usr/local/include
}

# Clone the repositories
clone_repositories() {
    git clone https://github.com/renesas-rcar/mmngr_lib.git &
    git clone https://github.com/renesas-rcar/mmngr_drv.git &
    wait
}

# Checkout the specific commits
checkout_commits() {
    (cd mmngr_lib && git checkout -b tmp ${MMNGR_LIB_COMMIT})
    (cd mmngr_drv && git checkout -b tmp ${MMNGR_DRV_COMMIT})
}

# Build function with directory as an argument
build_module() {
    local dir=$1
    local build_log="${dir}_buildlog.log"
    autoreconf -i
    ./configure --prefix=$PWD/tmp
    make > "$build_log"
    make install includedir=$INCSHARED >> "$build_log"
    cp -v tmp/lib/* "${WORK}/module_${dir}" 2>/dev/null || { echo "Failed to copy ${dir} files."; exit 1; }
    cat "$build_log"
}

# Build Kernel Image and MMNGR driver module
build_kernel_and_driver() {
    if [ ! -f "${KERNELSRC}/arch/arm64/boot/Image" ]; then
        echo "Kernel Image does not exist, building..."
        "${KERNELSRC}/build_Kernel.sh" || { echo "Kernel build failed."; exit 1; }
    else
        echo "Kernel Image exists, skipping build."
    fi
}

# Build the selected module
build_selected_module() {
    case "$1" in
        1) build_module "mmngr_lib/libmmngr/mmngr" ;;
        2) build_module "mmngr_lib/libmmngr/mmngrbuf" ;;
        3) build_kernel_and_driver ;;
        4) build_module "mmngr_drv/mmngr_drv/mmngrbuf/mmngrbuf-module/files/mmngrbuf/drv" ;;
        *) echo "Invalid choice. Please try again." ;;
    esac
}

# Main menu function
main_menu() {
    echo "Please choose an option:"
    echo "1. Build the MMNGR library"
    echo "2. Build the MMNGR buffer library"
    echo "3. Build the Kernel Image and MMNGR driver module"
    echo "4. Build the MMNGR buffer driver"
    read -p "Your choice: " choice
    build_selected_module "$choice"
    echo "Operation complete."
}

# Main script execution
setup_environment
clone_repositories
checkout_commits
main_menu

