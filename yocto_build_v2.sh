#!/bin/bash

# Board list and directories
BOARD_LIST=("h3ulcb" "m3ulcb" "draak" "ebisu" "m3nulcb" "salvator-x")
TARGET_BOARD=$1
PKGS_DIR="$PWD/proprietary"
TARGET_DIR="$PWD/${TARGET_BOARD}"
WORK="$PWD"

# Git commits
POKY_COMMIT="74b22db6879b388d700f61e08cb3f239cf940d18"
META_OE_COMMIT="814eec96c2a29172da57a425a3609f8b6fcc6afe"
META_RENESAS_COMMIT="13fd24957b9acc29a235ee0c7f398fd867f38b47"

# Usage function
Usage() {
    echo "Usage: $0 \${TARGET_BOARD_NAME}"
    echo "BOARD_NAME list: "
    for board in "${BOARD_LIST[@]}"; do echo "  - $board"; done
    exit 1
}

# Check Param
if ! printf '%s\n' "${BOARD_LIST[@]}" | grep -qx "${TARGET_BOARD}"; then
    Usage
fi

echo "$WORK"
mkdir -p "${TARGET_DIR}"

# Clone basic Yocto layers in parallel, if not already present
clone_if_not_exists() {
    if [ ! -d "$2" ]; then
        git clone "$1" "$2" &
    fi
}
clone_if_not_exists git://git.yoctoproject.org/poky "$WORK/poky"
clone_if_not_exists git://git.openembedded.org/meta-openembedded "$WORK/meta-openembedded"
clone_if_not_exists https://github.com/renesas-rcar/meta-renesas "$WORK/meta-renesas"
wait

# Function to checkout proper branches/commits
checkout_repo() {
    local repo_path=$1
    local commit=$2
    cd "$repo_path"
    git checkout -b tmp "$commit"
}
checkout_repo "${WORK}/poky" "$POKY_COMMIT"
checkout_repo "${WORK}/meta-openembedded" "$META_OE_COMMIT"
checkout_repo "${WORK}/meta-renesas" "$META_RENESAS_COMMIT"

# Populate meta-renesas with proprietary software packages
WORK_PROP_DIR="${WORK}/binaries_dir_PT3_newIMR"
cd "${WORK}/meta-renesas"
sh meta-rcar-gen3/docs/sample/copyscript/copy_proprietary_softwares.sh -f "${WORK_PROP_DIR}"

# Setup build environment and copy configurations
source "${WORK}/poky/oe-init-build-env" "${TARGET_DIR}"
CONF_DIR="${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/mmp"
cp "${CONF_DIR}"/*.conf "${TARGET_DIR}/conf/"

# Enable 3D Graphics and Multimedia package
cp "${TARGET_DIR}/conf/local-wayland.conf" "${TARGET_DIR}/conf/local.conf"

# Additional configuration steps (e.g., Multimedia packages) can be added here

# Building with bitbake
cd "${TARGET_DIR}"
#bitbake core-image-weston

