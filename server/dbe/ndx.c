/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/ndx.c
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

#include <string.h>
#include "hb.h"
#include "dbe.h"
#include "hbpcode.h"

#define ALIGNMENT_SIZE	sizeof(int)

int is_ndx_expr;	//needed by parse.c

int read_ndx_hdr(int fd, NDX_HDR *hd)
{
	char buf[HBPAGESIZE];
	char *tp = buf;
	lseek(fd, (off_t)0, 0);
	if (read(fd, buf, HBPAGESIZE) != HBPAGESIZE) return(-1);
	hd->rootno = read_pageno(tp); tp += sizeof(pageno_t);
	hd->pagecnt = read_pageno(tp); tp += sizeof(pageno_t);
	tp += sizeof(DWORD);
	hd->keylen = read_word(tp); tp += sizeof(WORD);
	hd->norecpp = read_word(tp); tp += sizeof(WORD);
	hd->keytyp = read_word(tp); tp += sizeof(WORD);
	hd->entry_size = read_word(tp); tp += sizeof(WORD);
	tp += sizeof(DWORD);
	strcpy(hd->ndxexp, tp);
	return(HBPAGESIZE);
}

void openidx(int mode)
{
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	char ndx_fn[FULFNLEN];
 
	strcpy(ndx_fn, form_fn(curr_ndx->fctl->filename, 1));
	if (mode & NEW_FILE) {
		int data_size = HBPAGESIZE - sizeof(WORD) - sizeof(WORD) - sizeof(pageno_t); //ndxpage entry_cnt + pad + pp
		HBVAL *v = pop_stack(CHARCTR);
		sprintf(nhd->ndxexp, "%.*s", v->valspec.len, v->buf);
		ndx_put_key();
		nhd->rootno = 0;
		nhd->pagecnt = 1;
		nhd->entry_size = sizeof(pageno_t) + sizeof(recno_t) + (nhd->keylen / ALIGNMENT_SIZE) * ALIGNMENT_SIZE;
		if (nhd->keylen % ALIGNMENT_SIZE) nhd->entry_size += ALIGNMENT_SIZE;
		nhd->norecpp = data_size / nhd->entry_size;
		curr_ndx->fctl->fd = s_open(ndx_fn, mode);
		if (write_ndx_hdr(curr_ndx->fctl->fd, nhd) < 0) db_error(DBWRITE, ndx_fn);
	}
	else {
		curr_ndx->fctl->fd = s_open(ndx_fn, mode);
		if (read_ndx_hdr(curr_ndx->fctl->fd, nhd) < 0) db_error(DBREAD, ndx_fn);
		ndx_put_key();
	}
	curr_ndx->w_hdr = 0;
	curr_ndx->fsync = !(mode & EXCL);
}

int write_ndx_hdr(int fd, NDX_HDR *hd)
{
	char buf[HBPAGESIZE];
	char *tp = buf;
	write_pageno(tp, hd->rootno); tp += sizeof(pageno_t);
	write_pageno(tp, hd->pagecnt); tp += sizeof(pageno_t);
	write_dword(tp, 0); tp += sizeof(DWORD);
	write_word(tp, hd->keylen); tp += sizeof(WORD);
	write_word(tp, hd->norecpp); tp += sizeof(WORD);
	hd->keytyp &= 0x7f;
	write_word(tp, hd->keytyp); tp += sizeof(WORD);
	write_word(tp, hd->entry_size); tp += sizeof(WORD);
	write_dword(tp, 0); tp += sizeof(DWORD);
	strcpy(tp, hd->ndxexp);
	lseek(fd, (off_t)0, 0);
	if (write(fd, buf, HBPAGESIZE) != HBPAGESIZE) return(-1);
	fsync();
	return(0);
}

void closendx(void)
{
	int fd = curr_ndx->fctl->fd;
	if (fd == -1) return;
	if (curr_ndx->w_hdr) {
		write_ndx_hdr(fd, &curr_ndx->ndx_h);
		curr_ndx->w_hdr = 0;
	}
	curr_ndx->page_no = 0;
	close(fd);
	curr_ndx->fctl->fd = -1;
}

void closeindex(void)
{
	int i, ctx_id = get_ctx_id();
 
	for (i=0; i<MAXNDX; ++i) {
		if (_chkndx(ctx_id, i) == IN_USE) {
			ndx_select(i);
			closendx();
		}
		delfn(&curr_ctx->ndxfctl[i]);
	}
	curr_ctx->order = -1;	 /** indicates no index (see chkndx()) *****/
}

void ndx_put_key(void)
{
	PCODE code_buf[CODE_LIMIT];
	BYTE data_buf[SIZE_LIMIT];
	PCODE_CTL save_ctl;
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	int i, ktyp, is_null_on = isset(NULL_ON);
	cmd_init(nhd->ndxexp, code_buf, data_buf, &save_ctl);
	is_ndx_expr = exe = TRUE;
	if (is_null_on) set_onoff(NULL_ON, S_OFF); //allow blank so we can get the correct keylen
	get_token();
	if (curr_tok == TNULLTOK) syn_err(BAD_EXP);
	expression();
	if (curr_tok == TCOMMA) {
		NDXCMBK *cmkp = &nhd->cmbkey;
		memset(cmkp, 0, sizeof(NDXCMBK));
		do {
			ktyp = topstack->valspec.typ;
			if ((ktyp != NUMBER) && (ktyp != DATE) && (ktyp != CHARCTR)) db_error(DB_INVALID_TYPE, nhd->ndxexp);
			if ((int)cmkp->nparts >= MAXCMBK || (int)cmkp->keylen + (int)topstack->valspec.len >= MAXKEYSZ) db_error(KEYTOOLONG, nhd->ndxexp);
			i = cmkp->nparts++;
			cmkp->keys[i].typ = ktyp;
			cmkp->keylen += cmkp->keys[i].len = ktyp == CHARCTR? topstack->valspec.len : sizeof(double);
			get_token();
			if (curr_tok == TNULLTOK) syn_err(BAD_EXP);
			expression();
		} while (curr_tok == TCOMMA);
		nhd->keytyp = CMBK;
		nhd->keylen = cmkp->keylen;
	}
	else {
		ktyp = topstack->valspec.typ;
		if ((ktyp != NUMBER) && (ktyp != DATE) && (ktyp != CHARCTR)) db_error(DB_INVALID_TYPE, nhd->ndxexp);
		if (topstack->valspec.len > MAXKEYSZ) db_error(KEYTOOLONG, nhd->ndxexp);
		nhd->keytyp = ktyp != CHARCTR;
		if (ktyp == DATE) nhd->keytyp |= 0x80;
		nhd->keylen = ktyp == CHARCTR? topstack->valspec.len : sizeof(double);
	}
	is_ndx_expr = FALSE;
	emit((CODE_BLOCK *)nhd->pbuf, NDXPBUFSZ);
	for (i=1; ; ++i) {
		pop_stack(DONTCARE);
		if (nhd->keytyp != CMBK || i >= (int)nhd->cmbkey.nparts) break;
	}
	if (is_null_on) set_onoff(NULL_ON, S_ON);
	restore_pcode_ctl(&save_ctl);
}

int ndx_get_key(int clr_stack, NDX_ENTRY *target)
{
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	BYTE *p = target->key;
	int i, nk = nhd->keytyp == CMBK? nhd->cmbkey.nparts : 1, is_null = 0;

	target->recno = curr_ctx->crntrec;
	target->pno = 0;

	execute((CODE_BLOCK *)nhd->pbuf);

	for (i=nk; i; --i) {
		HBVAL *v = stack_peek(i - 1);
		if (!v->valspec.len) {
			is_null = 1;
			break;
		}
		switch (v->valspec.typ) {
		case DATE:
			put_double(p, (double)get_date(v->buf));
			p += sizeof(double);
			break;
		case NUMBER:
			if (val_is_real(v)) put_double(p, get_double(v->buf));
			else put_double(p, (double)get_long(v->buf) / power10(v->numspec.deci));
			p += sizeof(double);
			break;
		case CHARCTR:
			memcpy(p, v->buf, v->valspec.len);
			p += v->valspec.len;
			break;
		default:
			db_error(DB_INVALID_TYPE, nhd->ndxexp);
		}
	}
	if (clr_stack) while (nk--) pop_stack(DONTCARE);
	return(is_null? 0 : nk);
}

void push_ndx_entry(NDX_ENTRY *ne)
{
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	number_t num;
	if (nhd->keytyp == CMBK) {
		NDXCMBK *cmkp = &nhd->cmbkey;
		if (cmkp->keys[0].typ == NUMBER || cmkp->keys[0].typ == DATE) {
			num.deci = REAL;
			num.r = get_double(ne->key);
			push_number(num);
		}
		else push_string(ne->key, cmkp->keys[0].len);
	}
	else if (nhd->keytyp) { //numeric type, double
		num.deci = REAL;
		num.r = get_double(ne->key);
		push_number(num);
	}
	else push_string(ne->key, nhd->keylen);
}

recno_t ndxtop(void)
{
	pageno_t pno;
	recno_t rno;
	NDX_PAGE *cpg;
 
	ndx_select(curr_ctx->order);
	if (!flagset(exclusive) && !flagset(denywrite)) read_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
	cpg = getpage(pno = curr_ndx->ndx_h.rootno, curr_ndx->pagebuf, 0);
	if (!(cpg->entry_cnt)) {
		setetof(0);
		ndx_unlock();
		return((recno_t)0);
	}
	while (ne_get_pageno(cpg, 0)) cpg = getpage(pno = ne_get_pageno(cpg, 0), curr_ndx->pagebuf, 0);
	curr_ndx->page_no = pno;
	curr_ndx->entry_no = 0;
	rno = ne_get_recno(cpg, 0);
	/*
	if (rno > curr_dbf->dbf_h->reccnt)
	    ndx_err(curr_ctx->order,OUTOFRANGE);
	*/
	ndx_unlock();
	return(rno);
}

recno_t ndxbot(void)
{
	pageno_t pno, pno2;
	recno_t recno;
	WORD last_in_page;
	NDX_PAGE *cpg;
 
	ndx_select(curr_ctx->order);
	if (!flagset(exclusive) && !flagset(denywrite)) read_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
	cpg = getpage(pno = curr_ndx->ndx_h.rootno, curr_ndx->pagebuf, 0);
	if (!(last_in_page = cpg->entry_cnt)) {
		setetof(0);
		ndx_unlock();
		return((recno_t)0);
	}
	pno2 = ne_get_pageno(cpg, last_in_page);
	while (pno2) {
		cpg = getpage(pno = pno2, curr_ndx->pagebuf, 0);
		last_in_page = cpg->entry_cnt;
		pno2 = ne_get_pageno(cpg, last_in_page);
	}
	curr_ndx->page_no = pno;
	curr_ndx->entry_no = last_in_page - 1;
	recno = ne_get_recno(cpg, last_in_page - 1);
	/*
	if (ll > curr_dbf->dbf_h->reccnt)
		ndx_err(curr_ctx->order,OUTOFRANGE);
	*/
	ndx_unlock();
	return(recno);
}

NDX_ENTRY *lfind(int dir, NDX_ENTRY *ne) //we are at end of page, find the next index entry
{
	NDX_PAGE *cpg;
	pageno_t pn;
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	NDX_ENTRY target;
	int len = nhd->keytyp == CMBK? nhd->cmbkey.keylen : nhd->keylen;
	target.recno = ne->recno;
	target.pno = 0;
	memcpy(target.key, ne->key, len);
	pn = btxrch(dir, curr_ndx->ndx_h.rootno, &target); //find the next page
	if (!pn) 
		return(NULL);
	cpg = getpage(pn, curr_ndx->pagebuf, 0);
	while (!is_leaf(cpg)) { //non-leaf
		pn = ne_get_pageno(cpg, dir == FORWARD ? 0 : cpg->entry_cnt);
		if (!pn) 
			return(NULL);
		cpg = getpage(pn, curr_ndx->pagebuf, 0);
	}
	curr_ndx->page_no = pn;
	return(get_entry(cpg, curr_ndx->entry_no = dir == FORWARD ? 0 : cpg->entry_cnt - 1));
}

recno_t nextndx(int dir, int push_key)
{
	NDX_ENTRY *ne = NULL, target;
	NDX_PAGE *cpg;
 
	ndx_select(curr_ctx->order);
	if (!flagset(exclusive) && !flagset(denywrite)) read_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
	if (!curr_ndx->page_no) {
		if (!ndx_get_key(1, &target)) goto done; //NULL key
		if (!btsrch(curr_ndx->ndx_h.rootno, TEQUAL, &target)) ndx_err(curr_ctx->order, NTNDX);
	}
	cpg = getpage(curr_ndx->page_no, curr_ndx->pagebuf, 0);
	if (curr_ndx->entry_no == (dir == FORWARD ? cpg->entry_cnt - 1 : 0)) {
		int cmp;
		memcpy(&target, get_entry(cpg, cpg->entry_cnt - 1), curr_ndx->ndx_h.entry_size);
		ne = lfind(dir, &target);
		if (!ne)
			return(0);
		cmp = memcmp(ne->key, target.key, curr_ndx->ndx_h.keylen);
		if (!cmp) cmp = ne->recno > target.recno? 1 : (ne->recno < target.recno? -1 : 0);
		if (dir == FORWARD && cmp <= 0 || dir == BACKWARD && cmp >= 0) //we are out of order. debug
			return(0);
	}
	else {
		ne = get_entry(cpg, curr_ndx->entry_no += dir==FORWARD? 1 : -1);
		if (!ne)
			return(0);
		if (push_key) push_ndx_entry(ne);
	}
done:
	ndx_unlock();
	if (ne) return(read_recno(&ne->recno));
	else return(0);
}

static int insert_new(NDX_ENTRY *target)
{
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	int rc;

	clrflag(e_o_f);
	rc = btsrin(nhd->rootno, target);
	if (rc) {
		if (rc == DUPKEY) return(0); //only if UNIQUE_ON ise set
		else return(-1);
	}

	if (target->recno || target->pno) {
		NDX_PAGE *cpg = getpage(nhd->pagecnt, curr_ndx->pagebuf, 1);
		cpg->entry_cnt = 1;
		ne_put_pageno(cpg, 0, nhd->rootno);
		nhd->rootno = nhd->pagecnt++;
		insert_entry(cpg, 0, 0, target);
		write_page(nhd->rootno, (BYTE *)cpg);
		if (flagset(exclusive)) curr_ndx->w_hdr = 1;
		else write_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
	}
	return(1);
}

static int remove_old_insert_new(void)
{
	WORD bypr = curr_dbf->dbf_h->byprec;
	NDX_ENTRY old, new;
	int old_is_null = 0, new_is_null = 0, do_update = 0;

	blkswap(get_rec_buf(), get_rbk_buf(), bypr);
	if (!ndx_get_key(1, &old)) old_is_null = 1;		/* OLD KEY */
	blkswap(get_rec_buf(), get_rbk_buf(), bypr);
	if (!ndx_get_key(1, &new)) new_is_null = 1;		/* NEW KEY */
	do_update = (old_is_null || new_is_null)? 1 : keycmp(old.key, new.key);
	if (do_update) {
		if (!old_is_null) {
			NDX_HDR *nhd = &curr_ndx->ndx_h;
			int rm;
			rm = btsrdel(nhd->rootno, &old);
			if (rm) {
				if (old.pno) {
					nhd->rootno = old.pno;
					old.pno = 0;
				}
				else { //all empty, time to reset
					nhd->rootno = 0;
					nhd->pagecnt = 1;
				}
				if (flagset(exclusive)) curr_ndx->w_hdr = 1;
				else write_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
			}
		}
		if (!new_is_null && insert_new(&new) < 0) return(-1);
		return(!(old_is_null && new_is_null));
	}
	return(0);
}

int updndx(int del)
{
	int i, ctx_id = get_ctx_id();
	int ndx_updated = 0;
	int prim_ndx_updated = 0;
	for (i=0; i<MAXNDX; ++i) {
		if (_chkndx(ctx_id, i) == CLOSED) break;
		ndx_select(i);
		if (!flagset(exclusive)) {
			ndx_lock(0, HB_WRLCK);
			read_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
		}
		if (del) {
			ndx_updated = remove_old_insert_new();
			if (ndx_updated < 0) {
				ndx_unlock();
				return(-1);
			}
		}
		else { //insert only
			NDX_ENTRY target;
			if (!ndx_get_key(1, &target)) {
				ndx_unlock();
				continue; //null key, no insert
			}
			if (insert_new(&target) < 0) {
				ndx_unlock();
				return(-1);
			}
			ndx_updated = 1;
		}
		if (curr_ndx->w_hdr) {
			write_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
			curr_ndx->w_hdr = 0;
		}
		if (i == curr_ctx->order && ndx_updated) prim_ndx_updated = 1;
		ndx_unlock();
	}
	ndx_select(curr_ctx->order);
	if (prim_ndx_updated) curr_ndx->page_no = 0; //reset so nextndx() will do a btsrch() first
	if (curr_ctx->crntrec <= curr_dbf->dbf_h->reccnt) go(curr_ctx->crntrec);
	return(prim_ndx_updated);
}

void ndx_select(int seq)
{
	NDX *p, *q;
	int ctx_id = get_ctx_id(), found = 0, swap = 0;
 
	q = (NDX *)NULL; p = ndx_buf;
	while (p) {
		if ((p->ctx_id == ctx_id) && (p->seq == seq)) {
			 q = p;
			 found = 1;
		}
		if (!q || _chkndx(p->ctx_id, p->seq) == CLOSED) q = p;
		p->age++;
		p = p->nextndx;
	}
	if (!q) {
		swap = 1;
		q = p = ndx_buf;
		while (p) {
			if (_chkndx(p->ctx_id, p->seq) == SWAPPED) {
				q = p;
				swap = 0;
				break;
			}
			if (p->age > q->age) q = p;
			p = p->nextndx;
		}
	}
	curr_ndx = curr_ctx->ndxp = q;
	curr_ndx->fctl = &curr_ctx->ndxfctl[seq];
	if (swap) closendx();
	q->age = 0;
	q->ctx_id = ctx_id;
	q->seq = seq;
	if (!found) {
		q->page_no = 0;
		q->entry_no = 0;
	}
	if (index_active() && _chkndx(ctx_id, seq) == SWAPPED) openidx(OLD_FILE | 
			(flagset(exclusive)? EXCL : 0) | 
				(flagset(readonly)? RDONLY : 0)); //order != -1
}

int _chkndx(int ctx_id, int seq)
{
   if (ctx_id == -1) return(CLOSED);
   if (!table_ctx[ctx_id].ndxfctl[seq].filename) return(CLOSED);
   if (table_ctx[ctx_id].ndxfctl[seq].fd == -1) return(SWAPPED);
   return(IN_USE);
}

void ndx_chk(void)
{
   if (!index_active()) db_error2(NOINDEX, get_ctx_id() + 1);
}

char *get_ndx_exp(int n)
{
	ndx_select(n);
	return(curr_ndx->ndx_h.ndxexp);
}

int nkcmp(BYTE *p, BYTE *q)
{
	number_t num1, num2;
	num1.deci = num2.deci = REAL;
	num1.r = read_double(p);
	num2.r = read_double(q);
	return(num_rel(num1, num2, TQUESTN));
}

int skcmp(BYTE *p, BYTE *q, int len)
{
	int i,c1,c2;
	for (i=0; i<len; ++i) {
		c1 = *p++; c2 = *q++;
		if (i && !c1) return(0);
		if (c1 > c2) return(1);
		if (c1 < c2) return(-1);
	}
	return(0);
}

int keycmp(BYTE *target, BYTE *source)	/**target or source not null-terminated **/
{
	NDX_HDR *nhd = &curr_ndx->ndx_h;
	NDXCMBK *cmkp = &nhd->cmbkey;
	BYTE *p = target, *q = source;
	int i, ktyp = nhd->keytyp;
	if (ktyp != CMBK) return(ktyp? nkcmp(p, q) : skcmp(p, q, nhd->keylen));
	for (i=0; i<(int)cmkp->nparts; ++i) {
		int len,r;
		ktyp = cmkp->keys[i].typ;
		len = ktyp == CHARCTR? cmkp->keys[i].len : sizeof(double);
		r = ktyp == CHARCTR? skcmp(p, q, len) : nkcmp(p, q);
		if (r) return(r);
		p += len; q += len;
	}
	if (!isset(EXACT_ON)) return(0);
	return(i == cmkp->nparts);
}
