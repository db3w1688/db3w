/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/init.c
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
#include <memory.h>
#include <string.h>
#include "hb.h"
#include "dbe.h"

DBF *alloc_dbf_buf(int n)
{
	DBF *q;

	if (!n) return((DBF *)NULL);
	if (!(q = (DBF *)malloc(sizeof(DBF)))) {
		maxbuf -= n;
		return((DBF *)NULL);
	}
	memset(q, 0, sizeof(DBF));
	q->nextdb = alloc_dbf_buf(n - 1);
	return(q);
}

void release_dbf_buf(DBF *dbp)
{
	if (!dbp) return;
	release_dbf_buf(dbp->nextdb);
	if (dbp->dbf_h) free(dbp->dbf_h);
	if (dbp->dbf_b) free(dbp->dbf_b);
	free(dbp);
}

NDX *alloc_ndx_buf(int n)
{
	NDX *q;

	if (!n) return((NDX *)NULL);
	if (!(q = (NDX *)malloc(sizeof(NDX)))) {
out:
		maxndx -= n;
		return(NULL);
	}
	q->pagebuf = aligned_alloc(sizeof(int), HBPAGESIZE);
	if (!q->pagebuf) {
		free(q);
		goto out;
	}
	q->nextndx = alloc_ndx_buf(n - 1);
	return(q);
}

void release_ndx_buf(NDX *ndxp)
{
	if (!ndxp) return;
	release_ndx_buf(ndxp->nextndx);
	free(ndxp->pagebuf);
	free(ndxp);
}

void db_init0(int mode)
{
	if (mode) { //called by sys_reset()
		hb_errcode = 1;		 /*** ignore further errors ***/
		cleardb();
	}
	DBF *p = dbf_buf;
	while (p) {
		p->ctx_id = -1;
		p = p->nextdb;
	}
	NDX *q = ndx_buf;
	while (q) {
		q->ctx_id = q->seq = -1;
		q = q->nextndx;
	}
	int i,j;
	for (i=0; i<MAXCTX + 1; ++i) { //including AUX
		CTX *p = &table_ctx[i];
		p->crntrec = 0;
		memset(&p->scope, 0, sizeof(struct _scope));
		p->order = -1;
		p->rela = -1;
		p->dbfctl.filename = (char *)NULL;
		p->dbfctl.fd = p->dbtfd = p->exfd = -1;
		p->dbfctl.fp = NULL;
		p->alias[0] = '\0';
		{
			struct flags *f = &p->db_flags;
			memset(f, 0, sizeof(p->db_flags));
			f->e_o_f = f->t_o_f = TRUE;
		}
		for (j=0; j<MAXNDX; ++j) {
			p->ndxfctl[j].filename = (char *)NULL;
			p->ndxfctl[j].fd = -1;
			p->ndxfctl[j].fp = NULL;
		}
		p->filter = p->relation = p->locate = NULL;
		p->rbkctl.fd = -1;
		p->rbkctl.recno = p->rbkctl.flags = 0;
	}
#ifdef EMBEDDED
	fn_buf.next_avail = fltr_buf.next_avail = rela_buf.next_avail = loc_buf.next_avail = 0;
#endif
	memfctl.filename = frmfctl.filename = profctl.filename = (char *)NULL;
	memfctl.fd = frmfctl.fd = profctl.fd = -1;
	memfctl.fp = frmfctl.fp = profctl.fp = NULL;
	ctx_select(PRIMARY);
}

int db_init(void)
{
	DBF *dbp;
	int i;
	for (i=0; i<MAXCTX + 1; ++i) {
		table_ctx[i].fltr_pbuf = (CODE_BLOCK *)malloc(FLTRPBUFSZ);
		table_ctx[i].loc_pbuf = (CODE_BLOCK *)malloc(LOCPBUFSZ);
		if (!table_ctx[i].fltr_pbuf || !table_ctx[i].loc_pbuf) return(-1);
	}
#ifdef EMBEDDED
//to prevent memory fragmentation
	fn_buf.buf = malloc(FNBUFSZ);
	rela_buf.buf = malloc(EXPBUFSZ);
	loc_buf.buf = malloc(EXPBUFSZ);
	fltr_buf.buf = malloc(EXPBUFSZ);
	if (!fn_buf.buf || !fltr_buf.buf || !loc_buf.buf || !rela_buf.buf) return(-1);
#endif
	if (maxbuf < MIN_NDBUF) maxbuf = MIN_NDBUF;
	dbp = alloc_dbf_buf(maxbuf + 1);
	if (maxbuf < MIN_NDBUF) return(-1);
	dbp->ctx_id = AUX; //first one in the link is reserved for AUX buffer
	if (!(dbp->dbf_h = (TABLE *)malloc(MAXHDLEN)) || !(dbp->dbf_b = malloc(MAXBUFSZ))) return(-1);
	memset(dbp->dbf_h, 0, MAXHDLEN);
	table_ctx[AUX].dbp = dbp;
	dbf_buf = dbp->nextdb; //table buffers start at next link
	ndx_buf = alloc_ndx_buf(maxndx);
	if (maxndx < MIN_NIBUF) return(-1);

	db_init0(0);
	return(0);
}

void db_release_mem(void)
{
	int i;
	release_ndx_buf(ndx_buf);
	release_dbf_buf(table_ctx[AUX].dbp); //AUX is at the start of the chain
	for (i=0; i<MAXCTX + 1; ++i) {
		if (table_ctx[i].loc_pbuf) free(table_ctx[i].loc_pbuf);
		if (table_ctx[i].fltr_pbuf) free(table_ctx[i].fltr_pbuf);
	}
#ifdef EMBEDDED
	if (loc_buf.buf) free(loc_buf.buf);
	if (fltr_buf.buf) free(fltr_buf.buf);
	if (rela_buf.buf) free(rela_buf.buf);
	if (fn_buf.buf) free(fn_buf.buf);
	loc_buf.buf = fltr_buf.buf = rela_buf.buf = fn_buf.buf = NULL;
	loc_buf.next_avail = fltr_buf.next_avail = rela_buf.next_avail = fn_buf.next_avail = 0;
#endif
}
