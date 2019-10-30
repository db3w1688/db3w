/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/hbapi.c
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
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "hb.h"
#include "hbpcode.h"
#include "hbapi.h"
#include "hbtli.h"

int dbe_apblank(HBCONN conn, int inc)
{
	HBREGS regs;

	regs.ax = DBE_APBLANK;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (inc) return(dbe_store_rec(conn));
}

int dbe_char_to_num(HBCONN conn, HBVAL *v)
{
	HBREGS regs;

	regs.ax = DBE_CHAR_TO_NUM;
	regs.bx = (char *)v;
	regs.cx = v->valspec.len + 2;
	return(hb_int(conn, &regs));
}

int dbe_check_open(HBCONN conn, char *fn)
{
	HBREGS regs;

	if (!fn) return(-1);
	regs.ax = DBE_CHECK_OPEN;
	regs.bx = fn;
	regs.cx = strlen(fn) + 1;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_delete_file(HBCONN conn, char *fn)
{
	HBREGS regs;

	if (!fn) return(-1);
	regs.ax = DBE_DEL_FILE;
	regs.bx = fn;
	regs.cx = strlen(fn);
	return (hb_int(conn, &regs));
}

int dbe_rename_file(HBCONN conn, char *srcfn, char *tgtfn, char *user)
{
	HBREGS regs;
	
	if (!srcfn || !tgtfn) return(-1);
	dbe_push_string(conn, srcfn, strlen(srcfn));
	dbe_push_string(conn, tgtfn, strlen(tgtfn));
	if (user) dbe_push_string(conn, user, strlen(user));
	regs.ax = DBE_REN_FILE;
	regs.cx = 0;
	regs.dx = user? 3 : 2;
	return(hb_int(conn, &regs));
}


int dbe_clr_stack(HBCONN conn, int n)
{
	HBREGS regs;

	regs.ax = DBE_STACK_OP;
	regs.cx = 0;
	regs.ex = STACK_RM_MASK | (n - 1);
	return(hb_int(conn, &regs));
}

int dbe_cmd_end(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_CMD_END;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_cmd0(HBCONN conn, char *cmd_ln)
{
	HBREGS regs;

	regs.ax = DBE_CMD;
	regs.cx = strlen(cmd_ln) + 1;	/** include NULL byte **/
	regs.bx = cmd_ln;
	if (hb_int(conn, &regs)) return(-1);
	return(0);
}

int dbe_cmd(HBCONN conn, char *cmd_ln)
{
	if (dbe_cmd0(conn, cmd_ln)) return(-1);
	return(dbe_cmd_end(conn));
}

int dbe_cmd_init(HBCONN conn, char *comm_line)
{
	HBREGS regs;

	regs.ax = DBE_CMD_INIT;
	regs.bx = comm_line;
	regs.cx = strlen(comm_line) + 1;
	return(hb_int(conn, &regs));
}

int dbe_codegen(HBCONN conn, int opcode,int opnd)
{
	HBREGS regs;

	regs.ax = DBE_CODEGEN;
	regs.cx = 0;
	regs.dx = MAKE_DWORD(opcode, opnd);
	return(hb_int(conn, &regs));
}

int hb_create_tbl(HBCONN conn, TABLE *hdr)
{
	int sav_ctx = dbe_get_stat(conn, STAT_CTX_CURR) - 1, rc = -1;
	if (sav_ctx < 0) return(-1);
	if (dbe_select(conn, AUX) < 0) goto end;
	if (dbe_use(conn, U_USE | U_CREATE | U_EXCL | ((0x80 & hdr->id)? U_DBT : 0)) < 0) goto end;
	if (hb_put_hdr(conn, hdr) < 0) goto end;
	if (dbe_use(conn, 0) < 0) goto end;
	rc = 0;
end:
	if (dbe_select(conn, sav_ctx)) rc = -1;
	return(rc);
}

char *dbe_date_to_char(HBCONN conn, date_t d, int is_dbdate)
{
	HBREGS regs;
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	
	v->valspec.typ = DATE;
	v->valspec.len = sizeof(date_t);
	write_date(v->buf, d);
	regs.ax = DBE_DATE_TO_CHAR;
	regs.bx = (char *)v;
	regs.cx = v->valspec.len + 2;
	regs.ex = is_dbdate;
	if (hb_int(conn, &regs) || (regs.cx <= 0)) return(NULL);
	return(regs.bx); //pointing to transfer buffer
}

int dbe_emit(HBCONN conn, char *b, int size)
{
	HBREGS regs;

	regs.ax = DBE_EMIT;
	regs.bx = b;
	regs.cx = 0;
	regs.ex = size;
	return(hb_int(conn, &regs));
}

int dbe_eval(HBCONN conn, char *exp_buf, int catch_error)
{
	HBREGS regs;

	regs.ax = DBE_EVALUATE;
	regs.bx = exp_buf;
	regs.cx = strlen(exp_buf) + 1;	/** include the null byte **/
	regs.ex = catch_error;
	if (hb_int(conn, &regs)) return(-1); //catch_error is 0
	return(regs.ax);
}

int dbe_execute(HBCONN conn, char *pbuf)
{
	HBREGS regs;

	regs.ax = DBE_PCODE_EXE;
	regs.bx = pbuf;
	regs.cx = pbuf[0];
	return(hb_int(conn, &regs));
}

int dbe_expression(HBCONN conn, TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_EXPRESSION;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	return(0);
}

int dbe_flock(HBCONN conn, int exclusive)
{
	HBREGS regs;

	regs.ax = DBE_FLOCK;
	regs.cx = 0;
	regs.ex = exclusive;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_get_command(HBCONN conn, TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_GET_COMMAND;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = TID;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	return(regs.ax);
}

int dbe_get_memo_fd(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_DBT_FD;
	regs.cx = 0;
	if (hb_int(conn, &regs) || regs.ax == 0xffff) return(-1);
	return(regs.ax);
}

char *dbe_get_dirent(HBCONN conn, int firsttime,int type,char *buf)
{
	HBREGS regs;

	regs.ax = DBE_FILE_DIR;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = type;
	regs.ex = !!firsttime;
	if (hb_int(conn, &regs)) return(NULL);
	return(regs.cx? buf : NULL);
}

int dbe_get_fcount(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_FLDCNT;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_get_fieldno(HBCONN conn, char *name)
{
	HBREGS regs;

	dbe_push_string(conn, name, strlen(name));	/** null byte omitted **/
	regs.ax = DBE_GET_FLDNO;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax+1);
}

char *dbe_get_filter(HBCONN conn, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_GET_FILTER;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = bufsz;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf);
}

char *dbe_get_hdr(HBCONN conn, char *buf, int buf_size)
{
	HBREGS regs;

	regs.ax = DBE_GET_HDR;
	regs.cx = 0;
	regs.bx = buf;
	regs.dx = buf_size;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf);
}

int dbe_get_hdr_len(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_HDRLEN;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

char *dbe_get_meta_hdr(HBCONN conn, char *buf, int buf_size)
{
	HBREGS regs;

	regs.ax = DBE_GET_META_HDR;
	regs.cx = 0;
	regs.bx = buf;
	regs.dx = buf_size;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf);
}

int dbe_get_meta_len(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_META_LEN;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

char *dbe_get_id(HBCONN conn, int what, int n, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_GET_ID;
	regs.cx = 0;
	regs.bx = buf;
	regs.dx = bufsz;
	regs.ex = what;
	regs.fx = n;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf);
}

char *dbe_get_locate_exp(HBCONN conn, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_GET_LOCATE_EXP;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = bufsz;
	if (hb_int(conn, &regs)) return(NULL);
	return(buf);
}

/** TRANSBUFSZ is used in reading and writing to avoid unnecessary fragmentation **/

blkno_t dbe_get_memo(HBCONN conn, char *fldname, char *memobuf, size_t bufsz)
{
	int memofd, n, stop = 0;
	blkno_t blkno;
	size_t total = 0;
	if (dbe_eval(conn, fldname, 1)) return((blkno_t)-1);
	if (dbe_pop_memo(conn, &memofd, &blkno)) return((blkno_t)-1);
	if (dbe_seek_file(conn, memofd, (off_t)blkno*BLKSIZE) == (off_t)-1) return((blkno_t)-1);
	do {
		if (total + TRANSBUFSZ > bufsz) return((blkno_t)-1);
		n = dbe_read_file(conn, memofd, memobuf + total, TRANSBUFSZ, 0); //text file, EOF_MKR marks the end
		if (n < 0) return((blkno_t)-1);
		if (n < TRANSBUFSZ) stop = 1;
		total += n;
	} while (!stop && total < MAXMEMOSZ);
	return((total + BLKSIZE - 1) / BLKSIZE);
}

char *dbe_get_ndx_exp(HBCONN conn, char *ndxname, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_GET_NDX_EXPR;
	regs.bx = ndxname;
	regs.cx = strlen(ndxname) + 1;
	regs.ex = NDX_EXP;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	if (regs.cx > bufsz) return(NULL);
	strcpy(buf, regs.bx);
	return(buf);
}

int dbe_get_ndx_fd(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_NDX_STAT;
	regs.cx = 0;
	regs.ex = NDX_FD;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

char *dbe_get_ndx_filename(HBCONN conn, char *buf)
{
	HBREGS regs;

	regs.ax = DBE_GET_NDX_STAT;
	regs.bx = buf;
	regs.cx = 0;
	regs.ex = NDX_FN;
	if (hb_int(conn, &regs)) return(NULL);
	return(buf);
}

char *dbe_get_ndx_key(HBCONN conn, char *buf)
{
	HBREGS regs;

	memset(buf, ' ', MAXKEYSZ);
	regs.ax = DBE_GET_NDX_KEY;
	regs.bx = buf;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(NULL);
	return(buf);
}

char *dbe_get_ndx_name(HBCONN conn, int n, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_GET_NDX;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = n;
	regs.ex = 0;
	regs.fx = bufsz;
	if ( hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf);
}

int dbe_get_ndx_type(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_NDX_STAT;
	regs.cx = 0;
	regs.ex = NDX_TYPE;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

blkno_t dbe_get_next_memo(HBCONN conn, int memofd)
{
	blkno_t blkno;
	dbe_seek_file(conn, memofd, (off_t)0);
	if (dbe_read_file(conn, memofd, (char *)&blkno, sizeof(blkno_t), 1) != sizeof(blkno_t)) return((blkno_t)-1);
	return(read_blkno(&blkno));
}

int dbe_get_nextcode(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_NEXTCODE;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

char *dbe_get_path(HBCONN conn, int mode, char *buf, int bufsz)
{
	if (bufsz < 0) return(NULL);
	HBREGS regs;
	regs.ax = DBE_GET_PATH;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = bufsz;
	regs.ex = mode;
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	return(buf); //buf is null terminated
}

recno_t dbe_get_reccnt(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_RECCNT;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return((recno_t)-1);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

char *dbe_get_rec(HBCONN conn, char *buf, int bufsize, int skip)
/* 1==FORWARD, 2==BACKWARD */
{
	HBREGS regs;

	regs.ax = DBE_GET_REC;
	regs.cx = 0;
	regs.bx = buf;
	regs.dx = bufsize;
	regs.ex = skip;
	if (hb_int(conn, &regs)) return(NULL);
	if (regs.cx) return(buf);
	else return(NULL);
}

int dbe_get_rec_batch(HBCONN conn, char *buf, int n, int dir, int recsiz, char *rec_arr[], recno_t recno_arr[])
{
	HBREGS regs;
	regs.ax = DBE_GET_REC_BATCH;
	regs.cx = 0;
	regs.bx = buf;
	regs.dx = n;
	regs.ex = dir;
	if (hb_int(conn, &regs)) return(-1);
	if (regs.cx>0) {
		char *rp;
		for (n=0,rp=buf; n<(int)regs.dx; ++n,rp+=recsiz+sizeof(recno_t)) {
			recno_arr[n] = get_recno(rp);
			rec_arr[n] = rp + sizeof(recno_t);
		}
		return(n);
	}
	return(0);
}

recno_t dbe_get_recno(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_RECNO;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return((recno_t)-1);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

int dbe_get_rec_sz(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_RECSZ;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_get_stat(HBCONN conn, int what)
{
	HBREGS regs;

	regs.ax = DBE_GET_STAT;
	regs.cx = 0;
	regs.ex = what;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_get_token(HBCONN conn, char *comm_line,TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_GET_TOKEN;
	regs.bx = comm_line;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	if (comm_line && regs.cx) strcpy(&comm_line[LO_DWORD(regs.dx)],regs.bx);
	return(0);
}

int dbe_get_tok(HBCONN conn, char *comm_line,TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_GET_TOK;
	regs.bx = comm_line;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	if (comm_line && regs.cx) strcpy(&comm_line[LO_DWORD(regs.dx)],regs.bx);
	return(0);
}

int dbe_get_wdeci(HBCONN conn, int *width, int *deci)
{
	HBREGS regs;

	regs.ax = DBE_GET_WDECI;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	*width = LO_DWORD(regs.dx);
	*deci = HI_DWORD(regs.dx);
	return(0);
}

int dbe_set_wdeci(HBCONN conn, int width, int deci)
{
	HBREGS regs;
	
	regs.ax = DBE_SET_WDECI;
	regs.cx = 0;
	regs.dx = MAKE_DWORD(width, deci);
	return(hb_int(conn, &regs));
}

int dbe_go(HBCONN conn, recno_t n)
{
	HBREGS regs;

	regs.ax = DBE_GO;
	regs.cx = 0;
	regs.dx = LO_LWORD(n);
	regs.fx = HI_LWORD(n);
	regs.ex = 1;
	return(hb_int(conn, &regs));
}

int dbe_go_bottom(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GO_BOT;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_go_top(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GO_TOP;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_gorec(HBCONN conn, recno_t n)
{
	HBREGS regs;

	regs.ax = DBE_GO;
	regs.cx = 0;
	regs.dx = LO_LWORD(n);
	regs.fx = HI_LWORD(n);
	regs.ex = 0;
	return(hb_int(conn, &regs));
}

int dbe_hb_cmd(HBCONN conn, int cmd,char *pbuf, TOKEN *tok)
{
	HBREGS regs;
	char dbe_pbuf[4986]; //PBUFSZ

	regs.ax = DBE_CMD2;
	regs.bx = dbe_pbuf;
	regs.cx = 0;
	regs.ex = cmd;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	if (pbuf) memcpy(pbuf, dbe_pbuf, regs.cx);
	return(0);
}

int dbe_index(HBCONN conn, char *ndxfn, char *ndxexp)
{
	HBREGS regs;
	char tbuf[256];
	int len = strlen(ndxexp);
	strcpy(tbuf,ndxexp);
	strcpy(tbuf+len+1,ndxfn);
	regs.ax = DBE_INDEX;
	regs.bx = tbuf;
	regs.cx = len + strlen(ndxfn) + 2;
	return(hb_int(conn, &regs));
}

#ifdef SHARE_FD
int dbe_is_only_copy(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_ONLYCOPY;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}
#endif

int dbe_isdelete(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_DELETE;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_iseof(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_EOF;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(1);
	return(regs.ax);
}

int dbe_isreadonly(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_READONLY;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_isset(HBCONN conn, int what)
{
	HBREGS regs;

	regs.ax = DBE_IS_SET;
	regs.cx = 0;
	regs.dx = what;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_isbof(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_BOF;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(1);
	return(regs.ax);
}

int dbe_istrue(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_TRUE;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_kw_search(HBCONN conn, int *kw_val)
{
	HBREGS regs;

	regs.ax = DBE_KWSEARCH;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (kw_val) *kw_val = regs.ex;
	return(regs.ax);
}

int dbe_lexload(HBCONN conn, int typ, int x, TOKEN *tok)
{
	HBREGS regs;
	int pdata;

	regs.ax = DBE_LEXLOAD;
	regs.cx = 0;
	regs.dx = x;
	regs.ex = typ;
	if (hb_int(conn, &regs)) return(-1);
	pdata = regs.ax;
	if (typ==L_EXPR) dbe_sync_token(conn, tok);
	return(pdata);
}

int dbe_locate(HBCONN conn, char *loc_cond, int dir)
{
	HBREGS regs;

	regs.ax = DBE_LOCATE;
	if (loc_cond) {
		regs.bx = loc_cond;
		regs.cx = strlen(loc_cond);
		if (!regs.cx) regs.ex = 0; //clear locate
	}
	else { //continue locate
		if (dir != FORWARD && dir != BACKWARD) return(-1);
		regs.cx = 0;
		regs.ex = 0x80 | dir;
	}
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_mark_rec(HBCONN conn, int mark) /* '*' for deletion, ' ' for recalling a deleted record */
{
	HBREGS regs;
	regs.ax = DBE_DELRCAL;
	regs.cx = 0;
	regs.ex = mark;
	if (hb_int(conn, &regs)) return(-1);
	if (mark == '*' && dbe_isset(conn, DELETE_ON)) {
		dbe_skip(conn, 1);
		if (dbe_iseof(conn)) dbe_go_bottom(conn);
	}
	return(0);
}

int dbe_mod_code(HBCONN conn, int which, int what)
{
	HBREGS regs;

	regs.ax = DBE_MOD_CODE;
	regs.cx = 0;
	regs.ex = which;
	regs.dx = what;
	return(hb_int(conn, &regs));
}

char *dbe_num_to_char(HBCONN conn, HBVAL *v, int len, int deci)
{
	HBREGS regs;

	regs.ax = DBE_NUM_TO_CHAR;
	regs.bx = (char *)v;
	regs.cx = v->numspec.len + 2;
	regs.dx = MAKE_DWORD(len, deci);
	if (hb_int(conn, &regs) || (regs.cx <= 0)) return(NULL);
	return(regs.bx); //points to internal buffer
}

int dbe_popstack(HBCONN conn, char *buf, int bufsz)
{
	HBREGS regs;

	regs.ax = DBE_STACK_OP;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = bufsz;
	regs.ex = buf? STACK_RM_MASK | STACK_LVL_MASK : STACK_RM_MASK;
	if (hb_int(conn, &regs) || !regs.cx) return(-1);
	return(0);
}

HBVAL *dbe_pop_stack(HBCONN conn, int type, char *buf, int bufsz)
{
	HBVAL *v = (HBVAL *)buf;
	if (!buf) return(NULL);
	if (dbe_popstack(conn, buf, bufsz)) return(NULL);
	if ((type != DONTCARE) && (v->valspec.typ != type)) {
		hb_error(conn, INVALID_TYPE, NULL, -1, -1);
		return(NULL);
	}
	return(v);
}

HBVAL *dbe_peek_stack(HBCONN conn, int lvl, int type, char *buf, int bufsz)
{
	HBREGS regs;
	HBVAL *v = (HBVAL *)buf;

	if (!buf) return(NULL);
	regs.ax = DBE_STACK_OP;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = bufsz;
	regs.ex = STACK_LVL_MASK | (N_MASK & lvl);
	if (hb_int(conn, &regs) || !regs.cx) return(NULL);
	if ((type != DONTCARE) && (v->valspec.typ != type)) {
		hb_error(conn, INVALID_TYPE, NULL, -1, -1);
		return(NULL);
	}
	return(v);
}

char *dbe_pop_string(HBCONN conn, char *buf, int bufsz)
{
	char vbuf[STRBUFSZ];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_pop_stack(conn, CHARCTR, vbuf, STRBUFSZ)) return(NULL);
	if (v->valspec.len + 1 > bufsz) return(NULL);
	sprintf(buf, "%.*s", v->valspec.len, v->buf);
	return(buf);
}

char *dbe_peek_string(HBCONN conn, int lvl, char *buf, int bufsz)
{
	char vbuf[STRBUFSZ];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_peek_stack(conn, lvl, CHARCTR, vbuf, STRBUFSZ)) return(NULL);
	if (v->valspec.len + 1 > bufsz) return(NULL);
	sprintf(buf, "%.*s", v->valspec.len, v->buf);
	return(buf);
}

int dbe_pop_int(HBCONN conn, int low, int high, int *err)
{
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	int i = 0, ok = 0;
	if (dbe_pop_stack(conn, NUMBER, vbuf, MAXDIGITS)) {
		if (val_is_integer(v)) {
			i = (int)read_long(v->buf);
			if ((i >= low) && (i <= high)) ok = 1;
		}
	}
	if (!ok) {
		if (err) *err = 1;
		return(-1);
	}
	if (err) *err = 0;
	return(i);
}

int dbe_peek_int(HBCONN conn, int lvl, int low, int high, int *err)
{
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	int i = 0, ok = 0;
	if (dbe_peek_stack(conn, lvl, NUMBER, vbuf, MAXDIGITS)) {
		if (val_is_integer(v)) {
			i = (int)read_long(v->buf);
			if ((i >= low) && (i <= high)) ok = 1;
		}
	}
	if (!ok) {
		if (err) *err = 1;
		return(-1);
	}
	if (err) *err = 0;
	return(i);
}

date_t dbe_pop_date(HBCONN conn)
{
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_pop_stack(conn, DATE, vbuf, MAXDIGITS)) return(-1);
	return(read_date(v->buf));
}

date_t dbe_peek_date(HBCONN conn, int lvl)
{
	char vbuf[64];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_peek_stack(conn, lvl, DATE, vbuf, 64)) return(-1);
	return(read_date(v->buf));
}

int dbe_pop_logic(HBCONN conn)
{
	char vbuf[16];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_pop_stack(conn, LOGIC, vbuf, 16)) return(-1);
	return(!!v->buf[0]);
}

int dbe_peek_logic(HBCONN conn, int lvl)
{
	char vbuf[16];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_peek_stack(conn, lvl, LOGIC, vbuf, 16)) return(-1);
	return(!!v->buf[0]);
}

double dbe_pop_real(HBCONN conn)
{
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_pop_stack(conn, NUMBER, vbuf, MAXDIGITS)) return((double)-1);
	if (val_is_integer(v)) return((double)read_long(v->buf));
	if (val_is_real(v)) return(read_double(v->buf));
	return((double)read_long(v->buf) / power10(v->numspec.deci));
}

double dbe_peek_real(HBCONN conn, int lvl)
{
	char vbuf[MAXDIGITS];
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_peek_stack(conn, lvl, NUMBER, vbuf, MAXDIGITS)) return((double)0);
	if (val_is_integer(v)) return((double)read_long(v->buf));
	if (val_is_real(v)) return(read_double(v->buf));
	return((double)read_long(v->buf) / power10(v->numspec.deci));
}

int dbe_pop_memo(HBCONN conn, int *fdp, blkno_t *start_blkno)
{
	char vbuf[MAXDIGITS];
	memo_t *memo;
	HBVAL *v = (HBVAL *)vbuf;
	if (!dbe_pop_stack(conn, MEMO, vbuf, MAXDIGITS)) return(-1);
	memo = (memo_t *)v->buf;
	*fdp = (int)read_dword(&memo->fd);
	*start_blkno = read_long(&memo->blkno);
	return(0);
}

int dbe_proc_level(HBCONN conn, int lvl)
{
	HBREGS regs;

	regs.ax = DBE_PROC_LEVEL;
	regs.cx = 0;
	regs.ex = lvl;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_push_logic(HBCONN conn, int tf)
{
	HBREGS regs;
	regs.ax = DBE_STACK_OP;
	regs.cx = 0;
	regs.cx = PUSH_LOGIC | (!!tf);
	return(hb_int(conn, &regs));
}

int dbe_push_date(HBCONN conn, date_t d)
{
	HBREGS regs;
	regs.ax = DBE_STACK_OP;
	regs.cx = 0;
	regs.dx = d; //NEEDSWORK: date_t could be long
	regs.ex = PUSH_DATE;
	return(hb_int(conn, &regs));
}

int dbe_push_int(HBCONN conn, int i)
{
	HBREGS regs;
	regs.ax = DBE_STACK_OP;
	regs.cx = 0;
	regs.dx = i;
	regs.ex = PUSH_INT;
	return(hb_int(conn, &regs));
}

int dbe_push_real(HBCONN conn, double r)
{
	HBREGS regs;
	char buf[sizeof(double)];
	write_double(buf,r);
	regs.ax = DBE_STACK_OP;
	regs.bx = buf;
	regs.cx = sizeof(double);
	regs.ex = PUSH_REAL;
	return(hb_int(conn, &regs));
}

int dbe_push_string(HBCONN conn, char *str, int len)
{
	HBREGS regs;

	regs.ax = DBE_STACK_OP;
	regs.bx = str;
	regs.cx = len;
	regs.ex = PUSH_STRING;
	return(hb_int(conn, &regs));
}

int dbe_put_hdr(HBCONN conn, char *buf, int len)
{
	HBREGS regs;

	regs.ax = DBE_PUT_HDR;
	regs.cx = 0; //transfer initiated by server which sets cx to -dx
	regs.bx = buf;
	regs.dx = len;
	if (hb_int(conn, &regs)) return(-1);
	regs.ax = DBE_READ_HDR;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

blkno_t dbe_put_memo(HBCONN conn, char *memobuf, int size, int memofd, blkno_t blkno)
{
	size_t total = 0;
	if (dbe_seek_file(conn, memofd, blkno * BLKSIZE) == (off_t)-1) return(-1);
	while (size > 0) {
		if (size < TRANSBUFSZ) {
			if (memobuf[total + size - 1] != 0x1a) memobuf[total + size] = 0x1a;
		}
		if (dbe_write_file(conn, memofd, memobuf + total, TRANSBUFSZ) != TRANSBUFSZ) return(-1);
		total += TRANSBUFSZ;
		size -= TRANSBUFSZ;
	}
	return((blkno_t)total / BLKSIZE);
}	

int dbe_put_next_memo(HBCONN conn, int memofd, blkno_t blkno)
{
	blkno_t blkno2;
	write_blkno(&blkno2, blkno);
	if (dbe_seek_file(conn, memofd, (off_t)0)) return(-1);
	if (dbe_write_file(conn, memofd, (char *)&blkno2, sizeof(blkno_t)) != sizeof(blkno_t)) return(-1);
	return(0);
}

int dbe_put_rec(HBCONN conn, char *buf, int rlock)
{
	HBREGS regs;

	regs.ax = DBE_PUT_REC;
	regs.cx = 0;
	regs.bx = buf;
	regs.ex = rlock;
	return(hb_int(conn, &regs));
}

int dbe_read_file(HBCONN conn, int fd, char *buf, int nbytes, int isbin)
{ //NEEDSWORK: buf size check, nbytes should be size_t
	HBREGS regs;

	regs.ax = DBE_FILE_READ;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = nbytes;
	regs.ex = fd;
	regs.fx = !!isbin;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax); //can also be -1 indicating failure in read() on server side
}

int dbe_isfound(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_IS_FOUND;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_rec_mark(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_REC_MARK;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_reindex(HBCONN conn)
{
	HBREGS regs;
	regs.ax = DBE_INDEX;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_rlock(HBCONN conn, int exclusive)
{
	HBREGS regs;

	regs.ax = DBE_RLOCK;
	regs.cx = 0;
	regs.ex = exclusive;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_rw_ndx_hdr(HBCONN conn, int mode, char *buf)  /* mode 0: read, 1: write */
{
	HBREGS regs;

	regs.ax = DBE_RW_NDX_HDR;
	regs.bx = buf;
	regs.cx = 0; //write is initiated by server which will set it with a negative value
	regs.ex = mode;
	return(hb_int(conn, &regs));
}

off_t dbe_seek_file(HBCONN conn, int fd, off_t pos)
{
	HBREGS regs;

	regs.ax = DBE_FILE_SEEK;
	regs.cx = 0;
	regs.dx = LO_LWORD(pos);
	regs.fx = HI_LWORD(pos);
	regs.ex = fd;
	if (hb_int(conn, &regs)) return(-1);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

int dbe_select(HBCONN conn, int ctx)
{
	HBREGS regs;

	regs.ax = DBE_SELECT;
	regs.cx = 0;
	regs.ex = ctx;
	return(hb_int(conn, &regs));
}

static int dbe_set(HBCONN conn, int what, int val)
{
	HBREGS regs;

	regs.ax = DBE_SET;
	regs.cx = 0;
	regs.dx = what;
	regs.ex = val; //1:ON 0:OFF or n for number of items on the stack, e.g. set index to ndx1,ndx2,ndx3, ndx1..ndx3 are on the stack and ex has 3
	return(hb_int(conn, &regs));
}

int dbe_set_talk(HBCONN conn, int onoff)
{
	return(dbe_set(conn, TALK_ON, onoff));
}

int dbe_set_datefmt(HBCONN conn, char *fmt)
{
	dbe_push_string(conn, fmt, strlen(fmt));
	return(dbe_set(conn, DATEFMT_TO, 1));
}

int dbe_set_filter(HBCONN conn, char *fltr)
{
	int n = 0;
	if (fltr) {
		int len = strlen(fltr);
		if (len) {
			if (dbe_push_string(conn, fltr, len)) return(-1);
			++n;
		}
	}
	return(dbe_set(conn, FILTER_TO, n));
}

int dbe_set_ndx(HBCONN conn, char *ndxname)
{
	int n = 0, len;
	if (ndxname) {
		for (; n < MAXNDX && (len=strlen(ndxname)); ++n,ndxname+=len+1) {
			if (dbe_push_string(conn, ndxname, len)) return(-1);
		}
	}
	return(dbe_set(conn, INDEX_TO, n));
}

int dbe_set_order(HBCONN conn, int i)
{
	return(dbe_set(conn, ORDER_TO, i));
}

int dbe_set_path(HBCONN conn, char *path)
{
	int n = 0;
	if (path) {
		int len = strlen(path);
		if (len) {
			if (dbe_push_string(conn, path, len)) return(-1);
			++n;
		}
	}
	return(dbe_set(conn, PATH_TO, n));
}

int dbe_set_relation(HBCONN conn, char *rel, int ctx)
{
	int n = 0;
	if (rel) {
		int len = strlen(rel);
		if (len) {
			if (dbe_push_string(conn, rel, len)) return(-1);
			if (dbe_push_int(conn, ctx)) return(-1);
			++n;
		}
	}
	return(dbe_set(conn, RELA_TO, n));
}

recno_t dbe_skip(HBCONN conn, int64_t n)
{
	HBREGS regs;

	if (!n) return((recno_t)0);
	regs.ax = DBE_SKIP;
	regs.cx = 0;
	regs.dx = LO_LWORD(n);
	regs.fx = HI_LWORD(n);
	if (hb_int(conn, &regs)) return(-1);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

int dbe_stackempty(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_STACK_OP;
	regs.cx = 0;
	regs.dx = 0;
	regs.ex = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_store_rec(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_STORE_REC;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(0);
	return(regs.ax);
}

int dbe_sync_token(HBCONN conn, TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_SYNC_TOKEN;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	return(0);
}

int dbe_sys_reset(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_RESET;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_unget_token(HBCONN conn, TOKEN *tok)
{
	HBREGS regs;

	regs.ax = DBE_UNGET_TOKEN;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	if (tok) {
		tok->tok_typ = regs.ax;
		tok->tok_s = LO_DWORD(regs.dx);
		tok->tok_len = HI_DWORD(regs.dx);
	}
	return(0);
}

int dbe_unlock(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_UNLOCK;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_use(HBCONN conn, int mode)
{
	HBREGS regs;

	regs.ax = DBE_USE;
	regs.cx = 0;
	regs.ex = mode;
	return(hb_int(conn, &regs));
}

int dbe_write_file(HBCONN conn, int fd,  char *buf, int nbytes)
{ //NEEDSWORK: nbytes should be size_t
	int n;
	int written = 0;
	HBREGS regs;
	regs.bx = buf;
	regs.cx = n = nbytes > TRANSBUFSZ? TRANSBUFSZ : nbytes;
	regs.ex = fd;
	while (nbytes) {
		regs.ax = DBE_FILE_WRITE;
		if (hb_int(conn, &regs)) return(-1);
		if (regs.ax != n) return(-1);
		written += n;
		regs.bx += n;
		nbytes -= n;
		regs.cx = n = nbytes > TRANSBUFSZ? TRANSBUFSZ : nbytes;
 	}
	return(written);
}

static int read_data_offset(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_word(xlat_buf + VERS_LEN + sizeof(DWORD)));
	else if (mkr == DB5_MKR) return(read_dword(xlat_buf + VERS_LEN + sizeof(LWORD)));
	else return(-1);
}

static int read_bytes_per_record(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_word(xlat_buf + VERS_LEN + sizeof(DWORD) + sizeof(WORD)));
	else if (mkr == DB5_MKR) return(read_dword(xlat_buf + VERS_LEN + sizeof(LWORD) + sizeof(DWORD)));
	else return(-1);
}

static recno_t read_record_count(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_dword(xlat_buf + VERS_LEN));
	else if (mkr == DB5_MKR) return(read_lword(xlat_buf + VERS_LEN));
	else return((recno_t)-1);
}

TABLE *hb_get_hdr(HBCONN conn)
{
	TABLE *hdr = NULL;
	char *xlat_buf = NULL, *tp;
	FIELD *f;
	int fcount = 0, meta_size, ok = 0;
	int mkr, hdlen, len, offset, byprec, hdrsz, fldnamelen, fldwd;

	hdlen = dbe_get_hdr_len(conn); //on-disk header len
	if (hdlen <= 0) return(NULL);
	xlat_buf = (char *)malloc(hdlen);
	if (!xlat_buf) goto end;
	if (!dbe_get_hdr(conn, xlat_buf, hdlen)) goto end; //on-disk header
	mkr = MKR_MASK & *xlat_buf;
	if (mkr != DB3_MKR && mkr != DB5_MKR) goto end;
	if (mkr == DB3_MKR) len = sizeof(TABLE) + ((hdlen - HD3SZ) / FLD3WD) * sizeof(FIELD);
	else len = sizeof(TABLE) + ((hdlen - HDRSZ) / FLDWD) * sizeof(FIELD);
	hdr = (TABLE *)malloc(len);
	if (!hdr) goto end;
	offset = read_data_offset(mkr, xlat_buf);
	if (offset < 0) goto end;
	byprec = read_bytes_per_record(mkr, xlat_buf);
	if (byprec < 0 || byprec > MAXRECSZ) goto end;
	hdr->id = (BYTE)*xlat_buf;
	hdr->reccnt = read_record_count(mkr, xlat_buf);
	if (hdr->reccnt == (recno_t)-1) goto end;
	hdr->doffset = offset;
	hdr->byprec = byprec;
	fldnamelen = mkr == DB3_MKR? FLD3NAMLEN : FLDNAMELEN;
	fldwd = mkr == DB3_MKR? FLD3WD : FLDWD;
	hdrsz = mkr == DB3_MKR? HD3SZ : HDRSZ;
	tp = xlat_buf + hdrsz;
	f = &hdr->fdlist[0];
	offset = 1;
	do {
		if (*tp == EOH) break;
		memset(f->name, 0, FLDNAMELEN + 1);
		strncpy(f->name, tp, fldnamelen);
		f->ftype = *(tp + fldnamelen + 1);
		f->x0 = 0;
		f->x1 = 0;
		if (f->ftype == 'C') {
			f->tlength = MAKE_WORD(*(tp + fldnamelen + 6), *(tp + fldnamelen + 7));
			f->dlength = 0;
		}
		else f->tlength = (BYTE)*(tp + fldnamelen + 6); f->dlength = (BYTE)*(tp + fldnamelen + 7);
		f->offset = offset;
		offset += f->tlength;
		++f; ++fcount; tp += fldwd;
	} while (fcount < MAXFLD);
	if (fcount < 1) goto end;
	hdr->fldcnt = fcount;
	if (hdr->fldcnt < MAXFLD) {
		if (*tp != EOH) goto end;
		++tp;
	}
	hdr->hdlen = (int)(tp - xlat_buf);
	if ((hdr->doffset < hdr->hdlen) || (hdlen != hdlen)) goto end;
	hdr->meta_len = hdr->doffset - hdr->hdlen;
	if (hdr->meta_len >= MIN_META_LEN) {
		tp += META_HDR_ALIGN;
		hdr->meta_len -= META_HDR_ALIGN;
	}
	else hdr->meta_len = 0;
	ok = 1;
end:
	if (xlat_buf) free(xlat_buf);
	if (!ok && hdr) free(hdr);
	return(ok? hdr : NULL);
}

int hb_get_value(HBCONN conn, char *expr, char *buf, int len, int catch_error) //CHARCTR expr only
{
	char vbuf[STRBUFSZ];
	HBVAL *v = (HBVAL *)vbuf;
	int errcode = dbe_eval(conn, expr, catch_error), blen;
	if (errcode) return(errcode); //-1 is returned if catch_error is 0 and error handler will be called
	if (len && dbe_popstack(conn, vbuf, STRBUFSZ)) return(-1);
	if (!buf) return(0);
	if (v->valspec.typ != CHARCTR) return(INVALID_TYPE);
	blen = v->valspec.len;
	if (blen > len) blen = len;
	memcpy(buf, v->buf, blen);
	buf[blen] = '\0';
	return(0);
}

static void write_record_count(int mkr, BYTE **tp, recno_t reccnt)
{
	if (mkr == DB3_MKR) { 
		write_dword(*tp, reccnt); 
		*tp += sizeof(DWORD); 
	}
	else { 
		write_lword(*tp, reccnt); 
		*tp += sizeof(LWORD); 
	}
}

static void write_data_offset(int mkr, BYTE **tp, int doffset)
{
	if (mkr == DB3_MKR) {
		write_word(*tp, doffset);
		*tp += sizeof(WORD);
	}
	else {
		write_dword(*tp, doffset);
		*tp += sizeof(DWORD);
	}
}

static void write_bytes_per_record(int mkr, BYTE **tp, int byprec)
{
	if (mkr == DB3_MKR) {
		write_word(*tp, byprec);
		*tp += sizeof(WORD);
	}
	else {
		write_dword(*tp, byprec);
		*tp += sizeof(WORD);
	}
}

int hb_put_hdr(HBCONN conn, TABLE *hdr)
{
	BYTE *xlat_buf = NULL, *tp;
	FIELD *f;
	int mkr = MKR_MASK & hdr->id, fldnamelen, fldwd, i, err = BAD_HEADER, rc = -1;

	if (hdr->hdlen < HD3SZ || hdr->hdlen > MAXHDLEN) goto end;
	xlat_buf = (BYTE *)malloc(hdr->hdlen);
	if (!xlat_buf) {
		err = NOMEMORY;
		goto end;
	}
	if (mkr != DB3_MKR && mkr != DB5_MKR) goto end;
	memset(tp = xlat_buf, 0, hdr->hdlen);
	*tp++ = (BYTE)(0xff & hdr->id);
	*tp++ = LEGACY_YY; *tp++ = LEGACY_MM; *tp++ = LEGACY_DD;
	write_record_count(mkr, &tp, hdr->reccnt);
	if (hdr->doffset < hdr->hdlen) hdr->doffset = hdr->hdlen;
	write_data_offset(mkr, &tp, hdr->doffset);
	write_bytes_per_record(mkr, &tp, hdr->byprec);
	fldnamelen = mkr == DB3_MKR? FLD3NAMLEN : FLDNAMELEN;
	fldwd = mkr == DB3_MKR? FLD3WD : FLDWD;
	tp = xlat_buf + (mkr == DB3_MKR? HD3SZ : HDRSZ);
	for (i=0; i<hdr->fldcnt; ++i,tp+=fldwd) {
		FIELD *f = &hdr->fdlist[i];
		if (tp + fldwd - xlat_buf > hdr->hdlen) goto end;
		strncpy(tp, f->name, fldnamelen);
		*(tp + fldnamelen + 1) = f->ftype;
		write_word(tp + fldnamelen + 2, 0); // x0, not used
		write_word(tp + fldnamelen + 4, 0); // x1, not used
		*(tp+fldnamelen + 6) = f->tlength;
		*(tp+fldnamelen + 7) = f->dlength;
	}
	if (tp - xlat_buf < MAXHDLEN) {
		if (tp - xlat_buf + 1 != hdr->hdlen) goto end;
		*tp++ = EOH; hdr->hdlen++;
	}
	if (dbe_put_hdr(conn, xlat_buf, hdr->hdlen)) goto end;
	err = rc = 0;
end:
	if (err) hb_error(conn, err, NULL, -1, -1);
	if (xlat_buf) free(xlat_buf);
	return(rc);
}

int dbe_set_serial(HBCONN conn, char *sername, uint64_t inival)
{
	HBREGS regs;

	regs.ax = DBE_SET_SERIAL;
	regs.bx = sername;
	regs.cx = strlen(sername) + 1;
	regs.dx = LO_LWORD(inival);
	regs.fx = HI_LWORD(inival);
	return(hb_int(conn, &regs));
}

uint64_t dbe_get_serial(HBCONN conn, char *sername)
{
	HBREGS regs;

	regs.ax = DBE_GET_SERIAL;
	regs.bx = sername;
	regs.cx = strlen(sername)+1;
	if (hb_int(conn, &regs)) return(-1L);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

int dbe_disp_structure(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_DISPSTRU;
	regs.cx = 0;
	regs.dx = regs.ex = regs.fx = 0;
	return(hb_int(conn, &regs));
}

int dbe_disp_memory(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_DISPMEMO;
	regs.cx = 0;
	regs.dx = regs.ex = regs.fx = 0;
	return(hb_int(conn, &regs));
}

int dbe_disp_record(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_DISPREC;
	regs.cx = 0;
	regs.dx = regs.ex = regs.fx = 0;
	return(hb_int(conn, &regs));
}

HBVAL *dbe_pcode_data(HBCONN conn, int idx, char *buf)
{
	HBREGS regs;

	regs.ax = DBE_GET_PDATA;
	regs.bx = buf;
	regs.cx = 0;
	regs.dx = idx;
	if (hb_int(conn, &regs) || (regs.cx <= 0)) return(NULL);
	return((HBVAL *)buf);
}

int dbe_get_epoch(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_EPOCH;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_set_epoch(HBCONN conn, int epoch)
{
	HBREGS regs;

	regs.ax = DBE_SET_ESCAPE;
	regs.cx = 0;
	regs.dx = epoch;
	return(hb_int(conn, &regs));
}

int dbe_set_escape(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_SET_ESCAPE;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

int dbe_open_file(HBCONN conn, int mode, char *ext)
{
	HBREGS regs;

	regs.ax = DBE_FILE_OPEN;
	if (ext && strlen(ext) <= EXTLEN + 1) {
		regs.bx = ext;
		regs.cx = strlen(ext) + 1; //include Null byte
	}
	else regs.cx = 0;
	regs.ex = mode;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.ax);
}

int dbe_close_file(HBCONN conn, int fd)
{
	HBREGS regs;

	regs.ax = DBE_FILE_CLOSE;
	regs.cx = 0;
	regs.ex = fd;
	return(hb_int(conn, &regs));
}

int dbe_form_rewind(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_FORM_REWIND;
	regs.cx = 0;
	if (hb_int(conn, &regs) || regs.ax < 0) return(-1);
	return(0);
}

int dbe_form_seek(HBCONN conn, off_t offset)
{
	HBREGS regs;

	regs.ax = DBE_FORM_SEEK;
	regs.cx = 0;
	regs.dx = LO_LWORD(offset);
	regs.fx = HI_LWORD(offset);
	if (hb_int(conn, &regs) || regs.ax < 0) return(-1);
	return(0);
}

off_t dbe_form_tell(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_FORM_TELL;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return((off_t)-1);
	return(MAKE_LWORD(regs.dx, regs.fx));
}

char *dbe_form_rdln(HBCONN conn, char *line, int max)
{
	HBREGS regs;

	regs.ax = DBE_FORM_RDLN;
	regs.bx = line;
	regs.cx = 0;
	regs.dx = max;
	if (hb_int(conn, &regs) || regs.ax < 0 || regs.cx == 0) return(NULL);
	return(line);
}

int dbe_vstore(HBCONN conn, char *vname)
{
	HBREGS regs;

	if (strlen(vname) > VNAMELEN) return(-1);
	regs.ax = DBE_VSTORE;
	regs.bx = vname;
	regs.cx = strlen(vname);
	return(hb_int(conn, &regs));
}

time_t dbe_get_ftime(HBCONN conn, int which)
{
	HBREGS regs;

	regs.ax = DBE_GET_FTIME;
	regs.cx = 0;
	regs.ex = which;
	if (hb_int(conn, &regs) || regs.ax < 0) return((time_t)-1);
	return((time_t)MAKE_LWORD(regs.dx, regs.fx));
}

char *dbe_transaction_get_id(HBCONN conn, char *buf)
{
	HBREGS regs;

	regs.ax = DBE_GET_TRANS_ID;
	regs.bx = buf;
	regs.cx = 0;
	if (hb_int(conn, &regs) || !strlen(buf)) return(NULL);
	return(buf);
}
