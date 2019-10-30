/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/var.c
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
#include <memory.h>
#include <string.h>
#include <setjmp.h>
#include "hb.h"
#include "dbe.h"

/* old constants, reference only
#define VARTABSZ     384
#define HASHTABSZ    256
#define HASHCONST    32
#define DCLNTABSZ    512
#define VALBUFSZ     0x2000
#define MAXLEVEL     16
*/
#define VARTABSZ     16384
#define HASHTABSZ    4096
#define HASHCONST    47
#define DCLNTABSZ    65536
#define VALBUFSZ     1048576
#define MAXLEVEL     32

#define INSERT       1
#define SEARCH       0

#define ON_STACK		0xff

#define is_pattern_match(mode)	(mode & 0xf0)
#define match_count(mode)	(mode & 0x0f)

/*************************** mvar management *********************/
static struct var {
	char name[VNAMELEN + 1];
	int hlink;
	int dec_entry;
} *var_table;		  /** 0th entry : free entry list link **/

static struct dcln {
	int level;		/** proc call level **/
	int var_entry;		/** link back to owner **/
	int link;		/** for same level **/
	int prior;		/** for previous level **/
	HBVAL *value;
} *dec_table;		  /** 0th entry : free entry list link **/

static int *hash_table;

static struct cbuf {
	char *buf;
	char *next;
} val_buf;

static int vlevel[MAXLEVEL];

static int array_de;		/*** dec_entry for array store and retriev ***/

extern int curr_lvl; //procedure call nest level

/**************************** mvar stuff *****************************/

void memvar_free(void);

void var_init(void)
{
	val_buf.next = val_buf.buf;
	{
		int i;
		for (i=0; i<VARTABSZ; ++i) {
			var_table[i].dec_entry = 0;
			var_table[i].hlink = i + 1;
		}
		for (i=0; i<DCLNTABSZ; ++i) {
			dec_table[i].level = 0;
			dec_table[i].var_entry = 0;
			dec_table[i].link = i + 1;
		}
	}
	memset(hash_table, 0, sizeof(int) * HASHTABSZ);
	memset(vlevel, 0, sizeof(int) * MAXLEVEL);

}

int var_init0(void)
{

	var_table = (struct var *)malloc(sizeof(struct var) * VARTABSZ);
	dec_table = (struct dcln *)malloc(sizeof(struct dcln) * DCLNTABSZ);
	if (!(val_buf.buf = malloc(VALBUFSZ))
		|| !(hash_table = (int *)malloc(sizeof(int) * HASHTABSZ))) {
		return(-1);
	}
	var_init();
	return(0);
}

void memvar_free(void)
{
	if (hash_table) free(hash_table);
	if (val_buf.buf) free(val_buf.buf);
	if (dec_table) free(dec_table);
	if (var_table) free(var_table);
}

/* old hash function, not good for large table size
static WORD hash()
{
	HBVAL *v = topstack;
	int len = v->len;
	BYTE *p = v->buf;

	if (len > 2)
		return((to_upper(p[0]) + to_upper(p[1]) + to_upper(p[len-1]) 
				  + to_upper(p[len-2]) + len * HASHCONST) % HASHTABSZ);
	else
		return((to_upper(p[0]) + to_upper(p[len-1]) 
				  + len * HASHCONST) % HASHTABSZ);
}
*/

static int hash(void)
{
	HBVAL *v = topstack;
	if (v->valspec.typ != CHARCTR) syn_err(INVALID_TYPE);

	int len = v->valspec.len;
	BYTE *p = v->buf;

	if (len < 1) syn_err(INVALID_VALUE);
	if (len > 2)
		return((HASHCONST * to_upper(p[0]) + 2 * HASHCONST * to_upper(p[1]) + len * to_upper(p[len - 1]) 
				  + (len - 1) * to_upper(p[len - 2]) + HASHCONST) % HASHTABSZ);
	else
		return((HASHCONST * to_upper(p[0]) + len * to_upper(p[len - 1]) 
				  + HASHCONST) % HASHTABSZ);
}

static HBVAL *add_val(HBVAL *v)
{
	int len;
	char *r;

	len = val_size(v);
	if ((int)(val_buf.next - val_buf.buf) + len > VALBUFSZ)
		return((HBVAL *)NULL);
	memcpy(r = val_buf.next, v, len);
	val_buf.next += len;
	return((HBVAL *)r);
}

static void del_val(HBVAL *v)
{
	if (v) {
		int i,j;
		int len = val_size(v);
		for (i=0; i<=curr_lvl; ++i) {
			for (j=vlevel[i]; j; j=dec_table[j].link) if (dec_table[j].value > v) {
				char *p = (char *)dec_table[j].value;
				dec_table[j].value = (HBVAL *)(p - len);
			}
		}
		if (val_buf.next - len < val_buf.buf) sys_err(INTERN_ERR);
		val_buf.next -= len;
		memcpy(v, (char *)v + len, val_buf.next - (char *)v);
	}
}

static int hlink(int mode, int *e)
{
	HBVAL *v = topstack;
	if (v->valspec.typ != CHARCTR) syn_err(INVALID_TYPE);
	if (var_table[*e].dec_entry) {
		if (namecmp(var_table[*e].name, v->buf, v->valspec.len, ID_VNAME))
			return(hlink(mode, &var_table[*e].hlink));
	}
	else {
		if (mode == SEARCH) return(0);
		if ((*e = var_table[0].hlink) == VARTABSZ) sys_err(VARTBL_OVERFLOW);
		var_table[0].hlink = var_table[*e].hlink;	 /** link up free entries **/
		var_table[*e].hlink = 0;
		{
			char *p = var_table[*e].name;
			BYTE *q = v->buf, c;
			int len = v->valspec.len;
			while (len--) {
				c = *q++;
				*p++ = to_upper(c);
				*p = '\0';
			}
		}
	}
	return(*e);
}

static int varsrin(int mode)
{
	return(hlink(mode, &hash_table[hash()]));
}

static void vclr_stack(int mode)
{
	if (is_pattern_match(mode)) {
		if (mode & R_EXCEPT) pop_stack(DONTCARE);
		if (mode & R_LIKE) pop_stack(DONTCARE);
	}
	else while (mode--) pop_stack(DONTCARE);
}

static void _vretrv(int array)		/*** array access flag **/
{
	struct dcln *avar;
	HBVAL *v;
	int de;

	if (array_de) {
		de = array_de;
		array_de = 0;
	}
	else {
		int e = varsrin(SEARCH);
		if (!e) syn_err(UNDEF_VAR);
		pop_stack(DONTCARE);
		de = var_table[e].dec_entry;
	}
	avar = &dec_table[de];
	if (avar->level) syn_err(UNDEF_VAR);
	if (!(v = avar->value)) syn_err(NULL_VALUE);
	if (array) {
		HBVAL *sp;
		int dim,i;
		BYTE *a;
		int nele = 1;
		if (v->valspec.typ != ARRAYTYP) syn_err(NOTARRAY);
		sp = topstack;
		dim = get_dword(sp->buf);
		if (dim != v->valspec.len / sizeof(WORD)) syn_err(BAD_DIM);
		a = v->buf;
		for (i=0; i<dim; ++i) {
			int temp = 1;
			int sdim;
			int j;
			int le;
			sp = stack_peek(dim-i);
			le = get_dword(sp->buf);
			if ((le <= 0) || (le > MAXAELE)) syn_err(BAD_DIM);
			sdim = le - 1;
			if (sdim > get_word(a + i * sizeof(WORD))) syn_err(BAD_DIM);
			for (j=i+1; j<dim; ++j) temp *= get_word(a + j * sizeof(WORD));
			nele += sdim * temp;
		}
		while (nele--) de = dec_table[de].link;
		array_de = de;
		vclr_stack(dim + 1);
	}
	else {
		if (v->valspec.typ == ARRAYTYP) syn_err(MISSING_DIM);
		while (v->valspec.typ == ADDR) v = dec_table[get_word(v->buf)].value;
		push(v);
	}
}

void v_retrv(void)
{
	_vretrv(0);
}

void arraychk(int aname)
{
	push_opnd(aname);
	_vretrv(1);
}

static int get_next_dec(int lvl, int ve)
{
	int e;
	struct dcln *dec;

	if ((e = dec_table[0].link) == DCLNTABSZ) return(0);
	dec = &dec_table[e];
	dec->prior = lvl ? -1 : 0; //-1 will be correctly set if needs to
	dec->var_entry = ve;
	dec->value = (HBVAL *)NULL;
	dec_table[0].link = dec->link;
	dec->link = vlevel[lvl];
	/** link up with other vars of same level **/
	vlevel[lvl] = e;
	return(e);
}

void _vstore(int public)
{
	int de;
	struct var *v;
	HBVAL *s,*t;
	int e;

	if (array_de) {
		de = array_de;
		array_de = 0;
		t = pop_stack(DONTCARE);
	}		
	else {
		e = varsrin(INSERT);
		pop_stack(DONTCARE);
		t = pop_stack(DONTCARE);
		v = &var_table[e];
		de = v->dec_entry;
		if (!de) {
			de = v->dec_entry = get_next_dec(public ? 0 : curr_lvl, e);
			if (!de) syn_err(DCTBL_OVERFLOW);
		}
		else {
			if (dec_table[de].level) {
				int prv_entry = de;
				if (public) 
					syn_err(VAR_LOCAL);
				de = v->dec_entry = get_next_dec(curr_lvl, e);
				dec_table[de].prior = prv_entry;
			}
			else {
				if (public && dec_table[de].prior > 0) 
					syn_err(VAR_LOCAL);
				if (t->valspec.typ == ARRAYTYP) syn_err(NAMECONFLICT);
				while (dec_table[de].value->valspec.typ == ADDR) de = get_word(dec_table[de].value->buf);
				s = dec_table[de].value;
				if (s->valspec.typ == ARRAYTYP) syn_err(MISSING_DIM);
				del_val(s);
				dec_table[de].value = (HBVAL *)NULL;
			}
		}
	}
	s = add_val(t);
	if (!s) sys_err(MEMBUF_OVERFLOW);
	if (s->valspec.typ == ARRAYTYP) {
		int dim = s->valspec.len / 2;
		int nele = 1;
		BYTE *a = s->buf;
		while (dim--) {
			nele *= get_word(a);
			a += sizeof(WORD);
		}
		while (nele--)
			if (!(de = get_next_dec(public ? 0 : curr_lvl, e))) syn_err(DCTBL_OVERFLOW);
		v->dec_entry = de;
	}
	dec_table[de].value = s;
	if (!public && isset(TALK_ON)) prnt_val(s,'\n');
}

void vstore(int name)
{
	if (name != ON_STACK) push_opnd(name);
	_vstore(0);
}

static int vmatch(int mode, char *name)
{
	HBVAL *v = (HBVAL *)NULL;
	if (is_pattern_match(mode)) {
		/** be careful about stack sequence **/
		if (mode & R_EXCEPT) {
			v = topstack;
			if (!namecmp(name, v->buf, v->valspec.len, ID_PARTIAL)) return(0);
		}
		if (mode & R_LIKE) {
			v = v ? stack_peek(1) : topstack;
			return(!namecmp(name, v->buf, v->valspec.len, ID_PARTIAL));
		}
		return(1);
	}
	else {
		int i;
		int cnt = match_count(mode);	  /** 15 names max **/
		for (i=0; i<cnt; ++i) {
			v = stack_peek(i);
			if (!namecmp(name, v->buf, v->valspec.len, ID_VNAME)) return(1);
		}
		return(0);
	}
}

void release(int mode)
{
	int *ip;
	struct var *v;
	struct dcln *dec;
	for (ip=&vlevel[curr_lvl]; *ip;) {
		int e = *ip;
		int ve;
		dec = &dec_table[e];
		v = &var_table[ve = dec->var_entry];
		if (vmatch(mode, v->name)) {
			v->dec_entry = dec->prior;
			if (v->dec_entry == -1) v->dec_entry = 0; //don't have any prior
			if (!v->dec_entry) {		 /** link up to the free chain **/
				v->hlink = var_table[0].hlink;
				var_table[0].hlink = dec->var_entry;
			}
			do {
				del_val(dec->value);
				dec->var_entry = 0;
				*ip = dec->link;
				dec->link = dec_table[0].link;
				dec_table[0].link = e;
				dec = &dec_table[e = *ip];
			} while (dec->var_entry == ve);
		}
		else ip = &dec->link;
	}
	if (mode & R_PROC) {		 /** curr_lvl must be > 0 **/
		int i;
		if (curr_lvl>1) for (i=vlevel[curr_lvl-1]; i;) {
			dec_table[i].level = 0;
			i = dec_table[i].link;
		}
		for (i=vlevel[0]; i;) {
			dec = &dec_table[i];
			if (dec->level == curr_lvl) dec->level = 0;
			i = dec->link;
		}
	}
	else vclr_stack(mode);
}

void public(int name)
{
	char null[3];
	HBVAL *v = (HBVAL *)null;
	v->valspec.typ = UNDEF;
	v->valspec.len = 0;
	if (name == ON_STACK) v = topstack;
	push((HBVAL *)null);
	if (name == ON_STACK) push(v);
	else push_opnd(name);
	_vstore(1);
	if (name == ON_STACK) pop_stack(DONTCARE);
}

void private(int mode)
{
	int i;
	if (!curr_lvl) syn_err(NO_PRIVATE);
	for (i=0; i<curr_lvl; ++i) {
		int j;
		for (j=vlevel[i]; j; j=dec_table[j].link) {
			struct dcln *dec = &dec_table[j];
			if (vmatch(mode, var_table[dec->var_entry].name)) 
				if (!dec->level) dec->level = curr_lvl;
		}
	}
	vclr_stack(mode);
}

void addr(int vname)
{
	int e;
	char temp[sizeof(WORD) + 2];
	HBVAL *v = (HBVAL *)temp;
	v->valspec.typ = ADDR;
	v->valspec.len = sizeof(WORD);
	if (vname != ON_STACK) {
		push_opnd(vname);
		e = varsrin(SEARCH);
		if (!e) syn_err(UNDEF_VAR);
		pop_stack(DONTCARE);
		e = var_table[e].dec_entry;
		if (dec_table[e].level) syn_err(UNDEF_VAR); /** hidden **/
		put_word(v->buf, e);
	}
	else {
		write_short(v->buf, array_de); /** must be an array element **/
		array_de = 0;
	}
	push(v);
}

void arraydef(int aname)
{
	char vbuf[STRBUFSZ];
	HBVAL *array = (HBVAL *)vbuf, *v;
	int public, dim, i;
	BYTE *a;
	int nele,tele;

	push_opnd(aname);
	v = stack_peek(1);
	dim = get_long(v->buf);
	public = 0x80 & dim;
	dim &= 0x7f;
	if (dim > MAXADIM) syn_err(TOOMANY_DIM);
	a = array->buf;
	tele = 1;
	for (i=0; i<dim; ++i) {
		v = stack_peek(dim - i + 1);
		if (v->valspec.typ != NUMBER) syn_err(INVALID_TYPE);
		if (val_is_real(v)) syn_err(INVALID_VALUE);
		nele = get_long(v->buf);
		if (nele <= 0) syn_err(BAD_DIM);
		if ((tele *= nele) > MAXAELE) syn_err(ARRAY_TOOBIG);
		put_word(a, nele);
		a += sizeof(WORD);
	}
	array->valspec.typ = ARRAYTYP;
	array->valspec.len = dim * sizeof(WORD);
	push(array);
	push(stack_peek(1));
	_vstore(public);
	vclr_stack(dim + 2);
}

static int vtype(HBVAL *v)
{
	if (v) switch (v->valspec.typ) {
		case NUMBER:
			return('N');

		case CHARCTR:
			return('C');

		case DATE:
			return('D');

		case LOGIC:
			return('L');

		case ADDR:
			return('@');

		case ARRAYTYP:
			return('A');

		default:
			return('U');
	}
	return('U');
}

void dispmemo(void)
{
	int i, j, cnt = 0;
	char vbuf[STRBUFSZ];
	HBVAL *str = (HBVAL *)vbuf;
	prnt_str(" @  %-*s Lvl Stat Typ Value\n", VNAMELEN, "Name");
	memset(str->buf, '-' ,VNAMELEN + 32);
	str->buf[3] = str->buf[VNAMELEN+4] = str->buf[VNAMELEN+4+4] = str->buf[VNAMELEN+4+4+5] = str->buf[VNAMELEN+4+4+5+4] = ' ';
	str->valspec.typ = CHARCTR;
	str->valspec.len = VNAMELEN+32;
	prnt_val(str, '\n');
	for (i=0; i<=curr_lvl; ++i)
		for (j=vlevel[i]; j;) {
			struct dcln *dec = &dec_table[j];
			HBVAL *v = dec->value;
			prnt_str("%03d %-*s %2d   %c    %c  ", j, VNAMELEN,
					  var_table[dec->var_entry].name,
					  i, dec->level ? 'H' : 'A', vtype(v));
			/*
			if (v) while (v->typ == ADDR) {
				dec = &dec_table[read_short(v->buf)];
				v = dec->value;
			}
			*/
			if (v) prnt_val(v,'\n');
			else {
				prnt_str("<Not Used>\n");
			}
			j = dec->link;
			++cnt;
		}
	prnt_str("*** Entries available : %d\n", DCLNTABSZ - cnt);
	prnt_str("*** Memory available  : %04d\n", (int)(val_buf.buf + VALBUFSZ - val_buf.next));
}

void save_var_ctx(ctxfp)
FILE *ctxfp;
{
}
