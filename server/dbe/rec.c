/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/rec.c
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
 
void gorec(recno_t recno)
{
	TABLE *hdr;
	int byprec;
	off_t start;
 
	if (!recno) return;
	hdr = curr_dbf->dbf_h;
	byprec = hdr->byprec;
	if (recno <= hdr->reccnt) {
		start = (off_t)(recno - 1) * byprec + hdr->doffset;
		if (dbread(get_rec_buf(), start, byprec) != byprec) db_error(DBREAD, curr_ctx->dbfctl.filename);
	}
	else memset(get_rec_buf(), ' ', byprec);
	curr_ctx->crntrec = recno;
}

BYTE *get_db_rec()
{
	if (etof(0)) db_error2(OUTOFRANGE, get_ctx_id() + 1);
	if (!flagset(exclusive) && !is_rec_locked(curr_ctx->crntrec, HB_RDLCK)) gorec(curr_ctx->crntrec);
	return(get_rec_buf());
}

int go(recno_t recno)
{
	clrflag(rec_found);
	if (recno < 1) {
		setetof(BACKWARD);
		return(0);
	}
	lock_hdr();	/* read latest header */
	unlock_hdr();
	if (recno > curr_dbf->dbf_h->reccnt) {
xx:
		setetof(FORWARD);
		return(0);
	}
	clretof(0);
	gorec(recno);
	if ((isset(DELETE_ON) && isdelete()) || (filterisset() && !cond(curr_ctx->fltr_pbuf))) goto xx;
	if (index_active()) {
		pageno_t cpno;
		NDX_ENTRY target;
		
		ndx_select(curr_ctx->order);
		if (curr_ndx->fsync) read_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
		cpno = curr_ndx->page_no;
		if (cpno && (recno == ne_get_recno(getpage(cpno, curr_ndx->pagebuf, 0), curr_ndx->entry_no))) {
			ndx_unlock();
			goto done;
		}
		if (ndx_get_key(1, &target)) { //non-NULL key
			int found = 0;
			found = btsrch(curr_ndx->ndx_h.rootno, TEQUAL, &target) != (recno_t)0;
			ndx_unlock();
			if (!found) ndx_err(curr_ctx->order, NTNDX);
		}
		else ndx_unlock();
	}
done :
	return go_rel();
}

void next(int dir)
{
	recno_t crntr = curr_ctx->crntrec;
 
	clrflag(rec_found);
	clretof(dir);
	if (index_active()) {
		if (is_scope_set() && curr_ctx->scope.nextrec) 
			crntr = curr_ctx->scope.nextrec;
		else crntr = nextndx(dir, 0);
	}
	else crntr += dir == FORWARD? 1 : -1;
	if (!crntr) setetof(dir);
	else if (crntr > curr_dbf->dbf_h->reccnt) {
		lock_hdr(); //re-read header
		unlock_hdr();
		if (crntr > curr_dbf->dbf_h->reccnt) setetof(FORWARD);
	}
	if (!etof(dir)) {
		gorec(crntr);
		clretof(dir == FORWARD? BACKWARD : FORWARD);
	}
	curr_ctx->crntrec = crntr;
	while (!go_rel()) next(dir);
}

void gobot(void)
{
	recno_t curr_recno;
 
	clrflag(rec_found);
	clretof(0);
	if (index_active()) {
		curr_recno = ndxbot();
	}
	else {
		lock_hdr();
		curr_recno = curr_dbf->dbf_h->reccnt;
		unlock_hdr();
	}
	if (!curr_recno) setetof(0);
	else gorec(curr_recno);
	while (!etof(BACKWARD)) {
		if ((isset(DELETE_ON) && isdelete()) || (filterisset() && !cond(curr_ctx->fltr_pbuf))) {
			next(BACKWARD);
			continue;
		}
		if (go_rel()) break; //got a valid record
	}
}

void gotop(void)
{
	recno_t curr_recno;
 
	clrflag(rec_found);
	clretof(0);
	lock_hdr();	/* read latest header */
	unlock_hdr();
	if (!curr_dbf->dbf_h->reccnt) setetof(0);
	else if (index_active()) {
		curr_recno = ndxtop();
	}
	else curr_recno = 1;
	if (!curr_recno) setetof(0);
	else gorec(curr_recno);
	while (!etof(FORWARD)) {
		if ((isset(DELETE_ON) && isdelete()) || (filterisset() && !cond(curr_ctx->fltr_pbuf))) {
			next(FORWARD);
			continue;
		}
		if (go_rel()) break; //got a valid record
	}
}

int skip_rel(int dir)
{
	int ctx_id = get_ctx_id(), rc = 0;
	recno_t recno;
	if (!relation_isset() || etof(0) || !_index_active(curr_ctx->rela)) goto end;
	evaluate(curr_ctx->relation, 0);
	ctx_select(curr_ctx->rela);
	if (skip_rel(dir)) { //a child table has a valid skip, stack popped
		rc = 1;
		goto end;
	}
	//result of relation expression is on the stack, now get the next index entry
	if (etof(dir) || !curr_ctx->crntrec) {
		pop_stack(DONTCARE);
		goto end;
	}
	recno = nextndx(dir, 1); //also push key onto the stack
	if (!recno) {
		pop_stack(DONTCARE);
		goto end;
	}
	bop(TEQUAL);
	if (pop_logic()) { //we have a match
		go(recno);
		rc = 1;
	}
end:
	ctx_select(ctx_id);
	return(rc);
}

recno_t skip(long n) //-2^63..+2^63
{
	int dir, is_filtered = 0;
	recno_t count;

	if (!n) return((recno_t)0);
	dir = n > 0 ? FORWARD : BACKWARD;
	n = count = dir == FORWARD? n : -n;
	if (etof(dir == FORWARD? BACKWARD : FORWARD)) {
		if (dir == FORWARD) gotop(); else gobot();
		if (etof(0)) return(0);
		--n;
	}
	while (n) {
		if (!skip_rel(dir)) {
			next(dir);
			int is_etof = etof(dir);
			if (!is_etof) {
				is_filtered = (isset(DELETE_ON) && isdelete()) || (filterisset() && !cond(curr_ctx->fltr_pbuf));
				if (is_filtered) continue; //don't decrement
			}
			--n;
			if (is_etof) break;
		}
		else --n;
	}
	return(count - n);
}

int go_rel(void)
{
	int my_ctx, rc = 0;
	if (!relation_isset()) return(1);
	my_ctx = get_ctx_id();
	if (etof(0)) {
		int dir = 0;
		if (flagset(e_o_f)) dir = FORWARD;
		if (flagset(t_o_f)) {
			if (dir) dir = 0;
			else dir = BACKWARD;
		}
		ctx_select(curr_ctx->rela);
		setetof(dir);
		goto done;
	}
	evaluate(curr_ctx->relation, 0);
	ctx_select(curr_ctx->rela);
	if (!topstack->valspec.len) { //NULL value
		pop_stack(DONTCARE);
		setflag(e_o_f);
		go_rel(); //set flag in child tables
		goto done;
	}
	if (index_active()) {
		push_int((long)1); //no combo, only 1 val
		find(TEQUAL);
		rc = isfound();
	}
	else {
		go((recno_t)pop_long());
		rc = !etof(0);
	}
done:
	ctx_select(my_ctx);
	return(rc);
}
