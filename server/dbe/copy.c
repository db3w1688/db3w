/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/copy.c
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

static int ctx_id;
static int has_dbt, has_blob, fdbt, tdbt, tempfd, txtcpy;
static TABLE *hdt, *hdf;
static char *trec, *frec, *dbtbuf, *tempfn;
static FILE *txtfp;
static jmp_buf sav_env;

void cpinit(int mode)	/** 0x80: text, 0x7f: delmtr **/
{
	int nflds, hdlen, recsz, doffset = 0, toffset, foffset, i, cperr = 0;
	char fldn[FLDNAMELEN + 1], tfn[FULFNLEN], *tp;
	HBVAL *v;
	FIELD *ff, *ft;

	valid_chk();
	ctx_id = get_ctx_id();
	hdf = get_db_hdr();
	hdt = NULL;
	trec = frec = NULL;
	hdlen = get_hdr_len(hdf); //in memory struct size
	recsz = get_rec_sz();
	if (txt_is_set(mode)) txtcpy = mode;
	else {
		v = (HBVAL *)&pdata[cb_data_offset(mode)];
		doffset = (int)get_long(v->buf);
	}
	has_dbt = has_blob = FALSE;
	frec = malloc(recsz);
	hdt = (TABLE *)malloc(hdlen);
	trec = malloc(recsz);
	if (!frec || !hdt || !trec) {
		cperr = NOMEMORY;
		goto end;
	}
	memset((char *)hdt, 0, hdlen);
	memcpy(sav_env, env, sizeof(jmp_buf));
	if (cperr = setjmp(env)) {
		if (get_ctx_id() == AUX) ctx_select(ctx_id);
		goto end;
	}
	nflds = pop_int(0, MAXFLD);
	if (nflds) {
		ft = hdt->fdlist;
		for (i=nflds,toffset=1; i; --i) {
			int j;
			v = (HBVAL *)stack_peek(i - 1);
			sprintf(fldn, "%.*s", v->valspec.len, v->buf);
			for (j=0,ff=hdf->fdlist,foffset=1; j<hdf->fldcnt; ++j,foffset+=ff->tlength,++ff)
				if (!strcmp(ff->name, chgcase(fldn, 1)) && ff->x0 >= HBREAD && !ft->x0) {
					memcpy(ft, ff, sizeof(FIELD));
					if (ff->ftype == 'M') has_dbt = TRUE;
					else if (ff->ftype == 'B') has_blob = TRUE;
					ft->x0 = foffset;
					ft->x1 = ff->tlength;
					++ft;
					toffset += ff->tlength;
					break;
				}
		}
		hdt->fldcnt = nflds;
		hdt->byprec = toffset;
		hdt->id = (has_dbt? DB3_MEMO : 0) | (has_blob? DB3_BLOB : 0) | DB5_MKR;
		hdt->reccnt = (recno_t)0;
		clr_stack(nflds);
	}
	else {
		memcpy(hdt, hdf, hdlen);
		table_set_mkr(hdt, DB5_MKR);
		hdt->reccnt = 0;
		hdt->meta_len = 0;
		has_dbt = !!(hdt->id & DB3_MEMO);
		has_blob = !!(hdt->id & DB3_BLOB);
		for (i=0,ft=hdt->fdlist,toffset=1; i<hdt->fldcnt; ++i,++ft) {
			ft->x0 = toffset;
			ft->x1 = ft->tlength;
			toffset += ft->tlength;
		}
	}
	hdt->hdlen = HDRSZ + hdt->fldcnt * FLDWD; //force db7 on-disk header size
	if (hdt->fldcnt < MAXFLD) hdt->hdlen += sizeof(DWORD);
	if (doffset > 0 && doffset < hdt->hdlen) db_error2(OFFSET_TOO_SMALL, doffset);
	hdt->doffset = doffset? doffset : hdt->hdlen;
	hdt->access = HBALL; /** grant all access for the creator **/
	if (!txtcpy && has_dbt) {
		fdbt = get_dbt_fd();
	}

	v = pop_stack(CHARCTR);
	if (v->buf[0] == SWITCHAR) strcpy(tfn, rootpath);
	else strcpy(tfn, form_fn1(1, curr_ctx->dbfctl.filename));
	tp = tfn + strlen(tfn);
	sprintf(tp, "%.*s", v->valspec.len, v->buf);
	if (!txtcpy) {
		push_string(tp, strlen(tp));
		ctx_select(AUX);
		use(U_USE | U_CREATE | U_EXCL | (has_dbt? U_DBT : 0));
		memcpy(get_db_hdr(), hdt, hdlen);
		setflag(dbmdfy);
		setflag(fextd);
		clrflag(fsync); //don't do fsync
		if (has_dbt) tdbt = get_dbt_fd();
		ctx_select(ctx_id);
	}
	else {
		if (!strchr(tp, '.')) strcat(tp, ".txt");
		txtfp = fopen(tfn, "w");
		if (!txtfp) db_error(DBCREATE, tfn);
		if (txt_is_struct_exp(txtcpy)) for (i=0,ft=hdt->fdlist; i<hdt->fldcnt; ++i,++ft) {
			fprintf(txtfp, "%s,%c,%d,%d\n", ft->name, ft->ftype, ft->tlength, ft->dlength);
		}
	}
	*trec = ' ';

end:
	memcpy(env, sav_env, sizeof(jmp_buf));
	if (cperr) {
		if (hdt) free(hdt);
		if (trec) free(trec);
		if (frec) free(frec);
		sys_err(cperr);
	}
}

void cprec(void)
{
	FIELD *ft;
	int i, toffset, fout;

	memcpy(frec, get_db_rec(), get_rec_sz());
	for (i=0,toffset=1,ft=hdt->fdlist,fout=0; i<hdt->fldcnt; ++i,++ft) {
		if (!txtcpy) {
			if (ft->ftype == 'M') {
				recno_t fblkno, tblkno;
				char temp[MEMOLEN + 1];
				sprintf(temp, "%.*s", ft->tlength, frec + ft->x0);
				fblkno = atol(temp);
				if (fblkno) {
					blkno_t nblks;
					tblkno = get_next_memo(tdbt);
					nblks = memo_copy(fdbt, tdbt, fblkno, tblkno);
					sprintf(temp, "%*ld", MEMOLEN, tblkno);
					memcpy(trec + toffset, temp, MEMOLEN);
					put_next_memo(tdbt, tblkno + nblks);
				}
				else memset(trec + toffset, ' ', MEMOLEN);
			}
			else memcpy(trec+toffset, frec + ft->x0, ft->tlength);
		}
		else {
			char dc = txt_get_dlmtr(txtcpy), *fstart = frec + ft->x0, *fend = fstart + ft->tlength;
			if (dc && fout) fputc(',', txtfp);
			switch (ft->ftype) {
			case 'C':
			case 'D':
			default:
				if (dc) {
					fputc(dc, txtfp);
					while (fend>fstart && *(fend - 1) == ' ') --fend;
				}
				fprintf(txtfp, "%.*s", fend - fstart, fstart);
				if (dc) fputc(dc, txtfp);
				break;
			case 'L':
				fputc(*fstart == 'T'? '1':'0', txtfp);
				break;
			case 'N':
			case 'M':       /* block number */
				if (dc) while (fstart < fend && *fstart == ' ') ++fstart;
				if (fstart == fend) fputc('0', txtfp);
				else fprintf(txtfp, "%.*s", fend - fstart, fstart);
				break;
			}
			fout = 1;
		}
		toffset += ft->tlength;
	}
	if (!txtcpy) {
		ctx_select(AUX);
		_apblank(0);
		memcpy(get_db_rec(), trec, get_rec_sz());
		save_rec(R_APPEND);
		store_rec();
		setflag(fextd);
		setflag(dbmdfy);
		ctx_select(ctx_id);
	}
	else fputc('\n', txtfp);
	cnt_message("COPIED", 0);
}

void cpnext(int dir)
{
	skip(dir == FORWARD? 1 : -1);
	if (!etof(0)) curr_ctx->scope.count++;
}

void cpdone(void)
{
	cnt_message("COPIED", 1);
	if (txtcpy) {
#ifndef UNIX
		fputc(EOF_MKR, txtfp);
#endif
		fclose(txtfp);
		goto qq;
	}
	ctx_select(AUX);
	use(0);
	ctx_select(ctx_id);
	if (has_dbt) {
		if (dbtbuf) free(dbtbuf);
		if (tempfd!=-1) close(tempfd);
		if (tempfn) {
			unlink(tempfn);
			free(tempfn);
		}
	}
qq:
	if (trec) free(trec);
	if (hdt) free(hdt);
	if (frec) free(frec);
}
