================================
simple-egl-leak for Weston 2.0
--------------------------------
					2017.10.12

* Overview
This program is based on simple-egl obtained from Weston 1.11.0.

repository:
	git://anongit.freedesktop.org/wayland/weston
branch:
	2.0
commit:
	4c4f13d0e93edf51008e0680ce7c8ee3790036e8

Files used:
weston/
    +-- protocol/
    |	`-- ivi-application.xml
    +-- shared/
	|	`--helpers.h
	|	`--os-compatibility.c
	|	`--os-compatibility.h
    |	`-- platform.h
    |	`-- weston-egl-ext.h
    `-- clients/
        `-- simple-egl.c

The following protocol specification xml in toolchain is used.
	xdg-shell-unstable-v6.xml

Changes：
  Added the following functions for testing.
	- Specifying window size.
	- Repeating the following functions execution to detect resource leaks. 
	  The repeat count can be specified.
		eglInitialize / eglTerminate
		eglCreateWindowSurface / eglDestroySurface
	- Changing SwapInterval during operation.
	- Set fixed angle.


Confirmed operating environment:
	Yocto v2.4.0 + Wayland/Weston 2.0

--------------------------------
* Usage

Usage: simple-egl [OPTIONS]
  -o    Create an opaque surface
  -s <w> <h>    Set window width to <w> and window height to <h>
  -l <mode>     Set loop mode to 0 ~ 3
                        0: disable
                        1: eglInitialize / eglTerminate
                        2: eglCreateWindowSurface / eglDestroySurface
                        3: eglCreatePixmapSurface / eglDestroySurface
  -c <value>    Set loop number
  -b    Don't sync to compositor redraw (eglSwapInterval 0)
  -p <x>,<y>    Set application position
  -a <angle_step>	Set fixed angle
  -h    This help text

Press F9 key:
	toggle the eglSwapInterval '0' and '1'
Press ESC key:
	quit

--------------------------------
* Build
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

