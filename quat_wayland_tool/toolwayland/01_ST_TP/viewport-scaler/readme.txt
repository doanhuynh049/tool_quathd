========================================
viewport-scaler for Weston 2.0
----------------------------------------
					2017.10.12

* Overview
Can make sure that part or all of the specified image is output with being
scaled to the specified size.

Confirmed operating environment:
	Yocto v2.4.0 + Wayland 1.13.0 / Weston 2.0

--------------------------------
* Revision history
2017.01.10
	Modify for Weston 1.11.0
2017.10.12
	Modify for Weston 2.0
2019.07.12
	Build using autotool

--------------------------------
* Usage

Usage: viewport-scaler <IMAGE_FILE> <CUT_X> <CUT_Y> <CUT_WIDTH> <CUT_HEIGTH> <OUT_WIDTH> <OUT_HEIGTH>
Options:
	IMAGE_FILE:
		Specify image data to edit
	CUT_X:
		Specify the horizontal position to start cropping
	CUT_Y:
		Specify the vertical position to start cropping
	CUT_WIDTH:
		Specify the cropping width
	CUT_HEIGTH:
		Specify the ccroppingut height
	OUT_WIDTH:
		Specify the output width of the cropped image
	OUT_HEIGTH:
		Specify the output height of the cropped image

ex) ./viewport-scaler num.png 200 190 180 180 360 360
	Cut out width "180" and height "180" from the coordinates (200, 190)
	of image data "num.png", and output it with width "360" and height "360".

--------------------------------
* Build
[procedure]
(1) Update file under "lib" directory to the latest version.
$ cd $WORKDIR
$ cp (WORK directoy of Yocto Project)/tmp/work/aarch64-poky-linux/weston/2.0.0-r0/build/.libs/libtoytoolkit.a ./lib/

(2) Build.
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make
