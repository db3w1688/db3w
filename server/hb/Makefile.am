AUTOMAKE_OPTIONS = foreign
CFLAGS = -g -DUNALIGNED_MEM -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DEMBEDDED -DBSDRENO -DNO_CHSIZE -DMAXUSERS=50
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/misc/libb64-1.2/include
LDFLAGS = -lm -lc -lbsd -lcrypt -lpthread
LDADD = ../../misc/tiger32/tiger.o ../../misc/tiger32/sboxes.o ../dbe/libdbe.a ../../misc/libcrc-2.0/libcrc.a ../../misc/libb64-1.2/libb64.a
bin_PROGRAMS = hbdbe
hbdbe_SOURCES = \
	$(top_srcdir)/include/hb.h \
	$(top_srcdir)/include/dbe.h \
	$(top_srcdir)/include/hbkwdef.h \
	$(top_srcdir)/include/hbpcode.h \
	$(top_srcdir)/include/hberrno.h \
	$(top_srcdir)/include/hbusr.h \
	$(top_srcdir)/include/hbtli.h \
	$(top_srcdir)/include/hbapi.h \
	cmd.c \
	code.c \
	date.c \
	fcn.c \
	init.c \
	int.c \
	lex.c \
	misc.c \
	parse.c \
	port.c \
	proc2.c \
	proc.c \
	serial.c \
	sock.c \
	sys.c \
	var.c
