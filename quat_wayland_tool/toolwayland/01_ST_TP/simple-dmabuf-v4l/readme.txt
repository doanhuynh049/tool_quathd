================================
simple-dmabuf-v4l for Weston 2.0
--------------------------------
					2017.10.12

* Overview
Modified simple-dmabuf-v4l in Weston 2.0

repository:
	git://anongit.freedesktop.org/wayland/weston
branch:
	2.0
commit:
	4c4f13d0e93edf51008e0680ce7c8ee3790036e8

Files:
weston/
    +-- shared/
    |	`-- zalloc.h
    |	`-- os-compatibility.c
    |	`-- os-compatibility.h
    `-- clients/
        `-- simple-dmabuf-v4l.c

Operational confirmation environment:
	Yocto v2.4 + Wayland 1.13.0 / Weston 2.0

--------------------------------
* Revision history
2017.07.28
	Created for testing dmabuf and its color format.
2017.10.12
	Modified for Weston 2.0
2017.12.15
	Modified to support setting position

--------------------------------
* Usage

Usage: ./simple-dmabuf-v4l [V4L2 device] [V4L2 format] [DRM format] [-p x,y]

The default V4L2 device is /dev/video0

Both formats are FOURCC values (see http://fourcc.org/)
V4L2 formats are defined in <linux/videodev2.h>
DRM formats are defined in <libdrm/drm_fourcc.h>
The default for both formats is YUYV.
If the V4L2 and DRM formats differ, the data is simply reinterpreted rather than converted.
-p Set application position.

ex)
~# ./simple-dmabuf-v4l /dev/video1 YUYV

--------------------------------
* Build

$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

