================================
simple-kms for Weston 2.0
--------------------------------
					2017.10.12

* Overview
This program is based on simple-shm obtained from Weston 2.0.

repository:
	git://anongit.freedesktop.org/wayland/weston
branch:
	2.0
commit:
	4c4f13d0e93edf51008e0680ce7c8ee3790036e8

Files used:
weston/
    +-- protocol/
    |    `-- ivi-application.xml
    +-- shared/
    |    +-- os-compatibility.c
    |    +-- os-compatibility.h
    |    `-- zalloc.h
    `-- clients/
        `-- simple-shm.c
            => Modified and renamed to "simple-kms.c"

The following protocol specification xmls in toolchain are used.
	fullscreen-shell-unstable-v1.xml
	xdg-shell-unstable-v6.xml

Confirmed operating environment:
	Yocto v2.4.0 + Wayland 1.13.0 / Weston 2.0

--------------------------------
* Revision history

2017.07.20
	Added an additional fullscreen mode.
	Added an option to specify the framerate.
2017.07.14
	Changed options.
	Added fullscreen mode.
2016.11.29
	Weston 1.11.0 compatible.
2016.10.31
	Added option to allocate memory using mmngr.
	Added size specification option.
2016.10.13
	Newly created based on simple-shm.
2017.10.12
	Modified for Weston 2.0
2017.10.27
	Modified to support using fixed timer
	Modified to support setting position

--------------------------------
* Usage

Usage: simple-kms [OPTIONS]

  -s, --size=WxH       Set the window width and height
  -m, --mmngr          Use mmngr
  -f, --fullscreen     Run in fullscreen mode (default)
  -d, --fullscreen-d   Run in fullscreen mode with driver method
  -r, --framerate=mHz  Set the framerate in mHz i.e., framerate of 60000 is 60Hz
  -t, --fix-timer      Use fixed timer
  -p, --pos <x> <y>    Set application position
  -h, --help           Display this help text and exit

Note: Before executing this program with mmngr, the following loadable modules should be added.
	mmngr, mmngrbuf
ex)
	# modprobe mmngr
	# modprobe mmngrbuf

--------------------------------
* Known issues

2016.10.31
	When mmngr is run with -m option, mmngr's logs are output at the end of program.
		At the time of calling mmngr_export_end_in_user_ext, memory seems to be
		still referenced by Weston.
	When this program is run repeatedly (about 200 times), incorrect display or
	freezing may occur.

--------------------------------
* Build

$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

