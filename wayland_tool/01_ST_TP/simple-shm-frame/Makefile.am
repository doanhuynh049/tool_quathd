bin_PROGRAMS = simple-shm

AM_CFLAGS = \
	-Wall -O3

simple_shm_SOURCES = simple-shm.c \
	shared/os-compatibility.c

simple_shm_LDADD = \
	$(AM_LDADD) \
        @WAYLAND_CLIENT_LIBS@ \
	@CAIRO_LIBS@

simple_shm_CFLAGS = \
	$(AM_CFLAGS) \
	@WAYLAND_CLIENT_CFLAGS@ \
	@CAIRO_CFLAGS@
