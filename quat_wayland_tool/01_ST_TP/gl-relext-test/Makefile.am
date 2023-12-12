BUILT_SOURCES = \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/xdg-shell-unstable-v6-client-protocol.h \
	protocol/ivi-application-protocol.c \
	protocol/ivi-application-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

bin_PROGRAMS = gl-relext-test

AM_CFLAGS = -Wall -O3 -I./protocol/

AM_LDADD = -lm

gl_relext_test_SOURCES = \
	./shared/os-compatibility.c \
	protocol/xdg-shell-unstable-v6-protocol.c \
	protocol/ivi-application-protocol.c \
	shader.c \
	main.c

gl_relext_test_CFLAGS = \
	$(AM_CFLAGS) \
	@KMS_CFLAGS@ \
	@GLES_CFLAGS@ \
	@WAYLAND_CFLAGS@

gl_relext_test_LDADD = \
	$(AM_LDADD) \
	@KMS_LIBS@ \
	@GLES_LIBS@ \
	@WAYLAND_LIBS@

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
