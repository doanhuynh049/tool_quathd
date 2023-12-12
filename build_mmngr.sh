#!/bin/bash

# WORK directory
WORK=`pwd`
MMNGR_DRV_DIR=${WORK}/mmngr_drv/mmngr_drv/mmngr/mmngr-module/files/mmngr/drv
# MMNGR library and driver commit IDs
MMNGR_LIB_COMMIT="0322548e54b45a064c9cecea29018ef50cdb8423"
#MMNGR_DRV_COMMIT="bd1c9b76cbddf80d5d47255031d6049a6fee21d8"
MMNGR_DRV_COMMIT="2439802426474136312bd10bc4c143fbf1c84850"
# Get commit from meta-renesas
	#meta-renesas/meta-rcar-gen3/recipes-kernel/kernel-module-mmngr$ cat mmngr_drv.inc
	#SRC_URI = "${MMNGR_DRV_URI};branch=rcar_gen3;protocol=https"
	#SRCREV = "2439802426474136312bd10bc4c143fbf1c84850"

# Set environment variables
# sudo locale-gen vi_VN
source /opt/poky/3.1.11/environment-setup-aarch64-poky-linux
export KERNELSRC=/home/quathd/rvc/project/linux-bsp-rcar3
export CP=cp
export MMNGR_CONFIG=MMNGR_EBISU
export MMNGR_SSP_CONFIG=MMNGR_SSP_DISABLE
export MMNGR_IPMMU_MMU_CONFIG=IPMMU_MMU_DISABLE
export INCSHARED=$SDKTARGETSYSROOT/usr/local/include

# Clone MMNGR library and driver repositories
git clone https://github.com/renesas-rcar/mmngr_lib.git &
git clone https://github.com/renesas-rcar/mmngr_drv.git &

# Wait for both repositories to be cloned
wait

# Checkout the specified commits for the MMNGR library and driver
cd ${WORK}/mmngr_lib
git checkout -b tmp ${MMNGR_LIB_COMMIT}
cd ${WORK}/mmngr_drv
git checkout -b tmp ${MMNGR_DRV_COMMIT}

# Display a menu of options
echo "Please choose an option:"
echo "1. Build the MMNGR library"
echo "2. Build the MMNGR buffer library"
echo "3. Build the Kernel Image and MMNGR driver module"
echo "4. Build the MMNGR buffer driver"

# Read the user's choice
read -p "Your choice: " choice

# Switch on the user's choice
case "$choice" in
1)
    # Build the MMNGR library
    cd ${WORK}/mmngr_lib/libmmngr/mmngr
    autoreconf -i
    ./configure ${CONFIGURE_FLAGS} --prefix=$PWD/tmp
    make > libmmngr_buildlog.log
    make install includedir=$INCSHARED >> libmmngr_buildlog.log
    mv libmmngr_buildlog.log ${WORK}/module_mmngr_lib
    cd ${WORK}
    cat libmmngr_buildlog.log
    #Verify the library
    cd ${WORK}/mmngr_lib/libmmngr/mmngr/tmp/lib 
    ls | grep libmmngr.so*
    cp libmmngr* ${WORK}/module_mmngr_lib	
    ;;
2)
    # Build the MMNGR buffer library
    cd ${WORK}/mmngr_lib/libmmngr/mmngrbuf
    autoreconf -i
    ./configure ${CONFIGURE_FLAGS} --prefix=$PWD/tmp
    make > libmmngrbuf_buildlog.log
    #make install includedir=$INCSHARED >> libmmngr_buildlog.log
    mv libmmngrbuf_buildlog.log ${WORK}/module_mmngr_lib
    cd ${WORK}/module_mmngr_lib
    cat libmmngrbuf_buildlog.log
    #Verify the library
    cd ${WORK}/mmngr_lib/libmmngr/mmngrbuf/if/.libs 
    ls | grep libmmngrbuf*
    cp libmmngrbuf* ${WORK}/module_mmngr_lib
    ;;
3)
    echo "Checking builded Image..."
    cd ${KERNELSRC}/arch/arm64/boot
    if [ ! -f "Image" ]; then
	echo "Image does not exist"
	cd ${KERNELSRC} && pwd
    	#Check exist file 
    	if [ ! -f "build_Kernel.sh" ]; then
	    echo "The file build_Kernel.sh does not exist."
       	    exit 1
    	fi
    	# Read confirmation to build Image
    	echo "Are you sure you want to build Kernel? (y/n)"
    	read confirm
    	if [ "$confirm" == "y" ]; then
	    echo "Building Kernel"
	    ./build_Kernel.sh	
    	fi
    else
    	echo "Image exist!"
	echo $MMNGR_CONFIG
    	echo $MMNGR_SSP_CONFIG
    	echo $MMNGR_IPMMU_MMU_CONFIG
    	cd ${MMNGR_DRV_DIR} && pwd
    	make clean
    	make > mmngr_buildlog.log
    	ls | grep *.ko
    	mv mmngr* ${WORK}/module_mmngr_drv
    	cd ${WORK}/module_mmngr_drv
	cat mmngr_buildlog.log
    fi
    ;;
4) 
    cd ${WORK}/mmngr_drv/mmngr_drv/mmngrbuf/mmngrbuf-module/files/mmngrbuf/drv
    make > mmngrbuf_buildlog.log
    #imv mmngrbuf_buildlog.log ${WORK}
    cp mmngr* ${WORK}/module_mmngr_drv
    cd ${WORK}/module_mmngr_drv
    cat mmngrbuf_buildlog.log
    ;;
*)
    # Invalid choice
    echo "Invalid choice. Please try again."
    ;;
esac
# Display a message indicating that the operation is complete
echo "Operation complete."


