/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/update.c
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
#include "dbe.h"

static int upd_random = 0;
static int ctx_id, from_ctx_id;
static char keyfld[FLDNAMELEN + 1];

static void _updmatch(void)
{
	int fldnlen = strlen(keyfld);
	if (upd_random) { //in from_ctx
		if (flagset(e_o_f)) return;
		push_string(keyfld, fldnlen);
		dbretriv();
		push_int(1); //only one field, no combo key
		ctx_select(ctx_id);
		find(TEQUAL);
		push_logic(isfound());
		if (!isfound()) ctx_select(from_ctx_id);
	}
	else {
		int rel = 1;
		while (rel > 0) { //ctx_id fld is greater than from_ctx_id fld
			push_string(keyfld, fldnlen);
			dbretriv();
			ctx_select(from_ctx_id);
			if (iseof()) {
				ctx_select(ctx_id);
				break;
			}
			push_string(keyfld, fldnlen);
			dbretriv();
			if (topstack->valspec.len == 0) { //from is null, skip
				pop_stack(DONTCARE);
				pop_stack(DONTCARE);
			}
			else {
				if ((stack_peek(1))->valspec.len == 0) {
					rel = 0; //force update
					pop_stack(DONTCARE);
					pop_stack(DONTCARE);
				}
				else {
					bop(TQUESTN);
					rel = pop_int(-1, 1);
				}
			}
			if (rel <= 0) break;
			skip(1);
			ctx_select(ctx_id);
		}
		push_logic(rel == 0);
	}
}

void updinit(int mode)
{
	HBVAL *v = pop_stack(CHARCTR);
	FIELD *ff, *ft;
	int fldno, fldnlen;
	valid_chk();
	ctx_id = get_ctx_id();
	upd_random = mode;
	if (upd_random) ndx_chk();
	curr_ctx->record_cnt = 0;
	fldnlen = v->valspec.len;
	sprintf(keyfld, "%.*s", fldnlen, v->buf);
	chgcase(keyfld, 1);
	fldno = _fieldno(keyfld);
	if (fldno < 0) db_error(NOSUCHFIELD, keyfld);
	ft = get_field(fldno);
	v = pop_stack(CHARCTR);
	from_ctx_id = alias2ctx(v);
	ctx_select(from_ctx_id);
	fldno = _fieldno(keyfld);
	if (fldno < 0) db_error(NOSUCHFIELD, keyfld);
	ff = get_field(fldno);
	if (ff->ftype != ft->ftype) db_error(DB_TYPE_MISMATCH, keyfld);
	gotop();
	if (!upd_random) {
		ctx_select(ctx_id);
		gotop();
	}
	_updmatch();
}

void updrec(void)
{
	store_rec();
	cnt_message("UPDATED", 0);
	r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
}

void updnext(void)
{
	if (upd_random) {
		ctx_select(from_ctx_id);
		skip(1);
	}
	else skip(1);
	_updmatch();
}

void upddone(void)
{
	ctx_select(ctx_id);
	cnt_message("UPDATED", 1);
}
