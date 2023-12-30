bin_PROGRAMS = render_node_map

AM_CFLAGS = -Wall -O3

render_node_map_SOURCES = \
	render_node_map.c

render_node_map_LDADD = \
	$(AM_LDADD) \
	@DRM_LIBS@ \
	@KMS_LIBS@ \
	@LIBGBM_LIBS@

render_node_map_CFLAGS = \
	$(AM_CFLAGS) \
	@DRM_CFLAGS@ \
	@KMS_CFLAGS@ \
	@LIBGBM_CFLAGS@

.SECONDEXPANSION:

