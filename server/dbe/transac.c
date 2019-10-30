/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/transac.c
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
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "hb.h"
#include "dbe.h"

uint32_t crc_32(const unsigned char *input_str, size_t num_bytes);

#define TRANS_DIR	"transactions"

static char trans_id[TRANSID_LEN + 1] = { 0 };

int transaction_active(void)
{
	return(!!trans_id[0]);
}

static void transaction_clear(void)
{
	trans_id[0] = '\0';
}

char *transaction_get_id(void)
{
	return(trans_id);
}

static void transaction_set_id(char *tid)
{
	if (strlen(tid) > TRANSID_LEN) syn_err(NAME_TOOLONG);
	strcpy(trans_id, tid);
	chgcase(trans_id, 1);
}

static int transaction_is_match(char *tid)
{
	return(!namecmp(tid, trans_id, strlen(tid), TRANSID_LEN));
}

int transaction_table_modified(void)
{
	return(curr_ctx->rbkctl.fd >= 0 && curr_ctx->rbkctl.recno > 0);
}

char *transaction_get_path(char *tid, char *trans_path, int max)
{
	char *tp;
	if (strlen(rootpath) + 1 + strlen(TRANS_DIR) + 1 + strlen(tid) >= max) syn_err(NAME_TOOLONG);
	sprintf(trans_path, "%s/%s/", rootpath, TRANS_DIR);
	tp = trans_path + strlen(trans_path);
	strcpy(tp, tid);
	chgcase(tp, 1);
	return(trans_path);
}

void transaction_chk_set_dir(void)
{
	char trans_path[FULFNLEN];
	snprintf(trans_path, FULFNLEN, "%s/%s", rootpath, TRANS_DIR);
	if (mkdir(trans_path, 0700) < 0 && errno != EEXIST) db_error(MKDIR_FAILED, trans_path);
}

char *form_rbk_fn(char *tid, char *basefn)
{
	static char rbkfn[FULFNLEN];
	char *tp;
	tp = strrchr(basefn, '.');
	if (!tp) tp = basefn + strlen(basefn);
	if ((int)(tp - basefn) + 1 + strlen(tid) + 1 + EXTLEN > FULFNLEN) syn_err(NAME_TOOLONG);
	sprintf(rbkfn, "%.*s.%s", (int)(tp - basefn), basefn, tid);
	tp = strrchr(rbkfn, '.');
	chgcase(tp, 1);
	strcat(tp, ".rbk");
	return(rbkfn);
}

int rbk_open(char *tid, RBK *rbk, char *dbf_fn, int mode) //OLD_FILE, NEW_FILE
{
	char *rbkfn = form_rbk_fn(tid, dbf_fn);
	rbk->fd = open(rbkfn, O_RDWR | (mode == NEW_FILE? O_CREAT | O_EXCL: 0), 0600);
	if (rbk->fd < 0) return(-1);
	return(rbk->fd);
}

void rbk_write(char *tid, RBK *rbk, int byprec)
{
	write_recno(rbk->iobuf + sizeof(DWORD), rbk->recno);
	write_dword(rbk->iobuf, crc_32(rbk->iobuf + RBK_RNO_OFF, byprec + sizeof(recno_t)));
	lseek(rbk->fd, 0, SEEK_END);
	if (write(rbk->fd, rbk->iobuf, rbk_iobufsize(byprec)) != rbk_iobufsize(byprec)) {
		db_error(DBWRITE, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	}
	fsync(rbk->fd);
}

int rbk_read_last(char *tid, RBK *rbk, int byprec)
{
	int bytes_read = 0;
	off_t pos = lseek(rbk->fd, 0, SEEK_END);
	if (pos == (off_t)-1) db_error(DBSEEK, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	if (pos == 0) return(0);
	pos -= rbk_iobufsize(byprec);
	if (pos < 0) db_error(RBK_INVAL, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	lseek(rbk->fd, pos, SEEK_SET);
	bytes_read = read(rbk->fd, rbk->iobuf, rbk_iobufsize(byprec));
	if (bytes_read < 0) db_error(DBREAD, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	if (bytes_read != rbk_iobufsize(byprec) || 
		read_dword(rbk->iobuf) != crc_32(rbk->iobuf + RBK_RNO_OFF, byprec + sizeof(recno_t))) 
			db_error(RBK_INVAL, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	lseek(rbk->fd, pos, SEEK_SET);
	rbk->recno = read_recno(rbk->iobuf + RBK_RNO_OFF);
	rbk->flags = 0; //not used
	return(1);
}

int rbk_read_prev(char *tid, RBK *rbk, int byprec)
{
	int bytes_read = 0;
	off_t pos = lseek(rbk->fd, 0, SEEK_CUR);
	if (pos == (off_t)-1) db_error(DBSEEK, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	if (pos == 0) return(0);
	pos -= rbk_iobufsize(byprec);
	if (pos < 0) db_error(RBK_INVAL, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	lseek(rbk->fd, pos, SEEK_SET);
	bytes_read = read(rbk->fd, rbk->iobuf, rbk_iobufsize(byprec));
	if (bytes_read < 0) db_error(DBREAD, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	if (bytes_read != rbk_iobufsize(byprec) || 
		read_dword(rbk->iobuf) != crc_32(rbk->iobuf + RBK_RNO_OFF, byprec + sizeof(recno_t))) 
			db_error(RBK_INVAL, form_rbk_fn(tid, curr_ctx->dbfctl.filename));
	lseek(rbk->fd, pos, SEEK_SET);
	rbk->recno = read_recno(rbk->iobuf + RBK_RNO_OFF);
	rbk->flags = 0; //not used
	return(1);
}

void transaction_cancel(char *tid)
{
	char trans_path[FULFNLEN];
	CTX *cbp;
	int i;
	if (!transaction_is_match(tid)) db_error(TRANSAC_NOTFOUND, tid);
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == IN_USE) {
		cbp = &table_ctx[i];
		if (cbp->rbkctl.fd != -1 && cbp->rbkctl.recno != 0) db_error(TRANSAC_NOEND, tid);
	}
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == IN_USE) {
		cbp = &table_ctx[i];
		if (cbp->rbkctl.fd != -1) {
			close(cbp->rbkctl.fd);
			cbp->rbkctl.fd = -1;
			cbp->rbkctl.recno = 0;
		}
		unlink(form_rbk_fn(tid, cbp->dbfctl.filename));
	}
	unlink(transaction_get_path(tid, trans_path, FULFNLEN));
	transaction_clear();
}

void transaction_add_table(char *tid, char *tbl_path)
{
	char trans_path[FULFNLEN];
	FILE *fp;
	if (!tid) tid = transaction_get_id();
	transaction_get_path(tid, trans_path, FULFNLEN);
	fp = fopen(trans_path, "a");
	if (!fp) db_error(DBOPEN, trans_path);
	fprintf(fp, "%s\n", tbl_path);
	fclose(fp);
}

void transaction_start(char *tid)
{
	char trans_path[FULFNLEN];
	int i, fd = -1, err = -1;
	FILE *tfp = NULL;
	if (transaction_active()) db_error(TRANSAC_ALRDY_ON, tid);
	transaction_set_id(tid);
	transaction_chk_set_dir();
	transaction_get_path(tid, trans_path, FULFNLEN);
	if ((fd = open(trans_path, O_CREAT | O_EXCL | O_RDWR, 0600)) < 0) {
		if (errno == EEXIST) db_error(TRANSAC_EXIST, tid);
		else db_error(DBOPEN, trans_path);
	}
	tfp = fdopen(fd, "a");
	if (!tfp) goto end;
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == IN_USE && !table_ctx[i].db_flags.readonly) {
		if (rbk_open(tid, &table_ctx[i].rbkctl, table_ctx[i].dbfctl.filename, NEW_FILE) < 0) goto end;
		fprintf(tfp, "%s\n", table_ctx[i].dbfctl.filename);
	}
	err = 0;
end:
	if (tfp) fclose(tfp);
	else if (fd != -1) close(fd);
	if (err) {
		transaction_cancel(tid);
		db_error(TRANSAC_FAIL, tid);
	}
}

void transaction_commit(char *tid)
{
	char trans_path[FULFNLEN];
	int i, ctx_id = get_ctx_id();
	if (!transaction_is_match(tid)) db_error(TRANSAC_NOTFOUND, tid);
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == IN_USE) {
		if (table_ctx[i].rbkctl.fd != -1) {
			ctx_select(i);
			if (rbk_read_last(tid, &curr_ctx->rbkctl, curr_dbf->dbf_h->byprec)) do {
				if (r_lock(curr_ctx->rbkctl.recno, HB_WRLCK, 1)) db_error0(NORLOCK);
				gorec(curr_ctx->rbkctl.recno);
				*get_rec_buf() = VAL_MKR;
				write_rec(1);
				r_lock(curr_ctx->rbkctl.recno, HB_UNLCK, 0);
			} while (rbk_read_prev(tid, &curr_ctx->rbkctl, curr_dbf->dbf_h->byprec));
			close(curr_ctx->rbkctl.fd);
			curr_ctx->rbkctl.fd = -1;
			curr_ctx->rbkctl.recno = 0;
			unlink(form_rbk_fn(tid, curr_ctx->dbfctl.filename));
		}
	}
	transaction_get_path(tid, trans_path, FULFNLEN);
	unlink(trans_path);
	transaction_clear();
	ctx_select(ctx_id);
}

void rbk_rollback(char *tid, RBK *rbk, int byprec, int update_index)
{
	if (rbk->fd < 0) db_error(TRANSAC_NORBK, tid);
	if (rbk_read_last(tid, rbk, byprec)) do {
		char *orig, *mod;
		if (r_lock(rbk->recno, HB_WRLCK, 1)) db_error(NORLOCK, tid);
		gorec(rbk->recno);
		orig = get_rbk_buf(); mod = get_rec_buf();
		if (blkcmp(orig + 1, mod + 1, byprec - 1)) {
			blkswap(mod, orig, byprec);
			write_rec(0); //write out the original record
			if (update_index && index_active()) updndx(1);
		}
		r_lock(rbk->recno, HB_UNLCK, 0);
	} while (rbk_read_prev(tid, rbk, byprec));
	close(rbk->fd);
	rbk->fd = -1;
	rbk->recno = 0;
}

void transaction_rollback(char *tid)
{
	char trans_path[FULFNLEN];
	int i, ctx_id = get_ctx_id();
	if (transaction_active()) {
		if (!transaction_is_match(tid)) db_error(TRANSAC_NOTFOUND, tid);
		for (i=0; i<MAXCTX; ++i) if (_chkdb(i) == IN_USE){
			if (table_ctx[i].rbkctl.fd != -1) {
				ctx_select(i);
				rbk_rollback(tid, &curr_ctx->rbkctl, curr_dbf->dbf_h->byprec, 1);
			}
		}
	}
	else {
		char dbf_fn[FULFNLEN];
		FILE *tfp;
		transaction_get_path(tid, trans_path, FULFNLEN);
		if (!(tfp = fopen(trans_path, "r"))) db_error(TRANSAC_NOTFOUND, tid);
		i = ctx_next_avail();
		if (i < 0) db_error(NO_CTX, dbf_fn);
		ctx_select(i);
		while (fgets(dbf_fn, FULFNLEN, tfp)) {
			char *tp = dbf_fn + strlen(dbf_fn);
			if (*(tp - 1) == '\n') *(tp - 1) = '\0';
			for (i=0; i<MAXCTX; ++i) 
				if (_chkdb(i) == IN_USE && !strcmp(table_ctx[i].dbfctl.filename, dbf_fn)) db_error(STILL_OPEN, dbf_fn);
			tp = strrchr(dbf_fn, SWITCHAR);
			if (tp) {
				*tp = '\0';
				push_string(dbf_fn, (int)(tp - dbf_fn));
				*tp++ = SWITCHAR;
				push_int(1);
				set_to(PATH_TO);
			}
			else tp = dbf_fn;
			push_string(tp, strlen(tp));
			use(U_USE);
			if (rbk_open(tid, &curr_ctx->rbkctl, dbf_fn, OLD_FILE) < 0) db_error(TRANSAC_NORBK, dbf_fn);
			rbk_rollback(tid, &curr_ctx->rbkctl, curr_dbf->dbf_h->byprec, 0); //don't update index
		}
		fclose(tfp);
	}
	transaction_set_id(tid);
	transaction_cancel(tid);
	ctx_select(ctx_id);
}
