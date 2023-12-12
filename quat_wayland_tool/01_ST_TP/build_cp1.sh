#!/bin/bash

# set -e

if [ $# -lt 2 ]; then
    echo "Lack of argument!!"
    echo "  arg1: Toolchain directory
    arg2: Output directory(Do not set output directory in folder with tp)"
    echo "Copy libtoytoolkit.a in WORK directory of Yocto Project to lib directory of OES3_Texture_Pixmap_EGLImage_wl and viewport-scaler."
    exit 1
fi

OUTPUT_DIR=${PWD}/$2

if [ ! -f $1/environment-setup-aarch64-poky-linux ]; then
    echo "Not found toolchain at: $1"
    exit 0
fi

source $1/environment-setup-aarch64-poky-linux

for i in $(ls); do
    if [ -d ./$i ]; then
        echo "===============================================================================$i"
        cd $i
        echo "$i"
        echo "1"
        rm -f log.txt
        if [ ! -f Makefile.am ]; then
            if [ -f $i ]; then
                make clean > log.txt
            fi
            make >> log.txt
        else
            if [ -f $i ]; then
                make distclean > log.txt
            fi
            autoreconf -vif >> log.txt
            ./configure $CONFIGURE_FLAGS >> log.txt
            make >> log.txt
        fi
        if [ $i == 'OES3_Texture_Pixmap_EGLImage_wl' ]; then
            cp OES3_FragShaderSample.fsh $OUTPUT_DIR
            cp OES3_VertShaderSample.vsh $OUTPUT_DIR
            cp OES3_VertShaderSampleCom.vsh $OUTPUT_DIR
            cp $i $OUTPUT_DIR
        else
            cp $i $OUTPUT_DIR
        fi
        cd ..
    else
        echo "-------------------------------------------------------------------------------Skip $i"
    fi
done

