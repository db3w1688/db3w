/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/int.c
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
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <setjmp.h>
#include <errno.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "hbusr.h"
#include "dbe.h"

#define CX_LOADED          0x0100   /** cx loaded mask **/
#define USR_MASK           0x007f

void _usr(int fcn)
{
	if (!usr_conn) sys_err(USR_NOTINSTALLED);
	if (!(CX_LOADED & fcn)) usr_regs.cx = 0;
	usr_regs.ax = USR_MASK & fcn;
	usr_regs.fx = 0; //to be set to 1 if escape is pressed at client
	if (usr_tli()) longjmp(env, USR_ERROR + usr_regs.ax);
	escape = usr_regs.fx;
}

char *usr2str(int usr_fcn)
{
	switch(usr_fcn) {
	case USR_FCN:
		return("USR_FCN");
	case USR_PRNT_VAL:
		return("USR_PRNT_VAL");
	case USR_DISPREC:
		return("USR_DISPREC");
	case USR_DISPSTR:
		return("USR_DISPSTR");
	case USR_READ:
		return("USR_READ");
	case USR_DISPFILE:
		return("USR_DISPFILE");
	case USR_CREAT:
		return("USR_CREAT");
	case USR_INPUT:
		return("USR_INPUT");
	default:
		return("USR_OTHER");
	}
}

long power10(int exp)
{
	long result = 1;
	while (exp) { result *= 10; --exp; }
	return result;
}

int val_size(HBVAL *v)
{
	if (v->valspec.len) switch (v->valspec.typ) {
		case NULLTYP:
			return 2;
	
		case LOGIC:
			return 3;
	
		case NUMBER:
			return v->numspec.len + 2;
	
		case DATE:
			return sizeof(date_t) + 2;
	
		case CHARCTR:
		default:
			return v->valspec.len + 2;
	}
	else return 2;
}

HBVAL *stack_peek(int level)
{
	HBVAL *v = topstack;
	while (level && (char *)v < exp_stack) {
		v = (HBVAL *)((char *)v + val_size(v));
		level--;
	}
	if (level && !restricted) sys_err(STACK_FAULT); //only allow NULL return in restricted mode
	return(level? NULL : ((char *)v < exp_stack? v : NULL));
}

HBVAL *stack_chk_set(int size)
{
	HBVAL *t = (HBVAL *)((char *)topstack - size);
	if (t < (HBVAL *)stack_limit) 
		sys_err(STACK_OVERFLOW);
	topstack = t;
	return t;
}

void push(HBVAL *v)
{
	int vsize = val_size(v);
	stack_chk_set(vsize);
	memmove(topstack, v, vsize); //v and topstack may overlap
}

HBVAL *pop_stack(int typ)
{
	if (typ != DONTCARE) typechk(typ);
	HBVAL *t = topstack;
	topstack = (HBVAL *)((char *)t + val_size(t));
	if (topstack > (HBVAL *)exp_stack) sys_err(STACK_UNDERFLOW);
	return t;
}

void push_opnd(int opnd)
{
	push((HBVAL *)&pdata[cb_data_offset(opnd)]);
}

void push_string(char *s, int len)
{
	HBVAL *v = stack_chk_set(len + 2);
	v->valspec.typ = CHARCTR;
	v->valspec.len = len;
	memcpy(v->buf, s, len);
}

void push_logic(int tf)
{
	HBVAL *v = stack_chk_set(tf >= 0? 3 : 2);
	v->valspec.typ = LOGIC;
	if (tf < 0) v->valspec.len = 0; //NULL
	else {
		v->valspec.len = 1;
		v->buf[0] = !!tf;
	}
}

void push_int(long n)
{
	HBVAL *v = stack_chk_set(sizeof(long) + 2);
	v->valspec.typ = NUMBER;
	v->numspec.deci = 0;
	v->numspec.len = sizeof(long);
	put_long(v->buf, n);
}

void push_number(number_t num)
{
	if (num_is_null(num)) {
		HBVAL *v = stack_chk_set(2);
		v->valspec.typ = NUMBER;
		v->valspec.len = 0;
	}
	else {
		int is_real = num_is_real(num);
		HBVAL *v = stack_chk_set((is_real? sizeof(double) : sizeof(long)) + 2);
		v->valspec.typ = NUMBER;
		v->numspec.len = is_real? sizeof(double) : sizeof(long);
		v->numspec.deci = num.deci;
		if (is_real) put_double(v->buf, num.r);
		else put_long(v->buf, num.n);
	}
}

void push_real(double r)
{
	if ((double)(long)r == r) {
		number_t num;
		num.deci = 0;
		num.n = (long)r;
		push_number(num);
		return;
	}
	HBVAL *v = stack_chk_set(sizeof(double) + 2);
	v->valspec.typ = NUMBER;
	v->numspec.len = sizeof(double);
	v->numspec.deci = REAL;
	put_double(v->buf, r);
}

void push_date(date_t d)
{
	HBVAL *v;
	if (d == (date_t)-1) { //NULL date
		v = stack_chk_set(2);
		v->valspec.len = 0;
	}
	else {
		v = stack_chk_set(sizeof(date_t) + 2);
		v->valspec.len = sizeof(date_t);
		put_date(v->buf, d);
	}
	v->valspec.typ = DATE;
}

number_t pop_date(void)
{
	HBVAL *v = pop_stack(DATE);
	number_t num;
	if (!v->valspec.len) num_set_null(num);
	else {
		num.deci = 0;
		num.n = (long)get_date(v->buf);
	}
	return num;
}

double pop_real(void)
{
	HBVAL *v = pop_stack(NUMBER);
	if (val_is_real(v)) return get_double(v->buf);
	return (double)get_long(v->buf) / power10(v->numspec.deci);
}

number_t pop_number(void)
{
	number_t num;
	HBVAL *v = pop_stack(NUMBER);
	if (v->valspec.len) {
		num.deci = v->numspec.deci;
		if (val_is_real(v)) num.r = get_double(v->buf);
		else num.n = get_long(v->buf);
	}
	else num_set_null(num); //NULL value
	return num;
}

int pop_logic(void)
{
	HBVAL *v = pop_stack(LOGIC);
	return v->valspec.len? !!v->buf[0] : -1; //NULL: -1
}

int pop_int(int low, int high)
{
	number_t num = pop_number();
	int i;
	if (!num_is_integer(num)) syn_err(INVALID_VALUE);
	i = (int)num.n;
	if (i < low || (high >= 0 && i > high)) syn_err(INVALID_VALUE);
	return i;
}

long pop_long(void) //signed
{
	number_t num = pop_number();
	if (!num_is_integer(num)) syn_err(INVALID_VALUE);
	return num.n;
}

void uop(int op)
{
	switch (op) {
		case TNOT: {
			int tf = pop_logic();
			push_logic(tf < 0? -1 : !tf);
			break; }
		case TMINUS: {
			number_t num = pop_number();
			if (num_is_null(num)) push_number(num);
			else {
				if (num_is_real(num)) num.r = -num.r;
				else num.n = -num.n;
				push_number(num);
			}
			break; }
		case TPLUS:
			typechk(NUMBER);
			break;
		default:
			syn_err(INVALID_OPERATOR);
	}
}

#include <math.h>
#include <float.h>

int num_rel(number_t n1, number_t n2, int op)
{
	int tf = 0;
	if (num_is_null(n1) || num_is_null(n2)) {
		tf = -1;
		goto end;
	}
	if (!num_is_real(n1) && !num_is_real(n2)) {
		long l1 = n1.n, l2 = n2.n, diff;
		if (n1.deci > n2.deci) l2 *= power10(n1.deci - n2.deci);
		else l1 *= power10(n2.deci - n1.deci);
		diff = l1 - l2;
		switch (op) {
			case TGREATER:
				tf = diff > 0L;
				break;
			case TLESS:
				tf = diff < 0L;
				break;
			case TEQUAL:
				tf = diff == 0L;
				break;
			case TNEQU:
				tf = diff != 0L;
				break;
			case TGTREQU:
				tf = diff >= 0L;
				break;
			case TLESSEQU:
				tf = diff <= 0L;
				break;
			case TQUESTN:
				tf = diff > 0L ? 1 : (diff < 0L ? -1 : 0);
		}
	}
	else {
		double r1, r2;
		if (num_is_real(n1)) r1 = n1.r;
		else r1 = (double)n1.n / power10(n1.deci);
		if (num_is_real(n2)) r2 = n2.r;
		else r2 = (double)n2.n / power10(n2.deci);
		double abs_r1 = fabs(r1);
		double abs_r2 = fabs(r2);
		double diff = fabs(abs_r1 - abs_r2);
		int isequal = (r1 == r2) ||
			(((r1 == 0) || (r2 == 0) || (diff < DBL_MIN)) && (diff < (double)DBL_EPSILON * DBL_MIN)) ||
			(diff / fmin((abs_r1 + abs_r2), DBL_MAX) < DBL_EPSILON);
		switch (op) {
		case TGREATER:
			tf = isequal? 0 : r1 > r2;
			break;
		case TLESS:
			tf = isequal? 0 : r1 < r2;
			break;
		case TEQUAL:
			tf = isequal;
			break;
		case TNEQU:
			tf = !isequal;
			break;
		case TGTREQU:
			tf = isequal || (r1 > r2);
			break;
		case TLESSEQU:
			tf = isequal || (r1 < r2);
			break;
		case TQUESTN:
			tf = isequal? 0 : (r1 > r2? 1 : -1);
		}
	}
end:
	return(tf);
}

number_t num_add(number_t n1, number_t n2, int op)
{
	number_t num;
	if (num_is_null(n1) || num_is_null(n2)) {
		num_set_null(num);
		goto end;
	}
	if (num_is_real(n1) || num_is_real(n2)) {
		double r1 = num_is_real(n1)? n1.r : (double)n1.n / power10(n1.deci);
		double r2 = num_is_real(n2)? n2.r : (double)n2.n / power10(n2.deci);
		num.deci = REAL;
		num.r = op == TPLUS? r1 + r2 : r1 - r2;
	}
	else {
		long l1 = n1.n;
		long l2 = n2.n;
		if (n1.deci > n2.deci) {
			l2 *= power10(n1.deci - n2.deci);
			if (n2.n > 0 && l2 < 0 || n2.n < 0 && l2 > 0) syn_err(INVALID_VALUE); //overflow
			num.deci = n1.deci;
		}
		else {
			l1 *= power10(n2.deci - n1.deci);
			if (n1.n > 0 && l1 < 0 || n1.n < 0 && l1 > 0) syn_err(INVALID_VALUE); //overflow
			num.deci = n2.deci;
		}
		num.n = op == TPLUS? l1 + l2 : l1 - l2;
		if ((op == TPLUS && ((l1 > 0 && l2 > 0 && num.n < 0) || (l1 < 0 && l2 < 0 && num.n > 0)))
			|| (op == TMINUS && ((l1 > 0 && l2 < 0 && num.n < 0) || (l1 < 0 && l2 > 0 && num.n > 0))))
			syn_err(INVALID_VALUE); //overflow
	}
end:
	return num;
}

number_t num_mul(number_t n1, number_t n2, int op)
{
	number_t num;
	if (num_is_null(n1) || num_is_null(n2)) {
		num_set_null(num);
		goto end;
	}
	if (num_is_real(n1) || num_is_real(n2) || (n1.deci + n2.deci > DECI_MAX)) {
		double r1 = num_is_real(n1)? n1.r : (double)n1.n / power10(n1.deci);
		double r2 = num_is_real(n2)? n2.r : (double)n2.n / power10(n2.deci);
		num.deci = REAL;
		num.r = op == TMULT? r1 * r2 : r1 / r2;
	}
	else {
		if (op == TMULT) {
			if (n1.deci + n2.deci <= DECI_MAX) {
				num.deci = n1.deci + n2.deci;
				num.n = n1.n * n2.n;
				if ((n1.n > 0 && n2.n > 0 && num.n < 0) 
					|| ((n1.n < 0 && n2.n > 0) || (n1.n > 0 && n2.n < 0)) && num.n > 0) 
					syn_err(INVALID_VALUE); //overflow
			}
			else {
				num.deci = REAL;
				num.r = ((double)n1.n / power10(n1.deci)) * ((double)n2.n / power10(n2.deci));
			}
		}
		else { //TDIV
			num.deci = REAL;
			num.r = ((double)n1.n / power10(n1.deci)) / ((double)n2.n / power10(n2.deci));
		}
	}
end:
	return num;
}

void bop(int op)
{
	HBVAL *opnd1 = topstack;
	HBVAL *opnd2 = stack_peek(1);
	number_t n1,n2;
	int l1, l2;

	if (opnd1->valspec.typ != opnd2->valspec.typ) {
		if (!(opnd2->valspec.typ == DATE && opnd1->valspec.typ == NUMBER && val_is_integer(opnd1))) syn_err(TYPE_MISMATCH);
	}
	if (op == TOR) {
		l1 = pop_logic();
		l2 = pop_logic();
		if (l1 < 0) push_logic(l2);
		else if (l2 < 0) push_logic(l1);
		else push_logic(l2 || l1);
	}
	else if (op == TAND) {
		l1 = pop_logic();
		l2 = pop_logic();
		push_logic(l1 < 0 || l2 < 0? -1 : (l1 & l2));
	}
	else if (is_relop(op)) {
		int tf;
		switch (opnd1->valspec.typ) {
			case NUMBER:
				n1 = pop_number();
				n2 = pop_number();
				tf = num_rel(n2, n1, op);
				break;
			case DATE:
				n1 = pop_date();
				n2 = pop_date();
				tf = num_rel(n2, n1, op);
				break;
			case CHARCTR: {
				int len = imin(opnd1->valspec.len, opnd2->valspec.len); //can be 0
				BYTE *p = opnd1->buf;
				BYTE *q = opnd2->buf;
				int i = 0;
				if (isset(NULL_ON) && !len) tf = op == TQUESTN? -2 : -1;
				else {
					while (len--) {
						int c1 = *q++;
						int c2 = *p++;
						//if (!isset(EXACT_ON)) { c1 = to_upper(c1); c2 = to_upper(c2); }
						if (i = c1 - c2) break;
					}
					if (!i && isset(EXACT_ON)) i = opnd2->valspec.len - opnd1->valspec.len;
					switch (op) {
						case TGREATER:  tf = i>0; break;
						case TLESS:	  tf = i<0; break;
						case TEQUAL:	 tf = !i; break;
						case TNEQU:	  tf = !!i; break;
						case TGTREQU:	tf = i>=0; break;
						case TLESSEQU:  tf = i<=0; break;
						case TQUESTN:	tf = i>0 ? 1 : (i<0 ? -1 : 0);
					}
					pop_stack(DONTCARE);
					pop_stack(DONTCARE);
				}
				break; }
			default: syn_err(INVALID_TYPE);
		}
		if (op == TQUESTN) {
			number_t num;
			if (tf == -2) num_set_null(num); //null value
			else {
				num.deci = 0;
				num.n = tf;
			}
			push_number(num);
		}
		else push_logic(tf);
	}
	else if (is_addop(op)) {
		switch (opnd1->valspec.typ) {
			case NUMBER:
				n1 = pop_number();
				if (opnd2->valspec.typ == DATE) {
					n2 = pop_date();
					n1 = num_add(n2, n1, op);
					push_date(num_is_null(n1)? (date_t)-1 : (date_t)n1.n);
				}
				else {
					n2 = pop_number();
					push_number(num_add(n2, n1, op));
				}
				break;
			case CHARCTR:
				if (op != TPLUS) syn_err(INVALID_OPERATOR);
				else {
					int len = opnd1->valspec.len + opnd2->valspec.len; //len may be 0
					char buf[MAXSTRLEN];
					if (len > MAXSTRLEN) syn_err(STR_TOOLONG);
					memcpy(buf, opnd2->buf, opnd2->valspec.len);
					memcpy(&buf[opnd2->valspec.len], opnd1->buf, opnd1->valspec.len);
					pop_stack(DONTCARE);
					pop_stack(DONTCARE);
					push_string(buf, len);
				}
				break;
			case DATE:
				n1 = pop_date();
				if (opnd2->valspec.typ == DATE) {
					if (op != TMINUS) syn_err(INVALID_OPERATOR);
					n2 = pop_date();
					push_number(num_add(n2, n1, op));
				}
				else {
					n2 = pop_number();
					n1 = num_add(n2, n1, op);
					push_date(num_is_null(n1)? (date_t)-1 : (date_t)n1.n);
				}
				break;
			default: syn_err(INVALID_TYPE);
		}
	}
	else if (is_mulop(op)) {
		n1 = pop_number();
		n2 = pop_number();
		push_number(num_mul(n2, n1, op));
	}
	else if (op == TPOWER) {
		n1 = pop_number();
		n2 = pop_number();
		if (num_is_null(n1) || num_is_null(n2)) {
			number_t num;
			num_set_null(num);
			push_number(num);
		}
		else {
			double r1, r2;
			r1 = num_is_real(n1)? n1.r : (double)n1.n / power10(n1.deci);
			r2 = num_is_real(n2)? n2.r : (double)n2.n / power10(n2.deci);
			push_real(pow(r2, r1));
		}
	}
	else syn_err(INVALID_OPERATOR);
}

void retriev(int name)
{
	if (name != OPND_ON_STACK) push((HBVAL *)&pdata[cb_data_offset(name)]);
	if (!dbretriv()) v_retrv();
}

void (*fcn[MAXFCN])(int parm_cnt);

void fcn_call(int fcn_no)
{
	(*fcn[fcn_no])(pop_int(0, 255));
}

void macro(int amprsnd)
{
	char comm_line[STRBUFSZ];
	HBVAL *v = (HBVAL *)&pdata[cb_data_offset(0xff & amprsnd)];
	PCODE code_buf[CODE_LIMIT];
	BYTE data_buf[SIZE_LIMIT];
	PCODE_CTL save_ctl;

	sprintf(comm_line, "%.*s", v->valspec.len, v->buf);
	cmd_init(comm_line, code_buf, data_buf, &save_ctl);
	switch (0xff00 & amprsnd) {
		case 0:
			get_token();
			if (curr_tok) expression();
			break;
		case MAC_FULFN:
			get_tok();
			push_opnd(loadfilename(FALSE));
			get_token();
			break;
		case MAC_FN:
			get_tok();
			push_opnd(loadfilename(TRUE));
			get_token();
			break;
		case MAC_ALIAS:
			get_token();
			if ((curr_tok != TID) && (curr_tok != TINT)) syn_err(BAD_ALIAS);
			push_opnd(loadstring());
			get_token();
			break;
		case MAC_ID: {
			int isarray;
			struct token tk;
			get_token();
			if ((curr_tok != TID)) syn_err(BAD_VARNAME);
			save_token(&tk);
			get_token();
			isarray = curr_tok == TLBRKT;
			rest_token(&tk);
			if (isarray) process_array();
			else push_opnd(loadstring());
			get_token();
			break;
			}
	}
	chk_nulltok();
	restore_pcode_ctl(&save_ctl);
}

void macro_fulfn(int amprsnd)
{
	macro(MAC_FULFN | amprsnd);
}

void macro_fn(int amprsnd)
{
	macro(MAC_FN | amprsnd);
}

void macro_alias(int amprsnd)
{
	macro(MAC_ALIAS | amprsnd);
}

void macro_id(int amprsnd)
{
	macro(MAC_ID | amprsnd);
}

void prnt_val(HBVAL *v, int fchar)
{
	usr_regs.bx = (char *)v;
	usr_regs.cx = (v->valspec.typ == NUMBER? v->numspec.len : v->valspec.len) + 2;
	usr_regs.ex = fchar;
	_usr(CX_LOADED | USR_PRNT_VAL);
}

void prnt(int fchar)
{
	HBVAL *v = pop_stack(DONTCARE);
	prnt_val(v, fchar);
}

int alias2ctx(HBVAL *v)
{
	int i;
	if (v->valspec.len == 1) {
		i = to_upper(v->buf[0]);
		if ((i>= 'A') && (i<='Z')) return(i-'A');
	}
	for (i=0; i<MAXCTX; ++i)
		if (!namecmp(table_ctx[i].alias, v->buf, v->valspec.len, ID_FNAME)) break;
	if (i >= MAXCTX) syn_err(NOSUCHALIAS);
	return(i);
}

static void select_(int n)
{
	if (!n) {
		HBVAL *v = pop_stack(CHARCTR);
		if (isdigit(v->buf[0])) n = atoi(v->buf);
		else n = alias2ctx(v) + 1;
	}
	else if (n == NEXT_AVAIL) n = ctx_next_avail() + 1;
	if (n < 1 || n > MAXCTX) db_error0(ILL_SELECT);
	ctx_select(n - 1);
}

static void go_(int r)
{
	valid_chk();
	switch (r) {
		case 0: {
			if (!go((recno_t)pop_long())) syn_err(OUT_OF_RANGE);
			break; }
		case 1:
			gotop();
			break;
		case 2:
			gobot();
			break;
		case 3: //from fcodegen()
			if (index_active() && is_scope_set()) 
				curr_ctx->scope.nextrec = nextndx(curr_ctx->scope.dir, 0);
			break;
		case 4:
		default:
			break; //do nothing, stay at current rec
	}
}

static void skip_(int r) //opnd is unsigned char
{
	valid_chk();
	long nrecs;
	if (r == 0xff) nrecs = pop_long();
	else {
		nrecs = 0x7f & r;
		if (0x80 & r) nrecs = -nrecs;
	}
	nrecs = skip(nrecs);
	if (is_scope_set()) curr_ctx->scope.count += nrecs;
}

void disprec(int argc)
{
	usr_regs.ex = argc;
	_usr(USR_DISPREC);
}

static void dispfil(int mode)
{
	usr_regs.ex = mode;
	_usr(USR_DISPFILE);
}

void scope_init(recno_t count, int dir)
{
	escape = 0; //reset to allow escape from keyboard
	curr_ctx->scope.total = count; //total == 0: disabled
	curr_ctx->scope.dir = dir;
	curr_ctx->scope.count = 0;
	curr_ctx->scope.nextrec = (recno_t)0;
	curr_ctx->record_cnt = 0;
}

static void scope_f(int cnt)
{
	recno_t count;
	if (cnt == 0xff) count = MAXNRECS;
	else {
		HBVAL *v = (HBVAL *)&pdata[cb_data_offset(cnt)];
		if (!val_is_integer(v)) syn_err(INVALID_VALUE);
		count = (recno_t)get_long(v->buf);
	}
	scope_init(count, FORWARD);
}

static void scope_b(int cnt)
{
	recno_t count;
	if (cnt == 0xff) count = MAXNRECS;
	else {
		HBVAL *v = (HBVAL *)&pdata[cb_data_offset(cnt)];
		if (!val_is_integer(v)) syn_err(INVALID_VALUE);
		count = (recno_t)get_long(v->buf);
	}
	scope_init(count, BACKWARD);
}

static void r_cnt(int mode)
{
	char *msg = "COUNTED";
	switch (mode) {
		case 0:
			cnt_message(msg, 0);
			break;
		case 1:
			push_int(curr_ctx->record_cnt);
		case 2:
			cnt_message(msg, 1);
	}
}

static void replace(int name)
{
	if (name != OPND_ON_STACK) push((HBVAL *)&pdata[cb_data_offset(name)]);
	dbstore();
}

static void nop(void)
{
}

static void inp_(int mode)
{
	usr_regs.ex = USR_GETS;
	_usr( USR_INPUT);
}

void _set_onoff(int parm)
{
	int what = parm >> N_ONOFF_BITS;
	set_onoff(what, ONOFF_MASK & parm);
	if (what == DELETE_ON && onoff_flags[what] && chkdb() == IN_USE && isdelete()) skip(1);
}

void dispstru(void)
{
	valid_chk();
	lock_hdr();
	_usr(USR_DISPSTR);
	unlock_hdr();
}

static void form_read(int set_stamp)
{
#ifdef FORM_FP_CLIENT
	if (!frmfctl.filename) db_error(BADFILENAME, "No FORM file specified");
	usr_regs.bx = frmfctl.filename;
	usr_regs.cx = strlen(frmfctl.filename) + 1;
#ifndef HBLIB
	usr_regs.dx = hb_session_stamp(set_stamp);
#else
	usr_regs.dx = 0;
#endif
	_usr(CX_LOADED | USR_READ);
#else
	if (!frmfctl.fp) db_error0(NOFORM);
	usr_regs.cx = 0;
#ifndef HBLIB
	usr_regs.dx = hb_session_stamp(set_stamp);
#else
	usr_regs.dx = 0;
#endif
	_usr(USR_READ);
#endif
}

void typechk(int typ)
{
	if (topstack->valspec.typ != typ) syn_err(INVALID_TYPE);
}

static void push_n(int i)
{
	push_int((long)i);
}

void quit(void)
{
	longjmp(env, -1);
}

static void dretrv(int name)
{
	int newctx = alias2ctx(pop_stack(CHARCTR));
	int my_ctx = get_ctx_id();
	int result;
	if (newctx == MAXCTX) syn_err(BAD_ALIAS);
	ctx_select(newctx);
	result = dbretriv();
	ctx_select(my_ctx);
	if (!result) syn_err(UNDEF_VAR);
}

static void usrfcn(int name)
{
	HBVAL *v = (HBVAL *)&pdata[cb_data_offset(name)];
	char proc_name[VNAMELEN + 1];
	int pcount = pop_int(0, 255);
	sprintf(proc_name, "%.*s", v->valspec.len, v->buf);	/** len checked at compile time **/
	if (execute_proc(proc_name, PRG_FUNC, pcount) >= 0) return;
	if (!usr_conn) syn_err(UNDEF_FCN);
	usr_regs.ex = pcount;
	push(v);
	_usr(USR_FCN);
}

static void usrldd(int n)
{
	usr_regs.ex = n;
}

static void parameter(int pname)
{
	int pcount;
	HBVAL *v;
	if (!(pcount = pop_pcount())) syn_err(INVALID_PARMCNT);
	push_pcount(pcount - 1);
	if (pname != OPND_ON_STACK) push_opnd(pname);
	v = topstack;
	private(1);
	topstack = (HBVAL *)((char *)topstack - val_size(v));
	push(stack_peek(pcount));
	push(v);
	vstore(OPND_ON_STACK);
	pop_stack(DONTCARE);
}

static void del_recall_(int mode)
{
	int mark = 0x7f & mode;
	char *msg = mark == DEL_MKR? "DELETED":"RECALLED";
	if (!(0x80 & mode)) {
		if (del_recall(mark)) cnt_message(msg, 0);
	}
	else { //final
		cnt_message(msg, 1);
		if (mark=='*' && isset(DELETE_ON)) {
			skip(1);
			if (flagset(e_o_f)) gobot();
		}
	}
}

static void locate_(int mode)
{
	valid_chk();
	if (!(0x80 & mode)) { //not a CONTINUE command
		del_exp(EXP_LOCATE);
		if (mode == 0) return;
		add_exp(EXP_LOCATE);
	}
	else if (!curr_ctx->locate) return;
	put_cond(curr_ctx->locate, curr_ctx->loc_pbuf, LOCPBUFSZ);
	_locate( 0x7f & mode, curr_ctx->loc_pbuf);
}

static void _reset(int mask)
{
	if (mask & RST_UNLOCK) unlock_all();
	if (mask & RST_SYS) sys_reset();
}

static void store_rec_(int mode)
{
	char *msg = "UPDATED";
	if (!mode) {
		if (!etof(0)) {
			store_rec();
			cnt_message(msg, 0);
		}
	}
	else {
		cnt_message(msg, 1);
		r_lock(curr_ctx->crntrec, HB_UNLCK, 0);
	}
}

#include <sys/types.h>
#include <time.h>

extern int db3w_admin;

static void dir_list(void)
{
	int type = pop_int(0, 65535);
	int i, to_dbf = !!(type & DIR_TO_DBF), ctx_id = get_ctx_id();
	char *dp;
	time_t fmt;
	size_t size;
	if (to_dbf) {
		TABLE *hdr;
		char *rec;
		ctx_select(AUX);
		use(U_USE | U_CREATE);
		hdr = get_db_hdr();
		hdr->id = DB3_MKR;
		hdr->reccnt = 0;
		hdr->hdlen = hdr->doffset = 225;
		hdr->byprec = 70;
		hdr->access = 255;
		hdr->fdlist[0] = (FIELD){ .name="NAME", .ftype='C', .tlength=32, .dlength=0 };
		hdr->fdlist[1] = (FIELD){ .name="EXT", .ftype='C', .tlength=4, .dlength=0 };
		hdr->fdlist[2] = (FIELD){ .name="ISDIR", .ftype='L', .tlength=1, .dlength=0 };
		hdr->fdlist[3] = (FIELD){ .name="PERM", .ftype='C', .tlength=2, .dlength=0 };
		hdr->fdlist[4] = (FIELD){ .name="MTIME", .ftype='N', .tlength=10, .dlength=0 };
		hdr->fdlist[5] = (FIELD){ .name="SIZE", .ftype='N', .tlength=20, .dlength=0 };
		hdr->fldcnt = 6;
		setflag(dbmdfy);
		setflag(fextd);
		for (i=0; dp=get_dirent(!i, F_ALL & type); ++i) {
			int offset = 1;
			_apblank(0);
			rec = get_db_rec();
			sprintf(rec + offset, "%-*.*s", hdr->fdlist[0].tlength, FNLEN, dp + DIRENTNAME);
			offset += hdr->fdlist[0].tlength;
			sprintf(rec + offset, "%-*.*s", hdr->fdlist[1].tlength, EXTLEN, dp + DIRENTEXT);
			offset += hdr->fdlist[1].tlength;
			*(rec + offset) = dp[DIRENTTYPE]? 'T' : 'F'; offset++;
			strcpy(rec + offset, dp[DIRENTPERM] == 1? "R-":"RW");
			offset += hdr->fdlist[3].tlength;
			fmt = read_ftime(dp + DIRENTMTIME);
			sprintf(rec + offset, "%*ld", hdr->fdlist[4].tlength, fmt);
			offset += hdr->fdlist[4].tlength;
			if (!dp[DIRENTTYPE]) {
				size = read_fsize(dp + DIRENTSIZ);
				sprintf(rec + offset, "%*ld", hdr->fdlist[5].tlength, size);
			}
			store_rec();
		}
		use(0);
		ctx_select(ctx_id);
	}
	else {
		char vbuf[STRBUFSZ];
		HBVAL *v = (HBVAL *)vbuf;
		int out = 0;
		size_t tsize;
		char *tp;
		prnt_str("Directory: %s\n", db3w_admin? defa_path : defa_path + strlen(rootpath));
		strcpy(v->buf, "File Type: ");
		tp = v->buf + strlen(v->buf);
		if ((type & F_ALL) == F_ALL) strcpy(tp, "ALL");
		else for (i=0; i<sizeof(short) * CHAR_BIT; ++i) {
			int typ = (F_ALL & type) & (1 << i);
			switch (typ) {
			case F_DBF: sprintf(tp, out? ",%s" : "%s", "DBF"); tp += strlen(tp); out = 1; break;
			case F_TBL: sprintf(tp, out? ",%s" : "%s", "TBL"); tp += strlen(tp); out = 1; break;
			case F_IDX: sprintf(tp, out? ",%s" : "%s", "IDX"); tp += strlen(tp); out = 1; break;
			case F_NDX: sprintf(tp, out? ",%s" : "%s", "NDX"); tp += strlen(tp); out = 1; break;
			case F_DBT: sprintf(tp, out? ",%s" : "%s", "DBT"); tp += strlen(tp); out = 1; break;
			case F_MEM: sprintf(tp, out? ",%s" : "%s", "MEM"); tp += strlen(tp); out = 1; break;
			case F_TXT: sprintf(tp, out? ",%s" : "%s", "TXT"); tp += strlen(tp); out = 1; break;
			case F_PRG: sprintf(tp, out? ",%s" : "%s", "PRG"); tp += strlen(tp); out = 1; break;
			case F_PRO: sprintf(tp, out? ",%s" : "%s", "PRO"); tp += strlen(tp); out = 1; break;
			case F_SER: sprintf(tp, out? ",%s" : "%s", "SER"); tp += strlen(tp); out = 1; break;
			case F_DIR: sprintf(tp, out? ",%s" : "%s", "DIR"); tp += strlen(tp); out = 1; break;
			default: break;
			}
		}
		v->valspec.typ = CHARCTR;
		v->valspec.len = strlen(v->buf);
		prnt_val(v, '\n');
		prnt_str("    Name             Type          Size    Date      Time  Attr\n");
		prnt_str("   ----------------- ----- ------------- ---------- ------ ----\n");
		for (i=0,tsize=0L; dp=get_dirent(!i, F_ALL & type); ++i) {
			fmt = read_ftime(dp + DIRENTMTIME);
			size = 0;
			struct tm *mt = localtime(&fmt);
			int hour = mt->tm_hour;
			int pm = hour >= 12;
			if (pm) hour -= 12;
			if (!hour) hour = 12;
			if (dp[DIRENTTYPE]) 
				sprintf(v->buf, "    %-*.*s <DIR> ", FNLEN, FNLEN, dp + DIRENTNAME);
			else 
				sprintf(v->buf, "    %-*.*s  %-*.*s ", FNLEN, FNLEN, dp + DIRENTNAME, EXTLEN, EXTLEN, dp + DIRENTEXT);
			tp = (char *)&v->buf[strlen(v->buf)];
			if (dp[DIRENTTYPE]) strcpy(tp, "            "); //directory
			else sprintf(tp, " %12ld", size = read_fsize(dp + DIRENTSIZ));
			tsize += size;
			tp += strlen(tp);
			sprintf(tp, "  %02d/%02d/%02d %2d:%02d%c  %s", mt->tm_mon + 1, mt->tm_mday, mt->tm_year + 1900, hour,
				mt->tm_min, pm? 'p':'a', dp[DIRENTPERM] == 1? "R-":"RW");
			v->valspec.len = strlen(v->buf);
			prnt_val(v, '\n');
		}
		tp = (char *)v->buf;
		sprintf(tp, "    %d entr%s", i, i > 1? "ies" : "y");
		if (tsize) sprintf(tp + strlen(tp), ", %ld byte%s total size", tsize, tsize>1? "s":"");
		v->valspec.len = strlen(v->buf);
		prnt_val(v, '\n');
	}
}

static void dbcreate(int argc)
{
	usr_regs.ex = argc;
	_usr( USR_CREAT);
}

int getaccess(char *name, int type)
{
	if (!usr_conn) return(HBALL);	/** default grant all access **/
	usr_regs.ax = USR_GETACCESS;
	usr_regs.bx = name;
	usr_regs.cx = strlen(name) + 1;
	usr_regs.ex = type;
	if (usr_tli()) return(HBALL);
	return(usr_regs.ax);
}

static void hbx_(void)
{
	int result = 0;
	FILE *pfp;
	char cmd[STRBUFSZ], data[STRBUFSZ], *tp;
	HBVAL *v = pop_stack(CHARCTR);
	sprintf(cmd, "%.*s", v->valspec.len, v->buf);
	pfp = popen(cmd, "r");
	if (!pfp) goto out;
	if (!pfp) goto out;
	if (!fgets(data, STRBUFSZ, pfp)) goto out;
	tp = data + strlen(data) - 1;
	if (*tp == '\n') *tp = '\0';
	if (strcmp(data, "SUCCESS") && strcmp(data, "FAIL")) goto out;
	push_string(data, strlen(data));
	push_string("hbx_result", 10);
	vstore(0xff);
	while (fgets(data, STRBUFSZ, pfp)) {
		tp = data + strlen(data) - 1;
		if (*tp == '\n') *tp = '\0';
		tp = strchr(data, '=');
		if (tp) {
			push_string(tp+1, strlen(tp+1));
			push_string(data, tp - data);
			vstore(0xff);
		}
	}
	result = 1;
out:
	if (pfp) pclose(pfp);
	if (!result) syn_err(BAD_EXP);
}

static void mkdir_(int argc)
{
	int i;
	char dirname[FULFNLEN];
	for (i=0; i<argc; ++i) {
		HBVAL *v = pop_stack(CHARCTR);
		sprintf(dirname, "%s%.*s", defa_path, v->valspec.len, v->buf);
		if (mkdir(dirname, 0775) < 0) db_error(MKDIR_FAILED, dirname);
	}
}

#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>

void delfil(int count)
{
	char filename[FULFNLEN], pattern[FULFNLEN], *tp;
	DIR *dirp = NULL;
	struct dirent *dp;
	int err = 0, found = 0;
	HBVAL *v;
	while (count > 0) {
		v = pop_stack(CHARCTR);
		if (v->buf[0] == SWITCHAR) {
			if (!db3w_admin) sprintf(filename, "%s%.*s", rootpath, v->valspec.len, v->buf);
			else sprintf(filename, "%.*s", v->valspec.len, v->buf);
		}
		else sprintf(filename, "%s%.*s", defa_path, v->valspec.len, v->buf);
		tp = strrchr(filename, SWITCHAR); //get directory from file path
		if (!tp) db_error(BADFILENAME, filename); //should not happen
		*tp = '\0';
		dirp = opendir(tp == filename? "/" : filename);
		if (!dirp) db_error(BADFILENAME, filename);
		*tp++ = '/';
		strcpy(pattern, tp);
		while ((dp = readdir(dirp))) {
			struct stat f_stat;
			if (strlen(dp->d_name) >= (FULFNLEN - (int)(tp - filename))) continue;
			strcpy(tp, dp->d_name);
			stat(filename, &f_stat);
			if (S_ISDIR(f_stat.st_mode)) continue;
			if (!fnmatch(pattern, dp->d_name, FNM_PATHNAME | FNM_PERIOD)) {
				found = 1;
				if (!db3w_admin && !(f_stat.st_mode & S_IWUSR)) {
					err = DB_ACCESS;
					goto end;
				}
				if (checkopen(filename)) {
					err = OPENFILE;
					goto end;
				}
				if (db3w_admin) seteuid(0);
				if (unlink(filename)) {
					err = DELFIL_FAILED;
					goto end;
				}
			}
		}
		closedir(dirp);
		dirp = NULL;
		count--;
	}
end:
	if (!found) err = FILENOTEXIST;
	if (db3w_admin) seteuid(hb_euid);
	if (dirp) closedir(dirp);
	if (err) db_error(err, filename);
}

#include <grp.h>
#include <pwd.h>

void renfil(int argc)
{
	char fromfn[FULFNLEN], tofn[FULFNLEN], *tp = NULL;
	HBVAL *v;
	int uid = -1, gid = -1, err = 0;
	if (argc < 2 || argc > 3) db_error0(MISSING_PARM);
	if (argc > 2) {
		v = pop_stack(CHARCTR);
		if (db3w_admin) { //ignored for non-admin
			char user[256], *grp;
			snprintf(user, sizeof(user), "%.*s", v->valspec.len, v->buf);
			if (grp = strchr(user, '.')) {
				*grp++ = '\0';
				struct group *g = getgrnam(grp);
				if (!g) db_error(INVALID_GRP, grp);
				gid = g->gr_gid;
			}
			struct passwd *pwd = getpwnam(user);
			if (!pwd) db_error(INVALID_USER, user);
			uid = pwd->pw_uid;
		}
	}
	v = pop_stack(CHARCTR);
	if (v->buf[0] == SWITCHAR) {
		if (!db3w_admin) sprintf(tofn, "%s%.*s", rootpath, v->valspec.len, v->buf);
		else sprintf(tofn, "%.*s", v->valspec.len, v->buf);
	}
	else
		sprintf(tofn, "%s%.*s", defa_path, v->valspec.len, v->buf);
	if (checkopen(tofn)) db_error(OPENFILE, tofn);
	if (fileexist(tofn)) db_error(FILEEXIST, tofn);
	v = pop_stack(CHARCTR);
	if (v->buf[0] == SWITCHAR) {
		if (!db3w_admin) sprintf(fromfn, "%s%.*s", rootpath, v->valspec.len, v->buf);
		else sprintf(fromfn, "%.*s", v->valspec.len, v->buf);
	}
	else
		sprintf(fromfn, "%s%.*s", defa_path, v->valspec.len, v->buf);
	if (checkopen(fromfn)) db_error(OPENFILE, fromfn);
	if (!fileexist(fromfn)) db_error(FILENOTEXIST, fromfn);
	if (!db3w_admin && (access(fromfn, W_OK) || uid != -1 || gid != -1)) db_error(DB_ACCESS, fromfn);
	//if uid != -1 or gid != -1, need to be admin. if both -1 it is noop
	if (db3w_admin) seteuid(0);
	tp = strrchr(tofn, SWITCHAR);
	if (tp) *tp = '\0';
	if (access(tp == tofn? ROOT_DIR : tofn, W_OK) < 0) {
		if (errno == EACCES) err = DB_ACCESS;
		else if (errno == ENOENT) err = INVALID_PATH;
		else err = RENAME_FAILED;
		goto end;
	}
	*tp = SWITCHAR; tp = NULL;
	if (rename(fromfn, tofn) < 0 || chown(tofn, uid, gid) < 0) err = RENAME_FAILED;
	sync();
end:
	if (tp) *tp = SWITCHAR;
	if (db3w_admin) seteuid(hb_euid);
	if (err) db_error(err, tofn);
}

#include <sys/stat.h>

static void deldir(int count)
{
	char filename[FULFNLEN];
	HBVAL *v;
	while (count > 0) {
		struct stat st;
		v = pop_stack(CHARCTR);
		sprintf(filename, "%s%.*s", defa_path, v->valspec.len, v->buf);
		if (stat(filename, &st)) db_error(DELDIR_FAILED, filename);
		if (!S_ISDIR(st.st_mode)) db_error(NOTDIR, filename);
		if (rmdir(filename)) db_error(DELDIR_FAILED, filename);
		count--;
	}
}

#include <pwd.h>

static void su_(int name)
{
	HBVAL *v;
	char user[LEN_USER + 1], *r;
	struct passwd *pwe;
	static save_admin = 0;
	if (name != OPND_ON_STACK) push_opnd(name);
	v = pop_stack(CHARCTR);
	sprintf(user, "%.*s", v->valspec.len, v->buf);
	if (!db3w_admin && !save_admin) db_error(DB_ACCESS, user);
	save_admin = 1;
	pwe = getpwnam(user);
	if (!pwe) sys_err(LOGIN_NOUSER);
	if (pwe->pw_uid == 0) db_error(DB_ACCESS, user); //do not allow su to root
	strcpy(hb_user, user);
	if (seteuid(0) || seteuid(pwe->pw_uid)) sys_err(LOGIN_SUID);
	hb_euid = pwe->pw_uid;
	//we are admin so no chroot and rootpath is the actual path
	strcpy(rootpath, pwe->pw_dir);
	sprintf(defa_path, "%s/", pwe->pw_dir);
	db3w_admin = hb_euid == hb_uid; //hb_uid is the admin uid otherwise it won't get here
}

static void export_(int argc)
{
	usr_regs.ex = argc;
	_usr(USR_EXPORT);
}

static void sleep_(int opnd)
{
	int nsec = pop_int(0, 100);
	sleep(nsec);
}

static void apndfil(int n)
{
	char txtfn[FULFNLEN], line[STRBUFSZ];
	FILE *fp = NULL;
	HBVAL *v;
	int i, err = 0;
	v = stack_peek(n - 1);
	if (v->buf[0] == SWITCHAR) {
		if (!db3w_admin) sprintf(txtfn, "%s%.*s", rootpath, v->valspec.len, v->buf);
		else sprintf(txtfn, "%.*s", v->valspec.len, v->buf);
	}
	else sprintf(txtfn, "%s%.*s", defa_path, v->valspec.len, v->buf);
	fp = fopen(txtfn, "a");
	if (!fp) db_error(DBOPEN, txtfn);
	for (i=n-1;  i> 0; i--) {
		v = stack_peek(i - 1);
		snprintf(line, STRBUFSZ, "%.*s", v->valspec.len, v->buf);
		if (fprintf(fp, "%s\n", line) < 0) {
			err = 1;
			goto end;
		}
	}
end:
	if (fp) fclose(fp);
	while (n--) pop_stack(DONTCARE);
	if (err) db_error(DBWRITE, txtfn);
}

static void dispndx(int n)
{
	int i;
	valid_chk();
	for (i=0; i<MAXNDX; ++i) {
		char *ndxfn = table_ctx[i].ndxfctl[i].filename;
		if (!ndxfn) break;
		push_string(ndxfn, strlen(ndxfn));
		usr_regs.ex = i;
		if (i == table_ctx[i].order) usr_regs.ex |= 0x80;
		_usr(USR_DISPNDX);
	}
}

static void transac(int mode)
{
	HBVAL *v = pop_stack(CHARCTR);
	char tid[STRBUFSZ];
	sprintf(tid, "%.*s", v->valspec.len, v->buf);
	switch (mode) {
	case TRANSAC_START:
		transaction_start(tid);
		break;
	case TRANSAC_COMMIT:
		transaction_commit(tid);
		break;
	case TRANSAC_ROLLBACK:
		transaction_rollback(tid);
		break;
	case TRANSAC_CANCEL:
		transaction_cancel(tid);
		break;
	default:
		break;
	}
}

void prnt_str(char *fmt, ...)
{
	char vbuf[STRBUFSZ];
	HBVAL *v = (HBVAL *)vbuf;
	va_list ap;
	va_start(ap, fmt);
	v->valspec.typ = CHARCTR;
	vsnprintf(v->buf, STRBUFSZ - 2, fmt, ap);
	v->valspec.len = strlen(v->buf);
	prnt_val(v, 0);
}

extern char datespec[];

static void dispstat(int mode)
{
	int i, my_ctx = get_ctx_id(), fcount = 0;
	prnt_str("     Root Path: %s%c\n", rootpath, SWITCHAR);
	prnt_str("  Default Path: %s\n", defa_path);
	for (i=0; i<MAXCTX; ++i) if (_chkdb(i) != CLOSED) ++fcount;
	prnt_str("   Open Tables: %d\n", fcount);
	for (i=0; i<MAXCTX; ++i) {
		int j;
		ctx_select(i);
		if (chkdb() != CLOSED) {
			++fcount;
			prnt_str("\nContext ID: %d\n", i + 1);
			prnt_str("    Table Path: %s\n       Indexes: ", curr_ctx->dbfctl.filename);
			for (j=0; j<MAXNDX; ++j) {
				if (_chkndx(get_ctx_id(), j) == CLOSED) break;
				prnt_str("%c%s%s", j? ',' : ' ', j == curr_ctx->order? "*" : "", curr_ctx->ndxfctl[j].filename);
			}
			if (!j) prnt_str("<None>");
			prnt_str("\n%14s: %lu\n", "@Record", curr_ctx->crntrec);
			if (filterisset()) prnt_str("Filter Expression: %s\n", curr_ctx->filter);
			if (curr_ctx->locate) prnt_str("Locate Expression: %s\n", curr_ctx->locate);
			if (curr_ctx->rela != -1) {
				prnt_str("Relation Context ID: %d\n", curr_ctx->rela + 1);
				prnt_str("Relation Expression: %s\n", curr_ctx->relation);
			}
			prnt_str("%14s:\n", "Flags");
			prnt_str("%14s: %s\n", "EXCLUSIVE", flagset(exclusive)? "Yes" : "No");
			prnt_str("%14s: %s\n", "READONLY", flagset(readonly)? "Yes" : "No");
			prnt_str("%14s: %s\n", "DENYWRITE", flagset(denywrite)? "Yes" : "No");
			prnt_str("%14s: %s\n", "EOF", flagset(e_o_f)? "Yes" : "No");
			prnt_str("%14s: %s\n", "BOF", flagset(t_o_f)? "Yes" : "No");
			prnt_str("%14s: %s\n", "FOUND", flagset(rec_found)? "Yes" : "No");
			prnt_str("%14s: %s\n", "Modified", flagset(dbmdfy)? "Yes" : "No");
		}
	}
	prnt_str("\nGlobal Parameters:\n");
	prnt_str("%14s: %s\n", "DATE", datespec);
	prnt_str("%14s: %d\n", "WIDTH", nwidth);
	prnt_str("%14s: %d\n", "DECIMAL", ndeci);
	//prnt_str("%14s: %s\n", "CENTURY", isset(CENTURY_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "DFPI", isset(DFPI_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "DELETE", isset(DELETE_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "EXACT", isset(EXACT_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "UNIQUE", isset(UNIQUE_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "DEBUG", isset(DEBUG_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "EXCLUSIVE", isset(EXCL_ON)? "ON" : "OFF");
	prnt_str("%14s: %s\n", "READONLY", isset(READONLY_ON) == S_AUTO? "AUTO" : (isset(READONLY_ON)? "ON" : "OFF"));
	prnt_str("%14s: %s\n", "NULL", isset(NULL_ON)? "ON" : "OFF");
	ctx_select(my_ctx);
}

static void serial_(int mode)
{
	char sername[FNLEN+8];
	HBVAL *v;
	long serial = 0;
	if (mode == 2) serial = pop_int(0, -1);
	else if (mode == 1) serial = -1;
	v = pop_stack(CHARCTR);
	if (v->valspec.len < 1 || v->valspec.len > FNLEN) syn_err(INVALID_VALUE);
	sprintf(sername, "%.*s", v->valspec.len, v->buf);
	hb_set_serial(sername, serial);
}

void (*hbint[MAXPCODE - PBRANCH])() = {
nop,		push_opnd,	uop,		bop,		retriev,	vstore,		parameter,	fcn_call, 
macro,		macro_fulfn,macro_fn,	macro_alias,macro_id,	nop,		nop,		nop, 
prnt,		use,		select_,	go_,		skip_,		locate_,	disprec,	del_recall_, 
replace,	store_rec_,	_apblank,	_set_onoff,	set_to,		_index,		reindex,	scope_f, 
r_cnt,		inp_,		find,		private,	release,	dispstru,	dispmemo,	form_read,
public, 	typechk,	push_logic,	push_n,		_reset,		quit,		dretrv,		arraychk,	
arraydef,	addr,		dbpack,		dir_list,	apinit,		aprec,		apnext,		apdone,		
cpinit, 	cprec,		cpnext,		cpdone, 	dbcreate, 	updinit,	updrec,		updnext,	
upddone, 	mkdir_,		dispfil, 	delfil,		renfil,		deldir,		su_, 		export_,	
sleep_,		apndfil,	dispndx,	transac,	dispstat,	scope_b, 	serial_, 	nop, 		
usrldd,		_usr, 		usrfcn, 	do_proc,	hbx_
};
