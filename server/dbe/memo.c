/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/memo.c
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

#include "hb.h"
#include "dbe.h"

blkno_t get_memo(int tmpfd, int memofd, blkno_t blkno)
{
	char memobuf[BLKSIZE];
	int n;
	size_t total = 0;
	lseek(tmpfd,( off_t)0, 0);
	if (lseek(memofd, (off_t)blkno * BLKSIZE, 0) == (off_t)-1) return(-1);
	do {
		n = read(memofd, memobuf, BLKSIZE);
		if (n < 0) return(-1);
		if (write(tmpfd, memobuf, n) != n) return(-1);
		total += n;
	} while (n == BLKSIZE);
	lseek(tmpfd, (off_t)0, 0);
	return((total + BLKSIZE - 1) / BLKSIZE);
}

blkno_t put_memo(int tmpfd, int memofd, blkno_t blkno)
{
	char memobuf[BLKSIZE];
	int n,stop;
	size_t total = 0;
	lseek(tmpfd, (off_t)0, 0);
	if (lseek(memofd, (off_t)blkno * BLKSIZE, 0) == (off_t)-1) return(-1);
	for (stop=FALSE; !stop;) {
		if ((n = read(tmpfd, memobuf, BLKSIZE)) < 0) return(-1);
		if (n < BLKSIZE) {
			if (memobuf[n - 1] != EOF_MKR) memobuf[n++] = EOF_MKR;
			stop = TRUE;
		}
		if (write(memofd, memobuf, BLKSIZE) != BLKSIZE) return(-1);
		fsync(memofd);
		total += BLKSIZE;
	}
	return(total / BLKSIZE);
}	

blkno_t memo_copy(int fdbt, int tdbt, blkno_t fblkno, blkno_t tblkno)
{
	char memobuf[BLKSIZE];
	blkno_t blk_cnt = 0;
	int stop, n, i;
	if (lseek(fdbt, (off_t)fblkno * BLKSIZE, 0) == (off_t)-1) return(-1);
	if (lseek(tdbt, (off_t)tblkno * BLKSIZE, 0) == (off_t)-1) return(-1);
	for (stop=0; !stop;) {
		if ((n = read(fdbt, memobuf, BLKSIZE)) < 0) return(-1);
		if (n < BLKSIZE) {
			memobuf[n++] = EOF_MKR;
			stop = 1;
		}
		else for (i=0; i<n; ++i) if (memobuf[i] == EOF_MKR) stop = 1;
		if (write(tdbt, memobuf, BLKSIZE) != BLKSIZE) return(-1);
		++blk_cnt;
	}
	return(blk_cnt);
}

blkno_t get_next_memo(int memofd)
{
	blkno_t blkno;
	BYTE blkno_buf[sizeof(blkno_t)];
	lseek(memofd, (off_t)0, 0);
	if (read(memofd, blkno_buf, sizeof(blkno_t)) != sizeof(blkno_t)) return(-1);
	blkno = read_blkno(blkno_buf);
	return(blkno);
}

int put_next_memo(int memofd, blkno_t blkno)
{
	BYTE blkno_buf[sizeof(blkno_t)];
	write_blkno(blkno_buf, blkno);
	lseek(memofd, (off_t)0, 0);
	if (write(memofd, blkno_buf, sizeof(blkno_t)) != sizeof(blkno_t)) return(-1);
}
