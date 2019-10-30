/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/fio.c
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
#include <sys/stat.h>
#include "hb.h"
#include "dbe.h"

#define IOBUFSIZE	0x4000

int s_open(char *fn, int mode)
{
	int fd;
retry:
	if ((fd = open(fn,
			(mode & RDONLY? O_RDONLY : O_RDWR) |
			(mode & NEW_FILE? O_CREAT | (mode & OVERWRITE? O_TRUNC : O_EXCL) : 0)
			, 0664)) < 0) {
				if (errno == EACCES) {
					if (!(mode & RDONLY) && isset(READONLY_ON) == S_AUTO) {
						clrflag(exclusive);
						setflag(readonly);
						mode |= RDONLY;
						mode &= ~EXCL;
						goto retry;
					}
					else db_error(DB_ACCESS, fn);
				}
				else if (errno == EEXIST) db_error(FILEEXIST, fn);
				else db_error(DBOPEN, fn);
			}
	if (mode & EXCL || mode & DENYWRITE) {
		struct flock hb_lock;
		hb_lock.l_type = mode & EXCL? F_WRLCK : F_RDLCK;
		hb_lock.l_whence = 0;
		hb_lock.l_start = 0;
		hb_lock.l_len = 0; /* to EOF */
		if (fcntl(fd, F_SETLK, &hb_lock)) {
			close(fd);
			db_error(NOFLOCK, fn);
		}
	}
	return(fd);
}

size_t dbread(char *buf, off_t offset, size_t nbytes)
{
	int i, j, k, n;
	int dbfd = curr_ctx->dbfctl.fd;
	char *db_fn = curr_ctx->dbfctl.filename;
 
	/* IOBUFSIZE is the read/write unit */
 
	if (lseek(dbfd, offset, 0) == (off_t)-1) db_error(DBSEEK, db_fn);
	k = nbytes % IOBUFSIZE;
	n = nbytes / IOBUFSIZE;
	for (i=0; i<n; ++i) {
		if ((j = read(dbfd, buf, IOBUFSIZE)) < 0) db_error(DBREAD, db_fn);
		if (j < IOBUFSIZE) goto end;
		buf += IOBUFSIZE;
	}
	if ((k > 0) && (j = read(dbfd, buf, k)) < 0) db_error(DBREAD, db_fn);
end:	return((size_t)i * IOBUFSIZE + j);
}

void dbwrite(char *buf, off_t offset, size_t nbytes)
{
	int i, j, k, n;
	int dbfd = curr_ctx->dbfctl.fd;
	char *db_fn = curr_ctx->dbfctl.filename;
 
	if (lseek(dbfd, offset, 0) == (off_t)-1) db_error(DBSEEK, db_fn);
	k = nbytes % IOBUFSIZE;
	n = nbytes / IOBUFSIZE;
	for (i=0; i<n; ++i) {
		if ((j = write(dbfd, buf, IOBUFSIZE)) != IOBUFSIZE) db_error(DBWRITE, db_fn);
		buf += IOBUFSIZE;
	}
	if ((j = write(dbfd, buf, k)) != k) db_error(DBWRITE, db_fn);
	if (flagset(fsync)) fsync(dbfd);
}
