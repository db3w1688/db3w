/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/vsort.c
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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

typedef unsigned char	BYTE;
typedef unsigned int	DWORD;
typedef unsigned long	LWORD;
typedef LWORD		recno_t;

#include "vsort.h"

#ifndef NO_MAIN
static char Pname[] = "vsort";
static char Version[] = "1.0";
static struct option const Options[] =
{
	{ "temp",     required_argument, NULL, 't' } ,
	{ "number",   required_argument, NULL, 'n' } ,
	{ "keysize",  required_argument, NULL, 'k' } ,
	{ "keyoffset",required_argument, NULL, 'b' } ,
	{ "recsize",  required_argument, NULL, 'r' } ,
	{ "count",    required_argument, NULL, 'c' } ,
	{ "offset",   required_argument, NULL, 's' } ,
	{ "label",    required_argument, NULL, 'l' } ,
	{ "odo",      no_argument,       NULL, 'o' } ,
	{ "verify",   no_argument,       NULL, 'V' } ,
	{ "help",     no_argument,       NULL, 'h' } ,
	{ "version",  no_argument,       NULL, 'v' } ,
	{ NULL, 0, NULL, 0 } ,
};
#endif

#define is_num_type(t)	(t==1)

static ssize_t vfio_read(VFIO *vf, BYTE *buf, off_t pos, DWORD size)
{
	ssize_t size_read;
	if (lseek(vf->fd, pos + vf->offset, 0) == (off_t)-1) return(VSORT_ERR_SEEK);
	size_read = read(vf->fd, buf, size);
	if (size_read < 0) return(VSORT_ERR_READ);
	return(size_read);
}

static ssize_t vfio_write(VFIO *vf, BYTE *buf, off_t pos, DWORD size)
{
	ssize_t size_written;
	if (lseek(vf->fd, pos + vf->offset, 0) == (off_t)-1) return(VSORT_ERR_SEEK);
	size_written = write(vf->fd, buf, size);
	if (size_written < 0) return(VSORT_ERR_READ);
	return(size_written);
}

#ifndef NO_MAIN
static int vsort_read_record(VSORT_CTL *v, recno_t n)
{
	off_t pos = (off_t)n * v->rec_size + v->start_offset;
	ssize_t nread;
	nread = vfio_read(&v->recvf, v->rec_buf, pos, v->rec_size);
	if (nread != v->rec_size) return(VSORT_ERR_READ);
	return(VSORT_SUCCESS);
}

static void vsort_get_key(VSORT_CTL *v, VSORT_REC *vrec, recno_t n)
{
	vrec->rec_no = n + 1;	//recno 0 is sentinel
	memcpy(vrec->key, v->rec_buf + v->key_offset, v->key_size);
}

static int qcomp(VSORT_CTL *v, VSORT_REC *p, VSORT_REC *q, int keylen, int key_type)
{
	// test for sentinel values first
	if (p->rec_no == q->rec_no) 
		return(0); //should be error
	if (q->rec_no == 0 || q->rec_no == (recno_t)-1) 
		return(-1);
	if (p->rec_no == 0 || p->rec_no == (recno_t)-1) 
		return(1);
	if (is_num_type(key_type)) {
		double n1 = *(double *)p->key;
		double n2 = *(double *)q->key;
		if (n1 == n2) return(p->rec_no > q->rec_no? 1 : -1);
		return(n1 > n2? 1 : -1);
	}
	else {
		int result = memcmp(p->key, q->key, keylen);
		if (result == 0) return(p->rec_no > q->rec_no? 1 : -1);
		return(result);
	}
}
#endif

static void mem_release(VSORT_CTL *v)
{
	if (v) {
		if (v->kbuf) free(v->kbuf);
		if (v->kbuf2) free(v->kbuf2);
		if (v->kbuf1) free(v->kbuf1);
		if (v->keyptrs) free(v->keyptrs);
		if (v->rec_buf) free(v->rec_buf);
		free(v);
	}
}

static VSORT_CTL *mem_alloc(DWORD rec_size, DWORD key_size, DWORD nkeys)
{
	ssize_t srtbufsz;
	VSORT_CTL *v = malloc(sizeof(VSORT_CTL));
	if (!v) return(NULL);
	memset(v, 0, sizeof(VSORT_CTL));
	v->rec_buf = malloc(rec_size);
	if (!v->rec_buf) goto end;
	v->nkeys = nkeys;
	v->rec_size = rec_size;
	v->key_size = key_size; //nhd->keylen
	v->vrec_size = (key_size >> ALIGN_SHIFT) << ALIGN_SHIFT;
	if (v->vrec_size < key_size) v->vrec_size += 1 << ALIGN_SHIFT;
	v->vrec_size += sizeof(VSORT_REC);	// sizeof(VSORT_REC) is sizeof(recno_t) will not include key,
	srtbufsz = v->vrec_size * nkeys;
	if (srtbufsz < 0) goto end; //overflow
	if (!(v->kbuf1 = (VSORT_REC *)malloc(srtbufsz)) || !(v->kbuf2 = (VSORT_REC *)malloc(srtbufsz))
		|| !(v->kbuf = (VSORT_REC *)malloc(srtbufsz)) 
		|| !(v->keyptrs = (VSORT_REC **)malloc(sizeof(VSORT_REC *) * nkeys))) goto end;
	memset(v->kbuf, 0, srtbufsz);
	memset(v->kbuf1, 0, srtbufsz);
	memset(v->kbuf2, 0, srtbufsz);
	v->recvf.fd = v->tempvf[0].fd = v->tempvf[1].fd = -1;
	return(v);
end:
	if (v) mem_release(v);
	return(NULL);
}

static void msg_display(char *msg)
{
	printf("%s\n", msg);
}

static void msg_progress(int pcnt, char *lbl, char *tag)
{
	static int last;
	if (last == 100) {
		last = 0;
		return;
	}
	last = pcnt;
	if (lbl) printf("%s: %d%% %s%c", lbl, pcnt, tag? tag : "" , pcnt==100? '\n' : '\r');
	else printf("%d%% %s%c", pcnt, tag? tag : "" , pcnt==100? '\n' : '\r');
}

static void insort(VSORT_CTL *v, int start, DWORD nitms)
{
	int i,k,cmp;
	VSORT_REC *x;

	for (k=1; k<nitms; ++k) {
		x = v->keyptrs[start + k];
		i = start + k - 1;
		while (i>=start) {
			cmp = (v->qcomp)(v, x, v->keyptrs[i], v->key_size, v->key_type);
			if (v->descending) cmp = -cmp;
			if (cmp < 0) {
				v->keyptrs[i + 1] = v->keyptrs[i];
				--i; 
			}
			else break;
		}
		v->keyptrs[i + 1] = x;
	}
}

static void _qsort_(VSORT_CTL *v, DWORD start, DWORD nitms)
{
	DWORD i,j;
	int cmp;
	VSORT_REC *x;

	if (nitms < INSORTSZ) {
		insort(v, start, nitms);
		return;
	}
	i = start;
	j = start + nitms - 1;
	x = v->keyptrs[start + nitms / 2];
	while (i<=j) {
		do {
			cmp = (v->qcomp)(v, v->keyptrs[i], x, v->key_size, v->key_type);
			if (v->descending) cmp = -cmp;
			if (cmp < 0) ++i;
		} while (cmp < 0);
		do {
			cmp = (v->qcomp)(v, v->keyptrs[j], x, v->key_size, v->key_type);
			if (v->descending) cmp = -cmp;
			if (cmp > 0) --j;
		} while (cmp > 0);
		if (i <= j) {
			VSORT_REC *tmp = v->keyptrs[i];
			v->keyptrs[i] = v->keyptrs[j];
			v->keyptrs[j] = tmp;
			++i; 
			--j;
		} 
	}
	if (j > start) _qsort_(v, start, j - start + 1);
	if (i < start+nitms-1) _qsort_(v, i, start + nitms - i);
}

int qsort_(VSORT_CTL *v, VSORT_REC *vrecs, DWORD nitms, DWORD segment)
{
	DWORD i;
	off_t offset = (off_t)segment * v->nkeys * v->vrec_size;

	for (v->keyptrs[0]=vrecs, i=0; i<nitms-1; ++i) {
		v->keyptrs[i + 1] = (VSORT_REC *)((BYTE *)v->keyptrs[i] + v->vrec_size);
	}
	_qsort_(v, 0, nitms);
	for (i=0; i<nitms; ++i)	{
		if (vfio_write(&v->tempvf[0], (BYTE *)v->keyptrs[i], offset, v->vrec_size) != v->vrec_size) return(VSORT_ERR_WRITE);
		offset += v->vrec_size;
	}
	return(0);
}

static int fillbuf(VFIO *vf, BYTE buf[], ssize_t size, off_t pos, int descending)
{
	ssize_t rr;

	if ((rr = vfio_read(vf, buf, pos, size)) < 0) return(VSORT_ERR_READ);
	if (rr < size) 
		memset(buf+rr, 0xff, size - rr);
	return(0);
}

static int merge(VSORT_CTL *v, int pass, recno_t n, int from, int to)
{
	VFIO *from_vf = &v->tempvf[from], *to_vf = &v->tempvf[to];
	VSORT_REC *p, *q, *r;
	ssize_t fsize, srtbufsz;
	DWORD titms, titms1, titms2, count;
	off_t pos1, pos2, tpos, tpos1, tpos2;
	int fill1, fill2, err;

	srtbufsz = v->vrec_size * v->nkeys;
	if (n >= v->rec_count) {
		err = fillbuf(from_vf, (BYTE *)v->kbuf1, srtbufsz, (off_t)0, v->descending);
		if (err < 0) return(-1);
		if (v->msg_progress) (v->msg_progress)(PASS1 + PASS2, v->msglbl, STR_COMPLETE);
		return(from);
	}
	fsize = (ssize_t)v->rec_count * v->vrec_size;
	titms = titms1 = titms2 = 0;
	err = fillbuf(from_vf, (BYTE *)(p = v->kbuf1), srtbufsz, pos1 = tpos1 = (off_t)0, v->descending);
	if (err < 0) return(-1);
	err = fillbuf(from_vf, (BYTE *)(q = v->kbuf2), srtbufsz, pos2 = tpos2 = (off_t)n * v->vrec_size, v->descending);
	if (err < 0) return(-1);
	tpos = 0;
	r = v->kbuf;
	fill1 = fill2 = 0;
	while (titms < v->rec_count) {
		int which, cmp;
		if (!fill1 && !fill2) { //both buffers not empty, p pointing to next item in kbuf1 and q pointing to kbuf2
			cmp = (v->qcomp)(v, p, q, v->key_size, v->key_type);
			if (v->descending) cmp = -cmp;
		}
		which = fill1? 1 : (fill2? 0 : (cmp > 0)); //which buffer to take the next item
		if (which == 0) {
			memcpy(r, p, v->vrec_size);
			++titms1;
			p = (VSORT_REC *)((BYTE *)p + v->vrec_size);
			if (titms1 == n) {
				pos1 += 2 * n * v->vrec_size;
				fill1 = 1;
			}
			else if ((BYTE *)p >= (BYTE *)v->kbuf1 + srtbufsz) {
				err = fillbuf(from_vf, (BYTE *)(p = v->kbuf1), srtbufsz,  tpos1 += srtbufsz, v->descending);
				if (err < 0) return (-1);
			}
		}
		else {
			memcpy(r, q, v->vrec_size);
			++titms2;
			q = (VSORT_REC *)((BYTE *)q + v->vrec_size);
			if (titms2 == n) {
				pos2 += 2 * n * v->vrec_size;
				fill2 = 1;
			}
			else if ((BYTE *)q >= (BYTE *)v->kbuf2 + srtbufsz) {
				err = fillbuf(from_vf, (BYTE *)(q = v->kbuf2), srtbufsz, tpos2 += srtbufsz, v->descending);
				if (err < 0) return(-1);
			}
		}
		if (fill1 && fill2) {
			if (pos1 < (off_t)fsize) {
				err = fillbuf(from_vf, (BYTE *)(p = v->kbuf1), srtbufsz, tpos1 = pos1, v->descending);
				if (err < 0) return(-1);
				fill1 = titms1 = 0;
			}
			if (pos2 < (off_t)fsize) {
				err = fillbuf(from_vf, (BYTE *)(q = v->kbuf2), srtbufsz, tpos2 = pos2, v->descending);
				if (err < 0) return(-1);
				fill2 = titms2 = 0;
			}
		}			
		r = (VSORT_REC *)((BYTE *)r + v->vrec_size);
		if ((BYTE *)r >= (BYTE *)v->kbuf + srtbufsz) {
			if (vfio_write(&v->tempvf[to], (BYTE *)(r = v->kbuf), tpos, srtbufsz) != srtbufsz) return(-1);
			tpos += srtbufsz;
		}
		++titms;
		if (v->msg_progress && ((titms>>10)<<10 == titms))
			(v->msg_progress)(PASS1 + (v->npass - pass) * PASS2 / v->npass + (int)(((long)titms * PASS2) / ((long)v->rec_count * v->npass)), v->msglbl, STR_COMPLETE);
	}
	if (vfio_write(&v->tempvf[to], (BYTE *)v->kbuf, tpos, count = (BYTE *)r -(BYTE *) v->kbuf) != count) return(-1);
	return(merge(v, pass-1, n * 2, to, from));
}

void vsort_end(VSORT_CTL *v, int keep)
{
	int i;
	char tempfn[MAX_PATH_SIZE];
	if (v) {
		for (i=0; i<2; ++i) if (i != keep) {
			if (v->tempvf[i].fd >= 0) close(v->tempvf[i].fd);
			if (v->temp_path[0]) {
				sprintf(tempfn, "%s%cvsort.$$%d", v->temp_path, SWITCHAR, i);
				unlink(tempfn);
			}
		}
	}
	mem_release(v);
}

VSORT_CTL *vsort_init(char *modname, char *msglbl, char *data_path, char *temp_path, int key_type, 
				int key_size, int key_offset, int rec_size, int start_offset,
				int (*get_record)(VSORT_CTL *, recno_t), int (*get_key)(VSORT_CTL *, VSORT_REC *, recno_t),
				int (*qcomp)(VSORT_CTL *, VSORT_REC *, VSORT_REC *, int, int), void (*msg_progress)(int, char *, char *),
				void (*msg_display)(char *), int *ccode)
{
	VSORT_CTL *v = NULL;
	char tempfn[MAX_PATH_SIZE], errmsg[MAX_MSG_SIZE];
	int i;
	if (key_size <= 0) {
		*ccode = VSORT_ERR_PARAM;
		goto end;
	}
	if (key_offset < 0) {
		*ccode = VSORT_ERR_PARAM;
		goto end;
	}
	if (rec_size <= 0) {
		*ccode = VSORT_ERR_PARAM;
		goto end;
	}
	if (start_offset < 0) {
		*ccode = VSORT_ERR_PARAM;
		goto end;
	}
	if (!(v = mem_alloc(rec_size, key_size, NKEYS))) {
		*ccode = VSORT_ERR_NOMEM;
		goto end;
	}
	v->modname = modname;
	v->msglbl = msglbl;
	strcpy(v->temp_path, temp_path);
	v->get_record = get_record;
	v->get_key = get_key;
	v->msg_progress = msg_progress;
	v->msg_display = msg_display;
	v->descending = 0;
	v->qcomp = qcomp;
	v->key_type = key_type;
	if (data_path) {
		v->recvf.fd = open(data_path, O_RDONLY, 0);
		if (v->recvf.fd < 0) {
			sprintf(errmsg, "%s: Failed to open %s, error=%d", modname, data_path, errno);
			if (v->msg_display) (v->msg_display)(errmsg);
			*ccode = VSORT_ERR_OPEN;
			goto end;
		}
		v->recvf.offset = start_offset;
	}
	for (i=0; i<2; ++i) {
		sprintf(tempfn, "%s%cvsort.$$%d", temp_path, SWITCHAR, i);
		v->tempvf[i].fd = open(tempfn, O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (v->tempvf[i].fd < 0) {
			sprintf(errmsg, "%s: Failed to open %s, error=%d", modname, tempfn, errno);
			if (v->msg_display) (v->msg_display)(errmsg);
			*ccode = VSORT_ERR_OPEN;
			goto end;
		}
		v->tempvf[i].offset = 0;
	}
	return(v);
end:
	vsort_end(v, -1);
	return(NULL);
}

int vsort_(VSORT_CTL *v, recno_t rec_count, int verify)
{
	recno_t n, nkey, count;
	DWORD segment;
	int which;
	char errmsg[MAX_MSG_SIZE];
	int ccode = 0;
	if (!v) return(-1);
	for (n=0, nkey=0, count=0, segment=0; n < rec_count; ++n) {
		VSORT_REC *vrec;
		ccode = (v->get_record)(v, n);
		if (ccode)
		{
			sprintf(errmsg, "%s: Error reading record %u\n", v->modname, n);
			if (v->msg_display) (v->msg_display)(errmsg);
			goto end;
		}
		if (((n>>10)<<10) == n) {
			if (v->msg_progress) (v->msg_progress)((int)((long)n * PASS1 / rec_count), v->msglbl, STR_COMPLETE);
		}
		// if skip() continue;
		vrec = (VSORT_REC *)((BYTE *)v->kbuf + nkey * v->vrec_size);
		if ((v->get_key)(v, vrec, n) < 0) continue;
		++nkey;
		if (nkey == v->nkeys) {
			qsort_(v, v->kbuf, v->nkeys, segment++);
			nkey = 0;
		}
		++count;
	}
	if (nkey) qsort_(v, v->kbuf, nkey, segment);
	if (v->msg_progress) (v->msg_progress)(PASS1, v->msglbl, STR_COMPLETE);
	v->rec_count = count;
	for (v->npass=0, n=v->nkeys; n<v->rec_count; n*=2, v->npass++);
	which = merge(v, v->npass, v->nkeys, 0, 1);
	if (v->msg_progress) {
		(v->msg_progress)(100, v->msglbl, STR_COMPLETE);
	}
	if (verify) {
		for (n=0; n<v->rec_count; ++n) {
			VSORT_REC *vrec = v->kbuf, *vrec2 = (VSORT_REC *)((BYTE *)vrec + v->vrec_size);
			ssize_t size_read;
			if (((n>>10)<<10) == n) {
				if (v->msg_progress) (v->msg_progress)((int)((long)n * 100 / v->rec_count), NULL, STR_VERIFY);
			}
			if (n > 0) memcpy((char *)vrec2, (char *)vrec, v->vrec_size);
			size_read = vfio_read(&v->tempvf[which], (BYTE *)vrec, n * v->vrec_size, v->vrec_size);
			if (size_read != v->vrec_size) {
				sprintf(errmsg, "%s: Verify failed to read entry %u\n", v->modname, n);
				if (v->msg_display) (v->msg_display)(errmsg);
				ccode = VSORT_ERR_VERIFY;
				break;
			}
			if (n > 0 && (v->qcomp)(v, vrec, vrec2, v->key_size, v->key_type) < 0) {
				sprintf(errmsg, "%s: Verify out of order at entry %u\n", v->modname, n);
				if (v->msg_display) (v->msg_display)(errmsg);
				ccode = VSORT_ERR_VERIFY;
				break;
			}
		}
		if (v->msg_progress && ccode == 0) {
			(v->msg_progress)(100, NULL, STR_VERIFY);
		}
	}
	lseek(v->tempvf[which].fd, (off_t)0, 0);
end:
	if (ccode) return(-1);
	return(which);
}

#ifndef NO_MAIN
int main(int argc, char *argv[])
{
	int ch, num;
	char temp_path[MAX_PATH_SIZE], tempfn[MAX_PATH_SIZE], *label = NULL;
	VSORT_CTL *v = NULL;
	int n, i;
	int ccode = 0, odo = 1, is_num = 0, is_verify = 0;
	int key_size = 0, key_offset = 0, rec_size = 0, start_offset = 0;
	recno_t rec_count = 0;

	if (argc <= 1) goto help;
	memset((char *) temp_path, '\0', MAX_PATH_SIZE);
	while ((ch = getopt_long(argc, argv, "t:n:k:b:r:c:s:l:Vohv", Options, &num)) >= 0)
	{
		switch(ch)
		{
			case 't':
				if (strlen(optarg) > MAX_PATH_SIZE - 10) { //vsort.$$1
					printf("%s: Temp Path too long: %s\n", Pname, optarg);
					goto help;
				}
				strcpy(temp_path, optarg);
				break;
			case 'o':
				odo = 0;
				break;
			case 'n':
				is_num = 1;
				break;
			case 'V':
				is_verify = 1;
				break;
			case 'k':
				key_size = strtol(optarg, NULL, 0);
				break;
			case 'b':
				key_offset = strtol(optarg, NULL, 0);
				break;
			case 'r':
				rec_size = strtol(optarg, NULL, 0);
				break;
			case 'c':
				rec_count = strtoul(optarg, NULL, 0);
				break;
			case 's':
				start_offset = strtol(optarg, NULL, 0);
				break;
			case 'l':
				label = optarg;
				break;
			case 'v':
				printf("%s version %s\n", Pname, Version);
				goto end;
			case 'h':
			default:
				goto help;
		}
	}
	if (strlen(argv[argc - 1]) >= MAX_PATH_SIZE) {
		fprintf(stderr, "%s: data path too long: %s\n", Pname, argv[argc - 1]);
		goto help;
	}
	if (key_size <= 0) {
		fprintf(stderr, "%s: Illegal key size: %d\n", key_size);
		goto help;
	}
	if (rec_size <= 0) {
		fprintf(stderr, "%s: Illegal record size: %d\n", rec_size);
		goto help;
	}
	if (key_offset < 0 || key_offset > rec_size - 1) {
		fprintf(stderr, "%s: Illegal key offset: %d\n", key_offset);
		goto help;
	}
	if (rec_count == 0 || rec_count >= MAX_RECCOUNT) {
		fprintf(stderr, "%s: Illegal record count: %u\n", rec_count);
		goto help;
	}
	if (start_offset < 0) {
		fprintf(stderr, "%s: Illegal offset: %u\n", start_offset);
		goto help;
	}
	v = vsort_init(Pname, label, argv[argc - 1], temp_path, is_num, 
					key_size, key_offset, rec_size, start_offset, 
					vsort_read_record, vsort_get_key, qcomp, 
					NULL, msg_display, &ccode);
	if (!v) goto end;
    if (odo) {
    	setbuf(stdout, NULL);
    	v->msg_progress = msg_progress;
    }
    n = vsort_(v, rec_count, is_verify);
end:
	vsort_end(v, n);
	return(ccode);
help:
	printf("Usage: %s [options] <data_path>\n", Pname);
	printf("Options:\n");
	printf("-t/--temp: path to store temporary data\n");
	printf("-n/--number: numeric key type\n");
	printf("-k/--keysize: key size\n");
	printf("-b/--keyoffset: key offset in record\n");
	printf("-r/--recsize: record size\n");
	printf("-c/--count: record count\n");
	printf("-s/--offset: record start offset\n");
	printf("-l/--label: odometer label\n");
	printf("-o/--odo: supress odometer\n");
	printf("-V/--verify: verify result\n");
	printf("-v/--version: print version\n");
	printf("-h/--help: print usage page\n");
	return(0);
}
#endif
