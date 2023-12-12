#!/bin/bash
# draak  ebisu  h3ulcb  m3nulcb  m3ulcb  salvator-x
BOARD_LIST=("h3ulcb" "m3ulcb" "draak" "ebisu" "m3nulcb" "salvator-x")
TARGET_BOARD=$1
PKGS_DIR=`pwd`/proprietary
TARGET_DIR=`pwd`/${TARGET_BOARD}

POKY_COMMIT=74b22db6879b388d700f61e08cb3f239cf940d18
META_OE_COMMIT=814eec96c2a29172da57a425a3609f8b6fcc6afe
META_RENESAS_COMMIT=13fd24957b9acc29a235ee0c7f398fd867f38b47

#GFX_MMP_LIB=R-Car_Gen3_Series_Evaluation_Software_Package_for_Linux-20220121.zip
#GFX_MMP_DRIVER=R-Car_Gen3_Series_Evaluation_Software_Package_of_Linux_Drivers-20220121.zip

Usage () {
    echo "Usage: $0 \${TARGET_BOARD_NAME}"
    echo "BOARD_NAME list: "
    for i in ${BOARD_LIST[@]}; do echo "  - $i"; done
    exit
}

#Check Param.
if ! `IFS=$'\n'; echo "${BOARD_LIST[*]}" | grep -qx "${TARGET_BOARD}"`; then
    Usage
fi

WORK=`pwd`
echo ${WORK}
mkdir -p ${TARGET_DIR}

# Clone basic Yocto layers in parallel
git clone git://git.yoctoproject.org/poky &
git clone git://git.openembedded.org/meta-openembedded &
git clone https://github.com/renesas-rcar/meta-renesas &

# Wait for all clone operations
wait

# Switch to proper branches/commits
cd ${WORK}/poky
git checkout -b tmp ${POKY_COMMIT}
cd ${WORK}/meta-openembedded
git checkout -b tmp ${META_OE_COMMIT}
cd ${WORK}/meta-renesas
git checkout -b tmp ${META_RENESAS_COMMIT}

# Populate meta-renesas with proprietary software packages
WORK_PROP_DIR=${WORK}/binaries_dir_PT3_newIMR
#mkdir -p ${WORK_PROP_DIR}
#cp /* ${WORK_PROP_DIR}
#unzip -qo ${PKGS_DIR}/* -d ${WORK_PROP_DIR}
#unzip -qo ${PKGS_DIR}/${GFX_MMP_DRIVER} -d ${WORK_PROP_DIR}
cd ${WORK}/
sh meta-renesas/meta-rcar-gen3/docs/sample/copyscript/copy_proprietary_softwares.sh -f ${WORK_PROP_DIR}

cd ${WORK}
source poky/oe-init-build-env ${WORK}/build

cp ${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/bsp/*.conf ${WORK}/build/conf

cp ${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/gfx-only/*.conf ${WORK}/build/conf/
cp ${WORK}/meta-renesas/meta-rcar-gen3/docs/sample/conf/${TARGET_BOARD}/poky-gcc/mmp/*.conf ${WORK}/build/conf

cd ${WORK}/build
cp conf/local-wayland.conf conf/local.conf

bitbake core-image-weston
cp ${WORK}/build * ${TARGET_DIR}
