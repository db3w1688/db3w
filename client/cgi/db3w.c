/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/db3w.c
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
#define _XOPEN_SOURCE
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include "w3util.h"
#include "db3w.h"
#include "hb.h"
#include "hbusr.h"
#include "hbapi.h"
#include "hbtli.h"

#define HBDEBUG

extern char *crypt();

#define GRP_DB3W	"db3w"
#define MAXCONTENTLEN	4000

int hb_sigs;
HBCONN conn = HBCONN_INVAL;
jmp_buf hb_env;

char login[DBE_LOGIN_LEN];
int hb_port = HB_PORT;
int sid = -1;
int stamp = -1;

char htmfn[MAXPATHLEN];
int lineno = -1;

static long last_pos;
static long hb_addr[MAXLEVEL];	/** locations in file **/
static int hb_lineno[MAXLEVEL];
static int top_addr = 0;
static int neststack[MAXLEVEL];	/** implement control structures **/
static int top = 0;

static int is_db3w_session = 0;
static int is_db3w_error = 0;

#define DB3W_KWTBLSZ	31

static HBKW db3w_kwds[DB3W_KWTBLSZ] = {
{ "ALL",		KW_ALL,			-1, -1, -1 },
{ "BEGINCASE",		KW_BEGINCASE,		-1, -1, -1 },
{ "CASE",		KW_CASE,		-1, -1, -1 },
{ "COOKIE",		KW_COOKIE,		-1, -1, -1 },
{ "DB3W_ERROR",		KW_DB3W_ERROR,		-1, -1, -1 },
{ "DB3W_SESSION",	KW_DB3W_SESSION,	-1, -1, -1 },
{ "DEBUG",		KW_DEBUG,		-1, -1, -1 },
{ "ELSE",		KW_ELSE,		-1, -1, -1 },
{ "ENDCASE",		KW_ENDCASE,		-1, -1, -1 },
{ "ENDIF",		KW_ENDIF,		-1, -1, -1 },
{ "ENDWHILE",		KW_ENDWHILE,		-1, -1, -1 },
{ "FOR",		KW_FOR,			-1, -1, -1 }, /** non-db3w keyword, to be ignored **/
{ "FROM",		KW_FROM,		-1, -1, -1 },
{ "GO",			KW_GO,			-1, -1, -1 },
{ "HTTP",		KW_HTTP,		-1, -1, -1 },
{ "IF",			KW_IF,			-1, -1, -1 },
{ "OFF",		KW_OFF,			-1, -1, -1 },
{ "ON",			KW_ON,			-1, -1, -1 },
{ "OTHERWISE",		KW_OTHERWISE,		-1, -1, -1 },
{ "OUTPUT",		KW_OUTPUT,		-1, -1, -1 },
{ "PRINT",		KW_PRNT,		-1, -1, -1 },
{ "RETURN",		KW_RETURN,		-1, -1, -1 },	/** non-db3w keyword, to be ignored **/
{ "SELECT",		KW_SELECT,		-1, -1, -1 },
{ "SENDMAIL",		KW_SENDMAIL,		-1, -1, -1 },
{ "SHOWVARS",		KW_SHOWVARS,		-1, -1, -1 },
{ "SKIP",		KW_SKIP,		-1, -1, -1 },
{ "SLEEP",		KW_SLEEP,		-1, -1, -1 },
{ "STORE",		KW_STORE,		-1, -1, -1 },
{ "SUBJECT",		KW_SUBJ,		-1, -1, -1 },
{ "TO",			KW_TO,			-1, -1, -1 },
{ "WHILE",		KW_WHILE,		-1, -1, -1 }
};

db3w_kwsrch(char *str, int len)
{
	int i;
	for (i=0; i<DB3W_KWTBLSZ; ++i) if (!strncasecmp(str, db3w_kwds[i].name, len)) return(db3w_kwds[i].kw_val);
	return(-1);
}

#define init_stack()    { neststack[0] = 0; }
#define zerolevel()     (!top)
#define crntlevel()     (0x00ff & neststack[top])
#define ignore()        (0x8000 & neststack[top])
#define i_ignore()      (0x8000 & neststack[top-1])
#define p_ignore()      (0x8000 & neststack[top+1])
#define s_ignore()      { neststack[top] |= 0x8000; }
#define r_ignore()      { neststack[top] &= 0x7fff; }
#define checklevel()    { if (++top == MAXLEVEL) syn_error(NULL,DB3W_TOOMANY_LEVELS); }
#define pushlevel(what) { checklevel(); neststack[top] = i_ignore() | what; }
#define poplevel()      (--top)
#define chglevel(what)  { neststack[top] = (0xff00 & neststack[top]) | what; }
#define set_c()         { neststack[top] |= 0x4000; }
#define c_ignore()      (0x4000 & neststack[top])
#define set_nf()        { neststack[top] |= 0x2000; }
#define nf_case()       (0x2000 & neststack[top])
#define set_s()         { neststack[top] |= 0x1000; }
#define s_case()        (0x1000 & neststack[top])

int process_cond(char *expr)
{
	dbe_eval(conn, expr, 0);
	return(dbe_istrue(conn));
}

char *root;	/** document root **/
char *homedir, hb_user[LEN_USER + 1];

int idcmp(char *val, HT_TOKEN *tok)
{
	char token[256];
	if (tok->token == HT_LIT) snprintf(token, 256, "%.*s", tok->tok_len - 2, tok->line + tok->tok_s + 1);
	else snprintf(token, 256, "%.*s", tok->tok_len, tok->line + tok->tok_s);
	return(strcasecmp(val, token));
}


void push_addr(long pos)
{
	if (top_addr == MAXLEVEL) syn_error(NULL, DB3W_TOOMANY_LEVELS);
	hb_addr[top_addr] = pos;
	hb_lineno[top_addr] = lineno - 1;
	++top_addr;
}

long pop_addr(void)
{
	if (--top_addr < 0) syn_error(NULL, DB3W_STACK_FAULT);
	lineno = hb_lineno[top_addr];
	return(hb_addr[top_addr]);
}

#include <ctype.h>

int get_expr(HT_TOKEN *tok)
{
	char c, *line = tok->line, *tp;
	tok->token = HT_NULLTOK;
	tp = &line[tok->tok_s += tok->tok_len];
	while (isspace(*tp)) { ++tp; ++tok->tok_s; }
	tok->tok_len =0;
	while (*tp && *tp!= '}') { ++tp; ++tok->tok_len; }
	if (*tp!='}') return(-1);
	tok->token = HT_EXPR;
	return(0);
}

char *db3w_resolve(char *start, int maxlen);

#define is_db3w_start(tp)	(*(tp+1)=='@')
#define HT_DB3W_LEN		2 //{@

int get_token(HT_TOKEN *tok)
{
	char c, *line = tok->line, *tp;
	tok->token = HT_NULLTOK;
	tp = &line[tok->tok_s += tok->tok_len];
	while (isspace(*tp)) { ++tp; ++tok->tok_s; }
	tok->tok_len = 0; c = *tp;
	if (!c) return(0);
	if (c == 0x1a) {
		tok->token = HT_EOF;
		return(0);
	}
	if (c == '"') {
		char tmp[MAXLINELEN], tail[MAXLINELEN];
		tok->token = HT_LIT;
		++tp; ++tok->tok_len;
		while (*tp && *tp != c) { ++tp; ++tok->tok_len; }
		if (!*tp) {
			tok->token = HT_UNBAL_QUOTE;
			return(-1);
		}
		++tok->tok_len;
		snprintf(tmp, MAXLINELEN, "%.*s", tok->tok_len, line + tok->tok_s);
		strcpy(tail, line + tok->tok_s + tok->tok_len);
		db3w_resolve(tmp, MAXLINELEN);
		snprintf(line + tok->tok_s, tok->maxlen - tok->tok_s, "%s%s", tmp, tail);
		tok->tok_len = strlen(tmp);
		return(0);
	}
	if (c == '<') {
		tok->token = HT_TAG;
		++tp; ++tok->tok_len;
		if (*tp == '/') {
			++tp; ++tok->tok_len;
			tok->token = HT_TAG_END;
			return(0);
		}
		if (!isalpha(*tp)) 
			tok->token = HT_LBRKT;
		return(0);
	}
	if (c == MACCHAR) {
		tok->token = HT_MACRO;
		++tp; ++tok->tok_len;
		if (atoi(tp)<=0) {
			tok->token = HT_DOLLAR;
			return(0);
		}
		while (isdigit(*tp)) { ++tok->tok_len; ++tp; }
		return(0);
	}
	if (isdigit(c)) {
		tok->token = HT_NUMBER;
		do { ++tok->tok_len; c = *(++tp); } while (isdigit(c));
		return(0);
	}
	if (isalpha(c)) {
		tok->token = HT_ID;
		++tp; ++tok->tok_len;
		while (*tp && (isalpha(*tp) || isdigit(*tp) || *tp=='_')) { ++tp; ++tok->tok_len; }
		return(0);
	}
	switch (c) {
	case '>':
		tok->token = HT_RBRKT;
		break;
	case '.':
		tok->token = HT_PERIOD;
		break;
	case ',':
		tok->token = HT_COMMA;
		break;
	case '@':
		tok->token = HT_AT;
		break;
	case ';':
		tok->token = HT_SEMICOLON;
		break;
	case '(':
		tok->token = HT_LPAREN;
		break;
	case ')':
		tok->token = HT_RPAREN;
		break;
	case ':':
		tok->token = HT_COLON;
		break;
	case '=':
		tok->token = HT_EQUAL;
		break;
	case '!':
		tok->token = HT_EXCLAMATION;
		break;
	case '{':
		if (is_db3w_start(tp)) {
			tok->token = HT_DB3W;
			tok->tok_len = HT_DB3W_LEN;
			goto end;
		}
		else tok->token = HT_LBRACE;
		break;
	case '}':
		tok->token = HT_DB3WEND;
		break;
	default:
		tok->token = HT_UNKNOWN;
		break;
	}
	++tok->tok_len;
end:
	return(0);
}

static char *db3w_slines[] = {
"<INPUT TYPE=HIDDEN NAME=\"_domain\" VALUE=\"{@_domain}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_userspec\" VALUE=\"{@_userspec}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_dbhost\" VALUE=\"{@_dbhost}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_port\" VALUE=\"{@_port}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_dbpath\" VALUE=\"{@_dbpath}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_sid\" VALUE=\"{@_sid}\">\n",
"<INPUT TYPE=HIDDEN NAME=\"_stamp\" VALUE=\"{@_stamp}\">\n",
NULL
};

void output_db3w_session(void)
{
	char line[MAXLINELEN];
	int i;
	is_db3w_session = 1;
	for (i=0; db3w_slines[i]; ++i) {
		strcpy(line, db3w_slines[i]);
		process_line(NULL, line, MAXLINELEN, NULL);
		fputs(line, stdout);
	}
}

HTTP_VAR *find_entry(char *name);

char *db3w_resolve(char *start, int maxlen)	/** only expressions, no commands **/
{
	char *start2, *expr_s, *tp;
	char expr[MAXLINELEN], tail[MAXLINELEN], tmp[MAXLINELEN];

	start2 = start;
	while (expr_s = strchr(start2, '{')) {
		if (is_db3w_start(expr_s)) {
			HT_TOKEN tok;
			tok.line = expr_s + HT_DB3W_LEN;
			tok.maxlen = maxlen - (int)(expr_s - start) - HT_DB3W_LEN;
			tok.tok_s = tok.tok_len = 0;
			get_token(&tok);
			if (tok.tok_len == 0 || tok.token == HT_DB3WEND) return(NULL);
			if (db3w_kwsrch(&tok.line[tok.tok_s], tok.tok_len) >= 0) return(NULL);	//not an expression
			if (!(tp = strchr(expr_s, '}'))) return(NULL);
			/*
			if (!(tp = strchr(expr_s,'}'))) syn_error(line,DB3W_MISSING_DB3WEND);
			*/
			
			snprintf(expr, MAXLINELEN, "%.*s", tp - expr_s - HT_DB3W_LEN, expr_s + HT_DB3W_LEN);
			strcpy(tail, tp + 1);
			if (expr[0] == '_') {
				HTTP_VAR *ep = find_entry(expr);
				if (!ep) *tmp = '\0';
				else strcpy(tmp, ep->val);
			}
			/*
			else if (hb_get_value(conn,expr,tmp,MAXLINELEN)) syn_error(exp,DB3W_ILL_EXPR);
			*/
			/** upon error, replace expression with empty string **/
			else if (hb_get_value(conn, expr, tmp, MAXLINELEN, 0)) *tmp = '\0';
			snprintf(expr_s, maxlen - (int)(expr_s - start), "%s%s", tmp, tail);
		}
		else start2 = expr_s + 1;
	}
	return(start);
}

int process_line(char *currpath, char *line, int maxlen, FILE *infp)
{
	char url[MAXLINELEN], *tp, *tp2;
	HT_TOKEN tok;
	int first_tok, kw, resolved;
	if (line[0] == '!' && line[1] == '$') {
		memmove(line, line + 2, strlen(line) + 1 - 2); // remember the null char
		return(!ignore());
	}
	tok.line = line;
	tok.maxlen = maxlen;
	tok.tok_s = tok.tok_len = 0;
	for (first_tok=1,resolved=0; get_token(&tok) >= 0; first_tok = FALSE) {
		if (tok.token == HT_NULLTOK ||tok.token == HT_EOF) break;
		if (tok.token == HT_DB3W) {
			if (first_tok) {
				get_token(&tok);
				/** if not followed by ID, must be just '{', ignore it. **/
				if (tok.token != HT_ID) continue;
				/*
				if (tok.token!=HT_ID) syn_error(line,DB3W_MISSING_TAG);
				*/
				kw = db3w_kwsrch(&tok.line[tok.tok_s], tok.tok_len);
				switch (kw) {
				case KW_WHILE:
					pushlevel(KW_WHILE);
					if (!ignore()) {
						if (get_expr(&tok) < 0) syn_error(line, DB3W_MISSING_DB3WEND);
						snprintf(url, MAXLINELEN, "%.*s", tok.tok_len, &tok.line[tok.tok_s]);
						if (!process_cond(url)) s_ignore()
						else push_addr(last_pos);
					}
					break;
				case KW_BEGINCASE:
					pushlevel(KW_CASE);
					break;
				case KW_ENDWHILE:
					if (crntlevel() != KW_WHILE) syn_error(line, DB3W_UNBAL_ENDWHILE);
					poplevel();
					if (ignore() || p_ignore()) break;
					if (infp) fseek(infp, pop_addr(), 0);
					else dbe_form_seek(conn, pop_addr());
					break;
				case KW_IF:
					pushlevel(KW_IF);
					if (!ignore()) {
						if (get_expr(&tok) < 0) syn_error(line, DB3W_MISSING_DB3WEND);
						snprintf(url, MAXLINELEN, "%.*s", tok.tok_len, &tok.line[tok.tok_s]);
						if (!process_cond(url)) s_ignore()
					}
					break;
				case KW_ELSE:
					if (crntlevel() != KW_IF) syn_error(line, DB3W_MISSING_IF);
					chglevel(KW_ELSE);		/* to prevent more ELSE */
					if (i_ignore()) break;	  /* upperlevel ignore */
					if (ignore()) r_ignore()
					else s_ignore()
					break;
				case KW_ENDIF:
					if ((crntlevel() != KW_IF) && (crntlevel() != KW_ELSE))
						syn_error(line, DB3W_UNBAL_ENDIF);
					poplevel();
					break;
				case KW_CASE:
					if (crntlevel() != KW_CASE) syn_error(line, DB3W_MISSING_CASE);
					if (i_ignore()) break;
					if (c_ignore()) s_ignore()
					else {
						r_ignore()
						if (get_expr(&tok) < 0) syn_error(line, DB3W_MISSING_DB3WEND);
						snprintf(url, MAXLINELEN, "%.*s", tok.tok_len, &tok.line[tok.tok_s]);
						if (process_cond(url)) set_c()
						else s_ignore()
					}
					break;
				case KW_OTHERWISE:
					if (crntlevel() != KW_CASE) syn_error(line, DB3W_MISSING_CASE);
					chglevel(KW_OTHERWISE);	 /* to prevent more OTHERWISE */
					if (!i_ignore()) {
						if (c_ignore()) s_ignore()
						else r_ignore()
					}
					break;
				case KW_ENDCASE:
					if ((crntlevel() != KW_CASE) && (crntlevel() != KW_OTHERWISE))
						syn_error(line, DB3W_UNBAL_ENDCASE);
					poplevel();
					break;
				case KW_DB3W_SESSION:
					if (!ignore()) {
						output_db3w_session();
					}
					break;
				case KW_STORE:
				case KW_SKIP:
				case KW_SELECT:
				case KW_GO:
				case KW_PRNT:
				case KW_SHOWVARS:
					tp = &line[tok.tok_s];
					tp2 = strchr(tp, '}');
					if (!tp2) syn_error(line, DB3W_MISSING_DB3WEND);
					if (ignore()) {
						tok.tok_s = tp2 - line;
						tok.tok_len = 0;
						break;
					}
					if (kw == KW_SHOWVARS) strcpy(url, "display memory");
					else snprintf(url, MAXLINELEN, "%.*s", tp2 - tp, tp);
					dbe_cmd(conn, url);
					resolved = 1;
					break;
				case KW_COOKIE:
					tp = &line[tok.tok_s + tok.tok_len];
					db3w_resolve(tp, maxlen - (int)(tp - line));
					tp2 = strchr(tp, '}');
					if (!tp2) syn_error(line, DB3W_MISSING_DB3WEND);
					*tp2 = '\0';
					set_http_cookie(tp);
					resolved = 1;
					break;
				case KW_SENDMAIL: {
					char *from = NULL, *to = NULL, *subj = NULL, *arg;
					int kw;
					tp = &line[tok.tok_s + tok.tok_len];
					db3w_resolve(tp, maxlen - (int)(tp - line));
					tp2 = strchr(tp, '}');
					if (!tp2) syn_error(line, DB3W_MISSING_DB3WEND);
					*tp2 = '\0';
					while (!from || !to || !subj) {
						get_token(&tok);
						if (tok.token != HT_ID) syn_error(line, DB3W_ILL_SYNTAX);
						kw = db3w_kwsrch(&line[tok.tok_s], tok.tok_len);
						get_token(&tok);
						if (tok.token != HT_EQUAL) syn_error(line, DB3W_MISSING_EQUAL);
						get_token(&tok);
						if (tok.token != HT_LIT || tok.tok_len<=2) syn_error(line, DB3W_MISSING_LIT);
						arg = malloc(tok.tok_len - 1);
						if (!arg) syn_error(line, DB3W_NOMEMORY);
						snprintf(arg, tok.tok_len - 1, "%.*s", tok.tok_len - 2, &line[tok.tok_s + 1]);
						if (kw == KW_TO) to = arg;
						else if (kw == KW_FROM) from = arg;
						else if (kw == KW_SUBJ) subj = arg;
						else syn_error(line, DB3W_ILL_SYNTAX);
					}
					if (!set_sendmail(to, from, subj)) syn_error(line, DB3W_SET_SENDMAIL);
					resolved = 1;
					break; }
				case KW_OUTPUT: {
					int kw;
					get_token(&tok);
					if (tok.token != HT_ID) syn_error(line, DB3W_MISSING_TAG);
					kw = db3w_kwsrch(&line[tok.tok_s], tok.tok_len);
					if (kw != KW_SENDMAIL && kw != KW_HTTP && kw != KW_ALL) syn_error(line, DB3W_ILL_SYNTAX);
					get_token(&tok);
					if (tok.token != HT_DB3WEND) syn_error(line, DB3W_MISSING_DB3WEND);
					set_output(kw);
					resolved = 1;
					break; }
				case KW_DEBUG:
					get_token(&tok);
					if (tok.token != HT_ID) syn_error(line, DB3W_MISSING_ONOFF);
					kw = db3w_kwsrch(&line[tok.tok_s], tok.tok_len);
					if (kw != KW_ON && kw != KW_OFF) syn_error(line, DB3W_MISSING_ONOFF);
					set_debug(kw == KW_ON);
					resolved = 1;
					break;
				case KW_SLEEP: {
					char sockname[VNAMELEN + 1] = { 0 };
					get_token(&tok);
					if (tok.token  != HT_ID) syn_error(line, DB3W_MISSING_NAME);
					snprintf(sockname, VNAMELEN+1, "%.*s", tok.tok_len, line + tok.tok_s);
					dbe_sleep(&conn, 300);
					hb_ipc_wait(sockname, sid, 60);
					conn = dbe_init_session((HBSSL_CTX)NULL, login, hb_port, stamp, 0, &sid);
					dbe_inst_usr(conn);
					break; }
				case KW_DB3W_ERROR:
					if (!ignore()) {
						is_db3w_error = 1;
					}
					break;
				default:
					if (ignore()) break;
					syn_error(line, DB3W_ILL_SYNTAX);
				}
				if (!resolved && !ignore()) {
					get_token(&tok);
					if (tok.token != HT_DB3WEND) syn_error(line, DB3W_MISSING_DB3WEND);
				}
				return(0);
			}
			else if (!resolved && !ignore()) {
				tp = &line[tok.tok_s];
				if (db3w_resolve(tp, maxlen - (int)(tp - line))) {
					resolved = 1;
					tok.tok_len = 0;
				}
				continue;
			}
		}
		else if (tok.token == HT_TAG) {
			get_token(&tok);
			if (tok.token != HT_ID) syn_error(line, DB3W_MISSING_TAG);
			do {
				if (!idcmp("BODY", &tok) || !idcmp("TD", &tok)) {
					get_token(&tok);
					if (!idcmp("BACKGROUND", &tok)) {
						get_token(&tok);
						if (tok.token == HT_EQUAL) {
							get_token(&tok);
							if (tok.token == HT_LIT) goto change;
						}
					}
					else if (tok.token == HT_RBRKT) break;
				}
				else if (!idcmp("A", &tok)) {
					get_token(&tok);
					if (!idcmp("HREF", &tok)) {
						get_token(&tok);
						if (tok.token == HT_EQUAL) {
							get_token(&tok);
							if (tok.token == HT_LIT) {
								char *url2, *tmp;
change:
								snprintf(url, MAXLINELEN, "%.*s", tok.tok_len - 2, line + tok.tok_s + 1);
								if (url[0] != '/' && url[0] != '#' && strncasecmp(url, "http", 4) && strncasecmp(url, "mailto", 5) && strncasecmp(url, "ftp", 3) && strncasecmp(url, "javascript:", 11)) {
									url2 = url + strlen(url) + 1;
									snprintf(url2, MAXLINELEN - strlen(url) - 1, "%s%s", currpath, url);
									tmp = url2 + strlen(url2) + 1;
									strcpy(tmp, line + tok.tok_s + tok.tok_len);
									snprintf(line + tok.tok_s, maxlen - tok.tok_s, "\"%s\"%s", url2, tmp);
									tok.tok_len = strlen(url2) + 2;
								}
							}
						}
					}
				}
				else if (!idcmp("IMG", &tok)) {
					get_token(&tok);
					if (!idcmp("SRC", &tok)) {
						get_token(&tok);
						if (tok.token == HT_EQUAL) {
							get_token(&tok);
							if (tok.token == HT_LIT) goto change;
						}
					}
				}
			} while (get_token(&tok) >= 0 && tok.token != HT_RBRKT && tok.token == HT_NULLTOK && tok.token == HT_EOF);
		}
		else if (tok.token == HT_TAG_END) {
			get_token(&tok);
			if (tok.token != HT_ID) syn_error(line, DB3W_MISSING_TAG);
			get_token(&tok);
			if (tok.token != HT_RBRKT) syn_error(line, DB3W_MISSING_RBRKT);
		}
	}
	return(!ignore());
}

#define MAXHTTPVARS  128
#define UL_NOFILE    "---NOFILE---"

static HTTP_VAR httpvars[MAXHTTPVARS] = { {NULL, NULL } };

HTTP_VAR *find_entry(char *name)
{
	HTTP_VAR *ep;
	int i;
	for (i=0,ep=httpvars; i<MAXHTTPVARS && ep->name; ++i,++ep)
		if (!strcasecmp(ep->name, name)) return(ep);
	return(NULL);
}

void add_entry(char *name, char *val)
{
	HTTP_VAR *ep;
	int i;
	for (i=0,ep=httpvars; i<MAXHTTPVARS && ep->name; ++i,++ep) if (!strcasecmp(name, ep->name)) break;
	if (i<MAXHTTPVARS) {
		ep->name = name;
		ep->val = val;
	}
}

static int ignore_var(char *varname)
{
	if (!strcasecmp(varname, "_passwd")) return(1);
	return(0);
}

void getvars(char *qs, char bchar)
{
	char *tp, *bp, *bp2;
	int i;
	for (i=0,tp=qs; tp && i<MAXHTTPVARS; ++i) {
		bp = strchr(tp, bchar);
		if (bp) *bp = '\0';
		unescape_url(tp);
		bp2 = strchr(tp, '=');
		if (bp2) {
			*bp2 = '\0';
			if (!ignore_var(tp)) add_entry(tp, bp2 + 1); //ignore _passwd in query string
		}
		if (bp) tp = bp + 1;
		else tp = NULL;
	}
}

FILE *rptfp = NULL;

static void dbg_print_vars(void)	/** for debugging **/
{
	HTTP_VAR *ep;
	int i;
	if (!rptfp) return;
	for (i=0,ep=httpvars; i<MAXHTTPVARS && ep->name; ++i,++ep);
	fprintf(rptfp, "Variables: %d\n", i);
	for (i=0,ep=httpvars; i<MAXHTTPVARS && ep->name; ++i,++ep) {
		fprintf(rptfp, "%s=%s\n", ep->name, ep->val);
	}
}

#include <stdarg.h>
static void dbg_print_line(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (!rptfp) return;
	vfprintf(rptfp, fmt, ap);
}

static char boundary[64], boundary_end[64];

int ismultipart(void)
{
	char *tp = getenv("CONTENT_TYPE"), *tp2;
	if (!tp || strncasecmp(tp, "multipart/form-data", 19) || !(tp2 = strchr(tp, '='))) return(0);
	snprintf(boundary, 64, "--%s", tp2 + 1);
	snprintf(boundary_end, 64, "%s--", boundary);
	return(1);
}

char *getfile(char *buf, FILE *in, FILE *out)
{
	char *tp, c;
	int i, bufsz = strlen(boundary);
	tp = buf;
	while (1) {
		if (tp - buf < bufsz) {
			c = fgetc(in);
			if (feof(in) || ferror(in)) {
				break;
			}
			*tp++ = c;
			continue;
		}
		if (!strncmp(buf, boundary, bufsz)) {
			return(tp);
		}
		if (out) fputc(buf[0], out);
		for (i=0; i<bufsz-1; ++i) buf[i] = buf[i+1];
		buf[i] = fgetc(in);
		if (feof(in) || ferror(in)) {
			break;
		}
	}
	return(NULL);
}

#define POSTLINELEN	1024

//#define HBDEBUG_INPUT
#ifdef HBDEBUG_INPUT
#include <errno.h>
int multivars(void)
{
	int c;
	FILE *outfp;
	outfp = fopen("tmp/db3w.out", "w"); //relative to cgi-bin root
	if (!outfp) return(0);
	while ((c = fgetc(stdin)) != EOF) fputc(c, outfp);
	fclose(outfp);
	return(0);
}
#else
int multivars(void)
{
	char line[POSTLINELEN], outfn[MAXPATHLEN], *vp, *tp, *tp2;
	FILE *outfp;
	int done = 0, empty_file = 0;
	tp = line;
	while (!feof(stdin) && !ferror(stdin)) {
		if (!fgets(tp, POSTLINELEN, stdin)) break; //tp may be from getfile(), boundary
		tp = line + strlen(line);
		while (tp > line && isspace(*(tp - 1))) --tp;
		*tp = '\0';
		tp -= strlen(boundary);
		if (tp < line) continue;
		if (tp - 2 == line && !strcmp(tp - 2, boundary_end)) { done = 1; break; }
		if (strcmp(tp, boundary)) continue;
		if (!fgets(line, POSTLINELEN, stdin) || strncmp(line, "Content-Disposition", 19)) break;
		tp = strchr(line, '=');
		if (!tp || *(tp + 1) != '\"') break;
		tp += 2;
		tp2 = strchr(tp, '\"');
		if (!tp2) break;
		if (!(vp = malloc(POSTLINELEN))) break; //freed by exit of cgi
		snprintf(vp, POSTLINELEN, "%.*s", tp2 - tp, tp); //var name
		if (tp = strchr(tp2, '=')) { //filename="????"
			if (*(tp + 1) != '\"') break;
			tp2 = vp + strlen(vp) + 1;
			snprintf(tp2, POSTLINELEN - strlen(vp) - 1, "%.*s", 8, tp - 8);
			if (strcasecmp(tp2, "filename")) break;
			tp += 2;
			char *tp3 = strchr(tp, '\"');
			if (!tp3) break;
			if (tp3 == tp) empty_file = 1;
			if (!empty_file) {
				snprintf(tp2, POSTLINELEN - strlen(vp) - 1, "%d.%d", time(), getpid());
				snprintf(outfn, MAXPATHLEN, "tmp/%s", tp2); //under DOCUMENT_ROOT
				outfp = fopen(outfn, "w");
				if (!outfp) break;
				add_entry(vp, tp2); //file=
				if (!(vp = malloc(POSTLINELEN))) break;
				strcpy(vp, "filename");
				tp2 = vp + strlen(vp) + 1;
				snprintf(tp2, POSTLINELEN - strlen(vp) - 1, "%.*s", (int)(tp3 - tp), tp); //filename=
				tp3 = strchr(tp2, '.');
				if (tp3) hb_lwrcase(tp3);
			}
			else {
				strcpy(tp2, UL_NOFILE);
				outfp = NULL;
			}
			if (!fgets(line, POSTLINELEN, stdin) || strncmp(line, "Content-Type", 12) || !fgets(line, POSTLINELEN, stdin)) break;
			if (!empty_file) {
				if (!(tp = getfile(line, stdin, outfp))) break;
				fclose(outfp);
			}
			add_entry(vp, tp2);
		}
		else {
			if (!fgets(line, POSTLINELEN, stdin) || !fgets(line, POSTLINELEN, stdin)) break;
			tp = line + strlen(line);
			while (tp > line && isspace(*(tp - 1))) --tp;
			*tp = 0;
			tp2 = vp + strlen(vp) + 1;
			strcpy(tp2, line);
			add_entry(vp, tp2);
			tp = line;
		}
	}
	return(done);
}
#endif

int postvars(void)
{
	char *line = getenv("CONTENT_LENGTH"), *name;
	int cl, foundvar=0;
	if (!line) return(0);
	else cl = atoi(line);
	while (cl &&  !feof(stdin)) {
		line = fmakeword(stdin, '&', &cl);
		plustospace(line);
		unescape_url(line);
		name = makeword(line, '=');
		add_entry(name, line);
		foundvar = 1;
	}
	return(foundvar);
}

void getcookies(void)
{
	char *ck, *ck2;

	ck = getenv("HTTP_COOKIE");
	if (!ck) return;
	ck2 = malloc(strlen(ck) + 100);
	if (!ck2) return;
	strcpy(ck2, ck);
	getvars(ck2, ';');
}

extern void print_header(), print_line(), print_footer();

int hb_html(char *frmfn) //frmfn==NULL server side
{
	char currpath[MAXPATHLEN], line[2*MAXLINELEN], *dd, *pfx;
	FILE *htmfp = NULL; //NULL means server side
	if (frmfn) {
		HTTP_VAR *ep = find_entry("_prefix");
		if (ep) pfx = ep->val;
		else pfx = "";
		dd = strrchr(frmfn, '/');
		if (dd) {
			snprintf(currpath, MAXPATHLEN, "%s%.*s", frmfn[0] == '/'? "" : "/", dd - frmfn + 1, frmfn);
			frmfn = dd + 1;
		}
		else strcpy(currpath, "/");
		snprintf(htmfn, MAXPATHLEN, "%s%s%s%s", root, pfx, currpath, frmfn);
		htmfp = fopen(htmfn, "r");
		if (!htmfp) {
			sprintf(line, "ERROR: Cannot open %s: %s", htmfn, strerror(errno));
			display_msg(line, NULL);
			return(-1);
		}
		dbg_print_line("---------- %s ----------\n", htmfn);
	}
	else {
		dbe_form_rewind(conn);
		dbg_print_line("---------- %s ----------\n", dbe_get_id(conn, ID_FRM_FN, 0, line, MAXLINELEN));
	}
	init_stack();
	lineno = 0;
	if (setjmp(hb_env)) return(-1);
	while (1) {
		if (htmfp) {
			last_pos = ftell(htmfp);
			if (!fgets(line, MAXLINELEN, htmfp)) break;
		}
		else {
			last_pos = dbe_form_tell(conn);
			if (!dbe_form_rdln(conn, line, MAXLINELEN)) break;
		}
		++lineno;
		dbg_print_line("%03d %s", lineno, line);
		/** process_line() may output data prior to header **/
		if (process_line(currpath, line, 2 * MAXLINELEN, htmfp)) {
			print_header();
			print_line(line);
		}
	}
	print_footer();
	if (!zerolevel())
	switch (crntlevel()) {
		case KW_IF:	syn_error(NULL, DB3W_UNBAL_IF);
		case KW_WHILE:	syn_error(NULL, DB3W_UNBAL_WHILE);
		case KW_CASE:	syn_error(NULL, DB3W_UNBAL_CASE);
		default:	syn_error(NULL, DB3W_UNKNOWN_CONTROL);
	}
	if (htmfp) fclose(htmfp);
	lineno = -1;
	return(0);
}

static int hb_timeout = 0;

extern int (*hb_usrfcn[])();
extern int hb_prnt_val();

static char *cgi_get_epasswd(char *salt)
{
	if (!salt) return(NULL);
	HTTP_VAR *ep = find_entry("_passwd"); //use system variable to avoid auto store as mvar (line 1069)
	if (!ep) return(NULL);
	return(crypt(ep->val, salt));
}

#include <fcntl.h>

int hb_expfil(HBCONN conn, int argc)
{
	int exfd = -1, local_fd = -1, len = 0, rc = -1;
	char iobuf[STRBUFSZ], filename[FULFNLEN];
	if (argc == 2) {
		if (!dbe_pop_string(conn, filename, FULFNLEN)) goto end;
		local_fd = open(filename, O_RDWR);
		if (local_fd < 0) goto end;
	}
	else {
		char *tp;
		local_fd = STDOUT_FILENO;
		if (!dbe_peek_string(conn, 0, filename, FULFNLEN)) goto end;
		tp = strrchr(filename, SWITCHAR);
		if (!tp) tp = filename;
		else ++tp;
		sprintf(iobuf, "Content-Type: application/octet-stream\nContent-Disposition: attachment; filename=\"%s\"\n\n", tp);
    }
	exfd = dbe_open_file(conn, O_RDONLY, NULL);
	if (exfd < 0) goto end;
	if (local_fd == STDOUT_FILENO) {
		len = strlen(iobuf);
		if (write(local_fd, iobuf, len) != len) goto end;
	}
	do {
		len = dbe_read_file(conn, exfd, iobuf, STRBUFSZ, 1);
		if (len < 0) goto end;
		if (write(local_fd, iobuf, len) != len) goto end;
	} while (len > 0);
	rc = 0;
end:
	if (local_fd != -1 && local_fd != STDOUT_FILENO) close(local_fd);
	if (exfd >= 0) dbe_close_file(conn, exfd);
	return(rc);
}

int usr(HBCONN notused, HBREGS *regs) //use the global conn
{
	char *bx = regs->bx;
	WORD ax = regs->ax, ex = regs->ex;
	int cx = regs->cx;
	DWORD dx = regs->dx;
	switch (ax) {
	case USR_FCN: {
		int fcn_no = usr_getfcn(conn);
		if (fcn_no < 0) {
			regs->ax = UNDEF_FCN;
			return(-1);
		}
		return((*hb_usrfcn[fcn_no])(conn, ex));
		}
	case USR_READ: {
		static char tmp[32];
		snprintf(tmp, 32, "%ld", dx);
		add_entry("_stamp", tmp);
		if (hb_html(cx? bx : NULL) < 0) break;
		if (is_db3w_session || is_db3w_error) {
			dbe_sleep(&conn, hb_timeout);
			exit(0);
		}
		else return(0);
		}
	case USR_GETACCESS:
		regs->ax = HBALL; //NEEDSWORK: implement access control
		return(0);
		break;
	case USR_PRNT_VAL:
		hb_prnt_val(conn, bx, ex);
		regs->cx = 0;
		return(0);
	case USR_AUTH: {
		char *epasswd = cgi_get_epasswd(bx);
		if (!epasswd) {
			regs->cx = 0;
			return(-1);
		}
		regs->ax = strcmp(bx, epasswd);
		regs->cx = 0;
		return(0); }
	case USR_EXPORT:
		return(hb_expfil(conn, ex));
	default:
		break;
	}
	return(-1);
}

static int is_legal_var_name(char *name)
{
	char *tp;
	int count;
	for (tp=name, count=0; *tp; ++tp, ++count) {
		if (count > VNAMELEN) return(0);
		if ((*tp >= '0' && *tp <= '9') || (*tp >= 'A' && *tp <= 'Z') || (*tp >= 'a' && *tp <= 'z') || (*tp == '_')) continue;
		return(0);
	}
	return(1);
}

static int is_redirect(char *url)
{
	return !strncasecmp(url, "http://", 7) || !strncasecmp(url, "ftp://", 6) || !strncasecmp(url, "https://", 8);
}

#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_NAME	"db3w_console"

static FILE *console_sock_open(void)
{
	struct sockaddr_un addr;
	FILE *sfp = NULL;
	int fd;
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) goto end;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	sprintf(addr.sun_path, "%c%s", '\0', SOCK_NAME);
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) goto end;
	sfp = fdopen(fd, "a");
end:
	return(sfp);
}

static char *usage = "USAGE: db3w.cgi?[_dbhost=<host>&][_port=<port>&][_domain=<domain>&][_dbpath=<path>&]_prg=<prg.proc>[&var=<val>...]";

int main(int argc, char *argv[])
{
	char *pinfo = getenv("PATH_INFO");
	char *qs = getenv("QUERY_STRING"), *qs2;
	char *host = "localhost", *dbpath = "/", *tp;
	char cmdline[MAXLINELEN], *addr;
	static char userspec[MAXLINELEN];
	int len, flags = 0, excl = 0;
	HTTP_VAR *ep;
	int new_session;
	struct group *grpent;
	struct passwd *pwent;

#ifdef HBDEBUG
while (!access("tmp/cgi.dbg", F_OK)) sleep(1); //relative to cgi-bin root
#endif
	/** if group db3w is defined and setgid bit is set... **/
	grpent = getgrnam(GRP_DB3W);
	if (grpent) setgid(grpent->gr_gid);
	/** otherwise run as the same group as httpd **/

	set_stdout();
	getcookies();

#ifdef ROOT_FROM_URL
	if (!access("docs", 0)) {
		char *cwd = getcwd(NULL, MAXPATHLEN);
		if (chdir("docs")<0) {
			display_msg("ERROR: Cannot change directory to docs", NULL);
			exit(1);
		}
		if (getcwd(cmdline, MAXPATHLEN) && (root = malloc(strlen(cmdline) + 20))) {
			snprintf(root, MAXPATHLEN, "DOCUMENT_ROOT=%s", cmdline);
			putenv(root);
		}
		if (cwd) free(cwd);
	}
	else if (pinfo) {
		pinfo = getenv("PATH_TRANSLATED");
		if (pinfo) {
			char *tp;
			tp = strrchr(pinfo, '/');
			if (tp) {
				root = malloc(tp - pinfo + 20);
				if (root) {
					snprintf(root, MAXPATHLEN, "DOCUMENT_ROOT=%.*s", tp - pinfo, pinfo);
					putenv(root);
				}
			}
		}
	}
#endif
	root = getenv("DOCUMENT_ROOT");
	//if (!root || !root[0]) root = getcwd(NULL, 1024);
	if (!root) {
		display_msg("ERROR: DOCUMENT_ROOT not found or invalid", NULL);
		exit(1);
	}
	if (chdir(root)<0) {
		display_msg("ERROR: Cannot change directory to DOCUMENT_ROOT", root);
		exit(1);
	}
	add_entry("_root", root);

	if (ismultipart()) {
		if (!multivars()) goto usage;
	}
	else if (!postvars() && (!qs || !qs[0])) {
usage:
		sprintf(cmdline, "ERROR: Invalid parameter specified.\n%s", usage);
		display_msg(cmdline, "Parameters can also be passed as variables via the POST method.");
		exit(1);
	}
	if (qs && qs[0]) {
		qs2 = malloc(strlen(qs) + 500);
		if (!qs2) {
			display_msg("ERROR: Not enough memory!", NULL);
			exit(1);
		}
		strcpy(qs2, qs);
		getvars(qs2, '&');
	}
	addr = getenv("REMOTE_ADDR");
	if (!addr) {	/** set by the HTTP protocol **/
		display_msg("ERROR: REMOTE_ADDR not set", NULL);
		exit(1);
	}

	ep = find_entry("_domain");
	if (!ep) {
		pwent = getpwuid(geteuid());
		if (!pwent) {
			sprintf(cmdline, "ERROR: Cannot get passwrd entry for uid %d\n", geteuid());
			exit(1);
		}
	}
	else {
		pwent = getpwnam(ep->val);
		if (!pwent) {
			sprintf(cmdline, "ERROR: DOMAIN %s does not exist\n", ep->val);
			display_msg(cmdline, NULL);
			exit(1);
		}
	}
	strcpy(hb_user, pwent->pw_name);
	homedir = pwent->pw_dir;
	len = strlen(root);
	if (!strncmp(homedir, root, len) && homedir[len] == '/') {
		tp = homedir + len;
		add_entry("_prefix", *tp? tp : "/."); //for client side form files, deprecated
	}
	ep = find_entry("_dbhost");
	if (ep) host = ep->val;
	ep = find_entry("_port");
	if (ep) hb_port = atoi(ep->val);
	ep = find_entry("_dbpath");
	if (ep) dbpath = ep->val;
	ep = find_entry("_timeout");
	if (ep) hb_timeout = atoi(ep->val);
	ep = find_entry("_sid");
	if (ep) sid = atoi(ep->val);
	new_session = sid == -1;
	ep = find_entry("_excl");
	if (ep) excl = !!atoi(ep->val);
	ep = find_entry("_rpt");
	if (ep) {
		char rptfn[MAXPATHLEN], *rpt;
		int append_rpt = 0;
		rpt = ep->val;
		ep = find_entry("_append");
		if (ep) append_rpt = !strcasecmp(ep->val, "YES") || !strcasecmp(ep->val, "TRUE");
		snprintf(rptfn, MAXPATHLEN, "%s%s%s", root, rpt[0] == '/'? "" : "/", rpt);
		rptfp = fopen(rptfn, append_rpt? "a" : "w");
	}
	else rptfp = console_sock_open();
	if (new_session) {
		snprintf(userspec, MAXLINELEN, "%s@%s", hb_user, addr);
		add_entry("_userspec", userspec);
		ep = find_entry("_prg");
		if (!ep) goto usage;
	}
	else {
		ep = find_entry("_stamp"); //set by USR_GETPASSWD or USR_READ command with dx=SET_STAMP
		if (!ep) {
			display_msg("ERROR: no session stamp", NULL);
			exit(1);
		}
		stamp = atoi(ep->val);
		ep = find_entry("_userspec");
		if (!ep) {
			display_msg("ERROR: userspec not set", NULL);
			exit(1);
		}
		snprintf(userspec, MAXLINELEN, "%s@%s", hb_user, addr);
		if (strcmp(ep->val, userspec)) {
			sprintf(cmdline, "ERROR: userspec mismatch: %s <> %s\n", userspec, ep->val);
			display_msg(cmdline, NULL);
			exit(1);
		}
	}
	dbg_print_vars();
	signal(SIGPIPE, sigpipe);	/** catch broken pipe signal, even though not likely to happen and no real harm done if it did **/
	flags = HB_SESS_FLG_SUID
			| (find_entry("_passwd")? HB_SESS_FLG_AUTH : 0) //if _passwd var is present, do auth
			| (excl? 0 : HB_SESS_FLG_MULTI); //if !excl allow multiple logins
	dbe_set_login(login, host, dbpath, hb_user, addr, flags);
	conn = dbe_init_session((HBSSL_CTX)NULL, login, hb_port, new_session? 0 : stamp, 0, &sid);
	if (conn == HBCONN_INVAL) {
		ep = find_entry("_alt");
		if (ep) {
			if (is_redirect(ep->val)) printf("Status: 302 Found\nUri: %s\nLocation: %s\nContent-Type: text/html\n\n", ep->val, ep->val);
			else hb_html(ep->val);
		}
		else { //sid has the error code
			dbe_errmsg(sid, cmdline);
			if (new_session) display_msg("Login failed!", cmdline);
			else display_msg("Invalid Session ID!", cmdline);
		}
		exit(1);
	}
	if (new_session) {
		static char tmp[32];
		dbe_inst_usr(conn);
		snprintf(tmp, 32, "%d", sid);
		add_entry("_sid", tmp);
	}
	else {
		ep = find_entry("_quit");
		if (ep) {
			int nextdoc = 1;
			ep = find_entry("_doc");
			if (ep) {
				if (is_redirect(ep->val)) nextdoc = 2;
				else if (hb_html(ep->val) < 0) {
					sprintf(cmdline, "ERROR: failed to read %s\n", ep->val);
					display_msg(cmdline, NULL);
					nextdoc = 0;
				}
			}
			else nextdoc = 0;
			dbe_end_session(&conn);
			if (nextdoc == 2) printf("Status: 302 Found\nUri: %s\nLocation: %s\nContent-type: text/html\n\n", ep->val, ep->val);
			else if (!nextdoc) {
				display_msg("DB3W Session Ended.", NULL);
			}
			exit(0);
		}
	}
	for (ep=httpvars; ep->name; ++ep) {
		if (!strncasecmp(ep->name, "_array_", 7)) {
			snprintf(cmdline, MAXLINELEN, "ARRAY %s[%s]", ep->name + 7, ep->val);
			dbe_cmd(conn, cmdline);
		}
	}
	for (ep=httpvars; ep->name; ++ep) {
		if (!new_session && ep->name[0] == '_') continue;	/** system variable, delt with already **/
		if (!is_legal_var_name(ep->name)) continue;
		if (ignore_var(ep->name)) continue;
		dbe_push_string(conn, ep->val, strlen(ep->val));	/** null byte omitted **/
		dbe_vstore(conn, ep->name);
		/*
		snprintf(cmdline, MAXLINELEN, "STORE \"%s\" TO %s", ep->val, ep->name);
		dbe_cmd(conn, cmdline);
		*/
	}
	if (new_session) {
		char prg[FNLEN + 1], *proc;
		ep = find_entry("_prg");
		if (!ep) { //already checked
			display_msg("UDP Library not specified", NULL);
			exit(1);
		}
		proc = strchr(ep->val, '.');
		if (!proc) {
			display_msg("UDP not specified", NULL);
			exit(1);
		}
		snprintf(prg, FNLEN+1, "%.*s", proc - ep->val, ep->val);
		snprintf(cmdline, MAXLINELEN, "set procedure to %s", prg);
		dbe_cmd(conn, cmdline);
		proc++;
		snprintf(cmdline, MAXLINELEN, "do %s final", proc); //final will cause server to end the session
		dbe_cmd0(conn, cmdline); //don't call dbe_cmd_end() just exit
	}
	else hbusr_return(&conn);  /** never returns **/
}

void display_msg(char *msg, char *msg2)
{
	if (lineno <= 0) { //no form processed yet
		char line[2*MAXLINELEN], htmfn[MAXPATHLEN], *frmfn;
		FILE *htmfp = NULL;
		HTTP_VAR *ep = find_entry("_msghtml");
		if (ep) {
			frmfn = ep->val;
			snprintf(htmfn, MAXPATHLEN, "%s%s%s", root, frmfn[0] == '/'? "" : "/", frmfn);
		}
		else snprintf(htmfn, MAXPATHLEN, "%s/msg.html", root);
		htmfp = fopen(htmfn, "r");
		if (htmfp) {
			print_header();
			while (1) {
				if (!fgets(line, MAXLINELEN, htmfp)) break;
				if (!strncasecmp(line, "$DB3W_MSG$", 10)) {
					html_encode(msg, line, strlen(msg), 2*MAXLINELEN, NL2BR);
					print_line(line);
					if (msg2) {
						html_encode(msg2, line, strlen(msg2), 2*MAXLINELEN, NL2BR);
						print_line("<BR>");
						print_line(line);
					}
				}
				else print_line(line);
			}
			print_footer();
			fclose(htmfp);
			return;
		}
		else {
			html_out(NL2BR, "%s", msg);
			if (msg2) html_out(NL2BR, "\n%s", msg2);
		}
	}
	else {
		char *tp = htmfn + strlen(root);
		if (*tp) html_out(NL2BR, "\nERROR at Line %d, File %s:\n%s", lineno, tp, msg);
		else html_out(NL2BR, "\nERROR at Line %d\n%s", lineno, msg);
		if (msg2) html_out(NL2BR, msg? "\n%s" : "%s", msg2);
	}
}
