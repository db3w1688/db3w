/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/page.c
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

int read_page(pageno_t pno, BYTE *buf)
{
	int fd = curr_ndx->fctl->fd, nbytes;
	WORD entry_cnt;
	NDX_PAGE *pg = (NDX_PAGE *)buf;
	if (lseek(fd, (off_t)pno * HBPAGESIZE, 0) == (off_t)-1) return(-1);
	if ((nbytes = read(fd, buf, HBPAGESIZE)) != HBPAGESIZE) return(-1);
	entry_cnt = read_word(&pg->entry_cnt);
	pg->entry_cnt = entry_cnt; //disk-to-host byte order
	pg->pad = 0;
	return(HBPAGESIZE);
}

int write_page(pageno_t pno, BYTE *buf)
{
	int fd = curr_ndx->fctl->fd, nbytes;
	WORD entry_cnt;
	NDX_PAGE *pg = (NDX_PAGE *)buf;
	entry_cnt = pg->entry_cnt;
	write_word(&pg->entry_cnt, entry_cnt); //disk order
	write_word(&pg->pad, 0);
	if (lseek(fd, (off_t)pno * HBPAGESIZE, 0) == (off_t)-1) return(-1);
	if ((nbytes = write(fd, buf, HBPAGESIZE))!= HBPAGESIZE) return(-1);
	if (curr_ndx->fsync) fsync(fd);
	pg->entry_cnt = entry_cnt;	//back to host order
	pg->pad = 0;
	return(HBPAGESIZE);
}

NDX_PAGE *getpage(pageno_t n, BYTE *buf, int exclusive)
{
	int newpg = n >= curr_ndx->ndx_h.pagecnt, newbuf = 0;

	ndx_lock(n, exclusive? HB_WRLCK : HB_RDLCK);
	if (!buf) {
		buf = aligned_alloc(sizeof(int), HBPAGESIZE);
		if (!buf) return(NULL);
		newbuf = 1;
	}
	if (newpg) {
		memset(buf, 0, HBPAGESIZE);
	}
	else {
		if (read_page(n, buf) < 0) {
			if (newbuf) free(buf);
			return(NULL);
		}
	}
	return((NDX_PAGE *)buf);
}

NDX_ENTRY *get_entry(NDX_PAGE *page, int item)
{
	char *tp = (char *)&page->entries[0];
	if (item < 0 || item > page->entry_cnt) 
		ndx_err(curr_ctx->order, INVALID_ENTRY);
	return((NDX_ENTRY *)(tp + item * curr_ndx->ndx_h.entry_size));
}

void move_entry(NDX_PAGE *pg1, int e1, NDX_PAGE *pg2, int e2) // entry here is defined as recno-key-pno
{
	NDX_ENTRY *ep1, *ep2, *ep3, *ep4;
	int keylen = curr_ndx->ndx_h.keylen;
 
 	ep1 = get_entry(pg1, e1);
 	ep2 = get_entry(pg2, e2);
 	ep3 = get_entry(pg1, e1 + 1);
 	ep4 = get_entry(pg2, e2 + 1);
 	ep4->pno = ep3->pno;
 	ep2->recno = ep1->recno;
 	memcpy(ep2->key, ep1->key, keylen);
}
 
void insert_entry(NDX_PAGE *p, pageno_t pno, int r, NDX_ENTRY *target)
{
	int isleaf = is_leaf(p);
	if ((isleaf && !target->recno) || (!isleaf && !target->pno)) 
		ndx_err(pno, INVALID_ENTRY);
	ne_put_recno(p, r, target->recno);
	memcpy(ne_get_key(p, r), target->key, curr_ndx->ndx_h.keylen);
	ne_put_pageno(p, r + 1, target->pno);
	if (is_leaf(p)) { curr_ndx->page_no = pno; curr_ndx->entry_no = r; }
}
