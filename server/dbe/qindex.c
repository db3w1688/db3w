/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/qindex.c
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

#include "vsort.h"

static void msg_display(char *msg)
{
	push_string(msg, strlen(msg));
	prnt('\n');
}

static void msg_progress(int pcnt, char *label, char *tag)
{
	char msg[256];
	static int last;
	if (last == 100) {
		last = 0;
		return;
	}
	last = pcnt;
	if (label) sprintf(msg, "%s: %d%% %s", label, pcnt, tag? tag : "");
	else sprintf(msg, "%d%% %s", pcnt, tag? tag : "");
	push_string(msg, strlen(msg));
	prnt(pcnt == 100? '\n':'\r');
}

static int qcomp(VSORT_CTL *v, VSORT_REC *p, VSORT_REC *q, int keylen, int keytyp)
{
	int i, r;
	BYTE *pk = p->key, *qk = q->key;
	// test for sentinel values first
	if (p->rec_no == q->rec_no) return(0);
	if (p->rec_no == 0 || q->rec_no == (recno_t)-1) return(-1);
	if (q->rec_no == 0 || p->rec_no == (recno_t)-1) return(1);
	if (keytyp == CMBK) {
		NDX_HDR *ndxh = v->hptr;
		NDXCMBK *cmkp = &ndxh->cmbkey;
		for (i=0; i<(int)cmkp->nparts; ++i) {
			int ktyp = cmkp->keys[i].typ;
			int len;
			len = ktyp == CHARCTR? cmkp->keys[i].len : sizeof(double);
			r = ktyp == CHARCTR? skcmp(pk,qk,len) : nkcmp(pk,qk);
			if (r) return(r);
			pk += len; qk += len;
		}
	}
	else {
		r = keytyp? nkcmp(pk, qk) : skcmp(pk, qk, keylen);
		if (r) return(r);
	}
	return(isset(UNIQUE_ON)? 0 : p->rec_no - q->rec_no);
}

static VSORT_REC *nextkeyent(VSORT_CTL *v, int fd)
{
	NDX_HDR *ndxh = v->hptr;
	VSORT_REC *start, *end, *qkent = v->kbuf1;
	DWORD keybufsz = v->vrec_size * v->nkeys;
	while (1) {
		int rr;
		start = v->keyptrs[0];
		end = v->keyptrs[1];
		if (start >= end) {
load_keys:
			rr = read(fd, v->kbuf, keybufsz);
			if (rr <= 0) return(NULL);
			end = v->keyptrs[1] = (VSORT_REC *)((BYTE *)v->kbuf + rr);
			if (rr < keybufsz) {
				memset((char *)end, v->descending? 0 : 0xff, keybufsz - rr);
			}
			start = v->keyptrs[0] = v->kbuf;
		}
		if (isset(UNIQUE_ON)) {
			while (start < end) {
				if (!qcomp(v, qkent, start, v->key_size, ndxh->keytyp)) {
					start = v->keyptrs[0] = (VSORT_REC *)((BYTE *)start + v->vrec_size);
				}
				else break;;
			}
			if (start >= end) goto load_keys;
			else break;
		}
		else break;
	}
	memcpy(qkent, start, v->vrec_size);
	v->keyptrs[0] = (VSORT_REC *)((BYTE *)start + v->vrec_size);
	return(qkent);
}		 

static int build_bt(VSORT_CTL *v, int f, pageno_t tpage, int recpp, pageno_t pno)
{
	NDX_HDR *ndxh = v->hptr;
	NDX_PAGE *cpg;
	VSORT_REC *k;
	NDX_ENTRY *p;
	int keysz = ndxh->keylen;
	int i, lastpg;
	int fd = v->tempvf[f].fd;

	cpg = getpage(pno, curr_ndx->pagebuf, 1);
	if (pno == ndxh->pagecnt) ++ndxh->pagecnt;
	if (tpage * recpp >= v->rec_count) { //leaf level
		DWORD nrec = v->rec_count - v->rec_prcd;

		lastpg = nrec <= (DWORD)recpp;
		if (lastpg) 
			recpp = (int)nrec;
		for (i=0; i<recpp; ++i) {
			ne_put_pageno(cpg, i, 0);	/* zero out pageno */
			k = nextkeyent(v, fd);
			if (!k) break;  // no more entries due to duplicates
			memcpy(ne_get_key(cpg, i), k->key, keysz);
			ne_put_recno(cpg, i, k->rec_no);
			++v->rec_prcd;
			if ((v->rec_prcd>>10) << 10 == v->rec_prcd) {
				if (isset(TALK_ON)) msg_progress((int)((long)v->rec_prcd * 100 / v->rec_count), "Loading index", STR_COMPLETE);
			}
			++cpg->entry_cnt;
		}
		ne_put_pageno(cpg, cpg->entry_cnt, 0);
		p = (NDX_ENTRY *)v->kbuf2;
		write_pageno(&p->pno, pno); 
		write_recno(&p->recno, 0);
		memcpy(p->key, ne_get_key(cpg, cpg->entry_cnt - 1), keysz);
		write_page(pno, (BYTE *)cpg);
	}
	else {
		pageno_t tpg = tpage * (recpp + 1);
		for (;;) {

			lastpg = build_bt(v, f, tpg, recpp, ndxh->pagecnt);

			if (lastpg < 0) return(-1);
			cpg = getpage(pno, curr_ndx->pagebuf, 1);
			p = (NDX_ENTRY *)v->kbuf2; //passed up by the leaf
			if (lastpg || (cpg->entry_cnt == recpp)) {
				ne_put_pageno(cpg, cpg->entry_cnt, read_pageno(p));
				write_pageno(p, pno); //pass it up
				write_page(pno, (BYTE *)cpg);
				break;
			}
			else {
				memcpy(get_entry(cpg, cpg->entry_cnt), p, ndxh->entry_size);
				++cpg->entry_cnt;
				write_page(pno, (BYTE *)cpg);
			}
		}
	}
	return(lastpg);
}

static int qindex_get_rec(VSORT_CTL *v, recno_t n)
{
	gorec(n + 1);
	return(0);
}

static int qindex_get_key(VSORT_CTL *v, VSORT_REC *vrec, recno_t n)
{
	NDX_HDR *ndxh = v->hptr;
	NDX_ENTRY target;
	vrec->rec_no = n + 1;	//recno 0 is sentinel
	if (!ndx_get_key(1, &target)) return(-1); //NULL key
	memcpy(vrec->key, target.key, ndxh->keylen);
	return(0);
}

int qindex(void)
{
	VSORT_CTL *v;
	TABLE *hdr;
	NDX_HDR *ndxh;
	char ndxpath[FULFNLEN], *tp;
	int rc, fd;

	strcpy(ndxpath, form_fn(curr_ndx->fctl->filename, 1));
	tp = strrchr(ndxpath, SWITCHAR);
	if (tp) *tp = '\0';
	hdr = curr_dbf->dbf_h;
	ndxh = &curr_ndx->ndx_h;
	v = vsort_init("HB*Engine", "Sorting keys", NULL, ndxpath, ndxh->keytyp, ndxh->keylen,
				0, hdr->byprec, 0, qindex_get_rec, qindex_get_key, qcomp, 
				isset(TALK_ON)? msg_progress : NULL, msg_display, &rc);
	if (!v) return(rc);
	v->hptr = ndxh;
	clrflag(e_o_f);
	rc = vsort_(v, curr_dbf->dbf_h->reccnt, 0);
	if (rc < 0) goto end;
	fd = v->tempvf[rc].fd;
	lseek(fd, (off_t)0, SEEK_SET);
	v->keyptrs[0] = v->keyptrs[1] = (VSORT_REC *)((BYTE *)v->kbuf + v->vrec_size * v->nkeys);
	memset((char *)v->kbuf1, 0, v->vrec_size);
	if (isset(UNIQUE_ON)) {
		recno_t count = 0;
		while (nextkeyent(v, fd)) ++count;
		v->rec_count = count;
		v->keyptrs[0] = v->keyptrs[1] = (VSORT_REC *)((BYTE *)v->kbuf + v->vrec_size * v->nkeys);
		memset((char *)v->kbuf1, 0, v->vrec_size);
		lseek(fd, (off_t)0, SEEK_SET);
	}
	v->rec_prcd = 0;
	rc = build_bt(v, rc, 1, ndxh->norecpp, ndxh->rootno = ndxh->pagecnt);
	curr_ndx->w_hdr = 1;
	if (isset(TALK_ON)) {
		char msg[80];
		sprintf(msg,"%06ld RECORD%s INDEXED.        ",v->rec_prcd,v->rec_prcd > (recno_t)1? "S":"");
		push_string(msg,strlen(msg));
		prnt('\n');
	}
	rc = 0;
end:
	vsort_end(v, -1);
	return(rc);
	/* if successful, k is -1 */
}
