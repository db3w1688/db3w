/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/debug.c
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

#include <stdio.h>
#include <ctype.h>
#include "hb.h"
#include "dbe.h"

static FILE *dbg;

static int prn(int c)
{
	return(isprint(c)? c:'.');
}

void dbg_open(void)
{
	if (!(dbg = fopen("dbg.out", "w"))) fprintf(stderr, "Error opening debug output!\n");
}

int print_page(pageno_t pno, NDX_PAGE *p)
{
	int i,nlines;
	BYTE *tp;
	if (!dbg) return(0);
	fprintf(dbg,"\nPage# %ld, %d entries\n",pno,p->entry_cnt);
	nlines = (curr_ndx->ndx_h.entry_size*p->entry_cnt+sizeof(p->entry_cnt)+sizeof(p->pad)+sizeof(pageno_t)) / 16 + 1;
	for (i=0,tp=(BYTE *)p; i<nlines; ++i,tp+=16) {
	fprintf(dbg,"%02x %02x %02x %02x %02x %02x %02x %02x-%02x %02x %02x %02x %02x %02x %02x %02x"
		,tp[0],tp[1],tp[2],tp[3],tp[4],tp[5],tp[6],tp[7],tp[8],tp[9],tp[10],tp[11],tp[12],tp[13],tp[14],tp[15]);
	fprintf(dbg,"  %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n"
		,prn(tp[0]),prn(tp[1]),prn(tp[2]),prn(tp[3]),prn(tp[4]),prn(tp[5]),prn(tp[6]),prn(tp[7])
		,prn(tp[8]),prn(tp[9]),prn(tp[10]),prn(tp[11]),prn(tp[12]),prn(tp[13]),prn(tp[14]),prn(tp[15]));
	}
	return(0);
}

int print_item(pageno_t pno, int r, recno_t recnum, pageno_t pagenum)
{
	fprintf(dbg, "Insert into page# %ld, item# %d: %ld %s %ld\n", pno, r, recnum,curr_dbf->keybuf, pagenum);
	return(0);
}

void dbg_close(void)
{
	if (dbg) fclose(dbg);
	dbg = (FILE *)NULL;
}
