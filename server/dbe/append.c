/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/append.c
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
#include <fcntl.h>
#include <sys/stat.h>
#include "hb.h"
#include "dbe.h"

#define BATCHMAX	100

void _apblank(int mode) //mode==0 is for append from...
{
	TABLE *hdr = curr_dbf->dbf_h;
	recno_t nrec = 1;

	valid_chk();
	if (mode > 1) { //batch
		if (index_active()) db_error2(NOINDEX, get_ctx_id() + 1);
		nrec = pop_int(1, BATCHMAX);
		curr_ctx->record_cnt = 0;
	}
	if (flagset(readonly)) db_error0(DB_READONLY);
	if (hdr->reccnt >= MAXNRECS)
		db_error(TOOMANYRECS, curr_ctx->dbfctl.filename);
	if (!(hdr->access & HBAPPEND)) db_error(DB_ACCESS, curr_ctx->dbfctl.filename);
	lock_hdr();
	clretof(0);
	while (nrec--) {
		gorec(hdr->reccnt + 1);
		if (mode && r_lock(curr_ctx->crntrec, HB_WRLCK, 0) < 0) db_error0(NORLOCK);
		memset(get_rec_buf(), ' ', hdr->byprec);
		save_rec(R_APPEND);
		if (mode) {
			store_rec();
			cnt_message("ADDED", 0);
		}
	}
	unlock_hdr();
	if (mode) cnt_message("ADDED", 1);
}

static int ctx_id;
static int txtapnd, isdbt, fdbt, tdbt, tempfd;
static TABLE *hdt = NULL, *hdf = NULL;
static char *trec = NULL, *frec = NULL, *dbtbuf = NULL, *tempfn = NULL;
static FILE *txtfp;
static jmp_buf sav_env;

#define ASTART		0
#define AINPUT		1
#define ASEEN_DLMTR	2
#define AWAIT_COMMA	3
#define AFEND		0x80
#define AREND		0x81

static void _apndrec(char *frec, char *trec)
{
	FIELD *f, *ff;
	int i;
	char *from, *to = trec + 1;
	char temp[STRBUFSZ], temp2[STRBUFSZ];
	int txtstate = ASTART;
	_apblank(0);		/** don't increment reccnt **/
	memset(trec, ' ', hdt->byprec);
	for (i=0,f=hdt->fdlist; i<hdt->fldcnt; ++i,to+=f->tlength,++f) {
		if (f->x1 && f->ftype != 'M') {      /* MEMO fields get copied by aprec() */
			memset(temp, ' ', STRBUFSZ);
			if (!txtapnd) {
				ff = &hdf->fdlist[f->x1-1];
				from = frec + ff->x1;
				memcpy(temp, from, ff->tlength);
			}
			else {
				int i = 0, c, dlmtr = txt_get_dlmtr(txtapnd);
				if (txtstate != AREND) txtstate = ASTART;
				while (txtstate != AREND && txtstate != AFEND && (c = fgetc(txtfp)) != EOF) {
					if (c == dlmtr) switch (txtstate) {
					case ASTART:
						txtstate = ASEEN_DLMTR;
						continue;

					case ASEEN_DLMTR:
						txtstate = AWAIT_COMMA;
						continue;

					case AINPUT:
					default:
						goto gg;
					}
					else if (c == ',' && dlmtr) {
						if (txtstate!=ASEEN_DLMTR) txtstate = AFEND;
						else goto gg;
					}
					else if (c=='\n') txtstate = AREND;
					else {
						if (txtstate==AWAIT_COMMA) continue;
						if (txtstate==ASTART) txtstate = AINPUT;
gg:
						if (i<STRBUFSZ) temp[i++] = c;
					}
				}
			}
			switch (f->ftype) {
			case 'C':
				if (!txtapnd && ff->ftype == 'N') sprintf(temp2, "%-*s", f->tlength, temp); //right justify
				else sprintf(temp2, "%*s", f->tlength, temp);
				memcpy(to, temp2, f->tlength);
				break;
			case 'N': {
				number_t num;
				temp[ff->tlength] = '\0';
				num = str2num(temp);
				num2str(num, f->tlength, f->dlength, temp);
				memcpy(to, temp, f->tlength);
				break; }
			case 'D':
				if (!txtapnd && ff->ftype == 'D') memcpy(to, temp, f->tlength);
				break;
			case 'L':
				if (ff->ftype == 'L') *to = *from;
				else *to = 'F';
				break;
			default:
				break;
			}
			
		}
	}
	if (txtapnd && txtstate!=AREND) while (!feof(txtfp) && fgetc(txtfp)!='\n');
	memcpy(get_db_rec(), trec, get_rec_sz());
	save_rec(R_APPEND);
}

void apinit(int mode)
{
	TABLE *hdr;
	FIELD *ff, *ft;
	int hdlen, i;
	int empty = FALSE, foffset, aperr = 0;
	hdt = hdf = NULL;
	trec = frec = NULL;
	txtapnd = mode;
	valid_chk();
	ctx_id = get_ctx_id();
	hdr = get_db_hdr();
	if (hdr->access < HBAPPEND) db_error0(DB_ACCESS);
	hdlen = get_hdr_len(hdr);
	memcpy(sav_env, env, sizeof(jmp_buf));
	if (aperr = setjmp(env)) {
		if (get_ctx_id() == AUX) ctx_select(ctx_id);
		goto end;
	}
	hdt = (TABLE *)malloc(hdlen);
	trec = malloc(get_rec_sz());
	if (!hdt || !trec) {
		aperr = NOMEMORY;
		goto end;
	}
	memcpy(hdt, hdr, hdlen);
	if (txtapnd) {
		char txtfn[FULFNLEN],*tp;
		HBVAL *v;
		strcpy(txtfn,form_fn1(1, curr_ctx->dbfctl.filename));
		tp = txtfn + strlen(txtfn);
		v = pop_stack(CHARCTR);
		sprintf(tp, "%.*s", v->valspec.len, v->buf);
		if (!strchr(tp, '.')) strcat(tp, ".txt");
		txtfp = fopen(txtfn, "r");
		if (!txtfp) db_error(DBOPEN, txtfn);
		if (feof(txtfp)) empty = TRUE;
		for (i=0,foffset=1,ft=hdt->fdlist; i<hdt->fldcnt; ++i,foffset+=ft->tlength,++ft) {
			ft->x1 = ft->x0 >= HBMODIFY? foffset : 0;
		}
	}
	else {
		ctx_select(AUX);
		use(U_USE);
		hdr = get_db_hdr();
		if (hdr->access < HBREAD) db_error0(DB_ACCESS);
		hdlen = get_hdr_len(hdr);
		hdf = (TABLE *)malloc(hdlen);
		frec = malloc(get_rec_sz());
		if (!hdf || !frec) {
			aperr = NOMEMORY;
			goto end;
		}
		memcpy(hdf, hdr, hdlen);
		isdbt = FALSE;
		for (i=0,ft=hdt->fdlist; i<hdt->fldcnt; ++i,++ft) {
			int j;
			ft->x1 = 0;
			if (ft->x0 < HBMODIFY) continue; //access less than HDMODIFY
			for (j=0,ff=hdf->fdlist,foffset=1; j<hdf->fldcnt; ++j,foffset+=ff->tlength,++ff) {
				if (!strcmp(ff->name, ft->name)) {
					if (ff->x0 < HBREAD) continue;
					if (ff->ftype=='M' && ft->ftype=='M') isdbt = TRUE;
					ft->x1 = j + 1;
					ff->x1 = foffset;
					break;
				}
			}
		}
		if (isdbt) fdbt = get_dbt_fd();
		if (!iseof()) memcpy(frec, get_db_rec(), get_rec_sz()); else empty = TRUE;
		ctx_select(ctx_id);
		if (isdbt) {
			int fnsz;
			char *hbtmp = curr_ctx->dbfctl.filename;
			fnsz = strlen(hbtmp) + 20;
			if (!(tempfn = malloc(fnsz)) || !(dbtbuf = malloc(BLKSIZE))) sys_err(NOMEMORY);
			strcpy(tempfn, hbtmp);
			hbtmp = strrchr(tempfn, '.'); /* must have .DBF */
			strcpy(hbtmp, ".tmp");
			if ((tempfd = open(tempfn, O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE)) == -1) {
				db_error(MEMOCREATE, tempfn);
			}
			tdbt = get_dbt_fd();
		}
	}
	if (empty) *trec = '\0';
	else {
		clrflag(fsync);
		_apndrec(frec, trec);
	}

end:
	memcpy(env, sav_env, sizeof(jmp_buf));
	if (aperr) {
		if (hdt) free(hdt);
		if (hdf) free(hdf);
		if (trec) free(trec);
		if (frec) free(frec);
		if (dbtbuf) free(dbtbuf);
		if (tempfn) free(tempfn);
		sys_err(aperr);
	}
}

void aprec(void) //take care of MEMO fields and call store_rec()
{
	FIELD *f, *ff;
	int i;
	char *to;
	if (*trec == '\0') return; //empty source
	if (!txtapnd) {
		for (i=0,f=hdt->fdlist,to=trec+1; i<hdt->fldcnt; ++i,to+=f->tlength,++f)
		if (f->x1 && f->ftype == 'M') {      /* MEMO fields */
			blkno_t fblkno, tblkno;
			char temp[16];
			ff = &hdf->fdlist[f->x1 - 1];
			if (ff->ftype == 'M') {
			sprintf(temp, "%.*s", ff->tlength, frec+ff->x1);   /* ff->tlength should be 10 */
				fblkno = atol(temp);
				if (fblkno) {
					blkno_t nblks;
					tblkno = get_next_memo(tdbt);
					if (tblkno == (blkno_t)-1) db_error0(MEMOREAD);
					nblks = memo_copy(fdbt, tdbt, fblkno, tblkno);
					if (nblks == (blkno_t)-1) db_error0(MEMOCOPY);
					sprintf(temp, "%10ld", tblkno);
					memcpy(to, temp, 10);
					if (put_next_memo(tdbt, tblkno + nblks) < 0) db_error0(MEMOWRITE);
				}
			}
		}
		if (isdbt) {
			memcpy(get_db_rec(), trec, get_rec_sz());
			save_rec(R_APPEND);
		}
	}
	store_rec();
	cnt_message("ADDED",0);
}
 
void apnext(void)
{
	int end_input = 0;
	if (!txtapnd) {
		ctx_select(AUX);
		skip(1);
		if (!iseof()) {
			memcpy(frec, get_db_rec(), get_rec_sz());
		}
		else end_input = 1;
		ctx_select(ctx_id);
		if (!end_input) curr_ctx->scope.count++;
	}
	else {
		int c = fgetc(txtfp);
		if (c == EOF_MKR || c == EOF) end_input = TRUE;
		else ungetc(c,txtfp);
	}
	if (!end_input) _apndrec(frec, trec);
	else {
		gobot();
		skip(1); //set the eof flag
	}
}
 
void apdone(void)
{
	cnt_message("ADDED", 1);
	if (txtapnd) fclose(txtfp);
	else {
		ctx_select(AUX);
		use(0);
		ctx_select(ctx_id);
	}
	if (isdbt) {
		if (dbtbuf) free(dbtbuf);
		if (tempfd!=-1) close(tempfd);
		if (tempfn) {
			unlink(tempfn);
			free(tempfn);
		}
	}
	if (frec) free(frec);
	if (hdf) free(hdf);
	if (trec) free(trec);
	if (hdt) free(hdt);
	if (dbtbuf) free(dbtbuf);
	if (tempfn) free(tempfn);
	setflag(fsync);
}
