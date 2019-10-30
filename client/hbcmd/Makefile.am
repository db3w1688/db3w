AUTOMAKE_OPTIONS = foreign
CFLAGS = -g -DUNALIGNED_MEM -DKW_MAIN
AM_CPPFLAGS = -I$(top_srcdir)/include
LDFLAGS = -lm -lc -lcrypt
LDADD = $(top_srcdir)/client/hbapi/libhbapi.a
bin_PROGRAMS = hbcmd hbhlp
hbcmd_SOURCES = \
	$(top_srcdir)/include/hb.h \
	$(top_srcdir)/include/hbapi.h \
	$(top_srcdir)/include/hbusr.h \
	$(top_srcdir)/include/hbkwdef.h \
	$(top_srcdir)/include/hbtli.h \
	hb.c \
	hbhlp.c \
	hbscrn.c \
	prntval.c \
	usr.c \
	usrfcn.c

hbhlp_CFLAGS = -g -DUNALIGNED_MEM -DHLP_MAIN

hbhlp_SOURCES = \
	hbhlp.c \
	hbscrn.c
