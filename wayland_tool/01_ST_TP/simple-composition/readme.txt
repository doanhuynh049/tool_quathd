================================
simple-composition for Weston 2.0
--------------------------------
					2018.02.16

* Overview
This program is based on weston-simple-egl in Weston 2.0.0.

repository:
	git://anongit.freedesktop.org/wayland/weston
branch:
	2.0
commit:
	4c4f13d0e93edf51008e0680ce7c8ee3790036e8


Files:
The folloing files in Weston 2.0 are used for this program.

weston/
    +-- shared/
    |	`--helpers.h
    |	`-- platform.h
    |	`-- weston-egl-ext.h
    `-- clients/
        `-- simple-egl.c


Changes:
  Added the following functions and options for testing.
	- s/size option for changing window size
		Previous 's' option for buffer size is removed.
	- p/pos option for setting window position
	- m/move option for moving window automatically
	- r/range option for setting window position in limited range
	- f/fps option for output frame rate
		Frame rate is not output without this option.
		Previous 'f' option for fullscreen is removed.
  Removed some features unused for testing.
	- ivi shell and xdg shell
		xdg shell is replaced with wl_shell
	- input operation and protocols (wl_seat, wl_pointer, wl_touch,
	  wl_keyboard, and wl_shm used for cursor)
  Changed the background color from transparent black to transparent white.


Confirmed operating environment:
	Yocto v3.4.0 + Wayland 1.14 + Weston 2.0

--------------------------------
* Revision history

2018.01.26
	Developed based on weston-simple-egl in weston 2.0

2018.02.16
	Added an argument to --opaque option

--------------------------------
* Usage

Usage: simple-composition [OPTIONS]

  -s, --size <w>x<h>    Window width and height (default: 250x250)
  -p, --pos <x>,<y>     Window position (default: random)
  -m, --move <dx>,<dy>  Motion vector of window (default: 0,0)
  -r, --range <x1>,<y1>,<x2>,<y2>
                        Range of window position (-p option is ignored)
  -o, --opaque <x>,<y>,<w>,<h>
                        Create an opaque surface
  -b, --go-faster       Don't sync to compositor redraw (eglSwapInterval 0)
  -f, --fps             Output frame rate in FPS
  -h, --help            This help text

--------------------------------
* Build

$ cd $WORKDIR
$ source /opt/poky/2.4.1/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

