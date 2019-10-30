/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/fcn.c
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <fenv.h>
#include <errno.h>
#include "hb.h"
#include "hbkwdef.h"
#include "dbe.h"

static char *nullstr = "";

/************************************************************************/

int function_id(void)
{
	int i = kw_bin_srch(0, KWTABSZ - 1);
	if (i < 0) return(-1);
	return(keywords[i].fcn_id);
}

void ttoc(int parm_cnt)
{
	int tn;
	char tt[100];
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	tn = pop_int(0, -1);
	sprintf(tt,"%02d:%02d:%02d",(tn/3600)%24,(tn%3600)/60,tn%60);
	push_string(tt, strlen(tt));
}

void dtoc(int parm_cnt)
{
	HBVAL *v;
	int len;
	int dn;
	char dd[DATELEN + 2 + 1];
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	memset(dd, ' ', DATELEN + 2);
	v = pop_stack(DATE);
	if (dn = get_date(v->buf)) {
		number_to_date(0, dn, dd);
		len = strlen(dd);
	}
	else len = isset(CENTURY_ON)? DATELEN + 2 : DATELEN;
	push_string(dd, len);
}

void ctod(int parm_cnt)
{
	char cbuf[STRBUFSZ];
	char *save_cmd = curr_cmd;
	HBVAL *v;
	struct token tk;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	v = pop_stack(CHARCTR);
	sprintf(cbuf, "[%.*s]", v->valspec.len, v->buf);
	save_token(&tk);
	curr_cmd = cbuf;
	currtok_s = 1; currtok_len = 0;
	push_date(process_date());
	curr_cmd = save_cmd;
	rest_token(&tk);
}

void recno(int parm_cnt)
{
	push_int(get_db_recno());
}

void str(int parm_cnt)
{
	int len, deci;
/*
	if (parm_cnt < 1) syn_err(INVALID_PARMCNT);
*/
	len = deci = -1;
	if (parm_cnt == 3) deci = pop_int(0, MAXDIGITS);
	if (parm_cnt > 1) len = pop_int(0, MAXDIGITS);
	else len = nwidth;
	if (deci == -1) deci = ndeci;
	else if (len > 0 && deci +2 > len) len = deci + 2;
	{
		char buf[MAXSTRLEN];
		num2str(pop_number(), len, deci, buf);
		push_string( buf, len>0? len : strlen(buf));
	}
}

void substr(int parm_cnt)
{
	HBVAL *v;
	char buf[MAXSTRLEN];
	int start, len1;
	int len = -1;	  /* sentinel value */
/*
	if (parm_cnt < 2) syn_err(INVALID_PARMCNT);
*/
	if (parm_cnt == 3) len = pop_int(0, MAXSTRLEN);
	start = pop_int(1, MAXSTRLEN) - 1;
	v = pop_stack(CHARCTR);
	buf[0] = '\0';
	if (start < v->valspec.len) {
		len1 = v->valspec.len - start;
		if ((len == -1) || (len > len1)) len = len1;
		memcpy(buf, &v->buf[start], len);
	}
	else len = 0;
	push_string(buf, len);
}

void val_(int parm_cnt)
{
	HBVAL *v;
	char numbuf[STRBUFSZ];
	number_t num;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	v = pop_stack(CHARCTR);
	sprintf(numbuf, "%.*s", v->valspec.len, v->buf);
	push_number(str2num(numbuf));
}

void eof_(int parm_cnt)
{
	push_logic(iseof());
}

void bof_(int parm_cnt)
{
	push_logic(istof());
}

void upr_lwr(up)
{
	HBVAL *v = topstack;
	typechk(CHARCTR);
	{
		int i;
		for (i=0; i<(int)v->valspec.len; ++i)
			v->buf[i] = up ? to_upper(v->buf[i]) : to_lower(v->buf[i]);
	}
}

void lower(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	upr_lwr(0);
}

void upper(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	upr_lwr(1);
}

void exp_(int parm_cnt)
{
	double x, r;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	errno = 0;
	feclearexcept(FE_ALL_EXCEPT);
	r = exp(pop_real());
	if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                        FE_UNDERFLOW)) syn_err(INVALID_VALUE);
	push_real(r);
}

void log_(int parm_cnt)
{
	double x, r;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	if ((x = pop_real()) <= 0) syn_err(INVALID_VALUE);
	errno = 0;
	feclearexcept(FE_ALL_EXCEPT);
	r = log(x);
	if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                        FE_UNDERFLOW)) syn_err(INVALID_VALUE);
	push_real(r);
}

void log10_(int parm_cnt)
{
	double x, r;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	if ((x = pop_real()) <= 0) syn_err(INVALID_VALUE);
	errno = 0;
	feclearexcept(FE_ALL_EXCEPT);
	r = log10(x);
	if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                        FE_UNDERFLOW)) syn_err(INVALID_VALUE);
	push_real(r);
}

void sqrt_(int parm_cnt)
{
	double r;
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	errno = 0;
	feclearexcept(FE_ALL_EXCEPT);
	r = sqrt(pop_real());
	if (errno || fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW |
                        FE_UNDERFLOW)) syn_err(INVALID_VALUE);

	push_real(r);
}

void mod(int parm_cnt)
{
	double x,y,z;
/*
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
*/
	y = pop_real();
	x = pop_real();
	if (!y) push_real(x);
	else {
		z = x/y;
		x = x - floor(z) * y;
		push_real((z<0 && y<0) ? y - x : x);
	}
}

void int_(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	push_int((long)pop_real());
}
	
void abs_(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	push_real(fabs(pop_real()));
}

void chr(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	{
		char c = pop_int(0,255);
		push_string(&c, 1);
	}
}

void asc(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	{
		HBVAL *v = pop_stack(CHARCTR);
		int c = v->buf[0]; //BYTE
		push_int((long)c);
	}
}

void at(int parm_cnt)
{
	HBVAL *v1,*v2;
	int i,n;
/*
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
*/
	v1 = pop_stack(CHARCTR);
	v2 = pop_stack(CHARCTR);
	if (v2->valspec.len <= v1->valspec.len) {
		n = v1->valspec.len - v2->valspec.len;
		for (i=0; i<=n; ++i) if (!memcmp(v2->buf, v1->buf + i, v2->valspec.len)) {
			push_int((long)i + 1);
			return;
		}
	}
	push_int(0L);
}

void trim(int parm_cnt, int dir)
{
	HBVAL *s;
	char *p;
	char temp[STRBUFSZ];

/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	s = pop_stack(CHARCTR);
	memcpy(temp, s->buf, s->valspec.len);
	if (dir) {
		p = temp + s->valspec.len;
		while (*(p-1) == ' ') --p;
		push_string(temp, p - temp);
	}
	else {
		p = temp;
		while (*p == ' ') ++p;
		push_string(p, temp + s->valspec.len - p);
	}
}

void ltrim(int parm_cnt)
{
	trim(parm_cnt, 0);
}

void rtrim(int parm_cnt)
{
	trim(parm_cnt, 1);
}

void len(int parm_cnt)
{
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	HBVAL *v = pop_stack(CHARCTR);
	int len = v->valspec.len;
	if (len > 0 && v->buf[len - 1] == '\0') --len;
	push_int((long)len);
}

void round_(int parm_cnt)
{
	number_t num;
	int deci = ndeci;
	double r, r2;
	if (parm_cnt > 1) deci = pop_int(-MAXDIGITS, MAXDIGITS);
	num = pop_number();
	if (num_is_real(num)) {
		r2 = pow((double)10, (double)-deci) * (long)(num.r * pow((double)10, (double)deci) + 0.5);
		if ((num.r > 0 && r2 < 0) || (num.r < 0 && r2 > 0)) syn_err(INVALID_VALUE); //overflow
		push_real(r2);
	}
	else {
		if (deci >= num.deci) push_number(num); //nothing to do
		else {
			long n = num.n / power10(num.deci), d = num.n % power10(num.deci);
			if (deci > 0) {
				d = (d + 5 * power10(num.deci - deci - 1)) / power10(num.deci - deci);
				num.deci = deci;
				num.n = n * power10(num.deci) + d;
			}
			else if (deci == 0) {
				d = (d + 5 * power10(num.deci - 1)) / power10(num.deci);
				num.deci = 0;
				num.n = n + d;
			}
			else { //deci < 0
				deci = -deci;
				d = (n % power10(deci) + 5 * power10(deci - 1)) / power10(deci);
				num.deci = 0;
				num.n = (n / power10(deci) + d) * power10(deci);
			}
		}
		push_number(num);
	}
}

void d_to_mdy(int parm_cnt, int *mm, int *dd, int *yy)
{
	HBVAL *v;

#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(DATE);
	n_to_mdy(get_date(v->buf), mm, dd, yy);
}

void day(int parm_cnt)
{
	int mm, dd, yy;

	d_to_mdy(parm_cnt, &mm, &dd, &yy);
	push_int((long)dd);
}

void month(int parm_cnt)
{
	int mm, dd, yy;

	d_to_mdy(parm_cnt, &mm, &dd, &yy);
	push_int((long)mm);
}

void year(int parm_cnt)
{
	int mm, dd, yy;

	d_to_mdy(parm_cnt, &mm, &dd, &yy);
	push_int((long)yy);
}

void left(int parm_cnt)
{
	char str[MAXSTRLEN];
	HBVAL *v;
	int len;
#ifdef PARMCHK
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
#endif
	len = pop_int(0, MAXSTRLEN);
	v = pop_stack(CHARCTR);
	if (len > (int)v->valspec.len) len = v->valspec.len;
	memcpy(str, v->buf, len);
	push_string(str, len);
}
/*
void inrec(int parm_cnt)
int parm_cnt;
{
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	{
		HBVAL *v = pop_stack(CHARCTR);
		int len = get_rec_sz() - v->valspec.len;
		char *p = get_db_rec();
		int i;
		for (i=0; i<len; ++i) if (!memcmp(v->buf,p+i,v->valspec.len)) {
			push_logic(TRUE);
			return;
		}
	}
	push_logic(FALSE);
}
*/

void iif(int parm_cnt)
{
	HBVAL *v1,*v2;
	int tf;
#ifdef PARMCHK
	if (parm_cnt != 3) syn_err(INVALID_PARMCNT);
#endif
	v1 = pop_stack(DONTCARE);
	v2 = pop_stack(DONTCARE);
	tf = pop_logic();
	if (tf < 0) syn_err(INVALID_VALUE);
	push(tf ? v2 : v1);
}

void key(int parm_cnt) //for debug purposes
{
	NDX_ENTRY target;
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	if (!index_active()) db_error2(NOINDEX, get_ctx_id() + 1);
	ndx_get_key(0, &target);
}

void index_fcn(int parm_cnt)
{
	char *expr;
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	if (!index_active()) db_error2(NOINDEX, get_ctx_id() + 1);
	expr = curr_ndx->ndx_h.ndxexp;
	push_string(expr, strlen(expr));
}

void filter_fcn(int parm_cnt)
{
	char *expr = table_ctx->filter;
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	if (!expr) expr = "";
	push_string(expr, strlen(expr));
}

void locate_fcn(int parm_cnt)
{
	char *expr = table_ctx->locate;
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	if (!expr) expr = "";
	push_string(expr, strlen(expr));
}

void alias(int parm_cnt)
{
	char *p;

#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	p = curr_ctx->alias;
	if (!p) p = nullstr;
	push_string(p, strlen(p));
}

void between(int parm_cnt)
{
	number_t x, y, z;
#ifdef PARMCHK
	if (parm_cnt != 3) syn_err(INVALID_PARMCNT);
#endif
	z = pop_number();
	y = pop_number();
	x = pop_number();
	push_logic(num_rel(x, y, TGREATER) && num_rel(x, z, TLESS));
}

extern char daysofmonth[];

static char *dayofweek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
"Thursday", "Friday", "Saturday"};

static char *month_n[12] = {"January", "February", "March", "April", "May",
"June", "July", "August", "September", "October", "November", "December"};

void cdow(int parm_cnt)
{
	HBVAL *v;
	char *p;

#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(DATE);
	p = dayofweek[(get_date(v->buf) + 1) % 7];
	push_string(p, strlen(p));
}

void cmonth(int parm_cnt)
{
	HBVAL *v;
	int mm, dd, yy;

#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(DATE);
	n_to_mdy(get_date(v->buf), &mm, &dd, &yy);
	push_string(month_n[mm - 1], strlen(month_n[mm - 1]));
}

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

void date_(int parm_cnt)
{
	time_t curr_time = time((time_t *)NULL);
	struct tm *now = localtime(&curr_time);
	int year;

#ifdef PARMCHK
	if (parm_cnt>1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt && istrue()) {
		struct stat st;
		int month, day;
		valid_chk();
		if (fstat(table_ctx->dbfctl.fd, &st) < 0) db_error(DBREAD, table_ctx->dbfctl.filename);
		now = localtime(&st.st_mtime);
	}
	year = now->tm_year + 1900;
	push_date(date_to_number(now->tm_mday, now->tm_mon + 1, year));
}

void dbf(int parm_cnt)
{
	char *p;

#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	p = table_ctx->dbfctl.filename;
	if (!p) p = nullstr;
	push_string(p, strlen(p));
}

void deleted(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	push_logic(isdelete());
}

extern int db3w_admin;

#include <pwd.h>

void path_(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt == 1) {
		char user[LEN_USER + 1];
		HBVAL *v = pop_stack(CHARCTR);
		struct passwd *pwd;
		snprintf(user, LEN_USER + 1, "%.*s", v->valspec.len, v->buf);
		if (!db3w_admin) db_error(DB_ACCESS, user);
		pwd = getpwnam(user);
		if (!pwd) db_error(INVALID_USER, user);
		push_string(pwd->pw_dir, strlen(pwd->pw_dir));
	}
	else {
		int len = strlen(rootpath);
		int len2 = strlen(defa_path);
		push_string(defa_path + len, len2 - len);
	}
}

#include <sys/statvfs.h>

void diskspace(int parm_cnt)
{
	struct statvfs stvfs;
#ifdef PARMCHK
	if (parm_cnt) {
		syn_err(INVALID_PARMCNT);
	}
#endif
	if (statvfs(defa_path, &stvfs) < 0) db_error(INTERN_ERR, defa_path);
	push_int((long)stvfs.f_bsize * stvfs.f_bfree / 1048576);
}

void disksize(int parm_cnt)
{
	struct statvfs stvfs;
#ifdef PARMCHK
	if (parm_cnt) {
		syn_err(INVALID_PARMCNT);
	}
#endif
	if (statvfs(defa_path, &stvfs) < 0) db_error(INTERN_ERR, defa_path);
	push_int((long)stvfs.f_bsize * stvfs.f_blocks / 1048576);
}

void dow(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(DATE);
	push_int(get_date(v->buf) % 7 + 1);
}

void fieldcnt(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	push_int((long)get_fcount());
}

void fieldname(int parm_cnt)
{
	int fld_no;
	FIELD *f;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	fld_no = pop_int(1, MAXFLD) - 1;
	if (fld_no >= get_fcount()) syn_err(INVALID_VALUE);
	f = get_field(fld_no);
	push_string(f->name, strlen(f->name));
}

int _fieldno(char *fldname)
{
	int fld_no, fldcnt = get_fcount();
	for (fld_no=0; fld_no<fldcnt; ++fld_no) {
		FIELD *f = get_field(fld_no);
		if (!strcmp(f->name, fldname)) break;
	}
	if (fld_no == fldcnt) return(-1);
	return(fld_no);
}

void fieldno(int parm_cnt)
{
	char name[STRBUFSZ];
	HBVAL *v;
	int fld_no;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	v = pop_stack(CHARCTR);
	memcpy(name, v->buf, v->valspec.len);
	name[v->valspec.len] = '\0';
	chgcase(name, 1);
	fld_no = _fieldno(name);
	if (fld_no < 0) push_int((long)0);
	else push_int((long)fld_no + 1);
}

void fieldsize(int parm_cnt)
{
	int fld_no,fldcnt = get_fcount();
	FIELD *f;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	fld_no = pop_int(1, MAXFLD) - 1;
	if (fld_no >= fldcnt) syn_err(INVALID_VALUE);
	f = get_field(fld_no);
	push_int((long)f->tlength);
}

void file_(int parm_cnt)
{
	HBVAL *v;
	char fname[FULFNLEN];

#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	if ((int)v->valspec.len > FULFNLEN - 1) push_logic(0);
	else {
		if (v->buf[0] == SWITCHAR) {
			if (!db3w_admin) sprintf(fname, "%s%.*s", rootpath, v->valspec.len, v->buf);
			else sprintf(fname, "%.*s", v->valspec.len, v->buf);
		}
		else {
			sprintf(fname, "%s%.*s", defa_path, v->valspec.len, v->buf);
		}
		push_logic(fileexist(fname));
	}
}

void found_(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_logic(isfound());
}

void isalpha_(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	push_logic(v->valspec.len ? isalpha(v->buf[0]) : 0);
}

void isdigit_(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	push_logic(v->valspec.len ? isdigit(v->buf[0]) : 0);
}

void islower_(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	push_logic(v->valspec.len ? islower(v->buf[0]) : 0);
}

void isupper_(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	push_logic(v->valspec.len ? isupper(v->buf[0]) : 0);
}

static void minmax(int ismin, int parm_cnt)
{
	HBVAL *v1, *v2, *v;
	int gte;
#ifdef PARMCHK
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(DONTCARE);
	if (!(v2 = (HBVAL *)malloc(val_size(v)))) sys_err(NOMEMORY); //should use preallocated memory
	memcpy(v2, v, val_size(v));
	v = pop_stack(DONTCARE);
	if (!(v1 = (HBVAL *)malloc(val_size(v)))) sys_err(NOMEMORY);
	memcpy(v1, v, val_size(v));
	if (v1->valspec.typ != v2->valspec.typ) syn_err(TYPE_MISMATCH);
	switch (v1->valspec.typ) {
	case DATE:
	case NUMBER:
		if (!v1->valspec.len || !v2->valspec.len) syn_err(INVALID_VALUE);
		if (v1->valspec.typ == DATE) {
			gte = get_date(v1->buf) >= get_date(v2->buf);
		}
		else {
			number_t x, y;
			if (!v1->valspec.len) syn_err(INVALID_VALUE);
			else {
				x.deci = v1->numspec.deci;
				if (val_is_real(v1)) x.r = get_double(v1->buf);
				else x.n = get_long(v1->buf);
			}
			if (!v2->valspec.len) syn_err(INVALID_VALUE);
			else {
				y.deci = v2->numspec.deci;
				if (val_is_real(v2)) y.r = get_double(v2->buf);
				else y.n = get_long(v2->buf);
			}
			gte = num_rel(x, y, TGTREQU);
		}
		break;
	case CHARCTR: 
		if (!v1->valspec.len) gte = 0;
		else if (!v2->valspec.len) gte = 1;
		else {
		int len = imin(v1->valspec.len, v2->valspec.len);
			BYTE *p = v1->buf;
			BYTE *q = v2->buf;
			int i = 0;
			while (len--) {
				int c1 = *p++;
				int c2 = *q++;
				//if (!isset(EXACT_ON)) { c1 = to_upper(c1); c2 = to_upper(c2); }
				if (i = c1 - c2) break;
			}
			if (!i && isset(EXACT_ON)) i = v2->valspec.len - v1->valspec.len;
			gte = i >= 0;
		}
		break;
	case LOGIC:
		if (!v1->valspec.len || !v2->valspec.len) syn_err(INVALID_VALUE);
		gte = !!v1->buf[0];
		break;
	default:
		syn_err(INVALID_TYPE);
	}
	v = gte ? (ismin ? v2 : v1) : (ismin ? v1 : v2);
	push(v);
	free(v1);
	free(v2);
}

void mmax(int parm_cnt)
{
	minmax(0, parm_cnt);
}

void mmin(int parm_cnt)
{
	minmax(1, parm_cnt);
}

void ndx(int parm_cnt)
{
	char *p;
	int nth = 0;

#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt == 1) nth = pop_int(1, MAXNDX) - 1;
	else nth = table_ctx->order;
	if (nth < 0) p = nullstr;
	else {
		p = table_ctx->ndxfctl[nth].filename;
		if (!p) p = nullstr;
	}
	push_string(p, strlen(p));
}

void reccount(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	lock_hdr();
	push_int(get_db_rccnt());
	unlock_hdr();
}

void recsize(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	valid_chk();
	push_int((long)get_rec_sz());
}

void replicate(int parm_cnt)
{
	int n,i;
	HBVAL *v;
	char *p,str[MAXSTRLEN];
#ifdef PARMCHK
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
#endif
	n = pop_int(0, MAXSTRLEN);
	v = pop_stack(CHARCTR);
	//if (n*(int)v->valspec.len > MAXSTRLEN) syn_err(STR_TOOLONG);
	for (i=0,p=str; i<n; ++i) {
		int len = v->valspec.len;
		if (p + len - str > MAXSTRLEN) len = MAXSTRLEN - (p - str);
		memcpy(p, v->buf, len);
		p += len;
	}
	push_string(str, p - str);
}

void right(int parm_cnt)
{
	char str[MAXSTRLEN];
	HBVAL *v;
	int len;
#ifdef PARMCHK
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
#endif
	len = pop_int(0, MAXSTRLEN);
	v = pop_stack(CHARCTR);
	if (len > (int)v->valspec.len) len = v->valspec.len;
	memcpy(str, v->buf + v->valspec.len - len, len);
	push_string(str, len);
}

void space(int parm_cnt)
{
	int len;
	char str[MAXSTRLEN];
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	len = pop_int(0, MAXSTRLEN);
	memset(str, ' ', len);
	push_string(str, len);
}

void stuff(int parm_cnt)
{
	HBVAL *v;
	char str1[MAXSTRLEN], str2[MAXSTRLEN];
	int start, len, len1;
#ifdef PARMCHK
	if (parm_cnt != 4) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	len1 = v->valspec.len;
	memcpy(str1, v->buf, len1);
	len = pop_int(0, MAXSTRLEN);
	start = pop_int(1, MAXSTRLEN) - 1;
	v = pop_stack(CHARCTR);
	if (start > (int)v->valspec.len) start = v->valspec.len;
	len = imin((int)v->valspec.len - start, len);
	if ((int)v->valspec.len - len + len1 > MAXSTRLEN) syn_err(STR_TOOLONG);
	memcpy(str2, v->buf, start);
	memcpy(str2 + start, str1, len1);
	memcpy(str2 + start + len1, v->buf + start + len, v->valspec.len - start - len);
	push_string(str2, start + len1 + (v->valspec.len - start - len));
}

void time_(int parm_cnt)
{
	int is_fulltime = 1;
	time_t curr_time = time((time_t *)NULL);
#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	/*
	char tbuf[9];
	sprintf(tbuf,"%02d:%02d:%02d",now->tm_hour,now->tm_min,now->tm_sec);
	push_string(tbuf,8);
	*/
	if (parm_cnt) is_fulltime = pop_logic();
	if (is_fulltime) push_int((long)curr_time);
	else {
		struct tm *now = localtime(&curr_time);
		push_int((long)now->tm_hour*3600 + now->tm_min*60 + now->tm_sec);
	}
}

void timestr(int parm_cnt)
{
	number_t num;
	time_t t;
	char *tstr;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	num = pop_number();
	if (!num_is_integer(num) || num.n < 0) syn_err(INVALID_VALUE);
	t = (time_t)num.n;
	tstr = timestr_(&t);
	push_string(tstr, strlen(tstr));
}

void type_(int parm_cnt)
{
	HBVAL *v;
	char typ, exp_buf[STRBUFSZ];
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	memcpy(exp_buf, v->buf, v->valspec.len);
	exp_buf[v->valspec.len] = '\0';
	if (evaluate(exp_buf, 1)) typ = 'U';
	else {
		v = pop_stack(DONTCARE);
		switch (v->valspec.typ) {
		case CHARCTR: typ = 'C';
			break;
		case NUMBER: typ = 'N';
			break;
		case DATE: typ = 'D';
			break;
		case LOGIC: typ = 'L';
			break;
		default:
		case UNDEF: typ = 'U';
			break;
		}
	}
	push_string(&typ, 1);
}

void defined(int parm_cnt)
{
	HBVAL *v;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	type_(parm_cnt);
	v = pop_stack(CHARCTR);
	push_logic(v->buf[0] != 'U');
}

void inlist(int parm_cnt)
{
	HBVAL *v;
	HBVAL *tv;
	int isin;
#ifdef PARMCHK
	if (parm_cnt < 2) syn_err(INVALID_PARMCNT);
#endif
	v = (HBVAL *)stack_peek(parm_cnt - 1);
	tv = malloc(val_size(v));
	if (!tv) sys_err(NOMEMORY);
	memcpy(tv, v, val_size(v));
	while (1) {
		int len;
		v = pop_stack(DONTCARE);
		if (!--parm_cnt) break;
		if (v->valspec.typ != tv->valspec.typ) continue;
		if (v->valspec.typ == CHARCTR && v->valspec.len != tv->valspec.len) continue;
		if (v->valspec.typ == NUMBER && (v->numspec.len != tv->numspec.len || v->numspec.deci != tv->numspec.deci)) continue;
		len = v->valspec.typ == NUMBER? v->numspec.len : v->valspec.len;
		if (!memcmp(v->buf, tv->buf, len)) break;
	}
	isin = parm_cnt > 0;
	if (isin) while (parm_cnt--) pop_stack(DONTCARE);
	push_logic(isin);
	free(tv);
}

void occurs(int parm_cnt)
{
	HBVAL *v;
	char *str1 = NULL, *str2 = NULL;
	int len1, len2;
	int times = 0;
#ifdef PARMCHK
	if (parm_cnt != 2) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	if (!(str2 = malloc(len2 = v->valspec.len))) goto end;
	memcpy(str2, v->buf, len2);
	v = pop_stack(CHARCTR);
	if (!(str1 = malloc(len1 = v->valspec.len))) goto end;
	memcpy(str1, v->buf, len1);
	if (len1 <= len2) {
		int i,n;
		n = len2 - len1;
		for (i=0; i<n; ++i) if (!memcmp(str1, str2+i, len1)) ++times;
	}
	push_int((long)times);
end:
	if (str1) free(str1);
	if (str2) free(str2);
	if (!str1 || !str2) sys_err(NOMEMORY);
}

#define PAD_RIGHT		1
#define PAD_LEFT		2
#define PAD_CENTER	3

void pad(int mode, int parm_cnt)
{
	HBVAL *v;
	char *str1 = NULL, *str2 = NULL, *str = NULL;
	int len1, len2, len;
#ifdef PARMCHK
	if (parm_cnt != 3) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	if (!(str2 = malloc(len2 = v->valspec.len))) goto end;
	memcpy(str2, v->buf, len2);
	len = pop_int(1, MAXSTRLEN);
	if (!(str = malloc(len))) goto end;
	v = pop_stack(CHARCTR);
	if (!(str1 = malloc(len1 = v->valspec.len))) goto end;
	memcpy(str1, v->buf, len1);
	if (len <= len1) memcpy(str, str1, len);
	else {
		int i,n;
		char *p;
		n = len / len2;
		for (i=0,p=str; i<n; ++i,p+=len2) memcpy(p, str2, len2);
		memcpy(p, str2, len%len2);
		switch (mode) {
			case PAD_RIGHT: memcpy(str, str1, len1);
				break;
			case PAD_LEFT: memcpy(str + len - len1, str1, len1);
				break;
			case PAD_CENTER: memcpy(str + (len - len1) / 2, str1, len1);
				break;
		}
	}
	push_string(str, len);
end:
	if (str1) free(str1);
	if (str) free(str);
	if (str2) free(str2);
	if (!str1 || !str2 || !str) sys_err(NOMEMORY);
}

void padc(int parm_cnt)
{
	pad(PAD_CENTER, parm_cnt);
}

void padl(int parm_cnt)
{
	pad(PAD_LEFT, parm_cnt);
}

void padr(int parm_cnt)
{
	pad(PAD_RIGHT, parm_cnt);
}

#include <math.h>

void pi(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_real((double)M_PI);
}

void proper(int parm_cnt)
{
	HBVAL *v;
	int i, c;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	for (i=0,c=' '; i<(int)v->valspec.len;) {
		if (c == ' ') {
			if (v->buf[i] == ' ') {
				memcpy(&v->buf[i], &v->buf[i+1], v->valspec.len - i - 1);
				v->valspec.len--;
				continue;
			}
			c = v->buf[i] = to_upper(v->buf[i]);
		}
		else c = v->buf[i] = to_lower(v->buf[i]);
		++i;
	}
	push(v);
}

void rand_(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_int((long)rand());
}

#define MAXSALTLEN	16
static char *saltpool = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
extern char *crypt();

void crypt_(int parm_cnt)
{
	HBVAL *v;
	char buf[STRBUFSZ], salt[STRBUFSZ], *tp;
	int len = strlen(saltpool);
	if (parm_cnt == 2) { 
		v = pop_stack(CHARCTR);
		//allow salt to be the full epasswd
		if (v->valspec.len > MAXSTRLEN) syn_err(INVALID_VALUE);
		sprintf(salt, "%.*s", v->valspec.len, v->buf);
		if (salt[0] == '$') {
			tp = strchr(salt + 1, '$');
			if (!tp || tp == salt + 1) syn_err(INVALID_VALUE);
			tp++;
			if (*tp == '\0') {
				int i;
				for (i=0; i<MAXSALTLEN; ++i) *tp++ = saltpool[rand() % len];
				*tp++ = '$'; *tp = '\0';
			}
			else {
				char *tp2 = strchr(tp, '$');
				if (!tp2 || tp2 - tp < MAXSALTLEN) syn_err(INVALID_VALUE);
			}
		}
		else if (strlen(salt) < 2) syn_err(INVALID_VALUE);
	}
	else {
		salt[0] = saltpool[rand() % len];
		salt[1] = saltpool[rand() % len];
		salt[2] = '\0';
	}
	v = pop_stack(CHARCTR);
	sprintf(buf, "%.*s", v->valspec.len, v->buf);
	tp = crypt(buf, salt);
	if (!tp) syn_err(INVALID_VALUE);
	push_string(tp, strlen(tp));
}

static void soundex(int parm_cnt)
{
	HBVAL *v;
	int i,j,c;
	char code[4];
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	v = pop_stack(CHARCTR);
	memset(code, '0', 4);
	for (i=j=0; i<(int)v->valspec.len && j<4; ++i) {
		c = to_upper(v->buf[i]);
		if (!i) code[j++] = c;
		else {
			switch (c) {
				case 'B':
				case 'F':
				case 'P':
				case 'V': if (code[j-1] != '1') code[j++] = '1'; break;
				case 'C':
				case 'G':
				case 'J':
				case 'K':
				case 'Q':
				case 'S':
				case 'X':
				case 'Z': if (code[j-1] != '2') code[j++] = '2'; break;
				case 'D':
				case 'T': if (code[j-1] != '3') code[j++] = '3'; break;
				case 'L': if (code[j-1] != '4') code[j++] = '4'; break;
				case 'M':
				case 'N': if (code[j-1] != '5') code[j++] = '5'; break;
				case 'R': if (code[j-1] != '6') code[j++] = '6'; break;
				default: break;
			}
		}
	}
	push_string(code, 4);
}

static char *nums1[19] = {"one", "two", "three", "four", "five", "six", "seven",
"eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
"fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};

static char *nums2[8] = {"twenty", "thirty", "forty", "fifty", "sixty", "seventy",
"eighty", "ninety"
};

static void numspell(long n, char **p)
{
	char *q;
	int len;

	if (!n) return;
	if (n < 20) {
		q = nums1[n-1]; len = strlen(q);
		memcpy(*p, q, len);
		*p += len + 1;		 /** add a space at the end **/
	}
	else if (n < 100) {
		q = nums2[n / 10 - 2]; len = strlen(q);
		memcpy(*p, q, len);
		*p += len + 1;
		numspell(n % 10, p);
	}
	else if (n < 1000) {
		numspell(n / 100, p);
		memcpy(*p, "hundred", 7);
		*p += 8;
		numspell(n % 100, p);
	}
	else if (n < 1000000) {
		numspell(n / 1000, p);
		memcpy(*p, "thousand", 8);
		*p += 9;
		numspell(n % 1000, p);
	}
	else if (n < 1000000000) {
		numspell(n / 1000000, p);
		memcpy(*p, "million", 7);
		*p += 8;
		numspell(n % 1000000, p);
	}
	else {
		numspell(n/1000000000, p);
		memcpy(*p, "billion", 7);
		*p += 8;
		numspell(n % 1000000000, p);
	}
}

void spellnum(int parm_cnt)
{
	HBVAL *v;
	number_t num;
	long n;
	int cents;
	char *p, numbuf[STRBUFSZ];
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	memset(numbuf, ' ', STRBUFSZ);
	num = pop_number();
	p = numbuf;
	if (num.n < 0) {
		memcpy(p, "minus", 5);
		p += 6;
		num.n = -num.n;
	}
	if (num_is_integer(num)) {
		n = num.n;
		cents = 0;
	}
	else {
		if (num_is_real(num)) {
			n = (long)(num.r * 1000 + 5);
			cents = (n - n / 1000 * 1000) / 10;
			n /= 1000;
		}
		else {
			n = num.n / power10(num.deci);
			cents = num.n - n * power10(num.deci);
			if (num.deci == 1) cents *= 10;
			else if (num.deci > 2) {
				cents = (cents + 5 * power10(num.deci - 2 - 1)) / power10(num.deci - 2);
				if (cents >= 100) {
					n++;
					cents = 0;
				}
			}
		}
	}
	numspell(n, &p);
	sprintf(p, "and %02d/100", cents); p += 10;
	push_string(numbuf, p - numbuf);
}

static BYTE cpool[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
#define CPOOLLEN 62

void randstr(int parm_cnt)
{
	char strbuf[MAXSTRLEN];
	int len, i;
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	len = pop_int(1, MAXSTRLEN);
	for (i=0; i<len; ++i) {
		strbuf[i] = cpool[rand() % CPOOLLEN];
	}
	push_string(strbuf, len);
}

void epoch_(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_int((long)epoch);
}

void ff_lock(int parm_cnt)
{
	int exclusive = 0;
#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt) exclusive = pop_logic();
	push_logic(!f_lock(exclusive));
}

void rr_lock(int parm_cnt)
{
	int exclusive = 0;
#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt) exclusive = pop_logic();
	push_logic(!r_lock(table_ctx->crntrec, exclusive? HB_WRLCK : HB_RDLCK, 1));
}

#define SHELL_SERVER

#ifdef SHELL_SERVER //run cmd on server !!!CAUTION:SECURITY see set_session_user()

void system_(int parm_cnt)
{
	FILE *pcmd, *outfp = NULL;
	char cmd[STRBUFSZ], line[STRBUFSZ];
	HBVAL *v;
	int status;
#ifdef PARMCHK
	if (parm_cnt > 2 || parm_cnt < 1)  {
		syn_err(INVALID_PARMCNT);
	}
#endif
	if (parm_cnt == 2) {
		v = pop_stack(CHARCTR);
		if (v->buf[0] == SWITCHAR) sprintf(line, "%s/%.*s", rootpath, v->valspec.len, v->buf);
		else sprintf(line, "%s%.*s", defa_path, v->valspec.len, v->buf);
		outfp = fopen(line, "a");
	}
	v = pop_stack(CHARCTR);
	if (v->buf[0] == SWITCHAR) {
		if (db3w_admin) sprintf(cmd, "%.*s 2>&1", v->valspec.len, v->buf); //don't append rootpath);
		else sprintf(cmd, "%s%.*s 2>&1", rootpath, v->valspec.len, v->buf);
	}
	else sprintf(cmd, "%s%.*s 2>&1", defa_path, v->valspec.len, v->buf);
	if (db3w_admin) seteuid(0);
	pcmd = popen(cmd, "r");
	if (!pcmd) sys_err(BAD_COMMUNICATION);
	while (fgets(line, STRBUFSZ, pcmd)) {
		if (outfp) fputs(line, outfp);
	}
	status = pclose(pcmd);
	if (outfp) fclose(outfp);
	if (db3w_admin) seteuid(hb_euid);
	push_logic(WEXITSTATUS(status) == 0);
}

#else
//client side cmd
#include "hbusr.h"

void system_(int parm_cnt)
{
	usr_regs.ex = parm_cnt;
	push_string("system", 6);
	_usr(USR_FCN);
}

#endif //SHELL_SERVER

void isnull(void)
{
	HBVAL *v = pop_stack(DONTCARE);
	push_logic(!v->valspec.len);
}

#include "b64/cencode.h"	//from libb64 base64 library

void hash(int parm_cnt)
{
	HBVAL *v;
	DWORD res[6];
	int len;
	char hval[256];
	base64_encodestate estat;

#ifdef PARMCHK
	if (parm_cnt != 1)  {
		syn_err(INVALID_PARMCNT);
	}
#endif
	v = pop_stack(CHARCTR);
	tiger(v->buf, v->valspec.len, res);
	v = (HBVAL *)hval;
	v->valspec.typ = CHARCTR;
	base64_init_encodestate(&estat);
	len = base64_encode_block((char *)res, sizeof(DWORD)*6, v->buf, &estat);
	len += base64_encode_blockend(v->buf + len, &estat);
	v->valspec.len = len;
	push(v);
}

void fault(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt)  {
		syn_err(INVALID_PARMCNT);
	}
#endif
	push_logic(is_session_fault());
}

void auth(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt)  {
		syn_err(INVALID_PARMCNT);
	}
#endif
	push_logic(is_session_authenticated());
}

#define MAXTMPFN	64

void mktmp(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	char tmpfn[FULFNLEN];
	int fd, len = strlen(rootpath);
	HBVAL *v = pop_stack(CHARCTR);
	if (v->valspec.len >= MAXTMPFN) syn_err(STR_TOOLONG);
	sprintf(tmpfn, "%s/tmp/%.*s", rootpath, v->valspec.len, v->buf);
	fd = mkstemp(tmpfn);
	if (fd < 0) syn_err(INVALID_VALUE);
	close(fd);
	push_string(tmpfn + len, strlen(tmpfn) - len);
}

void peer_(int parm_cnt)
{
	char *addr;
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	addr = peer_ip_address(get_session_id());
	if (!addr) sys_err(LOGIN_NOUSER);
	push_string(addr, strlen(addr));
}

void whoami(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_string(hb_user, strlen(hb_user));
}

void parent(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	char tmpfn[FULFNLEN], *tp;
	HBVAL *v = pop_stack(CHARCTR);
	if (v->buf[0] == SWITCHAR) {
		if (!db3w_admin) sprintf(tmpfn, "%s%.*s", rootpath, v->valspec.len, v->buf);
		else sprintf(tmpfn, "%.*s", v->valspec.len, v->buf);
	}
	else {
		sprintf(tmpfn, "%s%.*s", defa_path, v->valspec.len, v->buf);
	}
	tp = tmpfn + v->valspec.len;
	if (tp - 1 > tmpfn && *(tp - 1) == '/') *(--tp) = '\0';
	tp = strrchr(tmpfn, '/');
	if (!tp) push_string(defa_path, strlen(defa_path)); //should not happen
	else {
		*(tp + 1) = '\0';
		push_string(tmpfn, strlen(tmpfn));
	}
}

void atroot(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	int len = strlen(rootpath);
	push_logic(!strncmp(rootpath, defa_path, len) && strlen(defa_path) == len + 1 && defa_path[len] == SWITCHAR);
}

#include <dirent.h>
void isempty(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt != 1) syn_err(INVALID_PARMCNT);
#endif
	char dirpath[FULFNLEN];
	HBVAL *v = pop_stack(CHARCTR);
	int n = 0;
	struct dirent *d;
	DIR *dir;
	snprintf(dirpath, FULFNLEN, "%s%.*s", v->buf[0] == SWITCHAR? rootpath : defa_path, v->valspec.len, v->buf);
	dir = opendir(dirpath);
	if (dir) while ((d = readdir(dir)) != NULL) if (++n > 2) break;
	closedir(dir);
	push_logic(n <= 2);
}

void write_ok(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt) syn_err(INVALID_PARMCNT);
#endif
	push_logic(euidaccess(defa_path, W_OK) == 0);
}

void ctx_id(int parm_cnt)
{
#ifdef PARMCHK
	if (parm_cnt > 1) syn_err(INVALID_PARMCNT);
#endif
	if (parm_cnt) {
		HBVAL *v = pop_stack(CHARCTR);
		push_int(alias2ctx(v) + 1);
	}
	else push_int(get_ctx_id());
}

extern void serial();

void (*fcn[MAXFCN])() = {
abs_,       alias,    asc,       at,        between,    bof_,      cdow,        chr,       
cmonth,     crypt_,   ctod,      date_,     day,        dbf,       deleted,     dow,       
dtoc,       eof_,     exp_,      fieldcnt,  fieldname,  fieldno,   fieldsize,   file_,     
filter_fcn, ff_lock,  found_,    iif,       index_fcn,  inlist,     int_,       isalpha_,  
isdigit_,   islower_, isupper_,  key,       left,       len,        locate_fcn, log_,     
log10_,     lower,    ltrim,     mmax,      mmin,       mod,        month,      ndx,        
occurs,     padc,     padl,      padr,      path_,      pi,         proper,     rand_,      
reccount,   recno,    recsize,   replicate, right,      rr_lock,    round_,     rtrim,     
serial,     soundex,  space,     spellnum,  sqrt_,      str,        stuff,      substr,    
system_,    time_,    ttoc,      type_,     upper,      val_,       year,       randstr,       
epoch_,     disksize, diskspace, isnull,    hash,       fault,      auth,       timestr,
defined,    mktmp,    peer_,     whoami,    parent,     atroot,     isempty,    write_ok,
ctx_id
};

void parm_chk(int final, int fcn, int cnt)
{
	switch (fcn) {
		case BETWEEN_FCN:
		case IIF_FCN:
		case PADC_FCN:
		case PADL_FCN:
		case PADR_FCN:
			if (final && cnt != 3) goto cnt_err;
			break;
		case STR_FCN:
			if ((cnt > 3) || (final && cnt < 1)) goto cnt_err;
			break;
		case SUBSTR_FCN:
			if ((cnt > 3) || (final && cnt < 2)) goto cnt_err;
			break;
		case CRYPT_FCN:
		case ROUND_FCN:
		case SYSTEM_FCN:
			if ((cnt > 2) || (final && cnt < 1)) goto cnt_err;
			break;
		case AT_FCN:
		case LEFT_FCN:
		case MAX_FCN:
		case MIN_FCN:
		case MOD_FCN:
		case OCCURS_FCN:
		case REPLICATE_FCN:
			if ((cnt > 2) || (final && cnt != 2)) goto cnt_err;
			break;
		case CID_FCN:
		case DATE_FCN:
		case TIME_FCN:
		case PATH_FCN:
		case FLOCK_FCN:
		case RLOCK_FCN:
			if (cnt > 1) goto cnt_err;
			break;
		case ABS_FCN:
		case ASC_FCN:
		case CDOW_FCN:
		case CHR_FCN:
		case CMONTH_FCN:
		case DAY_FCN:
		case DOW_FCN:
		case DTOC_FCN:
		case EXP_FCN:
		case FIELDNAME_FCN:
		case FIELDNO_FCN:
		case FIELDSIZE_FCN:
		case FILE_FCN:
		case HASH_FCN:
		case INT_FCN:
		case ISALPHA_FCN:
		case ISDIGIT_FCN:
		case ISEMPTY_FCN:
		case ISLOWER_FCN:
		case ISUPPER_FCN:
		case LOWER_FCN:
		case LOG_FCN:
		case LOG10_FCN:
		case LTRIM_FCN:
		case MKTMP_FCN:
		case MONTH_FCN:
		case NDX_FCN:
		case PARENT_FCN:
		case PROPER_FCN:
		case RANDSTR_FCN:
		case RTRIM_FCN:
		case SERIAL_FCN:
		case SPACE_FCN:
		case SPELLNUM_FCN:
		case SQRT_FCN:
		case TTOC_FCN:
		case TYPE_FCN:
		case DEFINED_FCN:
		case UPPER_FCN:
		case VAL_FCN:
		case YEAR_FCN:
			if (cnt != 1) goto cnt_err;
			break;
		case ALIAS_FCN:
		case BOF_FCN:
		case DBF_FCN:
		case DELETED_FCN:
		case DISKSPACE_FCN:
		case DISKSIZE_FCN:
		case EOF_FCN:
		case EPOCH_FCN:
		case FIELDCNT_FCN:
		case FOUND_FCN:
		case RECNO_FCN:
 		case FILTER_FCN:
		case INDEX_FCN:
		case LOCATE_FCN:
		case PEER_FCN:
		case PI_FCN:
		case RAND_FCN:
		case WHOAMI_FCN:
		case ATROOT_FCN:
		case WOK_FCN:
		case AUTH_FCN:
		  if (cnt) goto cnt_err;
	}
	if (final || !exe) return;
	switch (fcn) {
		case IIF_FCN:
		case DATE_FCN:
			if (cnt==1) typechk(LOGIC);
			break;
		case REPLICATE_FCN:
		case RIGHT_FCN:
		case ROUND_FCN:
		case LEFT_FCN:
		case RANDSTR_FCN:
		case STR_FCN:
			if (cnt > 1) push_int((long)pop_int(fcn == ROUND_FCN? -MAXSTRLEN: 0, MAXSTRLEN));
			else typechk((fcn==ROUND_FCN || fcn==STR_FCN || fcn==RANDSTR_FCN)? NUMBER : CHARCTR);
			break;
		case PADC_FCN:
		case PADL_FCN:
		case PADR_FCN:
		case SUBSTR_FCN:
		case STUFF_FCN:
			switch (cnt) {
			case 1:
				typechk(CHARCTR);
				break;
			case 2:
				push_int((long)pop_int(1, MAXSTRLEN));
				break;
			case 3:
				if (fcn==PADC_FCN || fcn==PADL_FCN || fcn==PADR_FCN) typechk(CHARCTR);
				else push_int((long)pop_int(0, MAXSTRLEN));
				break;
			}
			if (fcn==STUFF_FCN && cnt==4) typechk(CHARCTR);
			break;
		case FIELDNAME_FCN:
		case FIELDSIZE_FCN:
			push_int((long)pop_int(1, MAXFLD));
			break;
		case NDX_FCN:
			push_int((long)pop_int(1, MAXNDX));
			break;
		case SPACE_FCN:
			push_int((long)pop_int(0, MAXSTRLEN));
			break;
		case ASC_FCN:
		case AT_FCN:
		case FILE_FCN:
		case ISALPHA_FCN:
		case ISDIGIT_FCN:
		case ISEMPTY_FCN:
		case ISLOWER_FCN:
		case ISUPPER_FCN:
		case LTRIM_FCN:
		case MKTMP_FCN:
		case OCCURS_FCN:
		case PROPER_FCN:
		case RTRIM_FCN:
		case SERIAL_FCN:
		case SOUNDEX_FCN:
		case SYSTEM_FCN:
		case TYPE_FCN:
		case DEFINED_FCN:
		case VAL_FCN:
			typechk(CHARCTR);
			break;
		case CDOW_FCN:
		case CMONTH_FCN:
		case DAY_FCN:
		case DOW_FCN:
		case DTOC_FCN:
		case MONTH_FCN:
		case YEAR_FCN:
			typechk(DATE);
			break;
		case ABS_FCN:
		case BETWEEN_FCN:
		case CHR_FCN:
		case EXP_FCN:
		case INT_FCN:
		case LOG_FCN:
		case LOG10_FCN:
		case MOD_FCN:
		case RECCOUNT_FCN:
		case RECSIZE_FCN:
		case SPELLNUM_FCN:
		case SQRT_FCN:
		case TTOC_FCN:
			typechk(NUMBER);
			break;
	}
	return;
cnt_err: syn_err(INVALID_PARMCNT);
}
