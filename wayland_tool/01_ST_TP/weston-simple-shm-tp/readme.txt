========================================
weston-simple-shm-tp for Weston 2.0
----------------------------------------
					2017.10.12

* Overview
Following files in Weston 2.0 are used for this test program.
weston/
    +-- protocol/
    |    +-- ivi-application.xml
    |    `-- viewporter.xml
    +-- shared/
    |    +-- os-compatibility.c
    |    +-- os-compatibility.h
    |    `-- zalloc.h
    `-- clients/
        `-- simple-shm.c (simple-shm.c was renamed simple-shm-tp.c.)

Following functions were added for testing.
  - Select the scaling mode (scaling and cropping)
  - Set cropping region and scaling size
  - Move window
  - Set window coordinates
Customization can be identified in the patch (add-functions-for-testing.patch).

Operation has been confirmed with Yocto v2.19.0.

--------------------------------
* Revision history
2017.01.10
	Modified for Weston 1.11.0
2017.06.02
	Bug fix and refactoring.
	Removed the following unused options.
		pid, size (They will be ignored, if specified.)
	Removed some key operations.
2017.10.12
	Modified for Weston 2.0
2017.12.15
	Modified to support setting position
2018.08.30
	Add function fix timer

--------------------------------
* Usage

Usage: weston-simple-shm-tp [options]
Options:
	-scale=SCALING_MODE
		Select scaling mode
		  0: none
		  1: scaling
		  2: cropping
		  3: cropping & scaling
		  default: 0
	-pos=SRC_X,SRC_Y,SRC_W,SRC_H,DST_W,DST_H
		Set cropping region and scaling size
		  SRC_X: source x
		  SRC_Y: source y
		  SRC_W: source width
		  SRC_H: source height
		  DST_W: destination width
		  DST_H: destination height
		  default: 400,400,320,240,240,150
	-mov=ATTACH_X,ATTACH_Y
		Set moving window horizontal distance & vertical distance
		  ATTACH_X: horizontally moving distance of 1 step
		  ATTACH_Y: vertically moving distance of 1 step
		  default: 0,0
	-xy=INIT_X,INIT_Y
		Set window position when initializing application
		  INIT_X: x when initializing
		  INIT_Y: y when initializing
		  default: random
	-w
		Use wl_shell
	-f=<time_start><&time_step><time_num>
		Use fixed timer
	-h
		View help\n

--------------------------------
* Build

$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make


