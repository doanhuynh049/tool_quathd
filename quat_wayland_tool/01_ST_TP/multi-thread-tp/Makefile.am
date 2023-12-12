bin_PROGRAMS = multi-thread-tp

AM_CFLAGS = -Wall -O3 -I./protocol/ -I./shared/ -lpthread

multi_thread_tp_SOURCES = main.c

multi_thread_tp_LDADD = \
	$(AM_LDADD) \
	@SIMPLE_CLIENT_LIBS@

multi_thread_tp_CFLAGS = \
	$(AM_CFLAGS) \
	@SIMPLE_CLIENT_CFLAGS@

