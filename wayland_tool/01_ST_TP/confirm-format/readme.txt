========================================
confirm-format for Weston 2.0
----------------------------------------
					2017.10.12

* Overview
Confirm available format.
We support the following functions.
  - Confirm all formats to supoort
  - Use only one format for shm buffer
  - Use only one format for kms buffer

Confirmed operating environment:
	Yocto v2.4 + Wayland 1.13.0 / Weston 2.0

--------------------------------
* Revision history
2017.01.10
	Modified for Weston 1.11.0
2017.10.01
	Modified for Weston 2.0
2017.12.15
	Modified to support setting position
2019.07.12
	Build using autotool

--------------------------------
* Usage

Usage: confirm-format [options]
Options:
	-b	Confirm all formats to supoort(shm until 1-4 & kms until 1-15)
	-s format
		Use only one format for shm buffer
		  format: A positive number from 1 to 4
		  'format' is one of
		     1: ARGB8888
		     2: XRGB8888
		     3: RGB565
		     4: YUYV
	-k format
		Use only one format for kms buffer
		  format: A positive number from 1 to 15
		  'format' is one of
		     1: ARGB8888
		     2: XRGB8888
		     3: RGB565
		     4: YUYV
		     5: RGB888
		     6: UYVY
		     7: NV12
		     8: NV16
		     9: NV21
		    10: NV61
		    11: RGB332
		    12: BGR888
		    13: XBGR8888
		    14: YVYU
		    15: YUV420
	-p <x>,<y>
		Set application position

--------------------------------
* Build
[procedure]
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make
