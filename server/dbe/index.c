/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/index.c
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

#include <memory.h>
#include <string.h>
#include <setjmp.h>
#include "hb.h"
#include "dbe.h"

void __index(void)
{
	recno_t k = curr_dbf->dbf_h->reccnt;
	recno_t n = 0;
	NDX_PAGE *cpg;
	NDX_HDR *ndxh = &curr_ndx->ndx_h;
	NDX_ENTRY target;
	char *msg = "INDEXED";
	curr_ctx->record_cnt = 0;
	while (k--) {
		int rc;
		gorec(++n);
		if (isset(DELETE_ON) && isdelete()) continue;
		if (!ndx_get_key(1, &target)) continue; //NULL key encountered
		clretof(0);
		if (rc = btsrin(ndxh->rootno, &target)) {
			if (rc == DUPKEY && isset(UNIQUE_ON)) continue;
			db_error2(rc, n);
		}
		if (target.recno || target.pno) { //lower level page split
		   cpg = getpage(ndxh->pagecnt, curr_ndx->pagebuf, 1);
		   cpg->entry_cnt = 1;
		   ne_put_pageno(cpg, 0, ndxh->rootno);
		   ndxh->rootno = ndxh->pagecnt++;
		   insert_entry(cpg, 0, 0, &target);
		   write_page(ndxh->rootno, (BYTE *)cpg);
		}
		cnt_message(msg, 0); //increment record_cnt and output message if necessary
	}
	if (!curr_ctx->record_cnt) {
		cpg = getpage(ndxh->rootno = ndxh->pagecnt, curr_ndx->pagebuf, 1);
		ndxh->pagecnt++;
		cpg->entry_cnt = 0;
		ne_put_pageno(cpg, 0, 0);
		write_page(ndxh->rootno, (BYTE *)cpg);
	}
	cnt_message(msg, 1);
	curr_ndx->w_hdr = 1;
}

int n_indexes(void)
{
	int i, ctx_id = get_ctx_id();
	for (i=0; i<MAXNDX && _chkndx(ctx_id, i) != CLOSED; ++i);
	return(i);
}

void _index(int quick)
{
	valid_chk();
	closeindex();
	if (f_lock(0) < 0) db_error(NOFLOCK, curr_ctx->dbfctl.filename);
	ndx_select(0);
	get_fn(I_D_X | U_NO_CHK_EXIST, &curr_ctx->ndxfctl[0]);
	openidx(NEW_FILE | OVERWRITE | EXCL);
	clretof(0);
	if (quick) qindex();
	else __index();
	closendx();
	curr_ctx->order = 0;
	openidx(OLD_FILE | flagset(exclusive)? EXCL : 0);
	gotop();
	f_unlock();
}

void reindex(int quick)
{
	int i, n_ndx, save_order;

	valid_chk();
	if (!(n_ndx = n_indexes())) db_error2(NOINDEX, get_ctx_id() + 1);
	if (f_lock(0) < 0) db_error0(NOFLOCK);
	save_order = curr_ctx->order;
	for (i=0; i<n_ndx; ++i) {
		ndx_select(curr_ctx->order = i);
		push_string(curr_ndx->ndx_h.ndxexp, strlen(curr_ndx->ndx_h.ndxexp));
		closendx();
		openidx(NEW_FILE | OVERWRITE | EXCL);
		/*
		if (ext_ndx) _usr(USR_QINDEX);
		else __index();
		*/
		if (quick) qindex();
		else __index();
		closendx();
	}
	if ((curr_ctx->order = save_order) != -1) {
		ndx_select(save_order);
		gotop();
	}
	f_unlock();
}
