AUTOMAKE_OPTIONS = foreign
CFLAGS = -g -DUNALIGNED_MEM -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DEMBEDDED -DNO_MAIN
AM_CPPFLAGS = -I$(top_srcdir)/include
noinst_LIBRARIES = libdbe.a
libdbe_a_SOURCES = \
	$(top_srcdir)/include/hb.h \
	$(top_srcdir)/include/dbe.h \
	$(top_srcdir)/include/hbpcode.h \
	append.c \
	btree.c \
	copy.c \
	db.c \
	error.c \
	fio.c \
	fn.c \
	index.c \
	init.c \
	lock.c \
	memo.c \
	misc.c \
	ndx.c \
	pack.c \
	page.c \
	qindex.c \
	rec.c \
	setto.c \
	transac.c \
	update.c \
	vsort.c
