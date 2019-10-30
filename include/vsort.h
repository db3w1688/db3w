/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/vsort.h
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

#include <limits.h>
#ifndef VSORT_H_INCLUDED
#define VSORT_H_INCLUDED

#define SWITCHAR		'/'
#define MAX_PATH_SIZE	PATH_MAX
#define MAX_MSG_SIZE	1024

#define STR_COMPLETE	"completed"
#define STR_VERIFY		"verified"

#define ALIGN_SHIFT		2			//align to 4 byte boundary
#define MAX_RECCOUNT	(UINTMAX_MAX - 1) //2^64 - 2)
#define NKEYS			65536		//number of keys in buffer

#define VSORT_SUCCESS		0
#define VSORT_ERR_NOMEM		-100
#define VSORT_ERR_PARAM		-101
#define VSORT_ERR_OPEN		-102
#define VSORT_ERR_SEEK		-103
#define VSORT_ERR_READ		-104
#define VSORT_ERR_WRITE		-105
#define VSORT_ERR_VERIFY	-106

#define PASS1 60          /* percent time on sorting segments */
#define PASS2 40          /* ............... merge .......... */
#define INSORTSZ 256

typedef struct vfio {
	int fd;
	off_t offset;
} VFIO;

typedef struct vsort_rec {
	recno_t rec_no;
	BYTE key[];
} __attribute__((packed)) VSORT_REC;

typedef struct vsort_ctl {
	BYTE *modname;
	BYTE *msglbl;
	void *hptr;
	char temp_path[MAX_PATH_SIZE];
	BYTE key_type, descending;
	int key_size, vrec_size;          /* sort key size and aligned key+recno size */
	recno_t nkeys;
	VSORT_REC **keyptrs, *kbuf, *kbuf1, *kbuf2;  /* used for merge sort */
	VFIO recvf, tempvf[2];
	BYTE *rec_buf;
	recno_t rec_count, rec_prcd;
	int rec_size;
	int start_offset, key_offset;
	int npass;
	int (*get_record)(struct vsort_ctl *, recno_t);
	int (*get_key)(struct vsort_ctl *, VSORT_REC *, recno_t);
	int (*qcomp)(struct vsort_ctl *, VSORT_REC *, VSORT_REC *, int, int);
	void (*msg_progress)(int, char *, char *);
	void (*msg_display)(char *);
} VSORT_CTL;

VSORT_CTL *vsort_init(char *modname, char *msglbl, char *data_path, char *temp_path, int key_type, 
				int key_size, int key_offset, int rec_size, int start_offset,
				int (*get_record)(VSORT_CTL *, recno_t), int (*get_key)(VSORT_CTL *, VSORT_REC *, recno_t),
				int (*qcomp)(VSORT_CTL *, VSORT_REC *, VSORT_REC *, int, int), void (*msg_progress)(int, char *, char *),
				void (*msg_display)(char *), int *ccode);
void vsort_end(VSORT_CTL *v, int keep);
int vsort_(VSORT_CTL *v, recno_t rec_count, int verify);

#endif
