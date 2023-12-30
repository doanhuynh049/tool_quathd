=================================================
OES3_Texture_Pixmap_EGLImage_wl for Weston 2.0
-------------------------------------------------
					2017.10.12

* Overview
Display 3D image using Pixmap.

Note: OES3_Texture_Pixmap_EGLImage_wl is REL original test application
      that is included in Graphics Package for v1.7.0
      (RTM0RC7791GLRE0001SL34C/samples/unittest/es20_sample_EGLImage_wl).

Confirmed operating environment:
	Yocto v2.4 + Wayland 1.13.0 /Weston 2.0

--------------------------------
* Revision history
2017.01.10
	Modify for Weston 1.11.0
2017.10.12
	Modify for Weston 2.0
2017.12.15
	Modified to support setting position
2019.07.12
	Build using autotool

--------------------------------
* Usage

preparation: 
	(1) Store the following file in an arbitrary directory under ROOTFS.
		OES3_Texture_Pixmap_EGLImage_wl

	(2) Store the following files under OES3_Texture_Pixmap_EGLImage_wl's directory.
		OES3_FragShaderSample.fsh
		OES3_VertShaderSample.vsh
		OES3_VertShaderSampleCom.vsh

Usage: OES3_Texture_Pixmap_EGLImage_wl
	-p <x,y> Set application position


--------------------------------
* Build
[procedure]
(1) Update file under "lib" directory to the latest version.
$ cd $WORKDIR
$ cp (WORK directoy of Yocto Project)/tmp/work/aarch64-poky-linux/weston/2.0.0-r0/build/.libs/libtoytoolkit.a ./lib/
$ cp (WORK directoy of Yocto Project)/tmp/work/aarch64-poky-linux/weston/2.0.0-r0/build/.libs/libshared.a ./lib/

(2) Build.
$ cd $WORKDIR
$ source /opt/poky/2.3/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make
