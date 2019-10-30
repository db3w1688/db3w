AUTOMAKE_OPTIONS = foreign
CFLAGS = -g -DUNALIGNED_MEM -DNO_CHSIZE
AM_CPPFLAGS = -I$(top_srcdir)/include
lib_LIBRARIES = libhbapi.a
libhbapi_a_SOURCES = \
	$(top_srcdir)/include/hb.h \
	$(top_srcdir)/include/hbapi.h \
	breaks.c \
	ctlc.c \
	display.c \
	err.c \
	hbapi.c \
	hb_sock.c \
	msg.c \
	port.c \
	startup.c \
	upplwr.c \
	usr.c 
