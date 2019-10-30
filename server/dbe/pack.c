/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/pack.c
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

void dbpack(void)
{
	char *recbuf,*msg="SCANNED";
	int byprec;
	recno_t to, from, treccnt;
	blkno_t nextblk;
	TABLE *hdr;
	int isdbt, dbtfd, tempfd;
	char tempfn[FULFNLEN];

	valid_chk();
	if (f_lock(1) < 0) db_error0(NOFLOCK);
	hdr = get_db_hdr();
	isdbt = table_has_memo(hdr);
	treccnt = hdr->reccnt;
	byprec = hdr->byprec;
	recbuf = get_aux_buf();
	curr_ctx->record_cnt = (recno_t)0;
	if (isdbt) {
		dbtfd = curr_ctx->dbtfd;
		form_tmp_fn(tempfn, FULFNLEN);
		tempfd = s_open(tempfn, NEW_FILE | OVERWRITE);
		nextblk = 1;
		if (put_next_memo(tempfd, nextblk) < 0) db_error(DBWRITE, tempfn);
	}
	for (to=1,from=0; to<=treccnt; ++to) {
		gorec(to);
		cnt_message(msg, 0);
		if (isdelete()) {
			if (!from) from = to + 1;
			for (; from<=treccnt; ++from) {
				gorec(from);
				if (isdelete()) continue;
				memcpy(recbuf, get_rec_buf(), byprec);
				del_recall(DEL_MKR);
				gorec(to);
				memcpy(get_rec_buf(), recbuf, byprec);
				save_rec(R_PACK);
				store_rec();
				break;
			}
			if (from > treccnt) break;
		}
	}
	curr_ctx->record_cnt = treccnt;
	cnt_message(msg, 1);
	hdr->reccnt = to - 1;
	if (!hdr->reccnt) setetof(0);
	setflag(fextd);
	setflag(dbmdfy);
	update_hdr(0);
	curr_ctx->record_cnt = treccnt - hdr->reccnt;
	cnt_message("REMOVED", 1);
	if (isdbt) {
		char dbtfn[FULFNLEN];
		form_memo_fn(dbtfn, FULFNLEN);
		if (isset(TALK_ON)) {
			push_string("Packing MEMO file...", 20);
			prnt('\n');
		}
		for (to=1; to<=hdr->reccnt; ++to) {
			FIELD *f;
			BYTE *tp;
			gorec(to);
			for (f=get_fdlist(hdr),tp=get_rec_buf()+1; f->name[0]!=EOH; tp+=f->tlength,++f)
			if (f->ftype=='M') {
				char temp[MEMOLEN + 1];
				blkno_t blkno;
				sprintf(temp, "%.*s", MEMOLEN, tp);
				blkno = atol(temp);
				if (blkno > 0) {
					int nbytes;
					blkno_t currblk = nextblk;
					nextblk = memo_copy(dbtfd, tempfd, blkno, currblk);
					if (nextblk == (blkno_t)-1) db_error0(MEMOCOPY);
					sprintf(temp, "%*ld", MEMOLEN, currblk);
					memcpy(tp, temp, MEMOLEN);
				}
			}
		}
		if (isset(TALK_ON)) {
			char msg[100];
			snprintf(msg, 100, "%06ld MEMO BLOCKS COPIED.", nextblk);
			push_string(msg, strlen(msg));
			prnt('\n');
		}
		put_next_memo(tempfd, nextblk);
		close(tempfd);
		close(dbtfd);
		unlink(dbtfn);
		rename(tempfn, dbtfn);
		curr_ctx->dbtfd = s_open(dbtfn, OLD_FILE);
	}
	if (index_active() && (treccnt>hdr->reccnt)) {
		if (isset(TALK_ON)) {
			push_string("Reindexing...", 13);
			prnt('\n');
		}
		reindex(0);
	}
	else gotop();
	f_unlock();
}
