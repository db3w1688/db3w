/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/parse.c
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
#include <limits.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "dbe.h"

/************************************************************************/

int cb_data_offset(int data_idx)
{
	int i, offset = 0;
	for (i=0; i<data_idx; ++i) {
		HBVAL *v = (HBVAL *)&pdata[offset];
		offset += val_size(v);
	}
	return(offset);
}

static int data_chk(int size)
{
	int offset = cb_data_offset(nextdata);
	if (nextdata++ >= DATA_LIMIT) syn_err(EXP_TOOLONG);
	if (offset + size >= SIZE_LIMIT) syn_err(EXP_TOOLONG);
	return(offset);
}

int loadlogic(void)
{
	int currdata = nextdata;
	int offset;
	char c = to_upper(curr_cmd[currtok_s + 1]);
	HBVAL *v;

	offset = data_chk(3);
	v = (HBVAL *)(&pdata[offset]);
	v->valspec.typ = LOGIC;
	v->valspec.len = 1;
	v->buf[0] = (c == 'Y') || (c == 'T');
	return(currdata);
}

int loadstring(void)
{
	int currdata = nextdata;
	int offset;
	HBVAL *v;
	char *p = &curr_cmd[currtok_s];
	int len;

	if ((*p == '\'') || (*p == '"')) {
		len = currtok_len - 2;
		++p;
	}
	else len = currtok_len;
	offset = data_chk(len + 2);
	v = (HBVAL *)(&pdata[offset]);
	v->valspec.typ = CHARCTR;
	v->valspec.len = len;
	memcpy(v->buf, p, len);
	return(currdata);
}

extern int is_ndx_expr;

int loadexpr(int isndx)
{
	int save_code = nextcode, save_data = nextdata, save_exe = exe;
	int expr_start, expr, save_s, save_len;
	exe = FALSE;
	expr_start = currtok_s;
	is_ndx_expr = isndx;
	expression();
	if (isndx && curr_tok==TCOMMA) do {
		get_token();
		expression();
	} while (curr_tok==TCOMMA);
	is_ndx_expr = FALSE;
	nextcode = save_code; nextdata=save_data; exe=save_exe;
	save_s = currtok_s; save_len = currtok_len;
	currtok_len = currtok_s - expr_start;
	currtok_s = expr_start;
	expr = loadstring();
	currtok_s = save_s; currtok_len = save_len;
	return(expr);
}

int loadnumber(void) //only positive as minus is handled by uop()
{
	int currdata = nextdata;
	int offset;
	char numbuf[STRBUFSZ];
	HBVAL *v;
	number_t num;

	sprintf(numbuf, "%.*s", currtok_len, &curr_cmd[currtok_s]);
	num = str2num(numbuf);
	offset = data_chk(num_is_real(num)? sizeof(double) + 2 : sizeof(long) + 2);
	v = (HBVAL *)(&pdata[offset]);
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
	return(currdata);
}

int loadint(int n)
{
	int currdata = nextdata, offset;
	HBVAL *v;
	offset = data_chk(sizeof(long) + 2);
	v = (HBVAL *)(&pdata[offset]);
	v->valspec.typ = NUMBER;
	v->numspec.len = sizeof(long);
	v->numspec.deci = 0;
	put_long(v->buf, n);
	return(currdata);
}

int loadfilename(int simple)
{
	int i;
	int j = currtok_s;
	char *p = &curr_cmd[j];
	int ext = FALSE;
	if (currtok_len == 2 && !strncmp(p, "..", 2)) {
		if (simple) goto bad;
		return(loadstring());
	}
	if (currtok_len > (simple? FNLEN:FULFNLEN)) goto bad;
	for (i=0; i<currtok_len; ++i) {
		if (p[i] == ':') goto bad;
		if (p[i] == '.') {
			if (i == currtok_len - 1) continue;
			if (p[i+1] == '/') continue;
			if (i <= currtok_len - 2 && p[i+1] == '.') {
				 if (i < currtok_len - 2 && p[i+2] != '/') goto bad;
				 ++i;
				 continue;
			}
			if (ext) goto bad;
			else {
				ext = TRUE;
				if (!i || (i-j-1 > FNLEN)) goto bad;
				j = i;
			}
		}
		if (p[i] == '/') {
			if (simple) goto bad;
			if ((ext && i-j-1 > EXTLEN) 
				 || (i-j-1 > FNLEN)) goto bad; //no relative path
			j = i;
			ext = FALSE;
		}
	}
	return(loadstring());
bad:
	syn_err(BAD_FILENAME);
}

int loaddate(date_t date)
{
	int currdata = nextdata;
	int offset;
	HBVAL *v;

	offset = data_chk((date != (date_t)-1? sizeof(date_t) : 0) + 2);
	v = (HBVAL *)(&pdata[offset]);
	v->valspec.typ = DATE;
	v->valspec.len = date != (date_t)-1? sizeof(date_t) : 0;
	if (date) put_date(v->buf, date);
	return(currdata);
}

static int is_separator(int ctok)
{
	if (ctok == TDIV || ctok == TPERIOD || ctok == TMINUS || ctok == TCOMMA) return(1);
	return(0);
}

date_t process_date(void)
{
	int month = 0, day = 0, year = 0, i;
	TOKEN tok;
	char ystr[10], seq[4];
	save_token(&tok);
	get_token();
	if (curr_tok == TRBRKT) {
		rest_token(&tok);
		return((date_t)-1); //NULL
	}
	if (curr_tok != TINT) syn_err(BAD_DATE);
	datefmt(NULL, NULL, seq, NULL);
	for (i=0; i<3; ++i) switch (seq[i]) {
		case 'm':
			month = atoi(&curr_cmd[currtok_s]);
			get_token();
			if (curr_tok == TRBRKT) goto end;
			if (is_separator(curr_tok)) {
				get_token();
				if (curr_tok != TINT) syn_err(BAD_DATE);
			}
			else syn_err(BAD_DATE);
			break;
		case 'd':
			day = atoi(&curr_cmd[currtok_s]);
			get_token();
			if (curr_tok == TRBRKT) goto end;
			if (is_separator(curr_tok)) {
				get_token();
				if (curr_tok != TINT) syn_err(BAD_DATE);
			}
			else syn_err(BAD_DATE);
			break;
		case 'y':
			if (currtok_len > 4) syn_err(BAD_DATE);
			sprintf(ystr, "%.*s", currtok_len, &curr_cmd[currtok_s]);
			year = atoi(ystr);
			get_token();
			if (curr_tok == TRBRKT) goto end;
			if (is_separator(curr_tok)) {
				get_token();
				if (curr_tok != TINT) syn_err(BAD_DATE);
			}
			else syn_err(BAD_DATE);
			break;
		default:
			break;
	}
end:
	if (!verify_date(year, month, day)) syn_err(BAD_DATE);
	return(date_to_number(day, month, year));
}

int loadskel(int typ)
{
	int i;
	char *p = &curr_cmd[currtok_s];
	int dot = 0;
	int ext = 0;
	if (currtok_len > (typ == SKEL_VARNAME ? VNAMELEN : FNLEN+EXTLEN+1)) goto bad;
	for (i=0; i<currtok_len; ++i) {
		int c = p[i];
		if (!(isalpha(c) || ((i && (typ == SKEL_VARNAME)) || (typ == SKEL_FILENAME))
			 && isdigit(c))) {
			if ((typ == SKEL_FILENAME) && (c == '.')) {
				if (dot) goto bad;
				dot = TRUE;
			}
			else if ((c != '?') && (c != '*')) goto bad;
		}
		else if (dot) ++ext;
		if (ext > EXTLEN) goto bad;
	}
	return(loadstring());
bad:
	syn_err(BAD_SKEL);
}

static void factor(void)
{
	switch(curr_tok) {
		case TPLUS:

		case TMINUS:{
			int op = curr_tok;
			get_token();
			factor();
			codegen(UOP, op);
			}
			break;

		case TEOF:
		case TNULLTOK:
			syn_err(BAD_EXP);

		case TLIT:
			codegen(PUSH, loadstring());
			get_token();
			break;

		case TMAC:
			codegen(MACRO, loadstring());
			get_token();
			break;

		case TLPAREN:
			get_token();
			expression();
			if (curr_tok != TRPAREN) syn_err(MISSING_RPAREN);
			get_token();
			break;

		case TNOT:
			get_token();
			factor();
			codegen(UOP, TNOT);
			break;

		case TINT:
		case TNUMBER:
			codegen(PUSH, loadnumber());
			get_token();
			break;

		case TLOGIC: 
			codegen(PUSH, loadlogic());
			get_token();
			break;

		case TID: {
			int isfcn,isalias,isarray;
			TOKEN tk;
			save_token(&tk);
			get_token();
			isfcn = curr_tok == TLPAREN;
			isalias = curr_tok == TARROW;
			isarray = curr_tok == TLBRKT;
			rest_token(&tk);
			if (isfcn) {
				int fcn_no = function_id();
				int parm_cnt = 0;
				int usrfcn = -1;
				if (fcn_no==-1) {
					if (is_ndx_expr) syn_err(ILL_FCN);
					usrfcn = loadstring();
				}
				else if (is_ndx_expr && (fcn_no==RTRIM || fcn_no==LTRIM
							|| fcn_no==IIF || fcn_no==KEY)) syn_err(ILL_FCN);
				get_token();
				do {
					get_token();
					if ((curr_tok == TRPAREN) && !parm_cnt) break;
					expression();
					++parm_cnt;
					parm_chk(0,fcn_no,parm_cnt);
				} while (curr_tok == TCOMMA);
				if (curr_tok != TRPAREN) syn_err(MISSING_RPAREN);
				parm_chk(1,fcn_no,parm_cnt);
				codegen(PUSH_N, parm_cnt);
				if (fcn_no == -1) codegen(USRFCN, usrfcn);
				else codegen(FCN, fcn_no);
			}
			else if (isalias) {
				int a = loadstring();
				get_token();
				get_token();
				codegen(curr_tok==TMAC?MACRO_ID:PUSH, loadstring());
				codegen(PUSH, a);
				codegen(DRETRV, OPND_ON_STACK);
			}
			else if (isarray) {
				process_array();
				codegen(NRETRV, OPND_ON_STACK);
			}
			else codegen(NRETRV, loadstring());
			get_token();
			break; }

		case TLBRKT:
			codegen(PUSH, loaddate(process_date()));
			if (curr_tok != TRBRKT) syn_err(MISSING_RBRKT);
			get_token();
			break;

		default: syn_err(BAD_EXP);
	}
}

static void term1(void)
{
	factor();
	while (curr_tok == TPOWER) {
		get_token();
		factor();
		codegen(BOP, TPOWER);
	}
}

static void term(void)
{
	int op;

	term1();
	while (is_mulop(op = curr_tok)) {
		get_token();
		term1();
		codegen(BOP, op);
	}
}

static void simple_expr(void)
{
	int op;

	term();
	while (is_addop(op = curr_tok)) {
		get_token();
		term();
		codegen(BOP, op);
	}
}

static void rel_expr1(void)
{
	int op;

	simple_expr();
	if (is_relop(op = curr_tok)) {
		get_token();
		simple_expr();
		codegen(BOP, op);
	}
}

static void rel_expr(void)
{
	rel_expr1();
	while (curr_tok == TAND) {
		get_token();
		rel_expr1();
		codegen(BOP, TAND);
	}
}

void expression(void)
{
	rel_expr();
	while (curr_tok == TOR) {
		get_token();
		rel_expr();
		codegen(BOP, TOR);
	}
}

void process_array(void)
{
	int aname = loadstring();
	int dim = 0; //array dimension
	get_token();
	do {
		get_token();
		expression();
		++dim;
	} while (curr_tok == TCOMMA);
	if (curr_tok != TRBRKT) syn_err(MISSING_RBRKT);
	codegen(PUSH_N, dim);
	codegen(ARRAYCHK, aname);
}
