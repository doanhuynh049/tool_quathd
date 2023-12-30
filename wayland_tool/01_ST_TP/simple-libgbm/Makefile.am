bin_PROGRAMS = simple-libgbm

AM_CFLAGS = -Wall -O3 -DLIBGBM_BASE=$(ALIBGBM_BASE)

simple_libgbm_SOURCES = \
	simple-libgbm.c

simple_libgbm_LDADD = \
	$(AM_LDADD) \
	@DRM_LIBS@ \
	@KMS_LIBS@ \
	@GCOV_LIBS@ \
	@LIBGBM_LIBS@

simple_libgbm_CFLAGS = \
	$(AM_CFLAGS) \
	@DRM_CFLAGS@ \
	@KMS_CFLAGS@ \
	@GCOV_CFLAGS@ \
	@LIBGBM_CFLAGS@

.SECONDEXPANSION:

