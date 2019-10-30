/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/sys.c
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
#include <unistd.h>
#include <sys/stat.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "hbtli.h"
#include "hbapi.h"
#include "dbe.h"

char progid[128];
#ifdef DEMO
char *_progid = "HB*Engine (%s %d User) Version DEMO 5.0";
#else
char *_progid = "HB*Engine (%s %d User) Version 5.0";
#endif

/** procedure nested levels, for memory variable masking **/
extern int curr_lvl;

void sys_reset(void)
{
	if (frmfctl.fp) fclose(frmfctl.fp);
	if (profctl.fp) fclose(profctl.fp);
	stack_init();
	var_init();
	memset(cmd_line, 0, STRBUFSZ);
	nextcode = nextdata = 0;
	db_init0(1);
	hb_errcode = 0;
	get_root_path(defa_path, FULFNLEN);
}

static void clr_err(void)
{
	if (!restricted) return;
	f_unlock(); //unlock any record/hdr of current table
	if (is_db_err(hb_errcode)) clr_db_err(code2errno(hb_errcode));
	else if (curr_cmd == curr_ctx->filter) del_exp(EXP_FILTER);
	else if (curr_cmd == curr_ctx->relation) del_exp(EXP_RELA);
	clear_proc_stack();
	stack_init();
	hb_errcode = restricted = 0;
	ctx_select(0);
}

static int is_call_allowed(int callid)
{
	switch (callid) {
	case DBE_CLR_ERR:
	case DBE_SELECT:
	case DBE_IS_EOF:
	case DBE_IS_BOF:
	case DBE_IS_DELETE:
	case DBE_IS_FOUND:
	case DBE_IS_SET:
	case DBE_GET_STAT:
	case DBE_GET_ID:
	case DBE_GET_TRANS_ID:
	case DBE_GET_HDR:
	case DBE_GET_REC:
	case DBE_GET_HDRLEN:
	case DBE_GET_META_HDR:
	case DBE_GET_RECSZ:
	case DBE_DISPMEMO:
	case DBE_DISPSTRU:
	case DBE_DISPREC:
	case DBE_GET_PDATA:
	case DBE_DATE_TO_CHAR:
	case DBE_NUM_TO_CHAR:
	case DBE_GET_WDECI:
	case DBE_STACK_OP:
		return(1);
	default:
		return(0);
	}
}

#define LIB_SYS_NAME	"/_sys"

static void set_sys_procedure(void)
{
	char profn[FULFNLEN];
	sprintf(profn, "%s/%s.pro", rootpath, LIB_SYS_NAME);
	if (euidaccess(profn, R_OK)) {
		sprintf(profn, "%s/%s.prg", rootpath, LIB_SYS_NAME);
		if (euidaccess(profn, R_OK)) return;
	}
	int n = 1;
	n |= P_SYS;
	push_string(LIB_SYS_NAME, strlen(LIB_SYS_NAME));
	push_int(n);
	set_to(PROC_TO);
}

void log_printf(char *fmt, ...);
char *usr2str(int usr_fcn);

void log_hb_error(int hberrno, char *str, int is_cmd, int arg)
{
	int pid = getpid();
	log_printf("[%d]HB*Engine Error (%d)\n", pid, hberrno);
	if (is_cmd) log_printf("[%d][%s]:(%d,%d)\n", pid, str, LO_DWORD(arg), HI_DWORD(arg));
	else {
		if (str) log_printf("[%d]%s\n", pid, str);
		if (arg != -1) log_printf("[%d]HB*pcode:(%d,%d)\n", pid, LO_DWORD(arg), HI_DWORD(arg));
		if (is_usr_err(hberrno)) log_printf("[%d]%s\n", pid, usr2str(hberrno - USR_ERROR));
		else if (!stack_empty()) {
			char outbuf[STRBUFSZ];
			log_printf("[%d]TOS:%s\n", pid, val2str(topstack, outbuf, STRBUFSZ));
		}
	}
}

int hb_wakeup(int sid, int stamp, char *userspec);

int db3w_admin = 0;

int dbesrv(HBREGS *hb_regs)
{
	char *bx = hb_regs->bx;
	int ax = hb_regs->ax;
	int cx = hb_regs->cx;
	int dx = hb_regs->dx;
	int ex = hb_regs->ex;
	int fx = hb_regs->fx;
	BYTE pcode_buf[CBLOCKSZ];
	if (ax == DBE_LOGIN) { //must be the 1st dbesrv() call
		int sid, rc;
#ifndef HBLIB
		char *spath, *userspec, *passwd;
		if (!cx) {
			hb_regs->ax = LOGIN_NOUSER;
			goto login_fail;
		}
		hb_regs->cx = 0;
		spath = bx; userspec = spath + strlen(spath) + 1, passwd = userspec + strlen(userspec) + 1;
		sid = hb_regs->fx;

		rc = setjmp(env);
		if (rc) {
			hb_regs->ax = rc;
login_fail:
			hb_regs->cx = 0;
			hb_regs->dx = -1;
			return(-1);
		}

		if (sid != -1 && hb_wakeup(sid, dx, userspec + 1) < 0) { //dx has the stamp, 1st byte of userspec is flags
			hb_regs->ax = LOGIN_NOSESS;
			goto login_fail;
		}
		//if successful hb_wakeup() never returns, dbesrv() returns via hb_sleep() after wakeup. 
		db3w_admin = set_session_user(userspec, spath, passwd); //first byte of userspec is flags
		if (db3w_admin < 0) {
			hb_regs->ax = LOGIN_NOUSER;
			goto login_fail;
		}
#endif
		maxndx = dx; //new session, sid == -1
		exe = TRUE;
		nwidth = DEFAULTWIDTH;
		ndeci = DEFAULTDCML;
		usr_conn = NULL;
		if (db_init() || hb_init()) {
			hb_release_mem();
			db_release_mem();
			hb_regs->ax = NOMEMORY;
			return(-1);
		}
		if (*userspec & HB_SESS_FLG_SYSPRG) set_sys_procedure();
#ifndef HBLIB
		hb_regs->ex = get_session_id();
		hb_regs->fx = getpid();
#else
		hb_regs->ex = 0;
		hb_regs->fx = getpid();
#endif
		if (db3w_admin) hb_regs->ex |= 0x8000;
		return(0);
	}
	else if (ax == DBE_LOGOUT) {
		sys_reset();
		hb_regs->ex = 0;		/* initiated by client */
logoff:
		do_onexit_proc();
		hb_release_mem();
		db_release_mem();
		return(0);
	}
	else {
		jmp_buf save_env;
		int err;
		if (restricted && !is_call_allowed(ax)) {
invalid:
			hb_regs->dx = ax;
			hb_regs->ax = INVALID_CALL;
			hb_regs->cx = 0;
			hb_regs->ex = 0;
			return(-1);
		}
		if (ax == DBE_CLR_ERR) {
			clr_err();
			return(0);
		}
		memcpy(save_env, env, sizeof(jmp_buf)); //for nexted dbesrv() calls
		err = setjmp(env);
		if (!hb_errcode) hb_errcode = err;
		if (err) {
			restricted = 1;
			memcpy(env, save_env, sizeof(jmp_buf));
			if (hb_errcode == -1) {	 /* quit is called */
				sys_reset();
				hb_regs->ax = DBE_LOGOUT;
				hb_regs->cx = 0;
				hb_regs->ex = 1;		/* initiated by engine */
				goto logoff;
			}
			hb_regs->ax = code2errno(err);
			if (curr_cmd) { //if command line is available
				hb_regs->bx = curr_cmd;
				hb_regs->cx = strlen(curr_cmd) + 1;
				hb_regs->dx = MAKE_DWORD(currtok_s, currtok_len);
				log_hb_error(hb_errcode, curr_cmd, 1, hb_regs->dx);
				hb_regs->ex = 0;
			}
			else if (isset(PROC_TO)) { //if we are executing a procedure
				char *ctx_str = get_proc_context_str();
				hb_regs->bx = ctx_str;
				hb_regs->cx = ctx_str? strlen(ctx_str) + 1 : 0;
				if (!is_usr_err(hb_errcode) && pcode) hb_regs->dx = MAKE_DWORD(pcode[pc].opcode, pcode[pc].opnd);
				else hb_regs->dx = -1;
				log_hb_error(hb_errcode, ctx_str, 0, hb_regs->dx);
				hb_regs->ex = 1;
			}
			else {
				hb_regs->cx = 0;
				if (!is_usr_err(hb_errcode) && pcode) hb_regs->dx = MAKE_DWORD(pcode[pc].opcode, pcode[pc].opnd);
				else hb_regs->dx = -1;
				log_hb_error(hb_errcode, NULL, 0, hb_regs->dx);
				hb_regs->ex = 2;
			}
			return(-1);
		}
		switch (ax) {
		case DBE_RESET:
			sys_reset();
			break;
		case DBE_WHOAREYOU:
			bx = progid;
			cx = strlen(progid) + 1;		/** include NULL byte **/
			break;
		case DBE_INST_USR:	/** only comes through for linked in clients **/
#ifdef HBLIB
			usr_conn = 1;	/** dummy value to activate usr_tli(), which simply calls usr() **/
#else
			cx = 0;
			ext_ndx = 0x01 & (ex>>1);
#endif
			break;
		case DBE_SLEEP:
#ifndef HBLIB
			curr_cmd = NULL;
			hb_sleep(dx = get_session_id(), ex);
#endif
			break;
		case DBE_CMD:
		case DBE_CMD_INIT:
			memcpy(cmd_line, bx, cx);
			cmd_init(cmd_line, code_buf, data_buf, NULL);
			cx = 0;
			if (ax == DBE_CMD_INIT) break;
		case DBE_GET_COMMAND:
			ex = get_command(1);
			if (ax == DBE_GET_COMMAND) {
				ax = ex;
				dx = MAKE_DWORD(currtok_s, currtok_len);
				break;
			}
		case DBE_CMD2: //command ID in ex
			if (!process_cmd(ex, pcode_buf, CBLOCKSZ)) hb_cmd(ex, pcode_buf, CBLOCKSZ);
			chk_nulltok();
			if (ax == DBE_CMD2 || !exe) {	   /*** send back the pseudo code if generated **/
				cx = ((CODE_BLOCK *)pcode_buf)->block_sz; //may be 0
				bx = (char *)pcode_buf;
			}
			if (ax == DBE_CMD2) goto settok;
			break;
		case DBE_CMD_END:
			curr_cmd = NULL;
			break;
		case DBE_GET_META_HDR:
		case DBE_GET_HDR: {
			TABLE *hdr;
			BYTE *xlat_buf;
			valid_chk();
			hdr = get_db_hdr();
			xlat_buf = get_aux_buf();
			if (ax == DBE_GET_HDR) {
				cx = write_db_hdr(hdr, xlat_buf, dx);  /* disk image */
				if (cx > dx) cx = 0; //not enough space at client
			}
			else cx = read_meta_hdr(hdr, xlat_buf, dx);
			bx = (char *)xlat_buf;
			break; }
		case DBE_GET_REC:
			valid_chk();
			if (ex) skip(ex == FORWARD? 1 : -1);
			if (!etof(0)) {
				bx = (char *)get_db_rec();
				cx = get_rec_sz();
			}
			break;
		case DBE_GET_REC_BATCH: {
			char *recbuf, *rp;
			int nrec,rsiz,i;
			valid_chk();
			recbuf = get_aux_buf();
			rsiz = get_rec_sz() + sizeof(recno_t);	/** recno **/
			nrec = MAXBUFSZ / rsiz;
			if (nrec>(int)dx) nrec = (int)dx;
			for (i=0,rp=recbuf; i<nrec && !etof(ex); ++i,rp+=rsiz) {
				put_recno(rp, get_db_recno());
				memcpy(rp + sizeof(recno_t), get_db_rec(), rsiz - sizeof(recno_t));
				skip(ex == FORWARD? 1 : -1);
			}
			bx = recbuf;
			cx = i * rsiz;
			dx = i;
			break; }
		case DBE_GET_HDRLEN:
			valid_chk();
			ax = curr_dbf->dbf_h->hdlen; //on-disk header length
			break;
		case DBE_GET_META_LEN:
			valid_chk();
			ax = curr_dbf->dbf_h->meta_len;
			break;
		case DBE_GET_RECSZ:
			valid_chk();
			ax = get_rec_sz();
			break;
		case DBE_GET_RECCNT: {
			recno_t reccnt, recno;
			valid_chk();
			reccnt = get_db_rccnt();
			dx = LO_LWORD(reccnt);
			fx = HI_LWORD(reccnt);
			break;
		case DBE_GET_RECNO: 
			valid_chk();
			recno = get_db_recno();
			dx = LO_LWORD(recno);
			fx = HI_LWORD(recno);
			break; }
		case DBE_GO:
			if (ex) go(MAKE_LWORD(dx, fx));
			else gorec(MAKE_LWORD(dx, fx));
			break;
		case DBE_GO_TOP:
			gotop();
			break;
		case DBE_GO_BOT:
			gobot();
			break;
		case DBE_IS_EOF:
			ax = iseof();
			break;
		case DBE_IS_BOF:
			ax = istof();
			break;
		case DBE_IS_READONLY:
			ax = flagset(readonly);
			break;
		case DBE_GET_TOKEN:
		case DBE_GET_TOK:
			if (ax == DBE_GET_TOKEN) get_token();
			else if (get_tok()) { //cmd line changed. send back the cmd line starting with currtok_s
				bx = &curr_cmd[currtok_s];
				cx = strlen(bx) + 1;
			}
		case DBE_SYNC_TOKEN:
settok:
			ax = curr_tok;
			dx = MAKE_DWORD(currtok_s, currtok_len);
			break;
		case DBE_UNGET_TOKEN:
			rest_token(&last_tok);
			goto settok;
		case DBE_KWSEARCH:
			ax = kw_bin_srch(0, KWTABSZ - 1);
			if (ax < 0) ex = KW_UNKNOWN;
			else ex = keywords[ax].kw_val;
			break;
		case DBE_EXPRESSION:
			expression();
			goto settok;
		case DBE_STACK_OP: 
			if (!ex) ax = topstack == (HBVAL *)exp_stack; //stack empty
			else if (ex & STACK_OUT_MASK) {
				if (ex & STACK_LVL_MASK) {
					HBVAL *v = stack_peek(ex & N_MASK);
					cx = v? val_size(v) : 0;
					bx = (char *)v;
					if (cx > dx) cx = 0;
				}
				else cx = 0;			/** nothing to transfer back **/
				if (ex & STACK_RM_MASK) {
					int i = ex & N_MASK;
					while (i-->=0) pop_stack(DONTCARE);
				}
			}
			else {
				switch (ex & PUSH_TYP_MASK) {
				case PUSH_VAL:
					push((HBVAL *)bx);
					break;
				case PUSH_STRING:
					push_string(bx, cx);
					break;
				case PUSH_INT:
					push_int(dx);
					break;
				case PUSH_REAL:
					push_real(get_double(bx));
					break;
				case PUSH_LOGIC:
					push_logic((ex & N_MASK) != 0);
					break;
				case PUSH_DATE:
					push_date(dx);
					break;
				}
				cx = 0;
			}
			break;
		case DBE_IS_TRUE:
			ax = istrue();
			break;
		case DBE_IS_SET:
			ax = isset(dx);
			break;
		case DBE_IS_DELETE:
			valid_chk();
			ax = isdelete();
			break;
		case DBE_IS_FOUND:
			ax = isfound();
			break;
		case DBE_CODEGEN:
			codegen(LO_DWORD(dx), HI_DWORD(dx));
			break;
		case DBE_EMIT:
			emit(pcode_buf, ex, CBLOCKSZ);
			cx = ((CODE_BLOCK *)pcode_buf)->block_sz;
			bx = (char *)pcode_buf;
			break;
		case DBE_LEXLOAD:
			switch (ex) {
			case L_FULFN:
				ax = loadfilename(FALSE);
				break;
			case L_FN:
				ax = loadfilename(TRUE);
				break;
			case L_STRING:
				ax = loadstring();
				break;
			case L_NUMBER:
				ax = loadnumber();
				break;
			case L_EXPR:
				ax = loadexpr((int)dx);  /* SYNC_TOKEN should be called next */
			}
			break;
		case DBE_PUT_REC:
			valid_chk();
			bx = (char *)get_db_rec();
			save_rec(ex);
			cx = -get_rec_sz();
			break;
		case DBE_STORE_REC:
			valid_chk();
/*** this call necessary since PUT_DB_REC won't get the record until return **/
			ax = store_rec();
			break;
		case DBE_PUT_HDR: 
			valid_chk();
			bx = (char *)get_aux_buf();
			cx = -(int)dx;
			break;
		case DBE_READ_HDR: {
			valid_chk();
			int ctx_id = get_ctx_id();
			TABLE *hdr = get_db_hdr();
			if (hdr->reccnt > 0) db_error0(NOTEMPTY);
			if (ctx_id != AUX) {
				hdr = curr_dbf->dbf_h = realloc(hdr, MAXHDLEN);
				if (!hdr) sys_err(NOMEMORY);
			}
			if (read_db_hdr(hdr, get_aux_buf(), MAXBUFSZ, 0) < 0) db_error0(BAD_HEADER);
			if (ctx_id != AUX) {
				hdr = curr_dbf->dbf_h = realloc(hdr, get_hdr_len(hdr));
				if (!hdr) sys_err(NOMEMORY);
			}
			setflag(dbmdfy);
			setflag(fextd);
			break; }
		case DBE_GET_ID: {
			char *s = NULL;
			switch (ex) {
			case ID_ALIAS:
				s = curr_ctx->alias;
				break;
			case ID_DBF_FN:
				s = curr_ctx->dbfctl.filename;
				break;
			case ID_NDX_FN:
				s = curr_ctx->ndxfctl[fx].filename;
				break;
			case ID_FRM_FN:
				s = frmfctl.filename;
				break;
			}
			if (!s) cx = 0;
			else { 
				int len = strlen(s);
				bx = s;
				if (!len || len + 1 > dx) cx = 0;
				else cx = len + 1;
			}
			break; }
		case DBE_GET_STAT:
			switch (ex) {
			case STAT_CTX_CURR: ax = get_ctx_id() + 1;
				break;
			case STAT_CTX_NEXT: ax = ctx_next_avail() + 1;  /** ax==0 if non available **/
				break;
			case STAT_IN_USE: ax = chkdb();
				break;
			case STAT_N_NDX: ax = n_indexes();
				break;
			case STAT_ORDER: ax = curr_ctx->order + 1;
				break;
			case STAT_RO:
				if (chkdb() == CLOSED) ax = isset(READONLY_ON) == S_ON;
				else ax = flagset(exclusive)? 0 : (flagset(readonly) ? 1 : 0);
			}
			cx = 0;
			break;
		case DBE_MOD_CODE: {
			int which = ex;
			int what = (int)dx;
			if (what == -1) pcode[which].opnd = nextcode;
			else pcode[which].opnd |= what;
			break; }
		case DBE_SET:
			if (is_onoff(dx)) set_onoff(dx,  ex);
			else {
				push_int((long)ex);
				set_to(dx);
			}
			break;
		case DBE_APBLANK:
			valid_chk();
			_apblank(0);
			break;
		case DBE_SELECT:
			ctx_select(ex);
			break;
		case DBE_SKIP: {
			recno_t n = skip(MAKE_LWORD(dx, fx));
			dx = LO_LWORD(n);
			fx = HI_LWORD(n);
			break; }
		case DBE_GET_PATH: {
			char *p, tmp[FULFNLEN];
			if (ex) p = chkdb() == CLOSED? get_root_path(tmp, FULFNLEN) : form_fn1(1, curr_ctx->dbfctl.filename);
			else p = defa_path;
			bx = p;
			cx = strlen(p) + 1;
			if (cx > dx) cx = 0;
			break; }
		case DBE_GET_FILTER:
		case DBE_GET_LOCATE_EXP:
			bx = ax == DBE_GET_FILTER ? curr_ctx->filter : curr_ctx->locate;
			cx = bx? (strlen(bx) + 1) : 0;
			if (cx > dx) cx = 0;
			break;
		case DBE_GET_NDX: 
			valid_chk(); {
			int n = (int)dx;
			char *fn;
			if ((n > 0) && (n <= MAXNDX) && (fn = curr_ctx->ndxfctl[n - 1].filename)) {
				if (ex) bx = get_ndx_exp(n);
				else {
					char ndxname[FNLEN+1], *tp;
					tp = strchr(fn, '.');
					sprintf(ndxname, "%.*s", tp - fn, fn);
					//bx = (char *)chgcase(ndxname, 1);
					bx = ndxname;
				}
				cx = strlen(bx) + 1;
				if (cx > fx) cx = 0;
			}
			else cx = 0;
			break; }
		case DBE_SEEK:
			find(ex);
			break;
		case DBE_GET_NEXTCODE:
			ax = nextcode;
			break;
		case DBE_DELRCAL:
			del_recall(ex);		   /* dx has the mark, '*', ' ' or '' */
			break;
		case DBE_USE:
			use(ex);
			ax = curr_ctx->dbfctl.fd;
			break;
		case DBE_LOCATE:
			valid_chk();
			if (cx || !(0x80 & ex)) del_exp(EXP_LOCATE);
			if (cx) {
				push_string(bx, cx);
				add_exp(EXP_LOCATE);
				cx = 0;
			}
			if (curr_ctx->locate) {
				put_cond(curr_ctx->locate, curr_ctx->loc_pbuf, LOCPBUFSZ);
				ax = _locate(0x7f & ex, curr_ctx->loc_pbuf);
			}
			else ax = 0;
			break;
		case DBE_GET_WDECI:
			dx = MAKE_DWORD(nwidth, ndeci);
			break;
		case DBE_PCODE_EXE:
			memcpy(pcode_buf, bx, cx);
			execute((CODE_BLOCK *)pcode_buf);
			cx = 0;
			break;
		case DBE_GET_FLDCNT:
			valid_chk();
			ax = get_fcount();
			break;
		case DBE_GET_FLDNO:
			valid_chk();
			findfld(1, &ax);
			break;
		case DBE_EVALUATE:
			ax = evaluate(bx, ex);
			cx = 0;
			break;
		case DBE_INDEX:
			if (cx) {
				char *p = bx;
				push_string(p,strlen(p));
				p += strlen(p) + 1;
				push_string(p, strlen(p));
				_index(dx);
				cx = 0;
			}
			else reindex(dx);
			break;
		case DBE_RW_NDX_HDR:
			ndx_chk();
			bx = (char *)&curr_ndx->ndx_h;
			if (ex) {
				cx = -(int)sizeof(NDX_HDR);
				write_ndx_hdr(curr_ndx->fctl->fd, &curr_ndx->ndx_h);
			}
			else cx = sizeof(NDX_HDR);
			break;
		case DBE_GET_NDX_STAT:
			ndx_chk();
			switch (ex) {
			case NDX_FD:
				ax = curr_ctx->ndxfctl[curr_ctx->order].fd;
				break;
			case NDX_EXP:
			case NDX_FN: {
				char *p = ex == NDX_FN ? form_fn(curr_ctx->ndxfctl[curr_ndx->seq].filename, 1)
									 : curr_ndx->ndx_h.ndxexp;
				bx = p;
				cx = strlen(p) + 1;
				if (cx > dx) cx = 0;
				break; }
			case NDX_TYPE:
				ax = isntype();
			}
			break;
		case DBE_GET_NDX_EXPR: {
			char buf[STRBUFSZ], *ndxname = bx;
			int fd, ext = !!strrchr(ndxname, '.');
			if (curr_ctx->dbfctl.filename) sprintf(buf, "%s%s%s", form_fn1(1, curr_ctx->dbfctl.filename), ndxname, ext? "" : ".idx");
			else sprintf(buf, "%s%s%s", defa_path, ndxname, ext? "" : ".idx");
			fd = s_open(buf, OLD_FILE, 1);
			if (fd >= 0) {
				lseek(fd, (off_t)NDXEXP_OFFSET, 0);
				if (read(fd, buf, MAXKEXPLEN) != MAXKEXPLEN) buf[0] = '\0';
				close(fd);
				bx = buf;
				cx = buf[0]? strlen(buf) + 1 : 0;
			}
			else cx = 0;
			break; }
		case DBE_GET_NDX_KEY: {
			NDX_ENTRY *ne = (NDX_ENTRY *)get_aux_buf();
			ndx_chk();
			if (!ndx_get_key(1, ne)) cx = 0; //null key
			else {
				bx = ne->key;
				cx = curr_ndx->ndx_h.keylen;
			}
			break; }
		case DBE_GET_DBT_FD:
			ax = curr_ctx->dbtfd;
			break;
		case DBE_DATE_TO_CHAR: {
			static char temp[MAXDIGITS + 1];
			HBVAL *v = (HBVAL *)bx;
			if (v->valspec.typ != DATE) syn_err(TYPE_MISMATCH);
			number_to_date(ex, get_date(v->buf), temp);
			bx = temp;
			cx = strlen(temp) + 1;
			break;
		case DBE_NUM_TO_CHAR: {
			number_t num;
			HBVAL *v = (HBVAL *)bx;
			short width = LO_DWORD(dx), deci = HI_DWORD(dx); //unsigned to signed for -1
			if (v->valspec.typ != NUMBER) syn_err(TYPE_MISMATCH);
			if (!v->valspec.len) {
				if (isset(NULL_ON)) {
					bx = "<NULL>";
					cx = 7;
					break;
				}
				num.n = 0;
				num.deci = 0;
			}
			else {
				num.deci = v->numspec.deci;
				if (val_is_real(v)) num.r = get_double(v->buf);
				else num.n = get_long(v->buf);
			}
			num2str(num, width < 0? nwidth : width, deci < 0? ndeci : deci, temp);
			bx = temp;
			cx = strlen(temp) + 1;
		break; } }
		case DBE_CHAR_TO_NUM:
			push((HBVAL *)bx);
			val_(1);
			cx = 0;
			break;
#ifdef SHARE_FD
		case DBE_IS_ONLYCOPY:
			ax = find_dbfd() == -1;
			break;
#endif
		case DBE_REC_MARK:
			ax = *get_db_rec();
			break;
		case DBE_CHECK_OPEN:
			ax = checkopen(bx);
			cx = 0;
			break;
		case DBE_DEL_FILE:
			push_string(bx, cx);
			delfil(1);
			break;
		case DBE_REN_FILE:
			renfil(dx);
			break;
		case DBE_FILE_OPEN:
			ax = file_open(ex, cx? bx : NULL);
			cx = 0;
			break;
		case DBE_FILE_SEEK: {
			off_t offset = lseek(ex, (off_t)MAKE_LWORD(dx, fx), 0);
			cx = 0;
			dx = LO_LWORD(offset);
			fx = HI_LWORD(offset);
			break; }
		case DBE_FILE_READ:
			if (dx > MAXRECSZ) ax = -1;
			else ax = read(ex, bx = get_aux_buf(), dx);
			if (ax >= 0) {
				if (!fx) {   /* non-binary (text) */
					for (cx=0; cx<ax; ++cx) if (bx[cx] == EOF_MKR) { //legacy DOS
						ax = ++cx;
						break;
					}
				}
				else cx = ax;
			}
			else cx = 0;
			break;
		case DBE_FILE_WRITE:
			ax = write(ex, bx, cx);
			cx = 0;
			break;
		case DBE_FILE_CLOSE:
			ax = file_close(ex);
			cx = 0;
			break;
		case DBE_PROC_LEVEL:
			ax = curr_lvl = ex;
			break;
		case DBE_RLOCK:
			ax = !r_lock(curr_ctx->crntrec, ex? HB_WRLCK : HB_RDLCK, 1);
			break;
		case DBE_FLOCK:
			ax = !f_lock(ex);
			break;
		case DBE_UNLOCK:
			unlock_all();
			break;
		case DBE_FILE_DIR:
			bx = get_dirent(ex, (int)dx);
			cx = bx? DIRENTLEN : 0;
			break;
#ifndef HBLIB
		case DBE_GET_STAMP:
			dx = hb_session_stamp(0);
			break;
#endif
		case DBE_SET_SERIAL:
			ax = hb_set_serial(bx, MAKE_LWORD(dx, fx));
			break;
		case DBE_GET_SERIAL:
			dx = hb_get_serial(bx);
			break;
		case DBE_DISPMEMO:
			dispmemo();
			break;
		case DBE_DISPSTRU:
			dispstru();
			break;
		case DBE_DISPREC:
			disprec(0);
			break;
		case DBE_GET_PDATA:
			if (pcode) {
				HBVAL *v = (HBVAL *)(&pdata[cb_data_offset(dx)]);
				bx = (char *)v;
				cx = v->valspec.len + 2;
			}
			else cx = 0;
			break;
		case DBE_GET_EPOCH:
			ax = epoch;
			break;
		case DBE_SET_EPOCH:
			epoch = dx;
			break;
		case DBE_SET_ESCAPE:
			escape = 1;
			break;
		case DBE_GET_USER:
			bx = hb_user;
			cx = strlen(hb_user) + 1;
			break;
		case DBE_FORM_REWIND:
			if (frmfctl.fp) {
				rewind(frmfctl.fp);
				ax = 0;
			}
			else ax = -1;
			break;
		case DBE_FORM_SEEK:
			if (frmfctl.fp) ax = fseek(frmfctl.fp, MAKE_LWORD(dx, fx), 0);
			else ax = -1;
			break;
		case DBE_FORM_TELL:
			if (frmfctl.fp) {
				long offset = ftell(frmfctl.fp);
				dx = LO_LWORD(offset);
				fx = HI_LWORD(offset);
			}
			else dx = fx = -1;
			break;
		case DBE_FORM_RDLN:
			if (dx > MAXBUFSZ) dx = MAXBUFSZ;
			ax = -1;
			if (!fgets(bx = get_aux_buf(), dx, frmfctl.fp)) cx = 0;
			else {
				cx = strlen(bx) + 1;
				ax = 0;
			}
			break;
		case DBE_VSTORE:
			push_string(bx, cx);
			_vstore(1); //make it public
			break;
		case DBE_GET_FTIME: {
			uint64_t ftime;
			struct stat st;
			valid_chk();
			if (fstat(curr_ctx->dbfctl.fd, &st) < 0) db_error(DBREAD, curr_ctx->dbfctl.filename);
			ftime = ex == FTIME_A? st.st_atime : (ex == FTIME_M? st.st_mtime : st.st_ctime);
			dx = LO_LWORD(ftime);
			fx = HI_LWORD(ftime);
			break; }
		case DBE_GET_TRANS_ID:
			bx = transaction_get_id();
			cx = strlen(bx) + 1;
			break;
		case DBE_SET_WDECI:
			nwidth = LO_DWORD(dx);
			ndeci = HI_DWORD(dx);
			if (nwidth < MINWIDTH) nwidth = DEFAULTWIDTH;
			if (nwidth > MAXWIDTH) nwidth = MAXWIDTH;
			if (ndeci < 0) ndeci = DEFAULTDCML;
			if (ndeci > MAXDCML) ndeci = MAXDCML;
			break;
		default:
			goto invalid;
		}
		memcpy(env, save_env, sizeof(jmp_buf));
		hb_regs->ax = ax;
		hb_regs->bx = bx;
		hb_regs->cx = cx;
		hb_regs->dx = dx;
		hb_regs->ex = ex;
		hb_regs->fx = fx;
		return(0);
	}
}
