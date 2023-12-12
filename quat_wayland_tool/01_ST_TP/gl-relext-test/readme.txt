================================
gl-relext-test for Weston 2.0
--------------------------------
					2017.10.12

* Overview
TP for cofirming WSEGL supporting format.
This program is provided from Igel Co.,Ltd.


Confirmed operating environment:
	Yocto v2.4 + Wayland 1.13.0/Weston 2.0

--------------------------------
* Usage

Usage: ./gl-relext-test [options]
	Options:
	-f | --format color    Set color format (rgb565, argb1555, argb4444, uyvy, nv12, nv16)
	-d | --dump            dump texture to ./texture.dat
	-p | --pos <x>,<y>     Set application position
	-s | --size <w> <h>    Set application size
	-l | --lock			   Use fix image for random part
	-h | --help            Print this message

Press F11 key:
	fullscleen
Press ESC key:
	quit

--------------------------------
* Build
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

