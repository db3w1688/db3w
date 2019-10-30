/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/lock.c
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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
extern int errno;
#include "hb.h"
#include "dbe.h"

int rlock(int fd, off_t offset, off_t len, int type)
{
	struct stat f_stat;
	if (fstat(fd,&f_stat)) return(-1);
	//if (!(f_stat.st_mode & S_IWRITE)) return(0);
	struct flock hb_lock;
	int rc;
	memset(&hb_lock, 0, sizeof(struct flock));
	hb_lock.l_whence = 0;
	hb_lock.l_start = offset;
	hb_lock.l_len = len;
	switch (type) {
	case HB_RDLCK:
		hb_lock.l_type = F_RDLCK;
		break;
	case HB_WRLCK:
		hb_lock.l_type = F_WRLCK;
		break;
	case HB_UNLCK:
		hb_lock.l_type = F_UNLCK;
		break;
	default:
		db_error0(UNKNOWNLOCK);
		break;
	}
	rc = fcntl(fd, F_SETLK, &hb_lock);
	if (rc == -1) {
		log_printf("attempt to lock fd=%d offset=%ld len=%ld failed, type=%x,errno=%d\n", fd, offset, len, hb_lock.l_type, errno);
		fcntl(fd, F_GETLK, &hb_lock);
		log_printf("current lock pid=%d offset=%ld len=%ld, type=%x\n", hb_lock. l_pid,hb_lock. l_start, hb_lock. l_len,hb_lock.l_type);
	}
	return(rc);
}

int is_rec_locked(recno_t recno, int type)
{
	if (type == HB_WRLCK && curr_ctx->lock.type == HB_FWLCK) return(1);
	if (type == HB_RDLCK && (curr_ctx->lock.type == HB_FWLCK || curr_ctx->lock.type == HB_FRLCK)) return(1);
	return((curr_ctx->lock.recno == 0 || curr_ctx->lock.recno == recno) 
		&& (curr_ctx->lock.type == type || curr_ctx->lock.type == HB_WRLCK));
}

int r_lock(recno_t recno, int type, int do_retry)
{
	if (chkdb() != IN_USE) return(-1);
	if (!recno) return(-1);
	if (flagset(readonly) && type == HB_WRLCK) return(-1);
	if (flagset(exclusive)) return(0);
	if (flagset(denywrite) && type == HB_RDLCK) return(0);
	if (is_rec_locked(recno, type)) return(0);

	int i, rc = -1, fd = curr_ctx->dbfctl.fd;
	int len = curr_dbf->dbf_h->byprec;
	off_t offset = (off_t)(recno - 1) * len + curr_dbf->dbf_h->doffset;
	for (i=0; i<LRETRY; ++i) { //100 retries, total 1 sec
		if ((rc = rlock(fd, offset, len, type)) < 0) {
			if (!do_retry) break;
			usleep(10000); //10 millisecs
			continue;
		}
		else break;
	}
	if (!rc) {
		curr_ctx->lock.recno = recno;
		curr_ctx->lock.type = type;
		if (type == HB_UNLCK) curr_ctx->lock.mode = 0;
	}
	return(rc);
}

void unlock_all(void)
{
	int ctx_id = get_ctx_id();
	int i;
	struct flock hb_lock;
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i)==IN_USE) {
		ctx_select(i);
		hb_lock.l_type = F_UNLCK;
		hb_lock.l_whence = 0;
		hb_lock.l_start = 0;
		hb_lock.l_len = 0;	/* to EOF */
		fcntl(curr_ctx->dbfctl.fd, F_SETLK, &hb_lock);
		curr_ctx->lock.type = HB_UNLCK;
		curr_ctx->lock.recno = 0;
		curr_ctx->lock.mode = 0;
	}
	ctx_select(ctx_id);
}

void f_unlock(void)
{
	if (flagset(exclusive) || flagset(denywrite)) return; //don't unlock
	fsync();
	struct flock hb_lock;
	hb_lock.l_type = F_UNLCK;
	hb_lock.l_whence = 0;
	hb_lock.l_start = 0;
	hb_lock.l_len = 0;	/* to EOF */
	if (!fcntl(curr_ctx->dbfctl.fd, F_SETLK, &hb_lock)) {
		curr_ctx->lock.type = HB_UNLCK;
		setflag(fsync);
	}
}

int f_lock(int exclusive)
{
	int i;
	if (chkdb() != IN_USE) return(-1);
	if (flagset(readonly) && exclusive) return(-1);
	if (flagset(exclusive) || curr_ctx->lock.type == HB_FWLCK) return(0);
	if (!exclusive && (flagset(denywrite) || curr_ctx->lock.type == HB_FWLCK || curr_ctx->lock.type == HB_FRLCK)) return(0);

	f_unlock();

	struct flock hb_lock;
	hb_lock.l_type = exclusive? F_WRLCK : F_RDLCK;
	hb_lock.l_whence = 0;
	hb_lock.l_start = 0;
	hb_lock.l_len = 0;	/* to EOF */
	for (i=0; i<LRETRY; ++i) {
		if (!fcntl(curr_ctx->dbfctl.fd, F_SETLK, &hb_lock)) {
			curr_ctx->lock.type = exclusive? HB_FWLCK : HB_FRLCK;
			curr_ctx->lock.recno = 0; //all records
			if (exclusive) clrflag(fsync);
			return(0);
		}
		usleep(10000);
		continue;
	}

	return(-1);
}

void ndx_lock(pageno_t pno, int type)
{
	if (flagset(exclusive) || (type == HB_RDLCK && flagset(denywrite))) return;
	int fd = curr_ctx->ndxfctl[curr_ndx->seq].fd, i;
	for (i=0; i<LRETRY; ++i) 
		if (rlock(fd, (off_t)pno * HBPAGESIZE, (off_t)HBPAGESIZE, type))
			usleep(10000);
		else break;
	if (i >= LRETRY) db_error(NONDXLOCK, curr_ctx->ndxfctl[curr_ndx->seq].filename);
}

void ndx_unlock(void)
{
	if (flagset(exclusive) || flagset(denywrite)) return;
	int fd = curr_ctx->ndxfctl[curr_ndx->seq].fd;
	rlock(fd, (off_t)0, (off_t)0, HB_UNLCK); //unlock to EOF
}

#define is_hdr_locked(hdr)	(0x8000 & hdr->id)
#define set_hdr_lock(hdr)	(hdr->id |= 0x8000)
#define clr_hdr_lock(hdr)	(hdr->id &= 0x7fff)

void lock_hdr(void)
{
	TABLE *hdr = curr_dbf->dbf_h;
	int hdrsz = is_db3(hdr)? HD3SZ : HDRSZ;
	char xbuf[HDRSZ];
	if (flagset(exclusive) || flagset(denywrite)) return;
	if (!is_hdr_locked(hdr)) {
		int i;
		for (i=0; i<LRETRY; ++i) 
			if (rlock(curr_ctx->dbfctl.fd, (off_t)0, (off_t)hdrsz, HB_RDLCK))
				usleep(10000);
			else break;
		if (i >= LRETRY) db_error(NORLOCK, curr_ctx->dbfctl.filename);
	}
	dbread(xbuf, (off_t)0, (size_t)hdrsz);
	read_db_hdr(hdr, xbuf, hdrsz, 1);
	set_hdr_lock(hdr);
}

void unlock_hdr(void)
{
	TABLE *hdr = curr_dbf->dbf_h;
	int is_db3 = MKR_MASK & hdr->id == DB3_MKR;
	if (flagset(exclusive) || flagset(denywrite)) return;
	if (is_hdr_locked(hdr)) rlock(curr_ctx->dbfctl.fd, (off_t)0, (off_t)HD3SZ, HB_UNLCK);
	clr_hdr_lock(hdr);
}

int test_lock_hdr(int type)
{
	struct flock hb_lock;
	int rc;
	memset(&hb_lock, 0, sizeof(struct flock));
	if (type == HB_RDLCK) hb_lock.l_type = F_RDLCK;
	else if (type == HB_WRLCK) hb_lock.l_type = F_WRLCK;
	else db_error0(UNKNOWNLOCK);
	hb_lock.l_whence = 0;
	hb_lock.l_start = 0;
	hb_lock.l_len = HD3SZ;
	if (fcntl(curr_ctx->dbfctl.fd, F_GETLK, &hb_lock) < 0) return(-1);
	return(hb_lock.l_type == F_UNLCK? 0 : 1);
}
