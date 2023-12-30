================================
simple-libgbm
--------------------------------
				2017.07.06

* Overview
This program is used for testing libgbm.

Files/
    +-- Makefile.am
    +-- configure.ac
    +-- readme.txt	(this file)
    `-- simple-libgbm.c

* Environment
Tested in Yocto v2.19.0

--------------------------------
* Revision history

2017.07.06
	Created newly.

--------------------------------
* Usage

Usage: simple-libgbm
  Before running this program, kill weston.

ex)
~# killall weston
~# ./simple-libgbm

When the test succeeds, this test program will return zero and 
the following message will be output at the end.
	[OK] Passed successfully

When the test fails, this test program will return non-zero and 
the following message will be output at the end.
	[NG] Did NOT pass

--------------------------------
* Build

ex)
$ cd $WORKDIR
$ source /opt/poky/2.1.2/environment-setup-aarch64-poky-linux 
$ autoreconf -vif
$ ./configure ${CONFIGURE_FLAGS}
$ make

