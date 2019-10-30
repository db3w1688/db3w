/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbcmd/usr.c
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
#include <fcntl.h>
#include <stdlib.h>
#define _XOPEN_SOURCE
#include <unistd.h>
#include <crypt.h>
#include <string.h>
#include "hb.h"
#include "hbapi.h"
#ifdef KW_MAIN
#undef KW_MAIN
#endif
#include "hbkwdef.h"
#define _HBCLIENT_
#include "hbusr.h"

extern int (*hb_usrfcn[])();

int hb_input(HBCONN conn, int mode)
{
	char ibuf[STRBUFSZ];
	int len;
	ibuf[0] = '\0';
	switch (0xff & mode) {
	case USR_GETC:
		hbscrn_gets(ibuf, STRBUFSZ, 0);
		if (dbe_push_string(conn, ibuf, 1)) return(-1);
		break;
	case USR_GETS:
	case USR_GETS0:
		hbscrn_gets(ibuf, STRBUFSZ, 1);
		len = strlen(ibuf);
		if (ibuf[len-1] == '\n') {
			ibuf[len-1] = '\0';
			--len;
		}
		if (dbe_push_string(conn, ibuf, mode == USR_GETS0? len + 1 : len)) return(-1);
		break;
	}
}

static int _create_tbl(HBCONN conn, TABLE *hdr)
{
	int sav_ctx = dbe_get_stat(conn, 0) - 1, rc = -1;
	if (sav_ctx < 0) return(-1);
	if (dbe_select(conn, AUX) < 0) goto end;
	if (dbe_use(conn, U_USE | U_CREATE | U_CHK_DUP | ((0x80 & hdr->id)? U_DBT : 0)) < 0) goto end;
	if (hb_put_hdr(conn, hdr) < 0) goto end;
	if (dbe_use(conn, 0) < 0) goto end;
	rc = 0;
end:
	if (dbe_select(conn, sav_ctx) < 0) rc = -1;
	return(rc);
}

static int badfn(char *fn)
{
	return(0);
}

#define  LTYPLEN     1
#define  DTYPLEN     8
#define  MTYPLEN     10

static BYTE iobuf[STRBUFSZ];

static int buf_readline(HBCONN conn, int fd, int *pos, char *buf, int max)
{
	int i;
	if (max > STRBUFSZ) max = STRBUFSZ;
	for (i=0; i<max-1; ++i) {
		if (*pos >= STRBUFSZ) {
			int len = dbe_read_file(conn, fd, iobuf, STRBUFSZ, 0);
			if (len < 0) return(-1);
			if (len < STRBUFSZ) iobuf[len] = EOF_MKR;
			*pos = 0;
		}
		if (iobuf[*pos] == EOF_MKR) break;
		if (iobuf[*pos] == '\r' && iobuf[*pos + 1] == '\n') ++(*pos); //DOS garbage, ignore
		if (iobuf[*pos] == '\n') {
			++(*pos);
			break;
		}
		buf[i] = iobuf[(*pos)++];
	}
	buf[i] = '\0';
	return(i);
}

#define MAXDATAOFFSET	0x10000

int hb_table_create(HBCONN conn, int mode)
{
	char buf[STRBUFSZ];
	TABLE *hdt = NULL;
	FIELD *ft;
	int finished, done, pos, exfd = -1, rc = -1;
	int byprec = 1;
	int fcount = 0;
	int doffset = -1;

	if (!(hdt = (TABLE *)malloc(MAXHDLEN))) {
		hb_error(conn, NOMEMORY, NULL, -1, -1);
		goto end;
	}
	memset(hdt, 0, MAXHDLEN);
	hdt->id = DB5_MKR; //allow only new db7 format to be created
	hdt->hdlen = HDRSZ;
	ft = hdt->fdlist;
	if (!mode) {
		while (1) {
			buf[0] = '\0';
			hbscrn_puts("Enter Table Name: ");
			if (hbscrn_gets(buf, STRBUFSZ, 1)) hbscrn_puts("Input timed out\n");
			if (!buf[0]) {
				hbscrn_puts("NO TABLE CREATED.\n");
				rc = 0;
				goto end;
			}
			if (badfn(buf)) hbscrn_puts("Bad name, try again!\n");
			else break;
		}
		if (dbe_push_string(conn, buf, strlen(buf))) goto end;
	}
	if (mode == 3) {
		doffset = dbe_pop_int(conn, 0, MAXDATAOFFSET, NULL);
		if (doffset < 0) goto end;
		--mode;
	}
	if (mode == 2) {
		if (doffset < 0) doffset = 0;
		exfd = dbe_open_file(conn, O_RDONLY, ".txt");
		if (exfd < 0) goto end;
		pos = STRBUFSZ;
	}
	finished = FALSE;
	while (!finished && byprec < MAXRECSZ && fcount < MAXFLD) {
		int maxfldlen = MAXRECSZ - byprec;
		done = FALSE;
		while (!done) {
			TOKEN tok;
			char typstr[8], fldname[FLDNAMELEN + 1];
			int i = 0;
			int j, typ;
			int width = 0;
			int w;
			if (exfd != -1) {
				if ((j = buf_readline(conn, exfd, &pos, buf, STRBUFSZ)) <= 0) {
					finished = 1;
					if (j < 0) {
						hb_error(conn, DBREAD, NULL, -1, -1);
						fcount = 0;
					}
					break;
				}
				if (dbe_isset(conn, TALK_ON)) hbscrn_printf("%s\n", buf);
			}
			else {
				buf[0] = '\0';
				hbscrn_printf("Field %03d ", fcount + 1);
				if (hbscrn_gets(buf, STRBUFSZ, 1)) {
					hbscrn_puts("Input timed out\n");
					finished = 1;
					fcount = 0;
					break;
				}
			}
			if (!buf[0]) { finished = TRUE; break; }
			if (dbe_cmd_init(conn, hb_uppcase(buf))) goto end;
			if (dbe_get_token(conn, buf, &tok)) goto end;
			if (tok.tok_typ != TID) {
				hbscrn_puts("BAD FIELD NAME!\n");
				continue;
			}
			memcpy(fldname, &buf[tok.tok_s], tok.tok_len);
			fldname[tok.tok_len] = '\0';
			for (j=0; j<fcount; ++j) {
				if (!strcmp(fldname, hdt->fdlist[j].name)) {
					hbscrn_puts("FIELD NAME ALREADY EXISTS!\n");
					break;
				}
			}
			if (j < fcount) continue;
			memcpy(ft->name, fldname, tok.tok_len);
			if (dbe_get_token(conn, buf, &tok)) goto end;
			if (tok.tok_typ == TCOMMA) dbe_get_token(conn, buf, &tok);
			if (maxfldlen > LTYPLEN) typstr[i++] = 'L';
			if (maxfldlen > DTYPLEN) typstr[i++] = 'D';
			if (maxfldlen > MTYPLEN) typstr[i++] = 'M';
			typstr[i++] = 'C'; typstr[i++] = 'N'; typstr[i++] = '\0';
			if ((tok.tok_typ != TID) || (tok.tok_len>1)) {
				hbscrn_puts("BAD FIELD TYPE!\n");
				continue;
			}
			typ = buf[tok.tok_s];
			if (!strchr(typstr, typ)) {
				hbscrn_puts("BAD FIELD TYPE!\n");
				continue;
			}
			ft->ftype = typ;
			if (typ=='L') width = LTYPLEN;
			else if (typ=='D') width = DTYPLEN;
			else if (typ=='M') { width = MTYPLEN; hdt->id |= 0x80; }
			if (dbe_get_token(conn, buf, &tok)) goto end;
			if (!width || tok.tok_typ) {
				if (tok.tok_typ == TCOMMA) dbe_get_token(conn, buf, &tok);
				if ((tok.tok_typ != TINT) || ((w = atoi(&buf[tok.tok_s])) <= 0)
				|| (w > maxfldlen) || (w > MAXSTRLEN)
				|| (typ == 'N' && (w > MAXDIGITS))
				|| (width && (w != width))) {
					hbscrn_puts("BAD FIELD WIDTH!\n");
					continue;
				}
				if (!width) width = w;
				dbe_get_token(conn, buf, &tok);
				if (tok.tok_typ == TCOMMA) dbe_get_token(conn, buf, &tok);
			}
			ft->tlength = width; byprec += width;
			ft->dlength = 0;
			if (!tok.tok_typ) done = TRUE;
			else {
				if (tok.tok_typ != TINT) {
					hbscrn_puts("BAD DECIMAL WIDTH!\n");
					continue;
				}
				if (((w = atoi(&buf[tok.tok_s])) < 0) || (w > ft->tlength - 1)) {
					hbscrn_puts("BAD DECIMAL WIDTH!\n");
					continue;
				}
				if (typ != 'N' && w > 0) {
					hbscrn_puts("DECIMAL WIDTH INPUT INVALID!\n");
					continue;
				}
				ft->dlength = w;
				done = TRUE;
			}
		}
		dbe_cmd_end(conn);
		if (!finished) {
			hdt->hdlen += FLDWD;
			++ft; ++fcount;
		}
	}
	hdt->byprec = byprec;
	hdt->fldcnt = fcount;
	hdt->reccnt = 0;
	if (!fcount) {
		if (dbe_isset(conn, TALK_ON)) hbscrn_puts("NO TABLE CREATED.\n");
		if (dbe_popstack(conn, NULL, 0)) goto end; /* get rid of file name */
		rc = 0;
	}
	else {
		if (hdt->fldcnt < MAXFLD) {
			ft->name[0] = EOH;
			hdt->hdlen++;
		}
		if (doffset < 0) {
			buf[0] = '\0';
			hbscrn_puts("Enter Data Offset: ");
			if (hbscrn_gets(buf, STRBUFSZ, 1)) hbscrn_puts("Input timed out\n");
			doffset = (int)strtol(buf, NULL, 0);
		}
		if (doffset <= hdt->hdlen) doffset = hdt->hdlen + META_HDR_ALIGN;
		else if (doffset > MAXDATAOFFSET) doffset = MAXDATAOFFSET;
		hbscrn_printf("Data Offset set to %d\n", doffset);
		hdt->doffset = doffset;
		if (!_create_tbl(conn, hdt)) rc = 0;
	}
end:
	if (exfd >= 0) dbe_close_file(conn, exfd);
	if (hdt) free(hdt);
	return(rc);
}

int hb_disprec(HBCONN conn, int argc)
{
	int rc = -1;
	argc &= 0x7f;
	if (dbe_iseof(conn)) { hbscrn_puts("[EOF]\n"); return(0); }
	if (dbe_isbof(conn)) { hbscrn_puts("[BOF]\n"); return(0); }
	hbscrn_printf("Record# %06lu", dbe_get_recno(conn));
	if (dbe_isdelete(conn)) hbscrn_puts("\t(Deleted)");
	hbscrn_puts("\n");
	if (argc) {
		int i = argc;
		do {
			char vbuf[STRBUFSZ], expr[STRBUFSZ];
			int errcode;
			HBVAL *v = (HBVAL *)vbuf;
			if (!dbe_peek_string(conn, i - 1, expr, STRBUFSZ)) return(-1);
			hbscrn_printf("         %s : ", expr);
			if (errcode = dbe_eval(conn, expr, 1)) {
				hbscrn_printf("Syntax error #%d!\n", errcode);
				break;
			}
			if (dbe_popstack(conn, vbuf, STRBUFSZ)) goto end;
			if (hb_prnt_val(conn, v,'\n')) goto end;
		} while (--i);
		if (dbe_clr_stack(conn, argc)) goto end;
		rc = 0;
	}
	else {
		FIELD *f;
		TABLE *hd = NULL;
		char *rec = NULL, *tp;
		int rec_sz, i;

		rec_sz = dbe_get_rec_sz(conn);
		if (rec_sz <= 0) goto end;
		rec = malloc(rec_sz);
		if (!rec || !(hd = hb_get_hdr(conn))) goto end;
		if (!dbe_get_rec(conn, tp = rec, rec_sz, 0)) goto end;
		++tp;
		for (i=0; i<hd->fldcnt; ++i) {
			f = &hd->fdlist[i];
			hbscrn_printf("  %*s : ",FLDNAMELEN, f->name);
			switch (f->ftype) {
			case 'N':
				hbscrn_printf("%.*s\n", f->tlength, tp);
				break;
			case 'C':
				hbscrn_printf("%.*s\n", f->tlength, tp);
				break;
			case 'D':
				hbscrn_printf("%.2s/%.2s/%.*s\n", tp+4, tp+6, 4, tp);
				break;
			case 'L':
				hbscrn_printf(".%c.\n", tp[0]);
				break;
			case 'M': {
				blkno_t blkno = (blkno_t)atol(tp);
				hbscrn_printf("%s%c", "<MEMO>" , blkno? ' ' : '\n');
				if (blkno) hbscrn_printf("BLK#%ld\n", blkno);
				break; }
			default:
				hbscrn_printf("Invalid or unsupported field type: %c\n", f->ftype);
				break;
			}
			tp += f->tlength;
		}
		rc = 0;
end:
		if (rec) free(rec);
		if (hd) free(hd);
	}
	hbscrn_puts("\n");
	return(rc);
}

#include <time.h>

int hb_dispstru(HBCONN conn)
{
	TABLE *hdr = NULL;
	FIELD *f;
	char *meta = NULL, *tp;
	int len, i, rc = -1;
	time_t ftime;
	struct tm *now;
	char buf[STRBUFSZ], *line = "----------------------------------------------------";


	if (!(hdr = hb_get_hdr(conn))) goto end;
	ftime = dbe_get_ftime(conn, FTIME_M);
	now = localtime(&ftime);
	hbscrn_printf("Context ID : %02d\n", dbe_get_stat(conn, STAT_CTX_CURR));
	hbscrn_printf("Table Name : %s\n", dbe_get_id(conn, ID_ALIAS, 0, buf, STRBUFSZ));
	hbscrn_printf("File: %s\n", dbe_get_id(conn, ID_DBF_FN, 0, buf, STRBUFSZ));
	hbscrn_printf("%s\nFld     Name            Type    Length  Dcml Offset\n%s\n", line, line);
	for (f=hdr->fdlist,i=0; i<hdr->fldcnt; ++f,++i)
		hbscrn_printf(" %02d     %-15s  %c        %3d   %3d  %4d\n",
			i + 1, f->name, f->ftype, f->tlength, f->dlength, f->offset);
	hbscrn_printf("%s\n*** Total characters per record : %d\n", line, hdr->byprec);
	hbscrn_printf("*** Total number of records : %ld\n", hdr->reccnt);
	hbscrn_printf("*** Data start offset : %ld\n", hdr->doffset);
	hbscrn_printf("*** Last modified : %d-%02d-%02d %02d:%02d:%02d %s\n", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, now->tm_zone);
	if (hdr->meta_len) {
		meta = malloc(hdr->meta_len);
		if (!meta) {
			hb_error(conn, NOMEMORY, NULL, -1, -1);
			goto end;
		}
		if (!dbe_get_meta_hdr(conn, meta, hdr->meta_len)) goto end;
		if (is_meta_ndx(meta)) {
			int cnt = meta_entry_cnt(meta);
			if (cnt > 0) hbscrn_printf("Index(es):\n");
			for (i=0, tp=meta_entry_start(meta); i<cnt; ++i, tp += META_ENTRY_LEN) {
				if (*tp == EOM) break;
				hbscrn_printf("\t%d->%s\n", i + 1, tp);
			}
		}
	}
	rc = 0;
end:
	if (hdr) free(hdr);
	if (meta) free(meta);
	return(rc);
}

char *hb_get_epasswd(char *salt)
{
	char passwd[100];
	if (!salt) return(NULL);
	if (hbscrn_getpasswd(passwd, 100)) {
		//return(NULL);
		hbscrn_puts("Input timed out\n");
		return("nologin");
	}
	return(crypt(passwd, salt));
}

#define MAX_PER_LINE	16

int hb_dispfil(HBCONN conn, int mode)
{
	int exfd, len = 0, iseof = 0, rc = -1;
	exfd = dbe_open_file(conn, O_RDONLY, NULL);
	if (exfd < 0) goto end;
	do {
		int i;
		len = dbe_read_file(conn, exfd, iobuf, STRBUFSZ, 1);
		if (len < 0) goto end;
		for (i=0; i<len;) {
			if (mode) { //hex or other
				char fmtstr[100], *tp = fmtstr, *tp2;
				int j;
				memset(fmtstr, ' ', 100);
				sprintf(tp, "%08xh:", i); tp += strlen(tp); tp2 = tp + 3 * MAX_PER_LINE;
				strcpy(tp2, " | "); tp2 += 3;
				for (j=0; j<MAX_PER_LINE; ++j) {
					if (i+j < len) {
						sprintf(tp, " %02x", iobuf[i + j]);
						tp += 3;
						tp2[j] = isprint(iobuf[i + j])? iobuf[i + j] : '.';
					}
					else break;
				}
				i += j; *tp = ' '; tp2[j] = '\0';
				hbscrn_printf("%s\n", fmtstr);
			}
			else { //ascii
				char c = iobuf[i];
				if (c == EOF_MKR) {
					len = i;
					iseof = 1;
					break;
				}
				if (isprint(c) || c == '\n' || c == '\t') putchar(c);
				else putchar('.');
				++i;
			}
		}
		if (!mode && len && iobuf[len - 1] != '\n') putchar('\n'); //in case last char outputed was not \n
	} while (!iseof && len == STRBUFSZ);
	rc = 0;
end:
	if (exfd >= 0) dbe_close_file(conn, exfd);
	return(rc);
}

int hb_expfil(HBCONN conn, int argc)
{
	int exfd = -1, local_fd = -1, len = 0, rc = -1;
	if (argc == 2) {
		char filename[FULFNLEN];
		if (!dbe_pop_string(conn, filename, FULFNLEN)) goto end;
		local_fd = open(filename, O_CREAT | O_RDWR, 0664);
		if (local_fd < 0) goto end;
	}
	else local_fd = STDOUT_FILENO;
	exfd = dbe_open_file(conn, O_RDONLY, NULL);
	if (exfd < 0) goto end;
	do {
		len = dbe_read_file(conn, exfd, iobuf, STRBUFSZ, 1);
		if (len < 0) goto end;
		if (write(local_fd, iobuf, len) != len) goto end;
	} while (len > 0);
	rc = 0;
end:
	if (local_fd != -1 && local_fd != STDOUT_FILENO) close(local_fd);
	if (exfd >= 0) dbe_close_file(conn, exfd);
	return(rc);
}

#include "dbe.h"

int hb_dispndx(HBCONN conn, int n)
{
	char vbuf[STRBUFSZ], iobuf[HBPAGESIZE], *tp;
	HBVAL *v = (HBVAL *)vbuf;
	NDX_HDR nhd;
	int ndxfd = -1, rc = -1, len, primary = !!(0x80 & n);
	if (!dbe_peek_stack(conn, 0, CHARCTR, vbuf, STRBUFSZ)) return(-1);
	hbscrn_printf("Index #%d%s:\n", (0x7f & n) + 1, primary? "*" : "");
	snprintf(iobuf, HBPAGESIZE, "%.*s", v->valspec.len, v->buf);
	hbscrn_printf("%20s: %s\n", "Filename", iobuf);
	ndxfd = dbe_open_file(conn, O_RDONLY, ".idx"); //".idx" should already be part of index name
	if (ndxfd < 0) goto end;
	len = dbe_read_file(conn, ndxfd, iobuf, HBPAGESIZE, 1);
	if (len != HBPAGESIZE) goto end;
	tp = iobuf;
	nhd.rootno = read_pageno(tp); tp += sizeof(pageno_t);
	nhd.pagecnt = read_pageno(tp); tp += sizeof(pageno_t);
	tp += sizeof(DWORD);
	nhd.keylen = read_word(tp); tp += sizeof(WORD);
	nhd.norecpp = read_word(tp); tp += sizeof(WORD);
	nhd.keytyp = read_word(tp); tp += sizeof(WORD);
	nhd.entry_size = read_word(tp); tp += sizeof(WORD);
	tp += sizeof(DWORD);
	strcpy(nhd.ndxexp, tp);
	hbscrn_printf("%20s: %s\n", "Key expression", nhd.ndxexp);
	hbscrn_printf("%20s: %c\n", "Key type", nhd.keytyp? 'N' : 'C');
	hbscrn_printf("%20s: %d\n", "Key length", nhd.keylen);
	hbscrn_printf("%20s: %d\n", "Page Size", HBPAGESIZE);
	hbscrn_printf("%20s: %ld\n", "Page count", nhd.pagecnt);
	hbscrn_printf("%20s: %ld\n", "Root page", nhd.rootno);
	rc = 0;
end:
	if (ndxfd >= 0) dbe_close_file(conn, ndxfd);
	return(rc);
}

extern int escape;

int usr(HBCONN conn, HBREGS *regs)
{
	char *bx = regs->bx;
	int ax = regs->ax, ex = regs->ex;
	int cx = regs->cx, rc;
	int dx = regs->dx;
	int fx = regs->fx;
	switch (ax) {
	case USR_FCN: {
		int fcn_no = usr_getfcn(conn);
		if (fcn_no<0) {
			regs->ax = UNDEF_FCN;
			return(-1);
		}
		return((*hb_usrfcn[fcn_no])(conn, ex));
		}
	case USR_PRNT_VAL:
		rc = hb_prnt_val(conn, (HBVAL *)bx, ex);
		regs->cx = 0;
		regs->fx = escape;
		return(rc);
	case USR_DISPREC:
		rc = hb_disprec(conn, ex);
		regs->fx = escape;
		return(rc);
	case USR_DISPSTR:
		return(hb_dispstru(conn));
	case USR_CREAT:
		return(hb_table_create(conn, ex));
	case USR_INPUT:
		return(hb_input(conn, ex));
	case USR_READ:
		hbscrn_printf("%s\n", bx);
		regs->cx = 0;
		return(0);
	case USR_GETACCESS:
		regs->ax = HBALL; //NEEDSWORK: implement access control
		return(0);
	case USR_AUTH: {
		char *epasswd = hb_get_epasswd(bx);
		if (!epasswd) {
			regs->cx = 0;
			return(-1);
		}
		regs->ax = strcmp(bx, epasswd);
		regs->cx = 0;
		return(0); }
	case USR_DISPFILE:
		return(hb_dispfil(conn, ex));
	case USR_EXPORT:
		return(hb_expfil(conn, ex));
	case USR_DISPNDX:
		return(hb_dispndx(conn, ex));
	default:
		break;
	}
	return(-1);
}

struct hb_kw hb_keywords[HBKWTABSZ] = { //HBKW_* must be in sequential order
"COMMANDS",		HBKW_CMDLST,
"DISPLAY",		HBKW_DISPLAY,
"FUNCTIONS",	HBKW_FCNLST,
"HELP",			HBKW_HELP,
"KEYWORDS", 	HBKW_KWLST,
"MEMORY",		HBKW_MEMORY,
"PROGRAMMING",	HBKW_CTLLST,
"QUIT",			HBKW_QUIT,
"RECORD",		HBKW_RECORD,
"RESUME",		HBKW_RESUME,
"SCOPE",		HBKW_SCOPE,
"SELECT",		HBKW_SELECT,
"SERIALIZE",	HBKW_SERIAL,
"SHELL",		HBKW_SHELL,
"SLEEP",		HBKW_SLEEP,
"STACK",		HBKW_STACK,
"STRUCTURE",	HBKW_STRUCTURE,
"TAKEOVER",		HBKW_TAKEOVER,
"WAIT",			HBKW_WAIT,
"WAKEUP",		HBKW_WAKEUP
};

int hb_namecmp(char *src, char *tgt, int len, int type)
{
	int i, d;
	if ((type == KEYWORD) && (len < 4)) type = ID_VNAME;
	for (i=0; i<len; ++i, ++src, ++tgt) {
		int t = *tgt;
		int s = *src;
		if (s == '*' || s == '?') return(-1); //src not allow to contain wildcard char
		if (type != ID_FNAME) { //not filename or alias
			s = c_upper(s);
			t = c_upper(t);
		}
		d = s - t;
		if (d) {
			if (type == ID_PARTIAL) {
				if (t == '?') continue;
				if (t == '*') {
					while ((t = *(++tgt)) == '*');
					if (!t) return(0);
					if (t == '?') continue;
					t = c_upper(t);
					while (s != t) s = c_upper(*(++src));
					continue;
				}
			}
			return(d);
		}
	}
	return(type == ID_VNAME ? *src : 0);
}

static int kw_bin_search(char *kw, int len, int start, int end)
{
	if (start > end) return(KW_UNKNOWN);
	{
	int mid = (start + end) / 2;
	int i = hb_namecmp(hb_keywords[mid].name, kw, len, KEYWORD);
/*
printf("%s %d %s\n",hb_keywords[mid].name,hb_keywords[mid].kw_val,kw);
*/
	return(!i ? hb_keywords[mid].kw_val : 
				(i > 0 ? kw_bin_search(kw, len, start, mid - 1) : 
						kw_bin_search(kw, len, mid + 1, end)));
	}
}

int hb_kw_search(TOKEN *tok, char *cmd_line)
{
	int c;
	char cmd_keyword[MAXHBKWLEN];
	if (tok->tok_len >= MAXHBKWLEN) return(KW_UNKNOWN);
	c = cmd_line[tok->tok_s + tok->tok_len];
	if (c && !isspace(c)) {
		++tok->tok_len;
		return(KW_UNKNOWN);
	}
	sprintf(cmd_keyword, "%.*s", tok->tok_len, cmd_line + tok->tok_s);
	return(kw_bin_search(cmd_keyword, tok->tok_len, 0, HBKWTABSZ - 1));
}

