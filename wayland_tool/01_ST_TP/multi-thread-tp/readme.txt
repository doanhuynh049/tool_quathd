================================
multi-thread-tp for Weston 2.0
--------------------------------
					2018.04.25

* Overview
This test program is used for checking multi-thread operation.
This test program is downloaded from following link.

Bug 91273 - Setting a queue on a wl_proxy is racy if some other thread is dispatching the default queue
https://bugs.freedesktop.org/show_bug.cgi?id=91273
	simple test application to reproduce this bug

Fixed:
	https://cgit.freedesktop.org/wayland/wayland/commit/?id=6d29c0da3cd168e08187cd204d2314188479c0f1
	https://cgit.freedesktop.org/wayland/wayland/commit/?id=69ec70fb0d3f75f4bcce449238d6297f6a986b5f

Confirmed operating environment:
	Yocto v3.6.0 + Wayland 1.13.0 /Weston 2.0

--------------------------------
* Revision history
2016.11.30
	created
2017.03.21
	translated readme.txt from japanese to english.
2017.07.28
	Fixed a bug in this test program.
		See wl_display_prepare_read_queue() in Wayland documentation.
		https://wayland.freedesktop.org/docs/html/apb.html
2017.10.12
	Modified for Weston 2.0.
2018.04.25
	Added a delay of 200 usec to reproduce the multi-thread issue easily.
	Replaced poll() and related process with thread safe APIs.

--------------------------------
* Usage

~# ./multi-thread-tp

--------------------------------
* Build

$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

