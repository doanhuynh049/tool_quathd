================================
dmabuf-test-fs for Weston 5.0
--------------------------------
					2019.05.08

* Overview
This program is based on the client application for linux_dmabuf test provided
from Igel Co.,Ltd. on August 1, 2016, those which can accept unsupported format,
for testing.
The source code changes can be confirmed with 'git diff'.

Confirmed operating environment:
	Yocto v3.19.0 + Wayland 1.16.0/Weston 5.0

--------------------------------
* Revision history
2016.08.24
	Added function to specify size and clipping region.
	Added function to cause parameter error intentionally with test mode option.
2016.09.15
	Fix to be able to specify '0' with size specification option.
2017.10.12
	Modified for Weston 2.0
2017.12.15
	Modified to support setting position
2018.01.30
	Added support ARGB8888, and BGRA8888 color format
2018.05.24
	Added a option '-d' to set a damage region with odd width and height
2018.05.31
	Added support BGRX8888 and XRGB8888 color format
2019.05.08
	Added a option '-o' to create opaque surface

--------------------------------
* Usage

Usage: ./dmabuf-test-fs [OPTIONS]
   -f <format>  Set color format
   -s <w> <h>   Set window width to <w> and window height to <h>
   -c <x> <y> <w> <h>   Set a clipping rectangle
   -t <num>     Set test mode to <num>
   -d           Set a damge rectangle that has an odd width and height
   -y           Set yflip invert
   -p <x>,<y>   set pos x y for window
   -o			Create opaque surface

Color format:
	argb8888, bgra8888,xrgb8888,bgrx8888
	yuyv, yvyu, uyvy, vyuy,
	nv12, nv21, nv16, nv61,
	yuv420, yvu420, yuv422, yvu422, yuv444, yvu444
	(others --> xrgb8888 (default value))
	Note: Some color formats require image data in the images directory during operation.

Test mode:
	ZLINUX_BUFFER_PARAMS_ERROR_PLANE_IDX = 1,
	ZLINUX_BUFFER_PARAMS_ERROR_PLANE_SET = 2,
	ZLINUX_BUFFER_PARAMS_ERROR_INCOMPLETE = 3,
	ZLINUX_BUFFER_PARAMS_ERROR_INVALID_DIMENSIONS = 5,
	ZLINUX_BUFFER_PARAMS_ERROR_OUT_OF_BOUNDS = 6,
	invalid parameter for the fullscreen mode  = -1

Press F11 key:
	fullscleen
Press F9 key:
	clipping

--------------------------------
* Build
[procedure]

$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

