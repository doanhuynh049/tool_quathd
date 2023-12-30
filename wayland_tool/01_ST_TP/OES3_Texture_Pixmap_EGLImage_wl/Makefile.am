bin_PROGRAMS = OES3_Texture_Pixmap_EGLImage_wl

AM_CFLAGS = -Wall -O3 -I./share -I./protocol -ltoytoolkit -ludev -lpthread
AM_LDADD = -L./lib

BUILT_SOURCES = \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/fullscreen-shell-unstable-v1-client-protocol.h \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

OES3_Texture_Pixmap_EGLImage_wl_SOURCES = \
	OES3_Texture_Pixmap_EGLImage_wl.cpp \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	shared/os-compatibility.c
	

OES3_Texture_Pixmap_EGLImage_wl_LDADD = \
	$(AM_LDADD) \
	@DRM_LIBS@ \
	@KMS_LIBS@ \
	@GLES_LIBS@ \
	@WAYLAND_LIBS@

OES3_Texture_Pixmap_EGLImage_wl_CXXFLAGS = \
	$(AM_CFLAGS) \
	@DRM_CFLAGS@ \
	@KMS_CFLAGS@ \
	@GLES_CFLAGS@ \
	@WAYLAND_CFLAGS@

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