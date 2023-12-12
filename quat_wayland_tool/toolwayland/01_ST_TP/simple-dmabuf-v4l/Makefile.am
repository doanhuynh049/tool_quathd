bin_PROGRAMS = simple-dmabuf-v4l

AM_CFLAGS = -Wall -O3 -I./protocol/

BUILT_SOURCES = \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/fullscreen-shell-unstable-v1-client-protocol.h \
	protocol/linux-dmabuf-unstable-v1-protocol.c \
	protocol/linux-dmabuf-unstable-v1-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

simple_dmabuf_v4l_SOURCES = \
	simple-dmabuf-v4l.c \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/linux-dmabuf-unstable-v1-protocol.c \
	./shared/os-compatibility.c

simple_dmabuf_v4l_LDADD = \
        $(AM_LDADD) \
        @DRM_LIBS@ \
        @WAYLAND_CLIENT_LIBS@

simple_dmabuf_v4l_CFLAGS = \
        $(AM_CFLAGS) \
        @DRM_CFLAGS@ \
        @WAYLAND_CLIENT_CFLAGS@

.SECONDEXPANSION:

define protostability
$(if $(findstring unstable,$1),unstable,stable)
endef

define protoname
$(shell echo $1 | sed 's/\([a-z\-]\+\)-[a-z]\+-v[0-9]\+/\1/')
endef

protocol/%-protocol.c : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) code < $< > $@

protocol/%-server-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) server-header < $< > $@

protocol/%-client-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) client-header < $< > $@

protocol/%-protocol.c : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) code < $< > $@

protocol/%-server-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) server-header < $< > $@

protocol/%-client-protocol.h : $(top_srcdir)/protocol/%.xml
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(wayland_scanner) client-header < $< > $@
