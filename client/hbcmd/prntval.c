/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbcmd/prntval.c
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
#include <string.h>
#include "hb.h"
#include "hbapi.h"

#ifdef HBCGI
extern FILE *rptfp;
#endif

char *hb_val2str(HBCONN conn, HBVAL *v, char *outbuf) //outbuf must be >= STRBUFSZ
{
	int isnull = !v->valspec.len;
	if (isnull && dbe_isset(conn, NULL_ON)) strcpy(outbuf, "<NULL>");
	else switch (v->valspec.typ) {
		case DATE:
			sprintf(outbuf, "[%s]", dbe_date_to_char(conn, isnull? 0 : read_date(v->buf), 0));
			break;
		case CHARCTR:
			sprintf(outbuf, "%.*s", (int)v->valspec.len, v->buf);
			break;
		case LOGIC:
			sprintf(outbuf, ".%c.", isnull? 'F' : (v->buf[0] ? 'T' : 'F'));
			break;
		case NUMBER: {
			char *str = dbe_num_to_char(conn, v, -1, -1);
			if (!str) str = "Num_to_char() failed!";
			strcpy(outbuf, str);
			break; }
		case ARRAYTYP: 
			if (!isnull) {
				int dim = v->valspec.len / sizeof(WORD);
				BYTE *a = v->buf;
				char *tp = outbuf;
				*tp++ = '['; *tp = '\0';
				while (dim-- > 1) {
					sprintf(tp, "%d,", get_word(a));
					a += sizeof(WORD);
					tp += strlen(tp);
				}
				sprintf(tp, "%d]", get_word(a));
				break;
			}
		case ADDR:
			if (!isnull) {
				sprintf(outbuf, "<@%03d>", get_word(v->buf));
				break;
			}
		default:
			strcpy(outbuf, "Unknown or invalid data type!");
	}
	return(outbuf);
}

int hb_prnt_val(HBCONN conn, HBVAL *v, int fchar)
{
	if (v->valspec.typ == MEMO && v->valspec.len) {
		if (fchar == -1) return(-1);
		char memobuf[BLKSIZE];
		memo_t *memo = (memo_t *)v->buf;
		blkno_t blkno = memo->blkno;
		int nbytes = 0;
		if (dbe_seek_file(conn, memo->fd, (off_t)blkno * BLKSIZE) == (off_t)-1) return(-1);
		do {
			int i;
			nbytes = dbe_read_file(conn, memo->fd, memobuf, BLKSIZE, 0);
			if (nbytes < 0) return(-1);
			for (i=0; i<nbytes; ++i) {
#ifdef HBCGI
				if (rptfp) fputc(memobuf[i], rptfp);
#else
				putchar(memobuf[i]);
#endif
			}
		} while (nbytes == BLKSIZE);
		if (fchar != -1) putchar(fchar);
	}
	else {
		char outbuf[STRBUFSZ];
		hb_val2str(conn, v, outbuf);
#ifdef HBCGI
		if (rptfp && fchar != -1) fprintf(rptfp, fchar? "%s%c":"%s", outbuf, fchar);
#else
		if (fchar != -1) hbscrn_printf(fchar? "%s%c":"%s", outbuf, fchar);
#endif
		else if (dbe_push_string(conn, outbuf, strlen(outbuf))) return(-1);
	}
	return(0);
}

