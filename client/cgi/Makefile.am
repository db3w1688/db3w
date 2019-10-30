AUTOMAKE_OPTIONS = foreign
CFLAGS = -g -DUNALIGNED_MEM -DHBCGI
AM_CPPFLAGS = -I$(top_srcdir)/include
LDFLAGS = -lm -lc -lcrypt
LDADD = $(top_srcdir)/client/hbapi/libhbapi.a
bin_PROGRAMS = db3w.cgi showvars.cgi viewsource.cgi console
db3w_cgi_SOURCES = \
	db3w.c \
	err.c \
	usrfcn.c \
	$(top_srcdir)/client/hbcmd/prntval.c \
	w3util.c

showvars_cgi_SOURCES = \
	showvars.c

viewsource_cgi_SOURCES = \
	viewsource.c \
	w3util.c

console_SOURCES = \
	console.c

