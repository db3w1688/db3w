/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/upplwr.c
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

#include <ctype.h>

#ifndef _toupper
#define _toupper(c)	toupper(c)
#define _tolower(c)	tolower(c)
#endif

int c_upper(int c)
{
	return(islower(c)? _toupper(c) : c);
}

int c_lower(int c)
{
	return(isupper(c)? _tolower(c) : c);
}

char *hb_uppcase(char *str)
{
	char c, *tp = str;
	while (c = *tp) *tp++ = c_upper(c);
	return(str);
}

char *hb_lwrcase(char *str)
{
	char c, *tp = str;
	while (c = *tp) *tp++ = c_lower(c);
	return(str);
}

