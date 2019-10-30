/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/w3util.h
 *
 *  Copyright (C) 1997-2019 Kimberlite Software <info@kimberlitesoftware.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define NL2BR			0x0001	//html_encode new line to <BR> flag

#ifndef _NO_PROTO
char *makeword(char *line, char stop);
char *fmakeword(FILE *f, char stop, int *len);
char x2c(char *what);
void unescape_url(char *url);
void plustospace(char *str);

char *skipblank(char *line);
char *space2plus(char *str);
char *plus2space(char *str);
void print_header(void);
void html_out(int flags, char *fmt, ...);
void html_encode(char *src, char *dest, int srclen, int destlen, int flags);
#else
char *makeword();
char *fmakeword();
char x2c();
void unescape_url();
void plustospace();

char *skipblank();
char *space2plus();
char *plus2space();
void print_header();
void html_out();
void html_encode();
#endif
