#!/bin/bash
# Install environment 
# sudo apt-get install gawk wget git-core diffstat unzip texinfo gcc-multilib build-essential chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 xterm


# Define board list and directories
BOARD_LIST=("h3ulcb" "m3ulcb" "draak" "ebisu" "m3nulcb" "salvator-x")
TARGET_BOARD=$1
PKGS_DIR="$PWD/proprietary"
TARGET_DIR="$PWD/${TARGET_BOARD}"
WORK="$PWD"

# Define git commits
POKY_COMMIT="74b22db6879b388d700f61e08cb3f239cf940d18"
META_OE_COMMIT="814eec96c2a29172da57a425a3609f8b6fcc6afe"
META_RENESAS_COMMIT="764ee9fe2c0fc9393e57d140e56a0c9f500b5699"

# Function to display usage information
Usage() {
    echo "Usage: $0 [TARGET_BOARD_NAME]"
    echo "BOARD_NAME list: "
    for board in "${BOARD_LIST[@]}"; do
        echo "  - $board"
    done
    exit 1
}

# Check if valid board name was provided
if ! printf '%s\n' "${BOARD_LIST[@]}" | grep -qx "${TARGET_BOARD}"; then
    Usage
fi

echo "$WORK"
mkdir -p "${TARGET_DIR}"

# Function to clone repositories if they don't exist
clone_if_not_exists() {
    if [ ! -d "$2" ]; then
        git clone "$1" "$2" &
    fi
}
clone_if_not_exists git://git.yoctoproject.org/poky "$WORK/poky"
clone_if_not_exists git://git.openembedded.org/meta-openembedded "$WORK/meta-openembedded"
clone_if_not_exists https://github.com/renesas-rcar/meta-renesas "$WORK/meta-renesas"
wait

# Function to checkout or switch to the correct branch
checkout_or_switch_branch() {
    local repo_path=$1
    local commit=$2
    cd "$repo_path"
    git checkout "$commit"
}
checkout_or_switch_branch "${WORK}/poky" "$POKY_COMMIT"
checkout_or_switch_branch "${WORK}/meta-openembedded" "$META_OE_COMMIT"
checkout_or_switch_branch "${WORK}/meta-renesas" "$META_RENESAS_COMMIT"


# Prompt for build option
echo "Select the build option:"
echo "1. BSP + 3D Graphics + Multimedia package"
echo "2. BSP + 3D Graphics"
echo "3. BSP only"
echo -n "Choose your option: "
read build_case


# Function to check if cp command was successful
check_cp_command() {
    if [ $? -eq 0 ]; then
        echo "Copy operation successful."
    else
        echo "Copy operation failed."
        exit 1
    fi
}

# Path to the local.conf file
local_conf="${TARGET_DIR}/conf/local.conf"

check_package_status() {
    local package=$1
    if grep -q "^DISTRO_FEATURES_append.*\" $package\"" "$local_conf"; then
        echo "Package $package is enabled."
    elif grep -q "^#DISTRO_FEATURES_append.*\" $package\"" "$local_conf"; then
        echo "Package $package is disabled."
    else
        echo "Package $package status is unknown (not found in local.conf)."
    fi
}

# Function to enable a multimedia package
enable_package() {
    local package=$1
    echo "Enabling package: $package"
    # Uncomments the line if it is commented, or adds it if it doesn't exist
    sed -i "/^#DISTRO_FEATURES_append.*\" $package\"/s/^#//" "$local_conf"
    if ! grep -q "DISTRO_FEATURES_append.*\" $package\"" "$local_conf"; then
        echo "DISTRO_FEATURES_append += \" $package\"" >> "$local_conf"
    fi
    check_package_status "$package"
}

# Function to disable a multimedia package
disable_package() {
    local package=$1
    echo "Disabling package: $package"
    # Comments the line if it exists
    sed -i "/^DISTRO_FEATURES_append.*\" $package\"/s/^/#/" "$local_conf"
    check_package_status "$package"
}

set_soc_family_salvator() {
    echo "[H3] SOC_FAMILY r8a7795 | [M3] SOC_FAMILY r8a7796  [M3N] | SOC_FAMILY r8a77965"
    read -p "Enter the SOC type name for Salvator X/XS: " soc_type
    echo "Setting SOC_FAMILY in local.conf to $soc_type..."
    # Add or update SOC_FAMILY in local.conf
    if grep -q "^SOC_FAMILY" "$local_conf"; then
        sed -i "s/^SOC_FAMILY.*/SOC_FAMILY = \"$soc_type\"/" "$local_conf"
    else
        echo "SOC_FAMILY = \"$soc_type\"" >> "$local_conf"
    fi
    # Verify if SOC_FAMILY is correctly set and print it out
    if grep -q "^SOC_FAMILY = \"$soc_type\"" "$local_conf"; then
        echo "SOC_FAMILY successfully set to $soc_type."
        echo "Current setting of SOC_FAMILY in local.conf:"
        grep "^SOC_FAMILY" "$local_conf"
    else
        echo "Failed to set SOC_FAMILY in local.conf."
    fi
}

configure_multimedia_package(){
	echo "Multimedia Package Configuration"
        echo "1. Enable a package"
        echo "2. Disable a package"
        echo "example: h263dec_lib "
        read -p "Enter your choice (1 or 2): " choice

        case $choice in
            1)
                read -p "Enter the package name to enable: " package_name
                enable_package "$package_name"
                ;;
            2)
                read -p "Enter the package name to disable: " package_name
                disable_package "$package_name"
                ;;
        *)
                echo "Invalid choice."
                exit 1
                ;;
        esac

}

# Function to set up build environment and copy configurations
setup_build_env_and_copy_conf() {
    source "${WORK}/poky/oe-init-build-env" "${TARGET_DIR}"
    local config_dir="${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/mmp"
    cp "${config_dir}"/*.conf "${TARGET_DIR}/conf/"
}

# Switch case for different build options
case $build_case in
    1)
        # Populate meta-renesas with proprietary software packages
	WORK_PROP_DIR=${WORK}/binaries_dir
	mkdir -p ${WORK_PROP_DIR}
	unzip -qo ${PROPRIETARY_DIR} -d ${WORK_PROP_DIR}
	#unzip -qo ${PROPRIETARY_DIR}/${GFX_MMP_DRIVER} -d ${WORK_PROP_DIR}
	cd ${WORK}/meta-renesas
	sh "${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/copyscript/copy_proprietary_softwares.sh" -f "${WORK_PROP_DIR}"
        
	setup_build_env_and_copy_conf
        cp "${TARGET_DIR}/conf/local-wayland.conf" "${TARGET_DIR}/conf/local.conf"
	configure_multimedia_package
        if [ "$TARGET_BOARD" == "ebisu" ]; then
                echo "TARGET_BOARD is ebisu. "
        else
                echo "TARGET_BOARD is not ebisu. Setting SOC_FAMILY for Salvator X/XS."
		set_soc_family_salvator

        fi
	cd "${TARGET_DIR}" && nice -n -20 bitbake core-image-weston
	exit
        ;;
    2)
        setup_build_env_and_copy_conf
        cd "${TARGET_DIR}" && bitbake core-image-minimal
        ;;
    3)  
	source "${WORK}/poky/oe-init-build-env" "${TARGET_DIR}"
	cp ${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/bsp/*.conf conf/
	#ln -s ${WORK}/poky/downloads ${TARGET_DIR}
        if [ $? -eq 0 ]; then
                echo "Copy operation was successful."
        else
                echo "Copy operation failed."
        fi

	#echo 'SOC_FAMILY ="r8a7795"' >> ${TARGET_DIR}/conf/local.conf
	if [ "$TARGET_BOARD" == "ebisu" ]; then
                echo "TARGET_BOARD is ebisu. "
        else
                echo "TARGET_BOARD is not ebisu. Setting SOC_FAMILY for Salvator X/XS."
                set_soc_family_salvator

        fi
	cd ${TARGET_DIR}
	nice -n -20 bitbake core-image-minimal
	exit
	;;
    *)
        echo "Invalid option. Exiting."
        exit 1
        ;;
esac
