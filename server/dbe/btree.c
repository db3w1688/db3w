/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/btree.c
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
#include "hb.h"
#include "dbe.h"

static int btlr(pageno_t pno, recno_t recnum) //go to the leaf page and check the record number to do compare
{
	NDX_PAGE *p;
	recno_t recno;
 
	p = getpage(pno, curr_ndx->pagebuf, 0);
	while (!is_leaf(p)) p = getpage(ne_get_pageno(p, p->entry_cnt), curr_ndx->pagebuf, 0);
	recno = ne_get_recno(p, p->entry_cnt - 1);
	return(recnum > recno ? 1 : (recnum < recno ? -1 : 0));
}

recno_t btsrch(pageno_t pno, int mode, NDX_ENTRY *target)
{
	NDX_PAGE *cpg = NULL;
	int e, e2, r, r0, r1;
	recno_t n3 = 0;
	pageno_t pn3, pn0;
 
	if (pno == 0) return(0);

	cpg = getpage(pno, NULL, 0);
	if (!cpg) return((recno_t)-1);

	e = 1; e2 = -1; r1 = r0 = -1; r = 0; pn3 = ne_get_pageno(cpg, 0);
	while ((e > 0) && (r < (int)cpg->entry_cnt)) {
		pageno_t pageno = ne_get_pageno(cpg, r1 = r+1);
		e = keycmp(target->key, ne_get_key(cpg, r));
		if (!e) {
			if (mode == TGREATER || mode == TGTREQU) {
				e2 = 0; e = 1;
				goto next_r;
			}
			n3 = target->recno;
			if (n3) {
				if (pn3) {
					e = btlr(pn3, n3);
				}
				else {
					recno_t recno = ne_get_recno(cpg, r);
					e = n3 > recno? 1 : (n3 < recno? -1 : 0);
				}
			}
			if (!e && !pn3) {
				if (mode == TLESS) {
					if (!r) goto end;
					r = r0;
				}
found:
				curr_ndx->page_no = pno;
				curr_ndx->entry_no = r;
				n3 = ne_get_recno(cpg, r);
				goto end;
			}
		}
		if (e > 0) {
next_r:
			r0 = r; r = r1; pn0 = pn3; pn3 = pageno;
		}
	}
	if (!pn3 && mode != TEQUAL) {
		if (mode==TGREATER) {
			if (e < 0) goto found;
			goto end;
		}
		if (mode == TGTREQU) {
			if (e > 0) {
				if (!e2) { r = r0; goto found; }
				return(0);
			}
			if (r) {
				if (e2) goto found;
				r = r0; goto found;
			}
			goto end;
		}
		if (r) {
			r = r0; goto found;
		}
	}
	n3 = btsrch(pn3, mode, target);
	if (!n3 && (mode == TLESS || mode == TGTREQU) && r0 != -1) n3 = btsrch(pn0, mode, target);

end:
	if (cpg) free(cpg);
	return(n3);
}

pageno_t btxrch(int dir, pageno_t pn, NDX_ENTRY *target) //find the next page in traversal
{
	NDX_PAGE *cpg = getpage(pn, curr_ndx->pagebuf, 0);
	int e, r, n;
	recno_t recno;
	pageno_t ppn, npn, np;

	if (is_leaf(cpg)) return(0);
	if (dir == FORWARD) {
		ppn = ne_get_pageno(cpg, 0);
		e = 1; r = 0; n = cpg->entry_cnt;
		while ((e > 0) && (r < n)) {
			npn = ne_get_pageno(cpg, r + 1);
			e = keycmp(target->key, ne_get_key(cpg, r));
			if (!e) {
				e = btlr(ppn, target->recno);
				getpage(pn, curr_ndx->pagebuf, 0);
				if (!e) return(npn);
			}
			if (e > 0) { ++r; ppn = npn; }
		}
		return(btxrch(dir, ppn, target));
	}
	else {
		r = cpg->entry_cnt - 1;
		ppn = ne_get_pageno(cpg, r + 1);
		npn = ne_get_pageno(cpg, r);
		e = -1;
		while ((e < 0) && (r >= 0)) {
			e = keycmp(target->key, ne_get_key(cpg, r));
			if (!e) {
				e = btlr(npn, target->recno);
				if (!e) np = btxrch(dir, npn, target);
				if (np) return(np);
				getpage(pn, curr_ndx->pagebuf, 0); //read back the page
			}
			if (e <= 0) {
				--r; 
				ppn = npn;
				if (r >= 0) npn = ne_get_pageno(cpg, r);
				else npn = 0;
			}
		}
		if ((np = btxrch(dir, ppn, target)) == 0) return(npn);
		else return(np);
	}
}

int btsrin(pageno_t pno, NDX_ENTRY *target)
{
	NDX_PAGE *p = NULL, *q = NULL;
	int r, e, rc = -1;
	pageno_t pn3;
 
	if (!pno) return(0);
	p = getpage(pno, NULL, 1);
	if (!p) goto end;
	r = 0; e = 1; pn3 = ne_get_pageno(p, 0);
	while ((e > 0) && (r < (int)p->entry_cnt)) {
		recno_t recno = ne_get_recno(p, r);
		pageno_t pageno = ne_get_pageno(p, r + 1);
		e = keycmp(target->key, ne_get_key(p, r));
		if (!e) {
			if (isset(UNIQUE_ON)) {
				rc = DUPKEY;
				goto end;
			}
			if (!pn3) {
				e = target->recno > recno? 1 : (target->recno < recno? -1 : 0);
			}
			else {
				e = btlr(pn3, target->recno);
			}
		}
		if (!e) {
			rc = DUPKEY;
			goto end;
		}
		if (e > 0) { ++r; pn3 = pageno; }
	}
	rc = btsrin(pn3, target);
	if (rc) goto end; //error
	if (target->recno || target->pno) { //lower level sent up unfinished work
		int n, i;
		e = curr_ndx->ndx_h.norecpp;
		n = e / 2;
		if ((int)p->entry_cnt < e) {
			for (i=p->entry_cnt++; i>r; --i) move_entry(p, i - 1, p, i);
			insert_entry(p, pno, r, target);
			write_page(pno, (BYTE *)p);
			target->recno = target->pno = 0;
		}
		else { //need to split page
			q = getpage(pn3 = curr_ndx->ndx_h.pagecnt, NULL, 1);
			if (!q) {
				rc = NOMEMORY;
				goto end;
			}
			q->entry_cnt = e - n; //set entry_cnt to avoid error in get_entry()
			if (r == n) {
				for (i=0; i<e-n; i++) move_entry(p, i + n, q, i);
				insert_entry(p, pno, n, target);
			}
			else {
				if (r < n) {
					for (i=0; i<e-n; i++) move_entry(p, i + n, q, i);
					for (i=n; i>r; --i) move_entry(p, i - 1, p, i);
					insert_entry(p, pno, r, target);
				}
				else { //r > n
					for (i=n+1; i<r; i++) move_entry(p, i, q, i - n - 1);
					insert_entry(q, pn3, r - n - 1, target);
					for (i=r; i<e; i++) move_entry(p, i, q, i - n);
				}
				target->recno = ne_get_recno(p, n);
				memcpy(target->key, ne_get_key(p, n), curr_ndx->ndx_h.keylen);
			}
			ne_put_pageno(q, 0, ne_get_pageno(p, n + 1));
			p->entry_cnt = is_leaf(p)? n + 1 : n; //leaf page has the last entry being used as non-leaf key
			target->pno = curr_ndx->ndx_h.pagecnt++;
			write_page(pno, (BYTE *)p);
			write_page(pn3, (BYTE *)q);
			if (curr_ndx->fsync) write_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
			else curr_ndx->w_hdr = 1;
		}
	}
	rc = 0;
end:
	if (q) free(q);
	if (p) free(p);
	return(rc);
}

static int del(pageno_t ppno, pageno_t apno, NDX_PAGE *q, int itmno, NDX_ENTRY *target)
//only called when we have a match in q at itemno which is to be deleted
{
	NDX_PAGE *p = NULL;
	pageno_t pn;
	int rm = 0, is_leaf;
	
	if (!ppno) return(1);
	p = getpage(ppno, NULL, 1);
	if (!p) sys_err(NOMEMORY);
	pn = ne_get_pageno(p, p->entry_cnt);
	is_leaf = !pn;
	if (del(pn, apno, q, itmno, target)) {
		if (!p->entry_cnt) { //should not reach here
			rm = 1;
			goto end;
		}
		if (is_leaf) {
			--p->entry_cnt;
			write_page(ppno, (BYTE *)p);
			if (!p->entry_cnt) {
				rm = 1;
				goto end;
			}
			pn = ne_get_pageno(q, itmno + 1);
			move_entry(p, p->entry_cnt - 1, q, itmno);
			ne_put_recno(q, itmno, 0); //q is non-leaf
			ne_put_pageno(q, itmno + 1, pn);
			write_page(apno, (BYTE *)q);
		}
		else { //non-leaf
			if (target->pno) { //lower level sent up an orphan
				ne_put_pageno(p, p->entry_cnt, target->pno);
				target->pno = 0;
				write_page(ppno, (BYTE *)p);
			}
			else { //move the last item up
				pn = ne_get_pageno(q, itmno + 1);
				ne_put_pageno(p, p->entry_cnt, pn); //save the right-side pointer
				move_entry(p, p->entry_cnt - 1, q, itmno);
				write_page(apno, (BYTE *)q);
				--p->entry_cnt;
				write_page(ppno, (BYTE *)p);
				if (!p->entry_cnt) { //we are empty so send up the orphan
					target->pno = ne_get_pageno(p, 0);
					rm = 1;
				}
			}
		}
	}
end:
	if (p) free(p);
	return(rm);
}

int btsrdel(pageno_t pno, NDX_ENTRY *target)
{
	NDX_PAGE *p = NULL;
	pageno_t pn3, npno;
	int e, r, match = 0, rm = 0;
	
	if (!pno) 
		ndx_err(curr_ctx->order, NTNDX); //no match found
	p = getpage(pno, NULL, 1);
	if (!p) sys_err(NOMEMORY);
	r = 0; e = 1; pn3 = ne_get_pageno(p, 0);
	while ((e > 0) && (r < p->entry_cnt)) {
		recno_t recno = ne_get_recno(p, r);
		e = keycmp(target->key, ne_get_key(p, r));
		if (!e) { //match
			if (!pn3) e = target->recno > recno? 1 : (target->recno < recno? -1 : 0);
			else { //non-leaf
				e = btlr(pn3, target->recno); //go to last leaf and compare recnum of last entry to see if we do have a match
			}
			if (!e) match = 1;
		}
		if (e > 0) { ++r; pn3 = ne_get_pageno(p, r); }
	}
	if (match) rm = del(pn3, pno, p, r, target);
	else rm = btsrdel(pn3, target);
	if (rm) {
		if (target->pno) { //lower level sent up an orphan
			ne_put_pageno(p, r, target->pno);
			target->pno = 0;
			write_page(pno, (BYTE *)p);
		}
		else {
			if (r < p->entry_cnt) {
				npno = ne_get_pageno(p, r + 1);
				for (e=r; e<p->entry_cnt - 1; ++e) move_entry(p, e + 1, p, e);
				ne_put_pageno(p, r, npno);
			}
			--p->entry_cnt;
			write_page(pno, (BYTE *)p);
			if (!p->entry_cnt) {
				target->pno = ne_get_pageno(p, 0);
				rm = 1;
				goto end;
			}
		}
		rm = 0;
	}
end:
	if (p) free(p);
	return(rm);
}
