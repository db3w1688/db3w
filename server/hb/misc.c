/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/misc.c
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
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "dbe.h"

int istrue(void)
{
	HBVAL *v = pop_stack(LOGIC);
	return(v->valspec.len? v->buf[0] : 0); //NULL logic type defaults to FALSE
}

int isset(int parm)
{
	if (is_onoff(parm)) return(onoff_flags[parm]);
	if (parm == FILTER_TO) return(!!curr_ctx->filter);
	if (parm == PROC_TO) return(!!profctl.filename);
	if (parm == FORM_TO) return(!!frmfctl.filename);
	return(0);
}

void num2str(number_t num, int len, int deci, char *buf)
{
	char buf2[STRBUFSZ]; //2048 bytes so no overflow
	long n, d;
	int len1;
	*buf = '\0';
	if (deci) {
		if (len <= 0) do {
			if (num_is_real(num)) sprintf(buf2,"%.*lf", deci, num.r);
			else if (!num.deci) sprintf(buf2, "%ld.%0*d", num.n, deci, 0);
			else {
				n = num.n; if (num.n < 0) n *= -1;
				if (num.deci > deci) d = (n % power10(num.deci) + 5 * power10(num.deci - deci - 1)) / power10(num.deci - deci);
				else d = (n % power10(num.deci)) * power10(deci - num.deci);
				sprintf(buf2, "%ld.%0*ld", num.n / power10(num.deci), deci, d);
			}
			len1 = strlen(buf2);
			if (len1 > MAXDIGITS) { //num.deci must not be 0
				deci -= len1 - MAXDIGITS;
				if (deci == 0 || deci == -1) goto nodeci; //count the decimal point
				if (deci < 0) { //overflow
					memset(buf, '*', MAXDIGITS);
					buf[MAXDIGITS] = '\0';
				}
			}
			else strcpy(buf, buf2);
		} while (!buf[0]);
		else do {
			if (len > MAXDIGITS) len = MAXDIGITS;
			if (num_is_real(num)) sprintf(buf2, "%*.*lf", len, deci, num.r);
			else if (!num.deci) sprintf(buf2, "%*ld.%0*d", len - deci - 1, num.n, deci, 0);
			else {
				n = num.n; if (num.n < 0) n = -n; //n needs to be positive
				if (num.deci > deci) d = (n % power10(num.deci) + 5 * power10(num.deci - deci - 1)) / power10(num.deci - deci);
				else d = (n % power10(num.deci)) * power10(deci - num.deci);
				if (n / power10(num.deci) < 1)
					sprintf(buf2, "%s0.%0*ld", d > 0 && num.n < 0? "-" : "", deci, d);
				else
					sprintf(buf2, "%*ld.%0*ld", len - deci - 1, num.n / power10(num.deci), deci, d);
			}
			len1 = strlen(buf2);
			if (len1 > len) {
				deci -= len1 - len;
				if (deci == 0|| deci == -1) goto nodeci; //count decimal point
				if (deci < 0) { //overflow
					memset(buf, '*', len);
					buf[len] = '\0';
				}
			}
			else strcpy(buf, buf2);
		} while (!buf[0]);
	}
	else {
nodeci:
		if (len <= 0) {
			if (num_is_real(num)) sprintf(buf2, "%.0lf", num.r);
			else if (!num.deci) sprintf(buf2, "%ld", num.n);
			else {
				d = (num.n % power10(num.deci) + 5 * power10(num.deci - 1)) / power10(num.deci);
				sprintf(buf2, "%ld", num.n / power10(num.deci) + d);
			}
			if (strlen(buf2) > MAXDIGITS) { //overflow
				memset(buf, '*', MAXDIGITS);
				buf[MAXDIGITS] = '\0';
			}
			else strcpy(buf, buf2);
		}
		else {
			if (len > MAXDIGITS) len = MAXDIGITS;
			if (num_is_real(num)) sprintf(buf2, "%*.0lf", len, num.r);
			else if (!num.deci) sprintf(buf2, "%*ld", len, num.n);
			else {
				d = (num.n % power10(num.deci) + 5 * power10(num.deci - 1)) / power10(num.deci);
				sprintf(buf2, "%*ld", len, num.n / power10(num.deci) + d);
			}
			len1 = strlen(buf2);
			if (len1 > len) {
				memset(buf, '*', len);
				buf[len] = '\0';
			}
			else strcpy(buf, buf2);
		}
	}
}

number_t str2num(char *numbuf) //numbuf must be null terminated
{
	number_t num;
	int is_real = 0, deci = 0;
	char *tp = strchr(numbuf, '.');
	if (!isset(DFPI_ON)) is_real = !!tp; //Decimal Floating Point is OFF and there is a decimal point
	else if (strchr(numbuf, 'e') || strchr(numbuf, 'E')) is_real = 1; //should also have decimal
	else if (tp) {
		deci = strlen(numbuf) - (int)(tp - numbuf) - 1; //don't count the .
		if (deci > DECI_MAX) is_real = 1;
		else memmove(tp, tp + 1, deci + 1); //include null byte
	}
	if (is_real) {
		double d;
		num.deci = REAL;
		errno = 0;
		d = strtod(numbuf, NULL);
		if ((errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL)) || (errno != 0 && d == 0)) {
			syn_err(INVALID_VALUE);
		}
		num.r = d;
	}
	else {
		long n;
		num.deci = deci;
		errno = 0;
		n = strtoul(numbuf, NULL, 10);
		if ((errno == ERANGE && n == ULONG_MAX)|| (errno != 0 && n == 0)) {
			syn_err(INVALID_VALUE); //overflow
		}
		num.n = n; //allow n to be negative so we can cover unsigned long
	}
	return num;
}

int namecmp(char *src, char *tgt, int len, int type)
{
	int i, d;
	if ((type == KEYWORD) && (len < 4)) type = ID_VNAME;
	for (i=0; i<len; ++i, ++src, ++tgt) {
		int t = *tgt;
		int s = *src;
		if (s == '*' || s == '?') return(-1); //src not allow to contain wildcard char
		if (type != ID_FNAME) { //not filename or alias
			s = to_upper(s);
			t = to_upper(t);
		}
		d = s - t;
		if (d) {
			if (type == ID_PARTIAL) {
				if (t == '?') continue;
				if (t == '*') {
					while ((t = *(++tgt)) == '*');
					if (!t) return(0);
					if (t == '?') continue;
					t = to_upper(t);
					while (s && s != t) s = to_upper(*(++src));
					if (!s) break;
					continue;
				}
			}
			return(d);
		}
	}
	return(type == ID_VNAME  || type == ID_FNAME? *src : 0);
}

int kw_bin_srch(int start, int end)
{
	if (!currtok_len || start>end) return(-1);
	else {
		int mid = (start+end)/2;
		char *tp = &curr_cmd[currtok_s];
		int i = namecmp(keywords[mid].name, tp, currtok_len, KEYWORD);
/*
printf("%s %x %.*s %d %d\n",keywords[mid].name,keywords[mid].kw_val,currtok_len,tp,curr_tok,i);
*/
		if (!i) {
			for (i=mid; i; --i)	/* get shortest match */
				if (namecmp(keywords[i-1].name, tp, currtok_len, KEYWORD)) break;
			return(i);
		}
		return(i>0 ? kw_bin_srch(start,mid-1) : kw_bin_srch(mid+1,end));
	}
}

int kw_search()
{
	int i = kw_bin_srch(0, KWTABSZ - 1);
	if (i < 0) return(KW_UNKNOWN);
	return(keywords[i].kw_val);
}

void cnt_message(char *msg, int last)
{
	if (!last) ++curr_ctx->record_cnt;
	if (isset(TALK_ON) && (last || !(curr_ctx->record_cnt % 100))) {
		prnt_str("%06ld RECORD%s %s%c%c", curr_ctx->record_cnt
					,curr_ctx->record_cnt > 1? "S":"", msg, last? '.' : ',', last? '\n' : '\r');
	}
	if (last) curr_ctx->record_cnt = 0;
}

void chk_nulltok(void)
{
	if (curr_cmd && curr_tok != TNULLTOK && curr_tok != TPOUND) syn_err(EXCESS_SYNTAX);
}

int evaluate(char *exp_buf, int catch_error)
{
	PCODE_CTL save_ctl;
 	jmp_buf sav_env;
	PCODE code_buf[CODE_LIMIT];
	BYTE data_buf[SIZE_LIMIT];
	HBVAL *sp = topstack;
	int errcode;

	memcpy(sav_env, env, sizeof(jmp_buf));
	cmd_init(exp_buf, code_buf, data_buf, &save_ctl);
	if (!(errcode = setjmp(env))) {
		get_token();
		expression();
		chk_nulltok();
	}
	restore_pcode_ctl(&save_ctl);
	memcpy(env, sav_env, sizeof(jmp_buf));
	if (errcode) {
		if (!catch_error) syn_err(errcode); //longjmp() to error handler in dbos()
		topstack = sp;
	}
	return(errcode);
}

static char hb_dirent[DIRENTLEN];

static int chk_ftype(char *tp, int typ)
{
	if (typ & F_DBF && (!strcmp(tp, "dbf") || !strcmp(tp, "DBF"))) return(TRUE);
	if (typ & F_NDX && (!strcmp(tp, "ndx") || !strcmp(tp, "NDX"))) return(TRUE);
	if (typ & F_DBT && (!strcmp(tp, "dbt") || !strcmp(tp, "DBT"))) return(TRUE);
	if (typ & F_MEM && (!strcmp(tp, "mem") || !strcmp(tp, "MEM"))) return(TRUE);
	if (typ & F_TXT && (!strcmp(tp, "txt") || !strcmp(tp, "TXT"))) return(TRUE);
	if (typ & F_PRG && (!strcmp(tp, "prg") || !strcmp(tp, "PRG"))) return(TRUE);
	if (typ & F_PRO && (!strcmp(tp, "pro") || !strcmp(tp, "PRO"))) return(TRUE);
	if (typ & F_SER && (!strcmp(tp, "ser") || !strcmp(tp, "SER"))) return(TRUE);
	if (typ & F_ZIP && (!strcmp(tp, "zip") || !strcmp(tp, "ZIP"))) return(TRUE);
	if (typ & F_TBL && (!strcmp(tp, "tbl") || !strcmp(tp, "TBL"))) return(TRUE);
	if (typ & F_IDX && (!strcmp(tp, "idx") || !strcmp(tp, "IDX"))) return(TRUE);
	return(FALSE);
}

static int is_hb_fn(char *fn, int typ)
{
	char *tp, *ext = strrchr(fn, '.');
	int len = 0, is_zip = FALSE;
	if (ext) {
		if (typ == F_ALL) return(TRUE);
		if (strlen(ext) > EXTLEN+1) return(FALSE);
		if (!chk_ftype(ext+1, typ)) return(FALSE);
		if (!strcmp(ext+1, "zip") || !strcmp(ext+1, "ZIP")) is_zip = TRUE;
	}
	else ext = fn + strlen(fn);
	for (tp=fn; tp<ext; ++tp) {
		char c = *tp;
		if (!isprint(c) || c==',' || c=='+' || c=='*' 
			|| c=='?' || c=='&' || c=='\\' || c=='/' 
			|| c=='<' || c=='>' || c=='|' || (c=='.' && !is_zip)
			) return(FALSE);
		++len;
		if (len > FNLEN) return(FALSE);
	}
	if (!len) return(FALSE);
	return(TRUE);
}

#define is_sys_file(fn)	(*fn=='_' || *fn == '.')

char *get_dirent(int firsttime, int typ)
{
	static DIR *dirp;
	static char fn[FULFNLEN], *tp;
	struct dirent *dp;
	int found;
	static int fnlen;
	if (firsttime) {
		if (dirp) closedir(dirp);
		strcpy(fn, defa_path);
		if (!(dirp = opendir(fn))) return(NULL);
		tp = fn + strlen(fn);
		fnlen = FULFNLEN - (tp - fn) - 1;
	}
	else if (!dirp) return(NULL);
	found = 0;
	while (!found && (dp=readdir(dirp))) {
		struct stat f_stat;
		char *tp2 = NULL;
		memset(hb_dirent, 0, DIRENTLEN);
		if (strlen(dp->d_name) > fnlen) continue;
		sprintf(tp, "%s", dp->d_name);
		if (euidaccess(fn, R_OK)) continue;
		hb_dirent[DIRENTPERM] = 1;
		if (!euidaccess(fn, W_OK)) hb_dirent[DIRENTPERM] = 2;
		stat(fn, &f_stat);
		if (S_ISDIR(f_stat.st_mode)) {
			if (!(typ & F_DIR)) continue;
			if (*tp == '.' || euidaccess(fn, X_OK) || (!db3w_admin && f_stat.st_uid != hb_uid)) continue;
			if (strlen(tp) >= FNLEN) continue;
			strcpy(&hb_dirent[DIRENTNAME], tp);
			hb_dirent[DIRENTTYPE] = 1; //directory
		}
		else {
			if (!S_ISREG(f_stat.st_mode)) continue;
			if (!db3w_admin && is_sys_file(tp)) continue;	//ignore _sys.prg if not admin
			if (*tp == '.') {
				if (typ != F_ALL || !db3w_admin) continue;
				if (strlen(tp) >= FNLEN) continue;
			}
			else {
				if (!is_hb_fn(tp, typ)) continue;
				if (tp2 = strrchr(tp,'.')) *tp2 = '\0';
			}
			strcpy(&hb_dirent[DIRENTNAME], tp);
			if (tp2) {
				strncpy(&hb_dirent[DIRENTEXT], tp2+1, EXTLEN);
				chgcase(&hb_dirent[DIRENTEXT], 1);
			}
		}
		write_ftime(&hb_dirent[DIRENTMTIME], f_stat.st_mtime);
		write_fsize(&hb_dirent[DIRENTSIZ], f_stat.st_size);
		found = 1;
	}
	if (!found) {
		closedir(dirp);
		dirp = (DIR *)NULL;
		return((char *)NULL);
	}
	return(hb_dirent);
}

int file_open(int mode, char *ext)
{
	char filename[FULFNLEN], *tp;
	HBVAL *v = pop_stack(CHARCTR);
	sprintf(filename, "%s%.*s", v->buf[0] == SWITCHAR? rootpath : defa_path, v->valspec.len, v->buf);
	tp = strrchr(filename, SWITCHAR);
	if (!tp) tp = filename;
	if (!strrchr(tp, '.') && ext && strlen(ext) <= EXTLEN + 1) strcat(tp, ext);
	if (curr_ctx->exfd != -1) close(curr_ctx->exfd);
	curr_ctx->exfd = open(filename, mode, 0664);
	if (curr_ctx->exfd == -1) db_error(DBOPEN, filename);
	return(curr_ctx->exfd);
}

int file_close(int fd)
{
	if (fd != curr_ctx->exfd) return(-1);
	close(curr_ctx->exfd);
	curr_ctx->exfd = -1;
	return(0);
}

#include <time.h>

char *timestr_(time_t *tp)
{
	struct tm *now;
	static char buf[64];
	now = localtime(tp);
	sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d %s", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, now->tm_zone);
	return(buf);
}

char *val2str(HBVAL *v, char *outbuf, int maxlen) //maxlen must be >= STRBUFSZ
{
	char *tp = outbuf + 3;
	switch (v->valspec.typ) {
		case DATE:
			strcpy(outbuf, "[D:");
			if (v->valspec.len) number_to_date(0, get_date(v->buf), tp);
			break;
		case CHARCTR:
			strcpy(outbuf, "[C:");
			if (v->valspec.len) snprintf(tp, maxlen - 4, "%.*s", (int)v->valspec.len, v->buf);
			break;
		case LOGIC:
			strcpy(outbuf, "[L:");
			if (v->valspec.len) sprintf(tp, ".%c.", v->buf[0] ? 'T' : 'F');
			break;
		case NUMBER:
			strcpy(outbuf, "[N:");
			if (v->valspec.len) {
				number_t num;
				int width = nwidth, deci = ndeci;
				num.deci = v->numspec.deci;
				if (val_is_real(v)) num.r = get_double(v->buf);
				else {
					num.n = get_long(v->buf);
					deci = num.deci;
					if (deci > ndeci) width += deci - ndeci;
				}
				num2str(num, width, deci, tp);
			}
			break;
		case ARRAYTYP:
			strcpy(outbuf, "[A:");
			if (v->valspec.len) {
				int dim = v->valspec.len / sizeof(WORD);
				BYTE *a = v->buf;
				while (dim-- > 1) {
					sprintf(tp, "%d,", get_word(a));
					a += sizeof(WORD);
					tp += strlen(tp);
				}
				sprintf(tp, "%d", get_word(a));
			}
			break;
		case ADDR:
			strcpy(outbuf, "[@:");
			if (v->valspec.len) sprintf(tp, "%03d", get_word(v->buf));
			break;
		default:
			strcpy(outbuf, "[U:");
			strcpy(tp, "Unknown or invalid data type");
	}
	if (!v->valspec.len) strcpy(tp, "<NULL>");
	strcpy(tp + strlen(tp), "]");
	return(outbuf);
}
