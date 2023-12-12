bin_PROGRAMS = viewport-scaler

AM_CFLAGS = -Wall -O3 -ltoytoolkit -lpixman-1 -lpng16 -ljpeg -lm -lpango-1.0 -lpangocairo-1.0 -lcairo -lxkbcommon
AM_CFLAGS += \
	-I./include \
	-I./protocol/ \
	-I$(SDKTARGETSYSROOT)/$(DEBUG_SRCDIR)/ \
	-I$(SDKTARGETSYSROOT)/$(DEBUG_SRCDIR)/clients/ \
	-I$(SDKTARGETSYSROOT)/$(DEBUG_SRCDIR)/shared/ \
	-I$(SDKTARGETSYSROOT)/usr/include/cairo/ \
	-I$(SDKTARGETSYSROOT)/usr/include/pango-1.0/ \
	-I$(SDKTARGETSYSROOT)/usr/include/pixman-1/ \
	-I$(SDKTARGETSYSROOT)/$(DEBUG_SRCDIR)/include/

AM_LDADD = -L./lib

BUILT_SOURCES = \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/fullscreen-shell-unstable-v1-client-protocol.h \
	protocol/linux-dmabuf-unstable-v1-protocol.c \
	protocol/linux-dmabuf-unstable-v1-client-protocol.h \
	protocol/viewporter-protocol.c \
	protocol/viewporter-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

viewport_scaler_SOURCES = \
	viewport_scaler.c \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/linux-dmabuf-unstable-v1-protocol.c \
	protocol/viewporter-protocol.c \
	shared/cairo-util.c \
	shared/cairo-util.h \
	shared/xalloc.c\
	shared/xalloc.h \
	shared/image-loader.c \
	shared/image-loader.h \
	shared/frame.c \
	shared/frame.h \
	shared/config-parser.c \
	shared/config-parser.h \
	shared/file-util.c \
	shared/file-util.h \
	shared/os-compatibility.c \
	shared/os-compatibility.h

viewport_scaler_LDADD = \
	$(AM_LDADD) \
	@WAYLAND_CLIENT_LIBS@

viewport_scaler_CFLAGS = \
	$(AM_CFLAGS) \
	@WAYLAND_CLIENT_CFLAGS@

viewport_scaler_CPPFLAGS = \
	-DDATADIR='"$(datadir)"' \
	$(AM_CPPFLAGS)

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
