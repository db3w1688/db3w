/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbcmd/hb.c
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
#include <fcntl.h>
#include <pwd.h>
#include "hb.h"
#include "hbapi.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#define _HBCLIENT_
#include "hbusr.h"
#include "hbtli.h"

char *version = "HB*Commander v5.0 (UNIX/Linux)";
char *copyright = "Copyright (C) 1997-2019 Kimberlite Software. All rights reserved.";

int hb_sigs;
HBCONN conn = HBCONN_INVAL;
int sid = -1, stamp = -1;
char login[DBE_LOGIN_LEN], hb_user[LEN_USER + 1];
int hb_port = -1;
int nwa = DEFA_NDBUF, nibuf = DEFA_NIBUF;
int debug = 0, is_admin = 0;

char dbe_comm_line[STRBUFSZ], *dbe_comm_buf;

char *homedir;

void hb_help(int k, TOKEN *tok)
{
	if (k < 0 || k == KW_UNKNOWN) {
		display_error(conn, "Unknown keyword", dbe_comm_buf, 1, tok->tok_s, tok->tok_len);
		return;
	}
	FILE *hlpfp;
	int list = 0, len;
	char hlpfn[FULFNLEN], *fn, *tp;
	if (k >= HBKW_START) {
		if (k == HBKW_CMDLST || k == HBKW_FCNLST || k == HBKW_CTLLST 
			|| k == HBKW_KWLST || k == HBKW_SCOPE || k == HBKW_HELP) list = 1;
		fn = hb_keywords[k - HBKW_START].name;
	}
	else fn = keywords[k].name;
	if (!get_docpath(hlpfn, FULFNLEN)) strcpy(hlpfn, "./");
	tp = hlpfn + strlen(hlpfn);
	len = FULFNLEN - (tp - hlpfn);
	if (list) snprintf(tp, len, "hbcmd/%s.hlp", fn);
	else snprintf(tp, len, "hbcmd/keywords/%s.hlp", fn);
	if (_hb_help(hlpfn, 1200) < 0) hbscrn_printf("HELP: Cannot open %s\n", hlpfn); //2 min timeout
}

void hbscrn_clr_input(void);
static char curr_path[FULFNLEN] = {0};

static int ln_init(char line[], int retry)
{
	int rc, first_time = 1;
	dbe_comm_buf = line;
	memset(dbe_comm_buf, 0, STRBUFSZ);
again:
	if (conn != HBCONN_INVAL) {
		if ((rc = dbe_get_stat(conn, STAT_CTX_CURR)) < 0) return(0); //sends dbos call periodically to keep connection alive
		if (first_time) {
			char path2[FULFNLEN];
			if (dbe_get_path(conn, 0, path2, FULFNLEN)) { //default path
				if (strcmp(path2, curr_path)) {
					strcpy(curr_path, path2);
					hbscrn_printf("Current Path: %s\n", curr_path);
				}
			}
			if (dbe_errno) hbscrn_printf("(%2d)>", rc); //debug mode
			else {
				char buf[64];
				if (dbe_get_stat(conn, STAT_IN_USE)) hbscrn_printf("%s>", dbe_get_id(conn, ID_ALIAS, 0, buf, 64));
				else hbscrn_printf("%2d%s>", rc, dbe_get_stat(conn, STAT_RO)? "-RO" : "");
			}
			first_time = 0;
		}
	}
	else hbscrn_printf("zz>");
	hbscrn_clr_input();
	if (hbscrn_gets(dbe_comm_buf, STRBUFSZ, conn != HBCONN_INVAL)) {
		--retry;
		if (!retry) {
			hbscrn_printf("Input timed out, exiting...\n");
			return(0);
		}
		goto again;
	}
	return(1);
}

int hb_get_token(TOKEN *tok, int reset)
{
	char *p;
	int len = strlen(dbe_comm_buf);
	if (reset) tok->tok_s = tok->tok_len = 0;
	else { tok->tok_s += tok->tok_len; tok->tok_len = 0; }
	p = dbe_comm_buf + tok->tok_s;
	while (isspace(*p) && tok->tok_s<len) { ++p; ++tok->tok_s; }
	while (!isspace(*p) && tok->tok_s+tok->tok_len < len) { ++p; ++tok->tok_len; }
	if (!tok->tok_len) {
		tok->tok_typ = TNULLTOK;
		return(0);
	}
	tok->tok_typ = TID;
	return(1); //not used
}

void hb_debug(void) //no command processing on server, only dbos() calls
{
	TOKEN tok;
	int cmd, kw;
	for (cmd=0; cmd!= QUIT;) {
		if (!ln_init(dbe_comm_line, 10)) break; //10 retries or 4 min
		if (!hb_get_token(&tok, 1)) continue;
		cmd = hb_kw_search(&tok, dbe_comm_buf);
		switch (cmd) {
		case HBKW_QUIT:
			continue;
		case HBKW_SELECT:
			if (!hb_get_token(&tok, 0)) display_error(conn, "Missing Context ID", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
			else {
				char *tp;
				long id = strtol(dbe_comm_buf + tok.tok_s, &tp, 10);
				if (id <= 0 || id > MAXCTX + 1 || (tp - dbe_comm_buf != tok.tok_s + tok.tok_len)) 
					display_error(conn, "Invalid Context ID", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
				else dbe_select(conn, (int)id - 1);
			}
			break;
		case HBKW_DISPLAY:
			if (!hb_get_token(&tok, 0)) kw = RECORD;
			else kw = hb_kw_search(&tok, dbe_comm_buf);
			switch (kw) {
			case HBKW_STRUCTURE:
				if (!dbe_get_stat(conn, STAT_IN_USE)) display_error(conn, "No TABLE in use", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
				else dbe_disp_structure(conn);
				break;
			case HBKW_MEMORY:
				dbe_disp_memory(conn);
				break;
			case HBKW_RECORD:
				if (!dbe_get_stat(conn, STAT_IN_USE)) display_error(conn, "No TABLE in use", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
				dbe_disp_record(conn);
				break;
			case HBKW_STACK: {
				int lvl = 0;
				char buf[STRBUFSZ];
				HBVAL *v;
				hbscrn_puts("Level  Type   Value\n");
				hbscrn_puts("-----------------------------------------------------------------------------\n");
				while (v = dbe_peek_stack(conn, lvl, DONTCARE, buf, STRBUFSZ)) {
					hbscrn_printf("%3d    ", lvl);
					switch (v->valspec.typ) {
					case CHARCTR:
						hbscrn_printf("  C    %.*s\n", v->valspec.len, v->buf);
						break;
					case LOGIC:
						hbscrn_printf("  L    %.%c.\n", v->buf[0]? 'T' : 'F');
						break;
					case NUMBER:
						if (val_is_real(v)) hbscrn_printf("  N(f) %.8lg\n", get_double(v->buf));
						else if (!v->numspec.deci) hbscrn_printf("  N(0) %ld\n", get_long(v->buf));
						else hbscrn_printf("  N(%d) %.*lf", v->numspec.deci, v->numspec.deci, (double)get_long(v->buf) / power10(v->numspec.deci));
						break;
					case DATE:
						hbscrn_printf("  D    %ld", get_long(v->buf));
						break;
					default:
						hbscrn_puts("  U    \n");
						break;
					}
					++lvl;
				}
				if (!lvl) hbscrn_puts("<Empty>\n");
				break; }
			default:
				display_error(conn, "Invalid DISPLAY parameter", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
				break;
			}
			break;
		default:
			display_error(conn, "Invalid command", dbe_comm_buf, 1, tok.tok_s, tok.tok_len);
			break;
		}
	}
}

void clr_scr(void);

#ifdef KW_MAIN
int escape = 0;
#else
extern int escape;
#endif
static void disable_escape(void);

static void esc_timer_handler(int signum)
{
	//if (hb_chk_escape()) dbe_set_escape(conn);
	if (hb_chk_escape()) {
		escape = 1;
		disable_escape();
	}
}

#include <sys/time.h>

static struct itimerval esc_timer = { {0, 250000}, {0, 250000}};

static void enable_escape(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &esc_timer_handler;
	sigaction(SIGVTALRM, &sa, NULL);
	esc_timer.it_value.tv_usec = 250000;
	setitimer(ITIMER_VIRTUAL, &esc_timer, NULL);
}

static void disable_escape(void)
{
	esc_timer.it_value.tv_usec = 0;
	setitimer(ITIMER_VIRTUAL, &esc_timer, NULL);
}

#define MAXCMDQ	10

#define is_sleeping(conn)	(conn == HBCONN_INVAL)
#define cmdq_reset()		(cmdq[next = 0] = 0)

void _hb(void)
{
	int cmd, cmdq[MAXCMDQ], next;
	TOKEN tok;
	char sockname[VNAMELEN + 1] = { 0 };
	for (cmdq_reset(),cmd=cmdq[next++];;cmd=cmdq[next++]) {
		if (dbe_errno) {
			if (hb_yesno("Do you wish to debug")) hb_debug();
			if (hb_yesno("Do you wish to quit")) {
				dbe_end_session(&conn);
				exit(1);
			}
			dbe_clr_err(conn, 1);
			hbscrn_puts("Error cleared\n");
			cmdq_reset();
			continue;
		}
		if (sys_break()) ctl_c();
		if (!cmd) {
			cmdq_reset();
			if (!ln_init(dbe_comm_line, is_admin? 60 : 5)) break;
			if (is_sleeping(conn)) {	/** sleeping connection **/
				if (hb_get_token(&tok, 1)) cmd = KW_UNKNOWN;
			}
			else {
				escape = 0;
				disable_escape();
				if (dbe_cmd_init(conn,dbe_comm_buf)) continue;
				cmd = dbe_get_command(conn, &tok);
			}
		}
		if (!cmd) continue;
		else if (cmd == EOF) {
			if (dbe_errno) {
				cmdq_reset();
				continue;
			}
			break;
		}
		if (cmd == RESET_CMD) {
			if (hb_yesno("Are you sure")) dbe_sys_reset(conn);
			continue;
		}
		else if (cmd == QUIT_CMD) {
			char buf[TRANSID_LEN + 1], *tid = dbe_transaction_get_id(conn, buf);
			if (tid) {
				hbscrn_printf("Transaction %s is still active. do COMMIT/ROLLBACK/CANCEL to finish\n", tid);
				continue;
			}
			clr_scr();
			hbscrn_printf("*** END RUN %s\n\n", version);
			dbe_end_session(&conn);
			exit(0);
		}
		else if (cmd == SLEEP_CMD) cmd = HBKW_SLEEP;
		else if (cmd == KW_COMMENT) continue;
		else if (cmd == KW_UNKNOWN) cmd = hb_kw_search(&tok, dbe_comm_buf);
		switch (cmd) {
			case HBKW_QUIT:	/** only when sleeping **/
				if (is_sleeping(conn)) {
					cmdq[next] = HBKW_WAKEUP;
					cmdq[next + 1] = QUIT;
					cmdq[next + 2] = 0;
					continue;
				}
				exit(0);
			case HBKW_HELP:
				dbe_get_token(conn, dbe_comm_buf, &tok);
				if (tok.tok_typ == TNULLTOK) hb_help(HBKW_HELP, &tok);
				else {
					int i = dbe_kw_search(conn, NULL);
					if (i < 0) i = hb_kw_search(&tok, dbe_comm_buf);
					hb_help(i, &tok);
				}
				break;
			case HBKW_SHELL:
				if (is_admin) {
					hbscrn_end();
					system("/bin/bash");
					hbscrn_init();
					hbscrn_printf("%s\n%s\n", version, copyright);
				}
				else hbscrn_printf("Permission denied\n");
				break;
			case HBKW_SLEEP:
			case HBKW_WAKEUP:
			case HBKW_RESUME:
			case HBKW_TAKEOVER: {
				static int save_sid = -1;
				hb_get_token(&tok, 0);
				if (tok.tok_typ == TNULLTOK && (cmd == HBKW_TAKEOVER || cmd == HBKW_RESUME)) {
					hb_error(conn, MISSING_VAR, dbe_comm_buf, tok.tok_s, tok.tok_len);
					break;
				}
				if (tok.tok_typ != TNULLTOK) {
					if (tok.tok_typ != TID) {
						hb_error(conn, INVALID_TYPE, dbe_comm_buf, tok.tok_s, tok.tok_len);
						break;
					}
					sprintf(sockname, "%.*s", tok.tok_len, dbe_comm_buf + tok.tok_s);
				}
				if (cmd == HBKW_SLEEP) {
					if (!is_sleeping(conn)) dbe_sleep(&conn, 300);
					else display_msg("Already sleeping!", NULL);
					if (is_sleeping(conn) && sockname[0]) {
						if (hb_ipc_wait(sockname, sid, 60) < 0) break;
						sockname[0] = '\0';
						cmdq[next] = HBKW_WAKEUP; cmdq[next + 1] = 0;
						continue;
					}
				}
				else if (cmd == HBKW_WAKEUP) {
					if (sockname[0]) {
						hb_ipc_resume(sockname);
						sockname[0] = '\0';
					}
					else if (is_sleeping(conn)) {
						conn = dbe_init_session((HBSSL_CTX)NULL, login, hb_port, stamp, 0, &sid);
						if (is_sleeping(conn)) exit(1);
						hbscrn_printf("Session ID: %ld\n", sid);
						dbe_inst_usr(conn);
						dbe_set_talk(conn, 1);
					}
					else display_msg("Already awake!", NULL);
				}
				else if (cmd == HBKW_TAKEOVER) { //sockname[0] is not NULL
					int tsid;
					if (!is_sleeping(conn)) dbe_sleep(&conn, 300);
					if (!is_sleeping(conn)) display_msg("SLEEP command failed:", sockname);
					else {
						tsid = hb_ipc_get_sid(sockname);
						sockname[0] = '\0';
						if (tsid >= 0) {
							save_sid = sid;
							sid = tsid;
						}
						cmdq[next] = HBKW_WAKEUP; cmdq[next + 1] = 0;
					}
				}
				else { //RESUME
					if (save_sid < 0) display_msg("Session not saved:", sockname);
					else {
						if (!is_sleeping(conn)) dbe_sleep(&conn, 300);
						if (!is_sleeping(conn)) display_msg("SLEEP command failed:", sockname);
						else {
							sid = save_sid; save_sid = -1;
							cmdq[next] = HBKW_WAKEUP; cmdq[next + 1] = HBKW_WAKEUP; cmdq[next + 2] = 0;
						}
					}
				}
				break; }
			case HBKW_SERIAL: {
				char sername[FNLEN + 8];
				long serial;
				dbe_get_token(conn, dbe_comm_buf, &tok);
				if (tok.tok_typ != TID) hb_error(conn, MISSING_VAR, dbe_comm_buf, tok.tok_s, tok.tok_len);
				sprintf(sername, "%.*s", tok.tok_len, dbe_comm_buf + tok.tok_s);
				dbe_get_token(conn,dbe_comm_buf, &tok);
				if (tok.tok_typ == TID) {
					if (strncasecmp("TO", dbe_comm_buf + tok.tok_s, tok.tok_len)) {
						hb_error(conn, MISSING_TO, dbe_comm_buf, tok.tok_s, tok.tok_len);
						break;
					}
					dbe_get_token(conn, dbe_comm_buf, &tok);
				}
				if (tok.tok_typ != TINT ||
					(serial = atol(dbe_comm_buf + tok.tok_s)) <= 0) {
						hb_error(conn, INVALID_VALUE, dbe_comm_buf, tok.tok_s, tok.tok_len);
						break;
				}
				dbe_set_serial(conn, sername, serial);
				break; }
			default:
				if (cmd == KW_UNKNOWN) {
					hb_error(conn,COMMAND_UNKNOWN, dbe_comm_buf, tok.tok_s, tok.tok_len);
					continue;
				}
				if (is_sleeping(conn)) display_msg("Connection not active!", NULL);
				enable_escape();
				dbe_hb_cmd(conn, cmd, NULL, &tok);
				if (cmd == SU_CMD) {
					char username[1024];
					struct passwd *pwe;
					if (dbe_get_user(conn, username)) {
						pwe = getpwnam(username);
						homedir = pwe->pw_dir;
					}
				}
				if (cmd != QUIT_CMD) dbe_cmd_end(conn);
				break;
		}
	}
}

void hist_free_entries(void);

static void cleanup(void)
{
	//dbe_set_escape(conn); //stop any pcode execute
	hist_free_entries();
	unblock_breaks();
}

static char *prgname;
extern char *error_msgs[];

void usage(void)
{
	printf("Usage: %s [-host <hostname/ip>] [-user <username>] [-port <port>] [-path <database_path>] [-i <n>] [prgram filename]\n",prgname);
	exit(1);
}

main(int argc, char *argv[])
{
	char *tp, *butler = NULL, *user = NULL, *dbpath = NULL;
	struct passwd *pwd = NULL;
	int flags, i, excl = 0;
	if (argc > 1) for (i=1; i<argc; ++i) {
#ifdef OPT_NDBUF
		//Since the control struct DBF contains only pointers to headers and record buffer
		//it is no longer necessary to manage the memory they occupy.
		if (!strcmp(argv[i], "-w")) {
			++i;
			nwa = atoi(argv[i] + 2);
			if (nwa < MIN_NDBUF) nwa = MIN_NDBUF;
		}
		else 
#endif
		if (!strcmp(argv[i], "-i")) {
			++i;
			nibuf = atoi(argv[i] + 2);
			if (nibuf < MIN_NIBUF) nibuf = MIN_NIBUF;
		}
		else if (!strcmp(argv[i], "-path")) {
			if (dbpath) usage();
			else {
				++i;
				if (i >= argc) usage();
				dbpath = argv[i];
			}
		}
		else if (!strcmp(argv[i], "-port")) {
			if (hb_port > 0) usage();
			else {
				++i;
				if (i >= argc) usage();
				hb_port = atoi(argv[i]);
				if (hb_port <= 0) usage();
			}
		}
		else if (!strcmp(argv[i], "-user")) {
			if (user) usage();
			else {
				++i;
				if (i >= argc) usage();
				user = argv[i];
			}
		}
		else if (!strcmp(argv[i], "-host")) {
			if (butler) usage();
			else {
				++i;
				if (i >= argc) usage();
				butler = argv[i];
			}
		}
		else if (!strcmp(argv[i], "-excl") || !strcmp(argv[i], "-exclusive")) {
			excl = 1;
			++i;
			if (i > argc) usage();
		}
		else usage();
	}

	if (!butler) butler = "localhost";
	flags |= HB_SESS_FLG_SUID | (excl? 0 : HB_SESS_FLG_MULTI) | HB_SESS_FLG_SYSPRG;
	if (user) {
		pwd = getpwnam(user);
		if (!pwd) {
			fprintf(stderr, "Invalid user: %s\n", user);
			exit(1);
		}
	}
	else {
		pwd = getpwuid(geteuid());
		if (!pwd) {
			fprintf(stderr, "Cannot determine effective user.\n");
			exit(1);
		}
		user = pwd->pw_name;
	}
	if (pwd->pw_uid == geteuid()) flags |= HB_SESS_FLG_SETAUTH;
	else flags |= HB_SESS_FLG_AUTH; //not the login user, do authenticate
	strcpy(hb_user, pwd->pw_name);
	homedir = pwd->pw_dir;
	if (!dbpath) dbpath = "";
	hbscrn_init();
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, sigpipe);
	atexit(cleanup);
	dbe_set_login(login, butler, dbpath, user, "", flags);
	hbscrn_printf("%s\n%s\n", version, copyright);
	conn = dbe_init_session((HBSSL_CTX)NULL, login, hb_port, nwa, nibuf, &sid);
	if (conn != HBCONN_INVAL) {
		is_admin = !!(0x8000 & sid);
		sid &= 0x7fff;
		stamp = dbe_get_stamp(conn);
		hbscrn_printf("User:%c%s\nSession ID: %ld\n", is_admin? '*' : ' ', user, sid);
		dbe_inst_usr(conn);
		dbe_set_talk(conn, 1);
		_hb();
	}
	else hbscrn_printf("Login failed: %s\n", error_msgs[sid - 1]);
#ifdef HBSCRN_CURSES
	hbscrn_wait("Press any key to continue...", 0);
#endif
}
