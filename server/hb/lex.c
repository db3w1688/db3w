/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/lex.c
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
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include "hb.h"
#include "hbkwdef.h"
#include "dbe.h"

/************************************************************************/

void save_token(TOKEN *save_tok)
{
	save_tok->tok_typ = curr_tok;
	save_tok->tok_s = currtok_s;
	save_tok->tok_len = currtok_len;
}

void rest_token(TOKEN *save_tok)
{
	curr_tok = save_tok->tok_typ;
	currtok_s = save_tok->tok_s;
	currtok_len = save_tok->tok_len;
}

int getvarname(int start)
{
	char c;
	int len = 0;
	do {
	  ++len; c = curr_cmd[++start];
	  if (c == '&') {
		 int maclen = getmac(start);
		 if (!exe) {
			curr_tok = TMAC;
			start += maclen;  len += maclen;
		 }
		 c = curr_cmd[start];
	  }
	} while (isalpha(c) || isdigit(c) || (c == '_'));
	return(len);
}

int getmac(int start)
{
	char *p = &curr_cmd[++start];
	int len;

	if (*p == '&') return(getmac(start));
	if (isalpha(*p) || *p=='_') {
		len = getvarname(start);
		if (exe) {
			TOKEN tk;
			save_token(&tk);
			currtok_s = start;  currtok_len = len;
			push_opnd(nextdata = loadstring());
			v_retrv();
			rest_token(&tk);
		}
	}
	else syn_err(ILL_SYMBOL);
	if (!exe) return(len + 1);	/* dont forget the & char */
	else {
		HBVAL *v = pop_stack(CHARCTR);
		char rest[STRBUFSZ];
		--p; --start;
		/*
		movmem(p+len+1, p+v->len, STRBUFSZ-start-v->len);
		*/
		strcpy(rest,p+len+1);
		memcpy(p, v->buf, v->valspec.len);
		sprintf(p + v->valspec.len, "%.*s", STRBUFSZ - start - v->valspec.len, rest);
		if (*p == '&') return(getmac(start));
		return(0);
	}
}

int get_tok(void)
{
	char c, *p;
	int cmd_changed = 0;
	save_token(&last_tok);
	curr_tok = TNULLTOK;
	p = &curr_cmd[currtok_s += currtok_len];
	while (*p && isspace(*p)) { ++p; ++(currtok_s); }
	currtok_len = 0;  c = *p;
	if (c == EOF_MKR) {
		curr_tok = TEOF;
		return;
	}
	if ((c == '\'') || (c == '"')) {
		char q = c;
		do { ++currtok_len; c = *(++p); } while (c && (c != q));
		if (!c) syn_err(UNBAL_QUOTE);
		++currtok_len;
		curr_tok = TLIT;
		return;
	}
	if (c == ',') {
		++currtok_len;
		curr_tok = TCOMMA;
		return;
	}
	if (c == '(') {
		++currtok_len;
		curr_tok = TLPAREN;
		return;
	}
	if (c == '{') c = '\0';		/*  comment  */
	while (c && !(isspace(c) || c==',' || c=='(')) {
		if (c == '&') {
			currtok_len += getmac(currtok_s + currtok_len);
			if (!exe) {
				curr_tok = TMAC;
				p = &curr_cmd[currtok_s + currtok_len];
			}
			else cmd_changed = 1; //if getmac() successfully returns
		}
		else { ++p;  ++currtok_len; }
		c = *p;
	}
	if (currtok_len && curr_tok==TNULLTOK) curr_tok = TTOK;
	return(cmd_changed);
}

void get_token(void)
{
	char c, *p;
	get_tok();
	if (curr_tok != TTOK) return;
	p = &curr_cmd[currtok_s];
	c = *p; currtok_len = 0;
	if (isalpha(c) || c=='_') {
	  currtok_len = getvarname(currtok_s);
	  if (currtok_len > VNAMELEN) syn_err(BAD_VARNAME);
	  curr_tok = TID;
	  return;
	}
	else if (isdigit(c)) {
	  do { ++currtok_len; c = *(++p); } while (isdigit(c));
	  curr_tok = TINT;
	}
	if (c == '.') {
	  if (isdigit(*(p + 1))) {
		 do { ++currtok_len; c = *(++p); } while (isdigit(c));
		 if ((c == 'e') || (c == 'E')) {
			++currtok_len; c = *(++p);
			if ((c == '+') || (c == '-')) { ++currtok_len; c = *(++p); }
			if (!isdigit(c)) syn_err(ILL_SYMBOL);
			do { ++currtok_len; c = *(++p); } while (isdigit(c));
		 }
		 curr_tok = TNUMBER;
		 return;
	  }
	  else {
		 if (currtok_len) return;
		 ++currtok_len; c = to_upper(*(++p));
		 if (((c == 'T') || (c == 'Y') || (c == 'F') || (c == 'N'))
			 && (*(p+1) == '.')) {
			curr_tok = TLOGIC;
			currtok_len = 3;
			return;
		 }
		 else if ((c == 'A') && (to_upper(*(p + 1)) == 'N') && (to_upper(*(p+2)) == 'D')
			 && (*(p+3) == '.')) {
			curr_tok = TAND;
			currtok_len = 5;
			return;
		 }
		 else if ((c == 'O') && (to_upper(*(p + 1)) == 'R') && (*(p+2) == '.')) {
			curr_tok = TOR;
			currtok_len = 4;
			return;
		 }
		 else if ((c == 'N') && (to_upper(*(p + 1)) == 'O') && (to_upper(*(p+2)) == 'T')
			 && (*(p+3) == '.')) {
			curr_tok = TNOT;
			currtok_len = 5;
			return;
		 }
		 else {
		 	curr_tok = TPERIOD;
		 	return; //currtok_len == 1
		}
	  }
	}
	if (curr_tok == TINT) return;
	switch (c) {
	  case '?':
		 curr_tok = TQUESTN;
		 break;
	  case '@':
		 curr_tok = TAT;
		 break;
	  case ';':
		 curr_tok = TSEMICOLON;
		 break;
	  case '(':
		 curr_tok = TLPAREN;
		 break;
	  case ')':
		 curr_tok = TRPAREN;
		 break;
	  case '+':
		 curr_tok = TPLUS;
		 break;
	  case '-':
		 if (*(p+1) == '>') { ++currtok_len; curr_tok = TARROW; }
		 else curr_tok = TMINUS;
		 break;
	  case '*':
		 if (*(p+1) == '*') { ++currtok_len; curr_tok = TPOWER; }
		 else curr_tok = TMULT;
		 break;
	  case '^':
		 curr_tok = TPOWER;
		 break;
	  case '/':
		 curr_tok = TDIV;
		 break;
	  case '=':
		 curr_tok = TEQUAL;
		 break;
	  case '>':
		 if (*(p+1) == '=') { ++currtok_len; curr_tok = TGTREQU; }
		 else curr_tok = TGREATER;
		 break;
	  case '<':
		 c = *(p+1);
		 if (c == '=') { ++currtok_len; curr_tok = TLESSEQU; }
		 else if (c == '>') { ++currtok_len; curr_tok = TNEQU; }
			else curr_tok = TLESS;
		 break;
	  case '[':
		 curr_tok = TLBRKT;
		 break;
	  case ']':
		 curr_tok = TRBRKT;
		 break;
	  case ':':
		 curr_tok = TCOLON;
		 break;
	  case '#':
	  	curr_tok = TPOUND;
	  	break;
	  default:
		 syn_err(ILL_SYMBOL);
	}
	++currtok_len;
}

int get_command(int allow_redir)
{
	get_token();
	if (curr_tok == TEOF) return(EOF);
	if (curr_tok == TNULLTOK) return(NULL_CMD);
	if ((curr_tok == TMULT) || (curr_tok == TPOWER) || (curr_tok == TPOUND)) return(KW_COMMENT);
	if (curr_tok == TQUESTN) return(PRINT_CMD);
	if (curr_tok == TAT && allow_redir) {
		HBVAL *v;
		get_token();
		expression();
		v = pop_stack(CHARCTR);
		sprintf(curr_cmd, "%.*s", v->valspec.len, v->buf);
		curr_tok = TNULLTOK;
		currtok_s = currtok_len = 0;
		return(get_command(0));
	}
	int i = kw_bin_srch(0, KWTABSZ - 1);
	if (i >= 0) {
		HBKW *keyword = &keywords[i];
		if (keyword->cmd_id == HELP_CMD || keyword->cmd_id == -1) return(KW_UNKNOWN); //let client handle it
		else return(keyword->cmd_id);
	}
	else {
		if (curr_tok == TID) {
			TOKEN tk;
			int is_assign;
			save_token(&tk);
			get_token();
			is_assign = curr_tok == TEQUAL;
			rest_token(&tk);
			if (is_assign) return(ASSIGN_CMD);
		}
		return(KW_UNKNOWN);
	}
}
