/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/db.c
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

#include <unistd.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include "hb.h"
#include "dbe.h"

static int read_data_offset(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_word(xlat_buf + VERS_LEN + sizeof(DWORD)));
	else if (mkr == DB5_MKR) return(read_dword(xlat_buf + VERS_LEN + sizeof(LWORD)));
	else return(-1);
}

static int read_bytes_per_record(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_word(xlat_buf + VERS_LEN + sizeof(DWORD) + sizeof(WORD)));
	else if (mkr == DB5_MKR) return(read_dword(xlat_buf + VERS_LEN + sizeof(LWORD) + sizeof(DWORD)));
	else return(-1);
}

static recno_t read_record_count(int mkr, char *xlat_buf)
{
	if (mkr == DB3_MKR) return(read_dword(xlat_buf + VERS_LEN));
	else if (mkr == DB5_MKR) return(read_lword(xlat_buf + VERS_LEN));
	else return((recno_t)-1);
}

static int is_valid_field(FIELD *f)
{
	if (!strchr("CNDLM", f->ftype)) return(0);
	switch (f->ftype) {
	case 'C':
		if (f->dlength != 0) return(0);
		if (f->tlength < 1 || f->tlength > MAXSTRLEN) return(0);
		break;
	case 'N':
		if (f->tlength < 1 || f->tlength > MAXDIGITS || f->dlength >= f->tlength) return(0);
		break;
	case 'L':
		if (f->dlength != 0 || f->tlength != 1) return(0);
		break;
	case 'D':
		if (f->dlength != 0 || f->tlength != DATELEN) return(0);
		break;
	case 'M':
		if (f->dlength != 0 || f->tlength != MEMOLEN) return(0); //NEEDSWORK: redefine MEMO 
		break;
	default:
		return(0);
	}
	return(1);
}

int read_db_hdr(TABLE *hdr, char *xlat_buf, int buf_size, int fixed)
{
	int mkr, offset, byprec, hdrsz, fldnamelen, fldwd, is_db3;
	mkr = MKR_MASK & *xlat_buf;
	if (mkr != DB3_MKR && mkr != DB5_MKR) return(-1);
	is_db3 = mkr == DB3_MKR;
	if ((is_db3 && buf_size < HD3SZ) || buf_size < HDRSZ) return(-1);
	offset = read_data_offset(mkr, xlat_buf);
	if (offset < 0) return(-1);
	byprec = read_bytes_per_record(mkr, xlat_buf);
	if (byprec < 0 || byprec > MAXRECSZ) return(-1);
	hdr->id = (BYTE)*xlat_buf;
	hdr->reccnt = read_record_count(mkr, xlat_buf);
	if (hdr->reccnt == (recno_t)-1) return(-1);
	hdr->doffset = offset;
	hdr->byprec = byprec;
	fldnamelen = mkr == DB3_MKR? FLD3NAMLEN : FLDNAMELEN;
	fldwd = mkr == DB3_MKR? FLD3WD : FLDWD;
	hdrsz = mkr == DB3_MKR? HD3SZ : HDRSZ;
	if (fixed) return(hdrsz);
	else {
		FIELD *f;
		char *tp = xlat_buf;
		int fcount = 0;
		tp += hdrsz;
		f = get_fdlist(hdr);
		do {
			if (*tp == EOH) break;
			if (tp + fldwd - xlat_buf > buf_size) break;
			memset(f->name, 0, FLDNAMELEN + 1);
			strncpy(f->name, tp, fldnamelen);
			f->ftype = *(tp + fldnamelen + 1);
			f->x0 = 0;
			f->x1 = 0;
			if (f->ftype == 'C') {
				f->tlength = MAKE_WORD(*(tp + fldnamelen + 6), *(tp + fldnamelen + 7));
				f->dlength = 0;
			}
			else f->tlength = (BYTE)*(tp + fldnamelen + 6); f->dlength = (BYTE)*(tp + fldnamelen + 7);
			if (!is_valid_field(f)) return(-1);
			++f; ++fcount; tp += fldwd;
		} while (fcount < MAXFLD);
		if (fcount < 1) return(-1);
		hdr->fldcnt = fcount;
		if (hdr->fldcnt < MAXFLD) {
			if (*tp != EOH) return(-1);
			++tp;
		}
		hdr->hdlen = (int)(tp - xlat_buf);
		if (hdr->doffset < hdr->hdlen) return(-1);
		hdr->meta_len = hdr->doffset - hdr->hdlen;
		if (hdr->fldcnt < MAXFLD) hdr->meta_len -= META_HDR_ALIGN;
		if (hdr->meta_len <  MIN_META_LEN) hdr->meta_len = 0;
		return(hdr->hdlen);
	}
}

int read_meta_hdr(TABLE *hdr, char *buf, int buf_size)
{
	if (buf_size < hdr->meta_len) return(0);
	off_t offset = hdr->fldcnt < MAXFLD? hdr->hdlen + META_HDR_ALIGN : hdr->hdlen;
	return(dbread(buf, offset, (size_t)hdr->meta_len));
 }

#ifdef SHARE_FD
int find_dbfd(void)
{
	int i, ctx_id = get_ctx_id(); 

	for (i=0; i<MAXCTX; ++i) {
		if (i == ctx_id) continue;
		if (_chkdb(i) != CLOSED && !strcmp(curr_ctx->dbfctl.filename, table_ctx[i].dbfctl.filename)) {
			if (table_ctx[i].dbfctl.fd == -1) continue;
			return(i);
		}
	}
	return(-1);
}
#endif

void opendb(void)
{
	TABLE *hdr;
	FIELD *f;
	char *db_fn = curr_ctx->dbfctl.filename, *buf = NULL;
	int ctx_id = get_ctx_id(), shared_ctx_id = -1, i, hdlen, byprec, offset, error;
	jmp_buf sav_env;

	memcpy(sav_env, env, sizeof(jmp_buf));
	if (error = setjmp(env)) goto err;
	if (!(buf = malloc(MAXHDLEN))) {
		error = NOMEMORY;
		goto err;
	}
#ifdef SHARE_FD
	shared_ctx_id = find_dbfd();
	if (shared_ctx_id == -1) {
#endif
		curr_ctx->dbfctl.fd = s_open(db_fn, OLD_FILE | 
			(flagset(exclusive)? EXCL : 0) | 
				(flagset(denywrite)? DENYWRITE : 0) | 
					(flagset(readonly)? RDONLY : 0));
		if (test_lock_hdr(flagset(readonly)? HB_RDLCK : HB_WRLCK)) {
			error = TBLLOCKED;
			goto err;
		}
		if (!flagset(readonly) && transaction_active()) {
			if (rbk_open(transaction_get_id(), &curr_ctx->rbkctl, curr_ctx->dbfctl.filename, NEW_FILE) < 0) {
				error = TRANSAC_NORBK;
				goto err;
			}
			transaction_add_table(NULL, db_fn);
		}
#ifdef SHARE_FD
	}
	else {
		curr_ctx->dbfctl.fd = table_ctx[shared_ctx_id].dbfctl.fd; //already open in another ctx
		curr_ctx->rbkctl.fd = table_ctx[shared_ctx_id].rbkctl.fd;
	}
#endif
	hdlen = dbread(buf, (off_t)0, (size_t)MAXHDLEN); //read raw header into buffer
	if (hdlen < 0) {
		error = DBREAD;
		goto err; 
	}
	if (ctx_id != AUX && !(curr_dbf->dbf_h = (TABLE *)malloc(sizeof(TABLE) + MAXFLD * sizeof(FIELD)))) goto err;
	memset(curr_dbf->dbf_h, 0, sizeof(TABLE) + MAXFLD * sizeof(FIELD));
	hdr = curr_dbf->dbf_h;
	if ((hdlen = read_db_hdr(hdr, buf, hdlen, 0)) < 0) {
		error = NTDBASE;
		goto err;
	}
	if (shared_ctx_id == -1) hdr->access = getaccess(db_fn, HBTABLE);
	else hdr->access = table_ctx[shared_ctx_id].dbp->dbf_h->access;
	if (hdr->access < HBREAD) {
		error = DB_ACCESS;
		goto err;
	}

	byprec = hdr->byprec;
	if (byprec < MINRECSZ || byprec > MAXRECSZ) {
		error = NTDBASE;
		goto err;
	}
	if (hdr->reccnt > MAXNRECS) {
		error = TOOMANYRECS;
		goto err;
	}
	if (table_has_memo(hdr)) {
		if (shared_ctx_id != -1) curr_ctx->dbtfd = table_ctx[shared_ctx_id].dbtfd;
		else {
			char dbtfn[FULFNLEN];
			if (!form_memo_fn(dbtfn, FULFNLEN)) {
				error = BADFILENAME;
				goto err;
			}
			if (euidaccess(dbtfn, F_OK)) {
				error = NODBT;
				goto err;
			}
			curr_ctx->dbtfd = s_open(dbtfn, OLD_FILE |
				(flagset(exclusive)? EXCL : 0) | 
					(flagset(denywrite)? DENYWRITE : 0) | 
						(flagset(readonly)? RDONLY : 0));
		}
	}
	for (i=0,f=get_fdlist(hdr),offset=1; i<hdr->fldcnt; offset+=f->tlength,++i,++f) {
		char xname[VNAMELEN + FLDNAMELEN + 2];
		sprintf(xname, "%s.%s", curr_ctx->alias, f->name);
		f->x0 = getaccess(xname, HBFIELD);
		f->offset = offset;
	}
	if (ctx_id != AUX && !(curr_dbf->dbf_h = realloc(hdr, get_hdr_len(hdr)))) {
		error = NOMEMORY;
		goto err;
	}
	if (ctx_id != AUX) {
		curr_dbf->dbf_b = malloc(byprec);
		curr_ctx->rbkctl.iobuf = malloc(rbk_iobufsize(byprec));
		if (!curr_dbf->dbf_b || !curr_ctx->rbkctl.iobuf) {
			error = NOMEMORY;
			goto err;
		}
		curr_ctx->rbkctl.recno = 0;
		memset(curr_ctx->rbkctl.iobuf, 0, rbk_iobufsize(byprec));
	}
	clrflag(fextd);
	clrflag(dbmdfy);
	if (flagset(exclusive) || flagset(denywrite)) clrflag(fsync);
	else setflag(fsync);
	free(buf);
	memcpy(env, sav_env, sizeof(jmp_buf));
	return;
err:
	if (buf) free(buf);
	if (curr_dbf->dbf_b) {
		free(curr_dbf->dbf_b);
		curr_dbf->dbf_b = NULL;
	}
	if (curr_ctx->rbkctl.iobuf) {
		free(curr_ctx->rbkctl.iobuf);
		curr_ctx->rbkctl.iobuf = NULL;
	}
	memcpy(env, sav_env, sizeof(jmp_buf));
	db_error(error, db_fn);
}

static void write_record_count(int mkr, BYTE **tp, recno_t reccnt)
{
	if (mkr == DB3_MKR) { 
		write_dword(*tp, reccnt); 
		*tp += sizeof(DWORD); 
	}
	else { 
		write_lword(*tp, reccnt); 
		*tp += sizeof(LWORD); 
	}
}

static void write_data_offset(int mkr, BYTE **tp, int doffset)
{
	if (mkr == DB3_MKR) {
		write_word(*tp, doffset);
		*tp += sizeof(WORD);
	}
	else {
		write_dword(*tp, doffset);
		*tp += sizeof(DWORD);
	}
}

static void write_bytes_per_record(int mkr, BYTE **tp, int byprec)
{
	if (mkr == DB3_MKR) {
		write_word(*tp, byprec);
		*tp += sizeof(WORD);
	}
	else {
		write_dword(*tp, byprec);
		*tp += sizeof(WORD);
	}
}

int write_db_hdr(TABLE *hdr, BYTE *xlat_buf, int buf_size)
{
	BYTE *tp = xlat_buf;
	int mkr = MKR_MASK & hdr->id, fldnamelen, fldwd, i;
	if (mkr != DB3_MKR && mkr != DB5_MKR) return(-1);
	memset(tp, 0, buf_size);
	*tp++ = (BYTE)(0xff & hdr->id);
	*tp++ = LEGACY_YY; *tp++ = LEGACY_MM; *tp++ = LEGACY_DD;
	write_record_count(mkr, &tp, hdr->reccnt);
	if (hdr->doffset < 0) hdr->doffset = MAXHDLEN;	//set to default of 65536 bytes
	if (hdr->doffset < hdr->hdlen) hdr->doffset = hdr->hdlen;
	write_data_offset(mkr, &tp, hdr->doffset);
	write_bytes_per_record(mkr, &tp, hdr->byprec);
	fldnamelen = mkr == DB3_MKR? FLD3NAMLEN : FLDNAMELEN;
	fldwd = mkr == DB3_MKR? FLD3WD : FLDWD;
	tp = xlat_buf + (mkr == DB3_MKR? HD3SZ : HDRSZ);
	for (i=0; i<hdr->fldcnt; ++i,tp+=fldwd) {
		FIELD *f = &hdr->fdlist[i];
		if ((int)(tp + fldwd - xlat_buf) >= buf_size) return(-1);
		strncpy(tp, f->name, fldnamelen);
		*(tp + fldnamelen + 1) = f->ftype;
		write_word(tp + fldnamelen + 2, 0); // x0, not used
		write_word(tp + fldnamelen + 4, 0); // x1, not used
		*(tp+fldnamelen + 6) = f->tlength;
		*(tp+fldnamelen + 7) = f->dlength;
	}
	if (hdr->fldcnt < MAXFLD) *tp++ = EOH;
	return((int)(tp - xlat_buf));
}

int write_meta_hdr(TABLE *hdr, char *meta, int meta_len)
{
	off_t offset = hdr->fldcnt < MAXFLD? hdr->hdlen + META_HDR_ALIGN : hdr->hdlen;
	hdr->meta_len = meta_len;
	dbwrite(meta, offset, (size_t)hdr->meta_len);
	return(hdr->meta_len);
}

void update_hdr(int do_lock)
{
	TABLE *hdr = curr_dbf->dbf_h;
	if (!hdr) return;
	recno_t reccnt = hdr->reccnt;
	int meta_size;

	if (do_lock) lock_hdr();
	if (flagset(fextd)) {
		int fd = curr_ctx->dbfctl.fd;
		char ee[2];
		ee[0] = ee[1] = EOF_MKR; /* to be backward compatible, ^Z */
		lseek(fd, (off_t)(hdr->reccnt = reccnt) * hdr->byprec + hdr->doffset, 0);
		write(fd, ee, 2);
		chsize(fd, lseek(fd, (off_t)0, SEEK_CUR));
		if (flagset(fsync)) fsync(fd);
		clrflag(fextd);
	}
	if (flagset(dbmdfy)) {
		BYTE *xlat_buf = get_aux_buf();
		int len = write_db_hdr(hdr, xlat_buf, MAXBUFSZ);
		dbwrite(xlat_buf, (off_t)0, (size_t)len);
		clrflag(dbmdfy);
	}
	meta_size = hdr->doffset - hdr->hdlen;
	if (hdr->fldcnt < MAXFLD) meta_size -= META_HDR_ALIGN;
	if (flagset(setndx) && meta_size  >= MIN_META_LEN) {
		int i;
		char *xlat_buf = get_aux_buf(), *meta_start, *tp;
		if (hdr->fldcnt < MAXFLD) meta_start = xlat_buf + META_HDR_ALIGN;
		else meta_start = xlat_buf;
		for (i=0,tp = meta_start + META_ENTRY_OFF; i<MAXNDX; ++i, tp += META_ENTRY_LEN) {
			char *ndxfn = curr_ctx->ndxfctl[i].filename;
			if (!ndxfn) break;
			if (tp + META_ENTRY_LEN - meta_start + 1 > meta_size) break;
			memset(tp, 0, META_ENTRY_LEN);
			strcpy(tp, ndxfn);
		}
		if (i) {
			meta_start[0] = META_NDX;
			meta_start[1] = i;
		}
		else tp = meta_start;
		*tp++ = EOM;
		write_meta_hdr(hdr, meta_start, (int)(tp - meta_start));
		clrflag(setndx);
	}
	if (do_lock) unlock_hdr();
}

void closedb(void)
{
	int i;
	if (curr_ctx->dbfctl.fd == -1) return;
	update_hdr(1);
	for (i=0; i<MAXCTX; ++i) {
		if (&table_ctx[i] == curr_ctx) continue;
		if (table_ctx[i].dbfctl.fd == curr_ctx->dbfctl.fd) break;
	}
	if (i == MAXCTX) {
		close(curr_ctx->dbfctl.fd);  /* closing already opened file, no error check necessary */
		if (curr_ctx->dbtfd != -1) close(curr_ctx->dbtfd);
		if (curr_ctx->rbkctl.fd != -1) close(curr_ctx->rbkctl.fd);
	}
	curr_ctx->dbfctl.fd = curr_ctx->dbtfd = curr_ctx->rbkctl.fd = -1;
	curr_ctx->rbkctl.recno = curr_ctx->rbkctl.flags = 0;

	if (get_ctx_id() != AUX) {
		if (curr_dbf->dbf_h) free(curr_dbf->dbf_h);
		curr_dbf->dbf_h = NULL;
		if (curr_dbf->dbf_b) free(curr_dbf->dbf_b);
		curr_dbf->dbf_b = NULL;
		if (curr_ctx->rbkctl.iobuf) free(curr_ctx->rbkctl.iobuf);
		curr_ctx->rbkctl.iobuf = NULL;
	}
}

void cleandb(void)
{
	if (chkdb() != CLOSED) {
/** except called by error during use(), stat is either IN_USE or CLOSED **/
		closedb();
		closeindex();
		delfn(&curr_ctx->dbfctl);
		del_exp(0);	/* relation */
		del_exp(1);	/* filter */
		curr_ctx->rela = -1;
		curr_ctx->crntrec = 0;
		scope_init(0, 1);
		curr_ctx->alias[0] = '\0';
		setetof(0);
		clrflag(exclusive);
		clrflag(readonly);
		if (curr_ctx->exfd != -1) {
			close(curr_ctx->exfd);
			curr_ctx->exfd = -1;
		}
	}
}

void cleardb(void)
{
	int i;
	int ctx_id = get_ctx_id();
	for (i=0; i<MAXCTX; ++i) {
/*
if (!temp || (_chkdb(i) == IN_USE)) {
*/
		ctx_select(i);
		cleandb();
	}
	ctx_select(ctx_id);
}

int _get_ctx_id(CTX *ctx)
{
	int i;
	for (i=0; i<MAXCTX + 1; ++i) if (ctx == &table_ctx[i]) return(i);
	return(-1);
}

void ctx_select(int ctx_id)
{
	DBF *p, *q;
	int curr_ctx_id = get_ctx_id();

	if (ctx_id < 0 || ctx_id > MAXCTX) db_error0(ILL_SELECT);
	if (ctx_id == curr_ctx_id) return;
	if (ctx_id == AUX) {
		curr_ctx = &table_ctx[AUX];
		curr_dbf = curr_ctx->dbp;
		return;
	}
	p = dbf_buf; q = (DBF *)NULL;
	while (p) {
		if (p->ctx_id == ctx_id) q = p;
		if (!q && (_chkdb(p->ctx_id) == CLOSED)) q = p;
		p = p->nextdb;
	}
	if (!q) db_error0(NO_CTX);
	curr_ctx = &table_ctx[ctx_id];
	curr_ctx->dbp = curr_dbf = q;
	q->ctx_id = ctx_id;
	switch (chkdb()) {
	case IN_USE:
		//strcpy(defa_path, form_fn1(1, curr_ctx->dbfctl.filename));
		break;
	default:
	case CLOSED:
		setflag(e_o_f);
		setflag(t_o_f);
		break;
	}
}

int ctx_next_avail(void)
{
	int i;
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == CLOSED) {
		return(i);
	}
	return(-1);
}

void valid_chk(void)
{
   if (chkdb() == CLOSED) db_error2(NOTINUSE, get_ctx_id() + 1);
   if (!etof(0) && curr_dbf->dbf_h->reccnt && !curr_ctx->crntrec) db_error2(OUTOFRANGE, get_ctx_id() + 1);
}

int _chkdb(int ctx_id)
{
   if (ctx_id == -1) return(CLOSED);
   if (!table_ctx[ctx_id].dbfctl.filename || table_ctx[ctx_id].dbfctl.fd == -1) return(CLOSED);
   return(IN_USE);
}

void write_rec(int mark_only)
{
	int byprec = curr_dbf->dbf_h->byprec;
	off_t offset = (off_t)(curr_ctx->crntrec - 1) * byprec + curr_dbf->dbf_h->doffset;
	dbwrite(get_rec_buf(), offset, mark_only? 1 : byprec);
}

int del_recall(int mark)   /* ' ','*' */
{
	valid_chk();
	if (!etof(0) && *get_rec_buf() != mark) {
		save_rec(R_DELRCAL); //lock record
		*get_rec_buf() = mark;
		write_rec(1); //wrute out the marker
		r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
		return(1);
	}
	return(0);
}

int cyclic(int ctx_id)
{
	if (ctx_id == -1) return(FALSE);
	if (ctx_id == get_ctx_id()) return(TRUE);
	return(cyclic(table_ctx[ctx_id].rela));
}

FIELD *findfld(int mode, int *n)
{
	TABLE *hdr = curr_dbf->dbf_h;
	FIELD *f;
	int i;

	*n = mode ? 0 : 1;
	for (i=0; i<hdr->fldcnt; ++i) {
		f = &hdr->fdlist[i];
		if (!namecmp(f->name, topstack->buf, topstack->valspec.len, ID_VNAME)) break;
		if (mode) ++(*n); else *n += f->tlength;
	}
	return(i>=hdr->fldcnt? NULL : f);
}

int dbretriv(void)
{
	FIELD *f;
	char *p;
	int len;
	HBVAL *v;
	int offset, isblank;
	char blank[MAXSTRLEN];

	if ((chkdb() == CLOSED) || !(f = findfld(0, &offset))) return(0);
	if (!(f->x0 & HBREAD)) return(0);
	pop_stack(DONTCARE);
	memset(blank, ' ', MAXSTRLEN);
	p = etof(0)? blank : get_rec_buf() + offset;
	switch (f->ftype) {
		case 'D': len = sizeof(date_t); break;
		case 'N': len = (f->dlength > DECI_MAX) ? sizeof(double) : sizeof(long); break;
		default:
		case 'C': len = f->tlength; break;
		case 'L': len = 1; break;
		case 'M': len = sizeof(memo_t); break;
	}
	isblank = !memcmp(blank, p, f->tlength);
	stack_chk_set(isblank && isset(NULL_ON)? 2 : len + 2);
	v = topstack;
	if (!isblank || !isset(NULL_ON)) { //not blank or blank as NULL flag not ON
		v->valspec.len = len;
		switch (f->ftype) {
		case 'M':
		case 'N': {
			char temp[STRBUFSZ];
			memcpy(temp, p, f->tlength);
			temp[f->tlength] = '\0';
			if (f->ftype == 'M') {
				memo_t *memo = (memo_t *)v->buf;
				memo->fd = get_dbt_fd();
				memo->blkno = atol(temp);
				memo->blkcnt = (blkno_t)-1; //NEEDSWORK, text only with EOF_MKR as eof indicator
				v->valspec.typ = MEMO;
			}
			else {
				number_t num = str2num(temp);
				v->valspec.typ = NUMBER;
				if (num_is_real(num)) {
					v->numspec.len = sizeof(double);
					v->numspec.deci = REAL;
					put_double(v->buf, num.r);
				}
				else {
					v->numspec.len = sizeof(long);
					v->numspec.deci = num.deci;
					put_long(v->buf, num.n); //allow n to be negative so we can cover unsigned long
				}
			}
			break; }

		case 'C':
			v->valspec.typ = CHARCTR;
			memcpy(v->buf, p, f->tlength);
			break;

		case 'L':
			v->valspec.typ = LOGIC;
			v->buf[0] = (*p == 'Y') || (*p == 'T') ? 1 : 0;
			break;

		case 'D':
			v->valspec.typ = DATE;
			if (!strncmp(p,"        ",8)) put_date(v->buf, 0);
			else {
				char temp[5];
				int day, month, year;
				memcpy(temp, p, 4); temp[4] = '\0';
				year = atoi(temp);
				memcpy(temp, p + 4, 2); temp[2] = '\0';
				month = atoi(temp);
				memcpy(temp, p + 6, 2); temp[2] = '\0';
				day = atoi(temp);
				put_date(v->buf, date_to_number(day, month, year));
			}
			break;
		default:
			db_error(BADFLDTYPE, f->name);
		}
	}
	else { //NULL value
//nullval:
		v->valspec.len = 0;
		switch (f->ftype) {
		case 'M':
			v->valspec.typ = MEMO;
			break;
		case 'N':
			v->valspec.typ = NUMBER;
			break;
		case 'C':
			v->valspec.typ = CHARCTR;
			break;
		case 'L':
			v->valspec.typ = LOGIC;
			break;
		case 'D':
			v->valspec.typ = DATE;
			break;
		default:
			db_error(BADFLDTYPE, f->name);
		}
	}
	return(1);
}

void save_rec(int mode)
{
	if (flagset(readonly)) db_error0(DB_READONLY);
	if (mode == R_UPDATE || mode == R_DELRCAL || mode == R_APPEND) {
		TABLE *hdr = curr_dbf->dbf_h;
		int retry = 0;
		if (!(hdr->access & HBMODIFY)) db_error0(DB_ACCESS);
lck:
		if (r_lock(curr_ctx->crntrec, HB_WRLCK, 0) < 0) db_error0(NORLOCK);
		if (mode == R_UPDATE) {
			RBK *rbk = &curr_ctx->rbkctl;
			char *recbuf = get_db_rec();
			if (*recbuf == TRANS_MKR) {
				r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
				++retry;
				if (retry >= LRETRY) db_error0(TRANSAC_EXCL);
				usleep(10000); //allow transaction to finish
				goto lck;
			}
			else if (*recbuf != VAL_MKR) db_error0(INVALID_REC);
			memcpy(get_rbk_buf(), recbuf, hdr->byprec); //save original
			rbk->recno = curr_ctx->crntrec;
			if (transaction_active()) {
				rbk_write(transaction_get_id(), rbk, hdr->byprec);
				*recbuf = TRANS_MKR; //mark it
				write_rec(1); //save it to disk
			}
		}
	}	
	curr_ctx->lock.mode = mode;
}

void dbstore(void) //current ctx must not be AUX
{
	if (etof(0)) return;

	FIELD *f;
	BYTE *fdata;
	HBVAL *v;
	int l, offset;
	
	if (!(f = findfld(0, &offset))) db_error1(UNDEF_FIELD, pop_stack(DONTCARE));
	if (!(f->x0 & HBMODIFY)) db_error1(DB_ACCESS, pop_stack(DONTCARE));
	pop_stack(DONTCARE);
	fdata = get_db_rec() + offset;
	if (!is_rec_saved()) {
		save_rec(R_UPDATE); //also locks record
	}
	l = f->tlength;
	memset(fdata, ' ', l);
	v = pop_stack(DONTCARE);
	switch (v->valspec.typ) {
	case CHARCTR :
		if (f->ftype != 'C') goto err;
		memcpy(fdata, v->buf, imin((int)v->valspec.len, l));
		break;

	case LOGIC :
		if (f->ftype != 'L') goto err;
		*fdata = v->buf[0] ? 'T' : 'F';
		break;

	case NUMBER : {
		number_t num;
		char temp[MAXDIGITS];
		num.deci = v->numspec.deci;
		if (val_is_real(v)) num.r = get_double(v->buf);
		else num.n = get_long(v->buf);
		num2str(num, l, f->dlength, temp);
		memcpy(fdata, temp, l);
		break; }

	case DATE : {
		char temp[DATELEN + 1];
		date_t dd;
		if (f->ftype != 'D') goto err;
		if (dd = get_date(v->buf)) {
			number_to_date(1, dd, temp);
			memcpy(fdata, temp, DATELEN);
		}
		break; }

	default:
		db_error1(DB_INVALID_TYPE, v);
	}
	return;
err:  db_error(DB_TYPE_MISMATCH, f->name);
}

int store_rec(void)
{
	int prim_ndx_updated = FALSE;
	int mode = curr_ctx->lock.mode;
	write_rec(0); //write out the full record, including TRANS_MKR if set
	setflag(dbmdfy);
	if (mode == R_APPEND) {
		TABLE *hdr = curr_dbf->dbf_h; //hdr should have been locked already
		++hdr->reccnt;
		setflag(fextd);
		update_hdr(0);
		curr_ctx->lock.mode = 0;
	}
	if (get_ctx_id() == AUX) return(0);
	if (mode != R_PACK) {
		if (mode == R_UPDATE) clr_rec_saved();
		//R_DELRCAL unlock is done by del_recall()
		if (mode == R_APPEND || blkcmp(get_rec_buf() + 1, get_rbk_buf() + 1, curr_dbf->dbf_h->byprec - 1)) { //don't compare the 1st byte
			if (index_active()) prim_ndx_updated = updndx(mode == R_UPDATE); //delete or recall will not change index
			r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
		}
	}
	return(prim_ndx_updated);
}

static int chk_alias(char *alias)
{
	int i, ctx_id = get_ctx_id();
	for (i=0; i<=MAXCTX; ++i) {
		if (i == ctx_id) continue;
		if (_chkdb(i) == IN_USE && !strcmp(table_ctx[i].alias, alias)) return(1);
	}
	return(0);
}

static void _set_alias(char *alias)
{
	char c;
	int len;
	HBVAL *v = pop_stack(CHARCTR);
	len = v->valspec.len;
	if ((len > VNAMELEN) || ((len == 1) && ((c = to_upper(v->buf[0])) >= 'A') && ( c <= 'J')))
		syn_err(BAD_ALIAS);
	memcpy(alias, v->buf, len);
	alias[len] = '\0';
}

static void _set_index(mode)
{
	int i = pop_int(1, MAXNDX);
	for (i; i>0; --i) {
		get_fn(I_D_X | (mode & GETFN_MASK), &curr_ctx->ndxfctl[i - 1]);
	}
}

int meta_chk_set_index()
{
	TABLE *hdr = curr_dbf->dbf_h;
	if (hdr->meta_len) {
		char *meta = get_aux_buf(), *tp;
		int len = read_meta_hdr(hdr, meta, hdr->meta_len);
		if (len >= MIN_META_LEN && is_meta_ndx(meta)) {
			int i = meta_entry_cnt(meta), j;
			for (j=0, tp=meta_entry_start(meta); j<i; ++j,tp+=META_ENTRY_LEN) {
				if (tp + META_ENTRY_LEN - meta > len) break;
				if (*tp == EOM) break;
				curr_ctx->ndxfctl[j].filename = addfn(tp);
			}
			return(j);
		}
	}
	return(0);
}

void use(int mode)
{
	if (transaction_active() && chkdb() == IN_USE && transaction_table_modified()) db_error(TRANSAC_NOEND, curr_ctx->dbfctl.filename);
	cleandb();
	if (!mode) return;

	char alias[VNAMELEN + 1];
	jmp_buf sav_env;
	int error = 0;
	/*** mode also reflects the order of items on the stack  *****/
	if (mode & U_CREATE) mode |= U_NO_CHK_EXIST;
	memcpy(sav_env, env, sizeof(jmp_buf));
	if (error = setjmp(env)) {
		memcpy(env, sav_env, sizeof(jmp_buf));
		if (code2errno(error) != FILENOTEXIST) longjmp(env, error);
		pop_stack(CHARCTR); //pop the error message and discard
		get_fn(D_B_F | (mode & GETFN_MASK), &curr_ctx->dbfctl);
	}
	else {
		get_fn(T_B_L | (mode & GETFN_MASK), &curr_ctx->dbfctl);
		memcpy(env, sav_env, sizeof(jmp_buf));
	}
	strcpy(alias, form_fn1(0, curr_ctx->dbfctl.filename));
	if (mode & U_INDEX && mode & U_ALIAS) {
		int alias_first = pop_logic();
		if (alias_first) { //alias is on top of stack
			_set_alias(alias);
			_set_index(mode);
		}
		else {
			_set_index(mode);
			_set_alias(alias);
		}
	}
	else if (mode & U_INDEX) _set_index(mode);
	else if (mode & U_ALIAS) _set_alias(alias);
	//chgcase(alias, 1);
	if (get_ctx_id() != AUX && chk_alias(alias)) db_error(DUP_ALIAS, alias);
	strcpy(curr_ctx->alias, alias);
	if (mode & U_CREATE) {
		setflag(exclusive);
		if ((curr_ctx->dbfctl.fd = s_open(curr_ctx->dbfctl.filename, NEW_FILE | (mode & U_EXCL? 0 : OVERWRITE) | EXCL)) < 0)
			db_error(DBCREATE, curr_ctx->dbfctl.filename);
		if (mode & U_DBT) {
			char dbtfn[FULFNLEN];
			blkno_t blkno = 1;
			form_memo_fn(dbtfn, FULFNLEN);
			if ((curr_ctx->dbtfd = s_open(dbtfn, NEW_FILE | OVERWRITE | EXCL)) < 0)
			db_error(DBCREATE, dbtfn);
			write(curr_ctx->dbtfd, &blkno, BLKSIZE);
			fsync(curr_ctx->dbtfd);
		}
	}
	else {
		if (mode & U_SHARED) clrflag(exclusive); //meaningful only if EXCL_ON
		else if ((mode & U_EXCL) || isset(EXCL_ON)) setflag(exclusive);
		if (mode & U_DENYW) setflag(denywrite);
		if (mode & U_READONLY || isset(READONLY_ON) == S_ON) setflag(readonly);
		opendb();
		if (use_index(mode)) curr_ctx->order = 0;
		else meta_chk_set_index(); //curr_ctx->order == -1
		gotop();
	}
}

void find(int mode)
{
	NDX_HDR *nhd;
	int ktyp, nk, i, l1, l2, is_null = 0;
	char *p, *q;
	recno_t recno;
	NDX_ENTRY target;

	valid_chk();
	ndx_chk();
	ndx_select(curr_ctx->order);
	ndx_lock(0, HB_RDLCK);
	read_ndx_hdr(curr_ndx->fctl->fd, nhd = &curr_ndx->ndx_h);
	clrflag(rec_found);
/*
	if (flagset(e_o_f) && flagset(t_o_f)) return;		/* no valid records *
*/
	ktyp = nhd->keytyp;
	nk = pop_int(0, 255);
	if (ktyp != CMBK && nk > 1) db_error(NOTNDXCMBK, nhd->ndxexp);
	p = target.key;
	for (i=nk; i; --i) {
		HBVAL *v = stack_peek(i - 1);
		int ktyp2 = nhd->cmbkey.keys[nk-i].typ;
		if (!v->valspec.len) {
			is_null = 1;
			break;
		}
		l1 = v->valspec.len;
		l2 = ktyp==CMBK? nhd->cmbkey.keys[nk-i].len : nhd->keylen;
		switch (v->valspec.typ) {
	  	case CHARCTR:
			if (ktyp) {
				if (ktyp != CMBK || ktyp2 != v->valspec.typ) goto nofind;
			}
			else if (l1 > l2) goto nofind;
	  		memset(p, ' ', l2);
	  		memcpy(p, v->buf, l1);
	  		if (!isset(EXACT_ON) && (!mode || mode != TEQUAL)) p[l1] = '\0';
	  		p += l2;
	  		break;

	  	case DATE :
	  	case NUMBER :
	  		if (!ktyp || (ktyp==CMBK && ktyp2 != v->valspec.typ)) goto nofind;
	  		if (v->valspec.typ == DATE) put_double(p, (double)get_word(v->buf));
	  		else if (val_is_real(v)) put_double(p, get_double(v->buf));
	  		else put_double(p, (double)get_long(v->buf) / power10(v->numspec.deci));
	  		p += sizeof(double);
	  		break;

	  	default : goto nofind;
		}
	}
	if (is_null) goto nofind;
	while (nk--) pop_stack(DONTCARE);
	clretof(0);
	if (!mode) mode = TEQUAL;
	target.recno = target.pno = 0;
	if (!(recno = btsrch(nhd->rootno, mode, &target))) goto nofind2;
	gorec(recno);
	while (!etof(0) && ((isset(DELETE_ON) && isdelete()) ||
				 (filterisset() && !cond(curr_ctx->fltr_pbuf)))) {
		NDX_ENTRY this;
		next((mode==TLESS || mode==TGTREQU)? BACKWARD : FORWARD);
		if (etof(0)) goto nofind2;
		ndx_get_key(0, &this);
		q = ne_get_key(getpage(curr_ndx->page_no, curr_ndx->pagebuf, 0), curr_ndx->entry_no);
		if (mode == TEQUAL && keycmp(this.key, q)) goto nofind2;
		if (mode == TLESSEQU && keycmp(this.key, q) > 0) {
			mode = TLESS;
			next(BACKWARD);
		}
	}
	if (go_rel()) setflag(rec_found);
	ndx_unlock();
	return;
nofind:
	while (nk--) pop_stack(DONTCARE);
nofind2:
	setflag(e_o_f);
	go_rel();
	ndx_unlock();
}

int _locate(int dir, CODE_BLOCK *pbuf)
{
	 while (!cond(pbuf)) {
		 skip(dir==FORWARD ? 1 : -1);
		 if (etof(dir)) return(0);
	 }
	 setflag(rec_found);
	 return(1);
}

int etof(int dir)
{
	int end = flagset(e_o_f);
	int top = flagset(t_o_f);
	return(dir ? (dir == FORWARD? end : top) : end || top);
}

void clretof(int dir)
{
	switch (dir) {
		case FORWARD:
			clrflag(e_o_f);
			break;

		default:
		case 0:
			r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
			clrflag(e_o_f);

		case BACKWARD:
			clrflag(t_o_f);
	}
}

void setetof(int dir)
{
	int t;

	switch (dir) {
		default:
		case 0:
			setflag(e_o_f);
			setflag(t_o_f);
			break;
		case FORWARD:
			setflag(e_o_f);
			curr_ctx->crntrec = curr_dbf->dbf_h->reccnt + 1;
			break;
		case BACKWARD:
			setflag(t_o_f);
			curr_ctx->crntrec = (recno_t)0;
	}
	if (etof(0) && ((t = curr_ctx->rela) != -1)) {
		int ctx_id = get_ctx_id();
		ctx_select(t);
		setetof(dir); //recursive
		ctx_select(ctx_id);
	}
}
