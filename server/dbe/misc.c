/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/misc.c
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
#include <sys/types.h>
#include "hb.h"

extern int isset();

int to_upper(int c)
{
	return(islower(c)? toupper(c) : c);
}

int to_lower(int c)
{
	return(isupper(c)? tolower(c) : c);
}

void swapmem(BYTE *p1, BYTE *p2, int n)
{
	char tmp;
	while (n--) {
		tmp = *p1;
		*p1++ = *p2;
		*p2++ = tmp;
	}
}

char *chgcase(char *str, int up)
{
	char c,*p;
 
	p = str;
	while (c = *p) *p++ = up? to_upper(c) : to_lower(c);
	return(str);
}

int blkcmp(char *p1, char *p2, int n)
{
	int c;
	while (n-- > 0) if (c = *p1++ - *p2++) return(c);
	return(0);
}
 