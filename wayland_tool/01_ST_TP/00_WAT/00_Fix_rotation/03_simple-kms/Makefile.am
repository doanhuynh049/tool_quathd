bin_PROGRAMS = simple-kms

AM_CFLAGS = -Wall -O3 -I./protocol/

MMNGR_LIBS = -lmmngr -lmmngrbuf
MMNGR_CFLAGS = -I${PKG_CONFIG_SYSROOT_DIR}/usr/local/include/

BUILT_SOURCES = \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/fullscreen-shell-unstable-v1-client-protocol.h \
	protocol/ivi-application-protocol.c \
	protocol/ivi-application-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

simple_kms_SOURCES = \
	simple-kms.c \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/fullscreen-shell-unstable-v1-protocol.c \
	protocol/ivi-application-protocol.c \
	shared/os-compatibility.c

simple_kms_LDADD = \
	$(AM_LDADD) \
	$(MMNGR_LIBS) \
	@DRM_LIBS@ \
	@KMS_LIBS@ \
	@SIMPLE_CLIENT_LIBS@

simple_kms_CFLAGS = \
	$(AM_CFLAGS) \
	$(MMNGR_CFLAGS) \
	@DRM_CFLAGS@ \
	@KMS_CFLAGS@ \
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

