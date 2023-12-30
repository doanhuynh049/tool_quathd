bin_PROGRAMS = weston-simple-egl-tp

AM_CFLAGS = -Wall -O3 -I./shared/ -I./protocol/ -lm

BUILT_SOURCES = \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h \
	protocol/ivi-application-protocol.c \
	protocol/ivi-application-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

weston_simple_egl_tp_SOURCES = \
	./shared/os-compatibility.c \
	simple-egl.c \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/ivi-application-protocol.c

weston_simple_egl_tp_LDADD = \
	$(AM_LDADD) \
	@GLES_LIBS@ \
	@SIMPLE_CLIENT_LIBS@

weston_simple_egl_tp_CFLAGS = \
	$(AM_CFLAGS) \
	@GLES_CFLAGS@ \
	@SIMPLE_CLIENT_CFLAGS@

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

