AUTOMAKE_OPTIONS = foreign
CFLAGS = -Wall -Wextra -Wstrict-prototypes -Wshadow -Wpointer-arith \
	-Wcast-qual -Wcast-align -Wwrite-strings -Wredundant-decls \
	-Wnested-externs -Werror -O3 -funsigned-char
AM_CPPFLAGS = -Iinclude
noinst_LIBRARIES = libcrc.a
libcrc_a_SOURCES = \
	src/crc16.c \
	src/crc32.c \
	src/crc8.c \
	src/crcccitt.c \
	src/crcdnp.c \
	src/crckrmit.c \
	src/crcsick.c \
	src/nmea-chk.c

