/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/setto.c
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
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <pwd.h>
#include "hb.h"
#include "hbpcode.h"
#include "dbe.h"

void put_cond(char *exp, CODE_BLOCK *pbuf, int size_max)
{
	PCODE code_buf[CODE_LIMIT];
	BYTE data_buf[SIZE_LIMIT];
	PCODE_CTL save_ctl;
	cmd_init(exp, code_buf, data_buf, &save_ctl);
	exe = FALSE;
	get_token();
	expression();
	emit(pbuf, size_max);
	if (curr_tok != TNULLTOK) syn_err(EXCESS_SYNTAX);
	restore_pcode_ctl(&save_ctl);
	exe = TRUE;
}

int cond(CODE_BLOCK *pbuf)
{
	execute(pbuf);
	HBVAL *v = pop_stack(LOGIC);
	return(v->valspec.len && v->buf[0]);
}

void del_exp(int type)
{
#ifdef EMBEDDED
	char *q;
	int len, i;
	switch (type) {
	case EXP_RELA:
		if (curr_ctx->relation) {
			q = curr_ctx->relation;
			len = strlen(q) + 1;
			for (i=0; i<MAXCTX+1; ++i) if (table_ctx[i].relation > q) table_ctx[i].relation -= len;
			q += len;
			memcpy(curr_ctx->relation, q, rela_buf.buf + EXPBUFSZ - q);
			rela_buf.next_avail -= len;
		}
		break;
	case EXP_FILTER:
		if (curr_ctx->filter) {
			q = curr_ctx->filter;
			len = strlen(q) + 1;
			for (i=0; i<MAXCTX+1; ++i) if (table_ctx[i].filter > q) table_ctx[i].filter -= len;
			q += len;
			memcpy(curr_ctx->filter, q, fltr_buf.buf + EXPBUFSZ - q);
			fltr_buf.next_avail -= len;
		}
		break;
	case EXP_LOCATE:
		if (curr_ctx->locate) {
			q = curr_ctx->locate;
			len = strlen(q) + 1;
			for (i=0; i<MAXCTX+1; ++i) if (table_ctx[i].locate > q) table_ctx[i].locate -= len;
			q += len;
			memcpy(curr_ctx->locate, q, loc_buf.buf + EXPBUFSZ - q);
			loc_buf.next_avail -= len;
		}
		break;
#else
	switch (type) {
	case EXP_RELA:
		if (curr_ctx->relation) free(curr_ctx->relation);
		curr_ctx->relation = NULL;
		break;
	case EXP_FILTER:
		if (curr_ctx->filter) free(curr_ctx->filter);
		curr_ctx->filter = NULL;
		break;
	case EXP_LOCATE:
		if (curr_ctx->locate) free(curr_ctx->locate);
		curr_ctx->locate = NULL;
		break;
#endif
	default:
		syn_err(INVALID_ARG);
	}
}

int add_exp(int type)
{
	HBVAL *v = pop_stack(CHARCTR);
	char *p, *q = (char *)v->buf, **ptr;
	int len = v->valspec.len;

	if (len) do {
		if (!isspace(*q)) break;
		++q;
	} while (--len);
	if (!len) return(0);
#ifdef EMBEDDED
	char *expbuf;
	WORD *next;
	switch (type) {
	case EXP_RELA:
		expbuf = rela_buf.buf;
		next = &rela_buf.next_avail;
		ptr = &curr_ctx->relation;
		break;
	case EXP_FILTER:
		expbuf = fltr_buf.buf;
		next = &fltr_buf.next_avail;
		ptr = &curr_ctx->filter;
		break;
	case EXP_LOCATE:
		expbuf = loc_buf.buf;
		next = &loc_buf.next_avail;
		ptr = &curr_ctx->locate;
		break;
	default:
		syn_err(INVALID_ARG);
	}
	if (*next + len + 1 > EXPBUFSZ) sys_err(NOMEMORY);
	p = expbuf + *next;
	*next += len + 1;
#else
	switch (type) {
	case EXP_RELA:
		ptr = &curr_ctx->relation;
		break;
	case EXP_FILTER:
		ptr = &curr_ctx->filter;
		break;
	case EXP_LOCATE:
		ptr = &curr_ctx->locate;
		break;
	default:
		syn_err(INVALID_ARG);
	}
	p = malloc(len + 1);
	if (!*p) sys_err(NOMEMORY);
#endif
	memcpy(p, q, len);
	p[len] = '\0';
	evaluate(p, 0);
	v = pop_stack(DONTCARE);
	if ((type && v->valspec.typ != LOGIC) || (!type && v->valspec.typ == LOGIC)) syn_err(INVALID_TYPE);
	*ptr = p;
	return(len);
}


#define CHG_USR		'~'
extern int db3w_admin, hb_uid;

void set_datefmt(int n);

void set_to(int what)
{
	HBVAL *v;
	int n = pop_int(0, 255);
	if (n == OPND_ON_STACK) n = pop_int(0, 10000);
	switch (what) {
		case FILTER_TO: {
			int result;
			valid_chk();
			del_exp(EXP_FILTER);
			if (!n) break;
			if (!(result = add_exp(EXP_FILTER))) break;
			if (result == -1) db_error0(FLTROVERFLOW);
			put_cond(curr_ctx->filter, curr_ctx->fltr_pbuf, FLTRPBUFSZ);
			go(curr_ctx->crntrec);
			break; }

		case INDEX_TO:
			valid_chk();
			closeindex();
			if (n) {
				if (n < 1 || n > MAXNDX) db_error2(INVALID_SET_PARM, n);
				while (n) get_fn(I_D_X, &curr_ctx->ndxfctl[--n]);
				curr_ctx->order = 0;
			}
			setflag(setndx);
			update_hdr(1);
			if (!etof(0) && curr_ctx->crntrec) go(curr_ctx->crntrec);
			break;

		case FORM_TO: {
			char formfn[FULFNLEN];
			if (frmfctl.fp) {
				fclose(frmfctl.fp);
				frmfctl.fp = NULL;
			}
			delfn(&frmfctl);
			if (!n) break;
			get_fn(F_R_M | U_NO_CHK_EXIST, &frmfctl);
			strcpy(formfn, form_fn(frmfctl.filename, 1));
			frmfctl.fp = fopen(formfn, "r");
			if (!frmfctl.fp) db_error(DBOPEN, formfn);
			break; }

		case PROC_TO: {
			int dbg = 0, is_sys = 0;
			if (profctl.fp) {
				fclose(profctl.fp);
				profctl.fp = NULL;
			}
			release_proc(LIB_USER);
			delfn(&profctl);
			if (!n) break;
			if (n & P_DBG) dbg = 1;
			if (n & P_SYS) is_sys = 1;
			get_fn(P_R_O | U_NO_CHK_EXIST, &profctl); /** get & check procedure filename **/

			load_proc(is_sys? LIB_SYS : LIB_USER, dbg);
			break; }

		case EPOCH_TO:
			epoch = n;
			break;

		case ORDER_TO:
			if ((n < 0) || (n > MAXNDX) || (n && _chkndx(get_ctx_id(), n - 1) == CLOSED)) db_error2(NOINDEX, n);
			curr_ctx->order = --n;
			if (!etof(0) && curr_ctx->crntrec) go(curr_ctx->crntrec);
			break;

		case WIDTH_TO:
			if (n < MINWIDTH) nwidth = DEFAULTWIDTH;
			else {
				if (n > MAXWIDTH) nwidth = MAXWIDTH;
				else nwidth = n;
			}
			if (isset(TALK_ON)) prnt_str("WIDTH set to %s%d\n", nwidth == MAXWIDTH? "maximum " : "", nwidth);
			break;

		case DECI_TO:
			if (n < 0) ndeci = DEFAULTDCML;
			else {
				if (n > MAXDCML) ndeci = MAXDCML;
				else ndeci = n;
			}
			if (isset(TALK_ON)) prnt_str("DECIMAL set to %s%d\n", ndeci == MAXDCML? "maximum " : "", ndeci);
			if (ndeci + 2 > nwidth) {
				nwidth = ndeci + 2;
				if (isset(TALK_ON)) prnt_str("WIDTH set to %s%d\n", nwidth == MAXWIDTH? "maximum " : "", nwidth);
			}
			break;

		case DEFA_TO:
			//not valid for non-DOS systems
			break;

		case PATH_TO: {
			char infn[FULFNLEN], *p, *q, bad_path[FULFNLEN];
			p = defa_path;
			if (n) {
				char *root_end;
				v = pop_stack(CHARCTR);
				int i, len = v->valspec.len;
				q = (char *)v->buf;
				strcpy(infn, p);
				if (*q == SWITCHAR) {
					if (db3w_admin) *p = '\0';
					else strcpy(p, rootpath);
				}
				if (db3w_admin) root_end = p; //allow admin to traverse anywhere
				else root_end = p + strlen(rootpath);
				p += strlen(p);
				memcpy(p, q, len);
				p += len;
				if (*(p - 1) != SWITCHAR) *p++ = SWITCHAR; *p = '\0';;
				for (p=strchr(defa_path, SWITCHAR); p; p = strchr(p + 1, SWITCHAR)) { /** get rid of relative dir **/
					if (!strncmp(p + 1, CURRENT_DIR, 2)) {
						for (q = p + 2; *q; ++q) *(q - 2) = *q;
						*(q - 2) = '\0';
						--p;
					}
					else if (!strncmp(p + 1, PARENT_DIR, 3)) {
						int i;
						q = p + 3;
						while (p > root_end && *(--p) != SWITCHAR);
						for (i=0; *q; ++q,++i) p[i] = *q;
						p[i] = '\0';
						--p;
					}
				}
				p = defa_path + strlen(defa_path);
				strcpy(p, "."); //test if path is a directory and exists
				if (!(i = fileexist(defa_path)) || !getaccess(defa_path, HBDB)) {
					*p = '\0';
					strcpy(bad_path, defa_path);
					strcpy(defa_path, infn);
					db_error(!i? INVALID_PATH:DB_ACCESS, bad_path);
				}
				*p = '\0';
			}
			else {
				if (curr_ctx->dbfctl.filename) strcpy(p, form_fn1(1, curr_ctx->dbfctl.filename));
				else get_root_path(p, FULFNLEN);
			}
			break; }

		case RELA_TO: {
			int i;
			valid_chk();
			unset_relation();
			if (!n) break;
			v = pop_stack(DONTCARE);
			if (v->valspec.typ == CHARCTR) i = alias2ctx(v);
			else if (v->valspec.typ == NUMBER && val_is_integer(v)) i = (int)get_long(v->buf);
			else syn_err(INVALID_TYPE);
			if (i >= MAXCTX) syn_err(BAD_ALIAS);
			if (cyclic(i)) db_error2(CYCLIC_REL, i);
			curr_ctx->rela = i;
			if (!(i = add_exp(EXP_RELA))) return;
			if (i == -1) db_error0(RELAOVERFLOW);
			//if (!etof(0)) go(curr_ctx->crntrec);
			gotop();
			break; }

		case DATEFMT_TO:
			set_datefmt(n);
			break;
		default:
			break;
	}
}
