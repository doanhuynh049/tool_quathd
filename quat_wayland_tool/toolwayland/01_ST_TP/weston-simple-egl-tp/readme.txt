========================================
weston-simple-egl-tp for Weston 2.0
----------------------------------------
					2017.10.12

* Overview
Used simple-egl obtained from Weston 2.0.
We added the following functions to the original simple-egl.
  - Set opaque region
  - Set window size
  - Resize window
  - Move window

Confirmed operating environment:
	Yocto v2.4 + Wayland 1.13.0 /Weston 2.0

--------------------------------
* Revision history
2017.01.10
	Modified for Weston 1.11.0
2017.10.12
	Modified for Weston 2.0
2017.12.15
	Modified to support setting position
2018.08.30
	Add function fix timer
2019.07.12
	Build using autotool

--------------------------------
* Usage

Usage: simple-egl [options]
Options:
	-f	Run in fullscreen mode
	-o	Create an opaque surface
	-s	Use a 16 bpp EGL config
	-b	Don't sync to compositor redraw (eglSwapInterval 0)
	-p=<x>,<y>
		Set application position.
	-opaque=X,Y,WIDTH,HEIGHT
		Coordinate and size of opaque region.
		default: 0,0,250,250
	-size=WIDTH,HEIGHT
		Window size.
		default: 250,250
	-resize=UNIT_SIZE
		The value of change in size when you press D/A/S/W key.
		default: 10
	-move=UNIT_DISTANCE
		Moving distance when you press Left/Right/Up/Down key.
		default: 10
	-a <angle_step>
		Set fixed angle
	-h	This help text


Press F11 key	: Fullscleen
Press Left key	: Move to the left direction
		  Moving distance : specified by the -move option
Press Right key	: Move to the right direction
		  Moving distance : specified by the -move option
Press Up key	: Move to the up direction
		  Moving distance : specified by the -move option
Press Down key	: Move to the down direction
		  Moving distance : specified by the -move option
Press J key	: Move to the left direction (x50)
		  Moving distance : 50 times the value which specified by the -move option
Press L key	: Move to the right direction (x50)
		  Moving distance : 50 times the value which specified by the -move option
Press I key	: Move to the up direction (x50)
		  Moving distance : 50 times the value which specified by the -move option
Press K key	: Move to the down direction (x50)
		  Moving distance : 50 times the value which specified by the -move option
Press D key	: Up the window width
		  Resize unit: specified by the -resize option
Press A key	: Down the window width
		  Resize unit: specified by the -resize option
Press S key	: Up the window height
		  Resize unit: specified by the -resize option
Press W key	: Down the window height
		  Resize unit: specified by the -resize option


--------------------------------
* Build
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make
