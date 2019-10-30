/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/proc2.c
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

#include <stdlib.h>
#include "hb.h"
#include "dbe.h"
#include "hbkwdef.h"
#include "hbpcode.h"

#define FC_ALL     0x80
#define FC_NEXT    0x40
#define FC_FOR     0x20
#define FC_WHILE   0x10

#define mod_code(which, what)	(pcode[which].opcode = what)

static void mod_op(int which, int what)
{
   if (what == -1) pcode[which].opnd = nextcode;
    else pcode[which].opnd |= what;
}

static void fcodegen(int offset, int r_opcode, int r_oprnd)
{
	codegen(SCOPE_F, 0);
	codegen(GO_, 1);	  /** if not from the top 1 is changed to 5, do nothing **/
	codegen(CALL, 0);	 /** WHILE cond **/
	codegen(JMP_FALSE, 0);
	codegen(JMP_EOF_0, 0);
	codegen(CALL, 0);	 /** FOR condition **/
	codegen(JMP_FALSE, offset + 8); //SKIP
	codegen(r_opcode, r_oprnd);
	codegen(SKIP_, 1);
	codegen(JUMP, offset + 2); //back to WHILE
}

static int process_fcode(int offset, int tok, int *syn_stat)
{
	switch (tok) {
	case ALL:
		if (*syn_stat & FC_ALL) syn_err(EXCESS_SYNTAX);
		*syn_stat |= FC_ALL;
		get_token();
		break;
	case NEXT:
	case PREV:
		if (*syn_stat & FC_NEXT) syn_err(EXCESS_SYNTAX);
		*syn_stat |= FC_NEXT;
		get_token();
		if (curr_tok != TINT) syn_err(INVALID_VALUE);
		mod_op(offset + 0, loadnumber());
		if (tok == PREV) {
			mod_code(offset + 0, SCOPE_B);
			mod_op(offset + 8, 0x81); //-1
		}
		get_token();
		break;
	case FOR:
		if (*syn_stat & FC_FOR) syn_err(EXCESS_SYNTAX);
		*syn_stat |= FC_FOR;
		mod_op(offset + 5, -1); //modify CALL opnd to nextcode which is start of cond
		get_token();
		expression();
		codegen(RETURN, 0);
		break;
	case WHILE:
		if (*syn_stat & FC_WHILE) syn_err(EXCESS_SYNTAX);
		*syn_stat |= FC_WHILE;
		mod_op(offset + 2, -1);
		get_token();
		expression();
		codegen(RETURN, 0);
		break;
	default:
		return(0);
	}
	return(1);
}

static void fmod_op(int offset,int syn_stat)
{
	if (!(syn_stat & FC_NEXT)) mod_op(offset + 0, 0xff);
	if (!(syn_stat & FC_FOR) || !(syn_stat & FC_WHILE)) {
		if (!(syn_stat & FC_FOR)) mod_op(offset + 5, -1);
		if (!(syn_stat & FC_WHILE)) mod_op(offset + 2, -1);
		codegen(PUSH_TF, TRUE);
		codegen(RETURN, 0);
	}
	if (syn_stat & FC_NEXT) mod_op(offset + 1, 4); //(GO_, 4)
	mod_op(offset + 3, -1);
	mod_op(offset + 4, -1);
}

#define A_TEXT			0x80
#define A_FROM			0x40
#define A_FOR			0x20

#define MAXAPLINES		10
       
void process_append(void)
{
	int syn_stat = 0, txtmode = 0, kw;
	get_token();
	kw = kw_search();
	if (kw == BLANK) {
		syn_stat = 1; //force a store_rec() now that NULL fields will not cause index update
		get_token();
		if (curr_tok) { /* allow numeric expression for bulk append **/
			syn_stat++;
			expression();
		}
		codegen(APBLANK, syn_stat);
		return;
	}
	if (kw == FILE_) {
		int n = 0;
		get_token();
		if (!curr_tok) syn_err(MISSING_FILE);
		expression();
		if (kw_search() != WITH) syn_err(MISSING_WITH);
		do {
			if (n >= MAXAPLINES) syn_err(TOOMANYARGS);
			get_token();
			if (!curr_tok) syn_err(MISSING_ARG);
			expression();
			n++;
		} while (curr_tok == TCOMMA);
		codegen(APNDFIL, n + 1);
		return;
	}
	codegen(CALL, 0);	 /** get filename **/
	codegen(AP_INIT, 0);
	codegen(SCOPE_F, 0xff);
	codegen(JMP_EOF_0, 0);
	codegen(CALL, 0);	 /** append condition **/
	codegen(JMP_FALSE, 7);
	codegen(AP_REC, 0);
	codegen(AP_NEXT, 0);
	codegen(JUMP, 3);	 /** go to eof test **/
	while (curr_tok) {
		int kw = kw_search();
		switch (kw) {
		case FROM:
			if (syn_stat & A_FROM) syn_err(EXCESS_SYNTAX);
			syn_stat |= A_FROM;
			mod_op(0,-1);
			get_tok();
			if (!curr_tok) syn_err(MISSING_FILENAME);
			if (curr_tok == TMAC)
				codegen(MACRO_FULFN, loadstring());
			else
				codegen(PUSH, loadfilename(FALSE));
			codegen(RETURN, 0);
			get_token();
			break;
		case FOR:
			if (syn_stat & A_FOR) syn_err(EXCESS_SYNTAX);
			syn_stat |= A_FOR;
			mod_op(4,-1);
			get_token();
			expression();
			codegen(RETURN, 0);
			break;
		case TYPE_:
			if (syn_stat & A_TEXT) syn_err(EXCESS_SYNTAX);
			syn_stat |= A_TEXT;
			get_token();
			if ((kw=kw_search()) != SDF && kw != DELIMITED) syn_err(INVALID_TYPE);
			break;
		case SDF:
			if (txtmode & 0x80) syn_err(EXCESS_SYNTAX);
			txtmode |= 0x80;
			syn_stat |= A_TEXT;
			get_token();
			break;
		case DELIMITED:
			if (txtmode & 0x7f) syn_err(EXCESS_SYNTAX);
			get_token();
			if (kw_search() == WITH) {
				get_token();
				if (currtok_len == 1) txtmode |= curr_cmd[currtok_s];
				else if (kw_search() == BLANK) txtmode |= ' ';
				else syn_err(ILL_DLMTR);
				get_token();
			}
			else txtmode |= '"';
			syn_stat |= A_TEXT;
			break;
		default:
			syn_err(ILL_SYNTAX);
		}
	}
	if (!(syn_stat & A_FROM)) syn_err(MISSING_FROM);
	if (!(syn_stat & A_FOR)) {
		mod_op(4,-1);
		codegen(PUSH_TF, TRUE);
		codegen(RETURN, 0);
	}
	if (txtmode) {
		txtmode |= 0x80;
		mod_op(1, txtmode);
	}
	mod_op(3,-1);
	codegen(AP_DONE, 0);
}

#define C_TEXT          0x8000
#define C_STRUCTURE     0x4000
#define C_TO            0x2000
#define C_FIELDS        0x1000
#define C_ALL           0x0800
#define C_NEXT          0x0400
#define C_FOR           0x0200
#define C_WHILE         0x0100
#define C_OFFSET	0x0080

extern char *curr_cmd;

void process_copy(void)
{
	int syn_stat = 0, txtmode = 0;
	get_token();
	codegen(CALL, 0);	/*** filename **/
	codegen(CALL, 0);	/*** fields list **/
	codegen(CP_INIT, loadint(0));	/*** for non-text copy opnd is pdata containing offset **/
	codegen(GO_, 1);  /** if WHILE 1 will be modified to 3 **/
	codegen(SCOPE_F, 0);
	codegen(CALL, 0);
	codegen(JMP_FALSE, 0);
	codegen(JMP_EOF_0, 0);
	codegen(CALL, 0);	 /*** condition **/
	codegen(JMP_FALSE, 11);
	codegen(CP_REC, 0);
	codegen(CP_NEXT, FORWARD);
	codegen(JUMP, 5);
	while (curr_tok) {
		int kw = kw_search();
		switch (kw) {
		case STRUCTURE:
			if (syn_stat && (syn_stat & C_TEXT)) syn_err(ILL_SYNTAX);
			syn_stat |= C_STRUCTURE;
			mod_op(4, loadint(0));
			get_token();
			break;
		case OFFSET:
			if (syn_stat && (syn_stat & C_TEXT)) syn_err(ILL_SYNTAX);
			syn_stat |= C_OFFSET;
			get_token();
			if (curr_tok != TINT) syn_err(INVALID_VALUE);
			mod_op(2, loadnumber());
			get_token();
			break;
		case EXPORT:
			if (!(syn_stat & C_STRUCTURE)) syn_err(ILL_SYNTAX);
			txtmode = TXT_STRU_EXP; //export structure
			get_token();
			break;
		case TO:
			if (syn_stat & C_TO) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_TO;
			mod_op(0, -1);
			get_tok();
			if (!curr_tok) syn_err(MISSING_FILENAME);
			if (curr_tok == TMAC)
				codegen(MACRO_FULFN, loadstring());
			else
				codegen(PUSH, loadfilename(FALSE)); //allow paths
			codegen(RETURN, 0);
			get_token();
			break;
		case FIELDS: {
			int nflds = 0;
			if (syn_stat & C_FIELDS) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_FIELDS;
			mod_op(1, -1);
			do {
				get_token();
				if (!curr_tok) syn_err(MISSING_FLDNAME);
				if (curr_tok != TID) syn_err(ILL_SYNTAX);
				codegen(PUSH, loadstring());
				++nflds;
				get_token();
			} while (curr_tok == TCOMMA);
			codegen(PUSH_N, nflds);
			codegen(RETURN, 0);
			break; }
		case ALL:
			if (syn_stat & C_STRUCTURE) syn_err(ILL_SYNTAX);
			if (syn_stat & C_ALL) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_ALL;
			get_token();
			break;
		case NEXT:
		case PREV:
			if ((syn_stat & C_STRUCTURE) || (syn_stat & C_ALL)
				 || (syn_stat & C_FOR)) syn_err(ILL_SYNTAX);
			if (syn_stat & C_NEXT) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_NEXT;
			get_token();
			if (curr_tok != TINT) syn_err(INVALID_VALUE);
			mod_op(4, loadnumber());
			if (kw == PREV) {
				mod_code(4, SCOPE_B);
				mod_op(11, BACKWARD); //CP_NEXT
			}
			get_token();
			break;
		case FOR:
			if (syn_stat & C_STRUCTURE) syn_err(ILL_SYNTAX);
			if (syn_stat & C_FOR) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_FOR;
			mod_op(8, -1);
			get_token();
			expression();
			codegen(RETURN, 0);
			break;
		case WHILE:
			if (syn_stat & C_STRUCTURE) syn_err(ILL_SYNTAX);
			if (syn_stat & C_WHILE) syn_err(EXCESS_SYNTAX);
			syn_stat |= C_WHILE;
			mod_op(5, -1);
			get_token();
			expression();
			codegen(RETURN, 0);
			break;
		case TYPE_:
			if (syn_stat & C_TEXT || syn_stat & C_STRUCTURE || syn_stat & C_OFFSET) syn_err(ILL_SYNTAX);
			syn_stat |= C_TEXT;
			get_token();
			if ((kw=kw_search())!=SDF && kw!=DELIMITED) syn_err(ILL_SYNTAX);
			break;
		case SDF:
			if (syn_stat & C_STRUCTURE || syn_stat & C_OFFSET) syn_err(ILL_SYNTAX);
			if (txt_is_set(txtmode)) syn_err(EXCESS_SYNTAX);
			txt_set_mode(txtmode);
			syn_stat |= C_TEXT;
			get_token();
			break;
		case DELIMITED:
			if (syn_stat & C_STRUCTURE || syn_stat & C_OFFSET) syn_err(ILL_SYNTAX);
			if (txt_get_dlmtr(txtmode) != 0) syn_err(EXCESS_SYNTAX);
			txtmode |= '"';
			syn_stat |= C_TEXT;
			get_token();
			break;
		default:
			syn_err(ILL_SYNTAX);
		}
	}
	if (!(syn_stat & C_TO)) syn_err(MISSING_TO);
	if (!txtmode && !(syn_stat & C_OFFSET)) mod_op(2, loadint(0)); //doffset=0
	if (!(syn_stat & C_FIELDS)) {
		mod_op(1, -1);
		codegen(PUSH_N, 0);
		codegen(RETURN, 0);
	}
	if (!(syn_stat & C_FOR) || !(syn_stat & C_WHILE)) {
		if (!(syn_stat & C_WHILE)) mod_op(5, -1);
		if (!(syn_stat & C_FOR)) mod_op(8, -1);
		codegen(PUSH_TF, TRUE);
		codegen(RETURN, 0);
	}
	if ((syn_stat & C_STRUCTURE) || (syn_stat & C_NEXT))
		mod_op(3, 3);
	if (!(syn_stat & C_STRUCTURE) && !(syn_stat & C_NEXT))
		mod_op(4, 0xff);
	if (txtmode) {
		txt_set_mode(txtmode);
		mod_op(2, txtmode);
	}
	mod_op(6, -1);
	mod_op(7, -1);
	codegen(CP_DONE, 0);
}

#define N_TO      0x100
#define N_ALL     0x80

void process_count(void)
{
	int offset;
	int syn_stat = 0;
	int var = -1;

	fcodegen(offset = nextcode, R_CNT, 0);
	get_token();
	while (curr_tok) {
		int this_tok = kw_search();
		if (this_tok == TO) {
				if (syn_stat & N_TO) syn_err(EXCESS_SYNTAX);
				syn_stat |= N_TO;
				get_token();
				if ((curr_tok != TID) && (curr_tok != TMAC)) syn_err(MISSING_VAR);
				var = curr_tok == TID ? loadstring() : 0x8000 | loadstring();
				get_token();
		}
		else if (!process_fcode(offset, this_tok, &syn_stat)) syn_err(ILL_SYNTAX);
	}
	fmod_op(offset, syn_stat);
	if (var != -1) {
		codegen(R_CNT, 1);
		if (0x8000 & var) {
			codegen(MACRO_ID, 0xff & var);
			codegen(VSTORE, OPND_ON_STACK);
		}
		else codegen(VSTORE, var);
	}
	else codegen(R_CNT, 2);
}

void process_create(void)
{
	int mode = 0;
	get_tok();
	if (curr_tok) {
		if (curr_tok == TMAC) codegen(MACRO_FN, loadstring());
		else codegen(PUSH, loadfilename(TRUE));
		++mode;
		get_token();
		if (kw_search() == FROM) { //NOTE: this is in client context as hb_table_create() is an usr() function
			get_tok();
			if (!curr_tok) syn_err(MISSING_FILENAME);
			if (curr_tok == TMAC) codegen(MACRO_FULFN, loadstring());
			else codegen(PUSH, loadfilename(FALSE));
			++mode;
			get_token();
			if (curr_tok && kw_search() == OFFSET) {
				get_token();
				expression();
				++mode;
			}
		}
	}
	codegen(CREAT_, mode);
}

void process_del_rcal(int cmd)
{
	int offset;
	int syn_stat = 0;
	int mark = cmd == RECALL_CMD? ' ' : DEL_MKR;

	get_token();
	if (curr_tok) {
		int kw = kw_search();
		if (kw == FILE_ || kw == DIRLST) {
			int n = 0;
			do {
				get_token();
				if (!curr_tok || curr_tok == TCOMMA) syn_err(MISSING_ARG);
				expression();
				++n;
			} while ((curr_tok == TCOMMA) && (n <MAXARGS));;
			if (n == 0) syn_err(MISSING_FILENAME);
			if (n >= MAXARGS) syn_err(TOOMANYARGS);
			codegen(kw == FILE_? DELFILE : DELDIR, n);
			return;
		}
		fcodegen(offset=nextcode, DEL_RECALL, mark);
		while (curr_tok)
			if (!process_fcode(offset, kw_search(), &syn_stat)) syn_err(ILL_SYNTAX);
		fmod_op(offset,syn_stat);
	}
	else codegen(DEL_RECALL, mark);
	codegen(DEL_RECALL, 0x80 | mark);
}

#define D_ALL    0x80

void process_display(int islist)
{
	int offset;
	int syn_stat = islist? D_ALL : 0;
	int argc = 0;
	int rcomm = 0;
	int fcomm = 0;

	get_token();
	if (curr_tok) {
		int k = kw_search();
		if (k == STRUCTURE) {
			codegen(DISPSTR, 0);
			get_token();
			return;
		}
		else if (k == MEMORY) {
			codegen(DISPMEM, 0);
			get_token();
			return;
		}
		else if (k == INDEX) {
			codegen(DISPNDX, 0);
			get_token();
			return;
		}
		else if (k == STATUS_) {
			codegen(DISPSTAT, 0);
			get_token();
			return;
		}
		else if (k == FILE_) {
			get_token();
			if (curr_tok) {
				int mode = 0;
				expression();
				if (curr_tok) {
					k = kw_search();
					if (k == HEX) mode = 1;
					else if (k != ASC) syn_err(ILL_SYNTAX);
					get_token();
				}
				codegen(DISPFILE, mode);
				return;
			}
			else syn_err(MISSING_FILENAME);
		}
		else if ((k != ALL) && (k != NEXT) && (k != PREV) && (k != FOR) && (k != WHILE)) {
			rcomm = 1;
			codegen(JUMP, 1);	 /** to be modified later if not single rec **/
			while (curr_tok) {
				codegen(PUSH, loadexpr(0));
				++argc;
				if (curr_tok == TCOMMA) get_token();
				else break;
			}
			codegen(DISPREC, islist? 0x80 | argc : argc);
		}
	}
	if (islist || curr_tok) {
		if (rcomm) {
			codegen(RETURN, 0);
			mod_op(0, -1);
		}
		fcomm = 1;
		fcodegen(offset=nextcode, CALL, 1);
		while (curr_tok) if (!process_fcode(offset, kw_search(), &syn_stat)) {
			if (!rcomm) {
				mod_op(7, -1);
				codegen(GO_, 3);
				while (curr_tok) {
					codegen(PUSH, loadexpr(0));
					++argc;
					if (curr_tok == TCOMMA) get_token();
					else break;
				}
				codegen(DISPREC, islist? 0x80 | argc : argc);
				codegen(RETURN, 0);
				rcomm = 1;
			}
			else syn_err(ILL_SYNTAX);
		}
		if (!rcomm) {
			mod_op(7, -1);
			codegen(GO_, 3);
			codegen(DISPREC, islist? 0x80 : 0);
			codegen(RETURN, 0);
			rcomm = 1;
		}
		if (fcomm) {
		}
		fmod_op(offset, syn_stat);
	}
	if (!rcomm) codegen(DISPREC, 0);
}

#define I_ON                0x01
#define I_TO                0x02
#define I_QS                0x04

void process_index(void)
{
	int syn_stat = 0;
	int kw;
	int towhat = -1;
	int qsort = 0;
	get_token();
	while (curr_tok) {
		kw = kw_search();
		if (kw == ON) {
			if (syn_stat & I_ON) syn_err(EXCESS_SYNTAX);
			get_token();
			codegen(PUSH, loadexpr(1));
			syn_stat |= I_ON;
		}
		else if (kw == TO) {
			if (syn_stat & I_TO) syn_err(EXCESS_SYNTAX);
			get_tok();
			if (!curr_tok) syn_err(BAD_FILENAME);
			if (syn_stat & I_ON) {
				if (curr_tok == TMAC)
					codegen(MACRO_FN, loadstring());
				else
					codegen(PUSH, loadfilename(TRUE));
			}
			else towhat = curr_tok == TMAC ? (0x8000 | loadstring()) : loadfilename(TRUE);
			syn_stat |= I_TO;
			get_token();
		}
		else if (kw == USING || kw == QSORT) {
			if (syn_stat & I_QS) syn_err(EXCESS_SYNTAX);
			if (kw == USING) {
				get_token();
				kw = kw_search();
				if (kw != QSORT) syn_err(MISSING_PARM);
			}
			syn_stat |= I_QS;
			qsort = 1;
			get_token();
		}
		else syn_err(ILL_SYNTAX);
	}
	if (!(syn_stat & I_ON)) syn_err(MISSING_ON);
	if (!(syn_stat & I_TO)) syn_err(MISSING_TO);
	if (towhat != -1) {
		if (0x8000 & towhat) codegen(MACRO_FN, 0x7fff & towhat);
		else codegen(PUSH, towhat);
	}
	codegen(INDEX_, qsort);
}

void process_input(int cmd)
{
	get_token();
	if (curr_tok) {
		if (kw_search() != TO) {
			expression();
			codegen(TYPECHK, CHARCTR);
			codegen(PRNT, 0);	 /** no <CR> **/
		}
		codegen(INP, 0);
		if (kw_search() == TO) {
			get_token();
			if ((curr_tok != TMAC) && (curr_tok != TID)) syn_err(MISSING_VAR);
			codegen(curr_tok == TMAC ? MACRO_ID : PUSH, loadstring());
			codegen(VSTORE, 0xff);
			get_token();
		}
		else if (curr_tok) syn_err(MISSING_TO);
	}
	else syn_err(MISSING_TO);
}

static void do_replace_cmd(void)
{
	codegen(GO_, 3); //valid_chk() & prefetch next rec if scope is set
	do {
		int mac = FALSE;
		int fldname;
		get_token();
		switch (curr_tok) {
			case TMAC:
				mac = TRUE;
			case TID:
				fldname = loadstring();
				break;
			default:
				syn_err(MISSING_FLDNAME);
		}
		get_token();
		if (kw_search() != WITH) syn_err(MISSING_WITH);
		get_token();
		expression();
		if (mac) {
			codegen(MACRO_ID, fldname);
			codegen(REPL, OPND_ON_STACK);
		}
		else codegen(REPL, fldname);
	} while (curr_tok == TCOMMA);
	codegen(PUTREC, 0);
}

void process_replace(void)
{
	int offset;
	int syn_stat = 0;
	int rcomm = 0;
	int fcomm = 0;

	get_token();
	if (curr_tok) {
		int k = kw_search();
		if ((k != ALL) && (k != NEXT) && (k != PREV) && (k != FOR) && (k != WHILE)) {
			rcomm = 1;
			codegen(JUMP, 1);	 /** to be modified later if not single rec **/
			rest_token(&last_tok);
			do_replace_cmd();
		}
	}
	if (curr_tok) {
		if (rcomm) {
			codegen(RETURN, 0);
			mod_op(0, -1);
		}
		fcomm = 1;
		fcodegen(offset=nextcode, CALL, 1);
		while (curr_tok) if (!process_fcode(offset, kw_search(), &syn_stat)) {
			if (!rcomm) {
				mod_op(7, -1);
				rest_token(&last_tok);
				do_replace_cmd();
				codegen(RETURN, 0);
				rcomm = 1;
			}
			else syn_err(ILL_SYNTAX);
		}
		if (fcomm) {
		}
		fmod_op(offset, syn_stat);
	}
	if (!rcomm) syn_err(BAD_EXP);
	codegen(PUTREC, 1);	/* non-zero mode causes final count to be displayed */
}

void process_set(void)
{
	HBKW *keyword;
	int setwhat, i;
	get_token();
	i = kw_bin_srch(0, KWTABSZ - 1);
	if (i < 0) syn_err(INVALID_SET_PARM);
	keyword = &keywords[i];
	if (keyword->setting_id == -1) syn_err(INVALID_SET_PARM);
	setwhat = keyword->kw_val;
	if (is_onoff(keyword->setting_id)) {
		int this_tok;
		get_token();
		this_tok = kw_search();
		if ((this_tok != ON && this_tok != OFF && this_tok != AUTO) || (keyword->setting_id != READONLY_ON && this_tok == AUTO)) syn_err(MISSING_ON_OFF);
		codegen(SET_ONOFF, (keyword->setting_id << N_ONOFF_BITS) | (this_tok == AUTO? S_AUTO : (this_tok == ON ? S_ON : S_OFF)));
		get_token();
	}
	else {	/** set ... to ... **/
		int what, n;
		get_token();
		if (kw_search() != TO) syn_err(MISSING_TO);
		what = keyword->setting_id;
		n = 0;
		switch (setwhat) {
		case RELATION:
		case FILTER:
			get_token();
			if (curr_tok) {
				codegen(PUSH, loadexpr(0));
				++n;
			}
			if (setwhat == RELATION) {
				if (curr_tok) {
					if (kw_search() != INTO) syn_err(MISSING_INTO);
					get_token();
					if (curr_tok == TMAC)
						codegen(MACRO_ID, loadstring());
					else {
						if (curr_tok != TID) syn_err(BAD_ALIAS);
						codegen(PUSH, loadstring());
					}
					++n;
					get_token();
				}
				else if (n) syn_err(MISSING_INTO);
			}
			break;
		case INDEX:
			do {
				get_tok();
				if (curr_tok) {
					++n;
					if (curr_tok == TMAC)
						codegen(MACRO_FN, loadstring());
					else
						codegen(PUSH, loadfilename(TRUE));
					get_token();
				}
			} while ((curr_tok == TCOMMA) && (n < MAXNDX));
			break;
		case FORM:
		case PROCEDURE:
			get_tok();
			if (curr_tok) {
				if (curr_tok == TMAC)
					codegen(MACRO_FULFN, loadstring());
				else
					codegen(PUSH, loadfilename(setwhat == PROCEDURE? TRUE : FALSE));
				get_token();
				++n;	//1
				if (curr_tok && kw_search() == SYSTEM_) {
					n |= P_SYS;
					get_token();
				}
				if (curr_tok && kw_search() == DEBUG) {
					n |= P_DBG;
					get_token();
				}
			}
			break;
		case ORDER:
		case EPOCH:
			get_token();
			if (curr_tok) {
				if (curr_tok != TINT) {
					n = OPND_ON_STACK; //indicating argument is on stack
					expression();
				}
				else {
					if (setwhat == EPOCH) {
						n = OPND_ON_STACK;
						codegen(PUSH, loadnumber());
					}
					else {
						n = atoi(&curr_cmd[currtok_s]);
						if ((n < 1) || (n > MAXNDX)) syn_err(ILL_VALUE);
					}
					get_token();
				}
			}
			break;
		case WIDTH:
		case DECIMAL:
			get_token();
			if (curr_tok) {
				if ((curr_tok != TINT) || ((n = atoi(&curr_cmd[currtok_s])) < 0)) syn_err(ILL_VALUE);
				get_token();
			}
			break;
		case DEFAULT:
#ifndef MS_DOS
			syn_err(INVALID_SET_PARM); //not valid for non-DOS systems
#else
			get_token();
			if (curr_tok) {
				if (currtok_len != 1) syn_err(BAD_DRIVE);
				n = toupper(curr_cmd[currtok_s]) - 'A' + 1;
				if ((n < 1) || (n > 16)) syn_err(BAD_DRIVE);
				get_token();
				if (curr_tok == TCOLON) get_token();
			}
#endif
			break;
		case PATH:
			get_tok();
			if (curr_tok) {
				if (curr_tok == TMAC)
					codegen(MACRO_FULFN, loadstring());
				else
					codegen(PUSH, loadstring());
				++n;
				get_token();
			}
			break;
		case DATE_:
		case DATEFMT:
			get_token();
			if (curr_tok) {
				expression();
				++n;
				get_token();
			}
			break;
		default:
			syn_err(INVALID_SET_PARM);
		}
		codegen(PUSH_N, n);
		codegen(SET_TO, what);
	}
}

#define U_FROM		0x10
#define U_ON		0x20
#define U_REPL		0x40
#define U_RAND		0x80

void process_update(void)
{
	int syn_stat = 0;
	codegen(CALL, 0); //from <alias>
	codegen(CALL, 0); //on <key>
	codegen(UPD_INIT, 0);
	codegen(JMP_EOF_0, 0);
	codegen(JMP_FALSE, 7); //UP_NEXT
	codegen(CALL, 0);
	codegen(UPD_REC, 0);
	codegen(UPD_NEXT, 0);
	codegen(JUMP, 3);
	get_token();
	while (curr_tok) {
		int kw = kw_search();
		switch (kw) {
		case FROM:
			if (syn_stat & U_FROM) syn_err(EXCESS_SYNTAX);
			syn_stat |= U_FROM;
			mod_op(0, -1);
			get_token();
			if (!curr_tok) syn_err(MISSING_ALIAS);
			if (curr_tok == TMAC)
				codegen(MACRO_FULFN, loadstring());
			else if (curr_tok != TID) syn_err(BAD_ALIAS);
			else
				codegen(PUSH, loadfilename(FALSE));
			codegen(RETURN, 0);
			get_token();
			break;
		case ON:
			if (syn_stat & U_ON) syn_err(EXCESS_SYNTAX);
			syn_stat |= U_ON;
			mod_op(1, -1);
			get_token();
			if (!curr_tok) syn_err(MISSING_FLDNAME);
			if (curr_tok == TMAC)
				codegen(MACRO_ID, loadstring());
			else if (curr_tok != TID) syn_err(BADFLDNAME);
			else
				codegen(PUSH, loadstring());
			codegen(RETURN, 0);
			get_token();
			break;
		case REPLACE:
			if (syn_stat & U_REPL) syn_err(EXCESS_SYNTAX);
			syn_stat |= U_REPL;
			mod_op(5, -1);
			hb_cmd(REPLACE_CMD, NULL, 0);
			break;
		case RANDOM:
			if (syn_stat & U_RAND) syn_err(EXCESS_SYNTAX);
			syn_stat |= U_RAND;
			mod_op(2, 1);
			break;
		default:
			syn_err(ILL_SYNTAX);
		}
	}
	if (!(syn_stat & U_FROM)) syn_err(MISSING_FROM);
	if (!(syn_stat & U_ON)) syn_err(MISSING_ON);
	if (!(syn_stat & U_REPL)) syn_err(MISSING_REPLACE);
	mod_op(3, -1);
	codegen(UPD_DONE, 0);
}
