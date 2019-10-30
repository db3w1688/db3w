/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/cmd.c
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
#include <string.h>
#include <errno.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "dbe.h"

void hb_cmd(int cmd, BYTE *pbuf, int size_max)
{
	TOKEN tk;
	int kw;
	if (is_control(cmd)) syn_err(ILL_SYNTAX);
	switch (cmd) {
		case NULL_CMD:
			break;
		case QUIT_CMD:
			get_token();
			codegen(QUIT_, 0);
			break;
		case PRINT_CMD:{
			int t = 0;
			get_token();
			if (!curr_tok) {
				codegen(PUSH, loadstring());
				codegen(PRNT, '\n');
				break;
			}
			do {
				if (t) get_token();
				expression();
				codegen(PRNT, curr_tok==TCOMMA ? '\t' : 
								  (curr_tok==TSEMICOLON ? 0 : '\n'));
				t = 1;
			} while ((curr_tok==TCOMMA) || (curr_tok==TSEMICOLON));
			break; }
		case STORE_CMD:
			get_token();
			if (curr_tok == TAT) get_token(); //item is already on stack
			else expression();
			if (kw_search() != TO) syn_err(MISSING_TO);
			get_token();
			switch (curr_tok) {
				case TID: {
					int isarray;
					save_token(&tk);
					get_token();
					isarray = curr_tok == TLBRKT;
					rest_token(&tk);
					if (isarray) {
						process_array();
						codegen(VSTORE, OPND_ON_STACK);
					}
					else codegen(VSTORE, loadstring());
					break; }
				case TMAC:
					codegen(MACRO_ID, loadstring());
					codegen(VSTORE, OPND_ON_STACK);
					break;
				default: 
					syn_err(MISSING_VAR);
			}
			get_token();
			break;
		case SELECT_CMD:
			get_token();
			if (curr_tok) {
				switch (curr_tok) {
				case TID:
					if (kw_search() == NEXT) {
						get_token();
						if (curr_tok) {
							if (kw_search() != AVAIL) syn_err(ILL_SYNTAX);
							get_token();
						}
						codegen(SELECT_, NEXT_AVAIL);
						break;
					}
				case TMAC:
					codegen(curr_tok == TID ? PUSH : MACRO_ALIAS, loadstring());
					codegen(SELECT_, 0);
					break;
				case TINT: {
					int i = atoi(&curr_cmd[currtok_s]);
					if ((i < 1) || (i > MAXCTX)) syn_err(ILL_SELECT);
					codegen(SELECT_, i);
					break; }
				default:
					syn_err(BAD_ALIAS);
				}
				get_token();
			}
			else codegen(SELECT_, NEXT_AVAIL);
			break;
		case USE_CMD: {
			int mode = 0, alias_first = 0;
			int dbf;
			get_tok();
			if (curr_tok) {
				mode = U_USE;
				dbf = curr_tok==TMAC? 0x8000 | loadstring() : loadfilename(TRUE);
				get_token();
			}
			while (curr_tok) switch (kw_search()) {
				case INDEX: {
					int n = 0, alias_first = 0;
					if (U_INDEX & mode) syn_err(ILL_KEYWORD);
					mode |= U_INDEX;
					do {
						get_tok();
						if (curr_tok) {
#ifdef DEFAULT_NDX
							if (kw_search() == DEFAULT) {
								if (n) syn_err(ILL_KEYWORD);
								mode |= U_DEFA_NDX;
								get_token();
								break;
							}
#endif
							if (curr_tok == TMAC)
								codegen(MACRO_FN, loadstring());
							else
								codegen(PUSH, loadfilename(TRUE)); //no path
							++n;
							get_token();
						}
					} while ((curr_tok == TCOMMA) && (n < MAXNDX));
					if (n >= MAXNDX) syn_err(TOOMANYNDX);
					if (n) codegen(PUSH_N, n);
					break; }
				case ALIAS:
					if (U_ALIAS & mode) syn_err(ILL_KEYWORD);
					if (U_INDEX & mode) alias_first = 1; //top of stack is alias
					get_token();
					if (curr_tok) {
						mode |= U_ALIAS;
						if (curr_tok == TMAC)
							codegen(MACRO_ID, loadstring());
						else {
							if (curr_tok != TID) syn_err(BAD_ALIAS);
							codegen(PUSH, loadstring());
						}
						get_token();
					}
					break;
				case SHARED:
					if (mode & U_EXCL) syn_err(ILL_KEYWORD);
					mode |= U_SHARED;
					get_token();
					break;
				case EXCLUSIVE:
					if (mode & U_SHARED || mode & U_READONLY) syn_err(ILL_KEYWORD);
					mode |= U_EXCL;
					get_token();
					break;
				case READONLY:
					if (mode & U_EXCL) syn_err(ILL_KEYWORD);
					mode |= U_READONLY;
					get_token();
					break;
				case DENYWRITE_:
					mode |= U_DENYW;
					get_token();
					break;
				default: syn_err(ILL_KEYWORD);
			}
			if (mode) {
				if (U_INDEX & mode && U_ALIAS & mode) codegen(PUSH_TF, alias_first);
				codegen(0x8000 & dbf? MACRO_FULFN : PUSH, 0x7fff & dbf);
			}
			codegen(USE_, mode);
			break; }
		case GO_CMD:
			get_token();
			if (!curr_tok) syn_err(MISSING_ARG);
			{
				int tb = kw_search();
				if (tb == TO) { get_token();  tb = kw_search(); }
				if (tb == RECORD) { get_token();  tb = kw_search(); }
				if (tb == TOP) { codegen(GO_, 1); get_token(); }
				else if (tb == BOTTOM) { codegen(GO_, 2); get_token(); }
				else {
					if (curr_tok == TINT) { //to allow unsigned long
						int is_rno_only;
						save_token(&tk);
						get_token();
						is_rno_only = !curr_tok;
						rest_token(&tk);
						if (is_rno_only) {
							errno = 0;
							recno_t n = strtoul(&curr_cmd[currtok_s], NULL, 10);
							if ((errno == ERANGE && n == ULONG_MAX)
								|| (errno != 0 && n == 0)) syn_err(INVALID_VALUE);
							codegen(PUSH, loadnumber());
							codegen(GO_, 0);
							get_token();
							break;
						}
					}
					expression();
					codegen(GO_, 0);
				}
			}
			break;
		case SKIP_CMD:
			get_token();
			switch (curr_tok) {
				case TNULLTOK:
					codegen(SKIP_, 1);
					break;
				case TINT: {
					errno = 0;
					long r = strtol(&curr_cmd[currtok_s], NULL, 10);
					if ((errno == ERANGE && (r == LONG_MAX || r == LONG_MIN))
						|| (errno != 0 && r == 0)) syn_err(INVALID_VALUE);
					if (r<=127 && r>=-126) {
						codegen(SKIP_, r>0? r : 0x80 | (-r));
						get_token();
						break;
					}}
				default:
					expression();
					codegen(SKIP_, OPND_ON_STACK);
			}
			break;
		case SEEK_CMD: {
			int mode=0, n;
			get_token();
			if (is_relop(curr_tok)) {
				if (curr_tok==TNEQU) syn_err(INVALID_OPERATOR);
				mode = curr_tok;
				get_token();
			}
			expression();
			n = 1;
			while (curr_tok==TCOMMA) {
				get_token();
				expression();
				++n;
			}
			codegen(PUSH_N, n);
			codegen(FIND_, mode);
			break; }
		case FIND_CMD: {
			int i, n=0, mode=0;
			get_token();
			if (is_relop(curr_tok)) {
				if (curr_tok==TNEQU) syn_err(INVALID_OPERATOR);
				mode = curr_tok;
				get_token();
			}
			do {
				if (n) get_token();
				switch (curr_tok) {
					case TMAC:
					case TID:
					case TLIT:
						i = loadstring();
						break;
					case TINT:
					case TNUMBER:
						i = loadnumber();
						break;
					default:
						syn_err(MISSING_LIT);
				}
				codegen(curr_tok==TMAC ? MACRO_ID : PUSH, i);
				get_token();
				++n;
			} while (curr_tok==TCOMMA);
			codegen(PUSH_N, n);
			codegen(FIND_, mode);
			break; }
		case PRIVATE_CMD:
		case RELEASE_CMD: {
			int mode = 0;
			int pr = cmd == PRIVATE_CMD;
			get_token();
			if (!curr_tok) syn_err(MISSING_VAR);
			if (kw_search() == ALL) {
				mode |= R_ALL;
				get_token();
				if (!curr_tok) goto done;
				if (kw_search() == LIKE) {
					get_tok();
					codegen(PUSH, loadskel(SKEL_VARNAME));
					mode |= R_LIKE;
					get_token();
					if (!curr_tok) goto done;
				}
				if (kw_search() == EXCEPT) {
					get_tok();
					codegen(PUSH, loadskel(SKEL_VARNAME));
					mode |= R_EXCEPT;
					get_token();
				}
			}
			else while (curr_tok) {
				codegen(curr_tok == TMAC ? MACRO_ID : PUSH, loadstring());
				++mode;
				get_token();
				if (curr_tok == TCOMMA) {
					get_token();
					if (!curr_tok) syn_err(MISSING_VAR);
				}
			}
done:
			codegen(pr ? MPRV : MREL, mode);
			break;
			}
		case PARAMETER_CMD:
		case PUBLIC_CMD: {
			int param = cmd == PARAMETER_CMD;
			do {
				get_token();
				if (!curr_tok) syn_err(MISSING_VAR);
				if (curr_tok == TMAC) {
					codegen(MACRO_ID, loadstring());
					codegen(param ? PARAM : MPUB, OPND_ON_STACK);
				}
				else codegen(param ? PARAM : MPUB, loadstring());
				get_token();
			} while (curr_tok == TCOMMA);
			break;
			}
		case RESET_CMD:
#ifdef ALLOW_RESET
			get_token();
			codegen(RESET, RST_ALL);
#else
			syn_err(COMMAND_UNKNOWN); //don't allow it in procedures
#endif
			break;
		case UNLOCK_CMD:
			get_token();
			codegen(RESET, RST_UNLOCK);
			break;
		case ARRAY_CMD: {
			int mode = 0;
			int aname;
			save_token(&tk);
			get_token();
			if (kw_search() != PUBLIC) rest_token(&tk);
			else mode |= 0x80;
			do {
				mode &= 0x80; //clear mode but retain PUBLIC bit if set
				get_token();
				if (curr_tok != TID) syn_err(BAD_VARNAME);
				aname = loadstring();
				get_token();
				if (curr_tok != TLBRKT) syn_err(MISSING_LBRKT);
				do {
					if ((0x7f & mode) == MAXADIM) syn_err(TOOMANY_DIM);
					get_token();
					expression();
					++mode;
				} while (curr_tok == TCOMMA);
				if (curr_tok != TRBRKT) syn_err(MISSING_RBRKT);
				codegen(PUSH_N, mode);
				codegen(ARRAYDEF, aname);
				get_token();
			} while (curr_tok == TCOMMA);
			break; }
		case LOCATE_CMD: {
			int mode = 0;
			get_token();
			kw = kw_search();
			if (kw == CANCEL) goto end_locate;
			if (kw == _FORWARD) mode = FORWARD;
			else if (kw == _BACKWARD) mode = BACKWARD;
			else if (kw != FOR) syn_err(ILL_COND);
			get_token();
			if (mode) {
				if (!curr_tok || kw_search() != FOR) syn_err(ILL_COND);
				get_token();
			}
			if (!curr_tok) syn_err(ILL_COND);
			if (curr_tok == TMAC) codegen(MACRO_ID, loadstring());
			else codegen(PUSH, loadexpr(0));
			if (mode && curr_tok) {
				kw = kw_search();
				if (kw == _FORWARD) mode = FORWARD;
				else if (kw == _BACKWARD) mode = BACKWARD;
				else break;
				get_token();
			}
			if (!mode) mode = FORWARD;
end_locate:
			codegen(LOCATE_, mode); //mode==0: remove locate condition
			break; }
		case CONTINUE_CMD: {
			int mode = 0;
			get_token();
			if (curr_tok) {
				kw = kw_search();
				if (kw == _FORWARD) mode = FORWARD;
				else if (kw == _BACKWARD) mode = BACKWARD;
				else syn_err(ILL_SYNTAX);
				get_token();
			}
			if (!mode) mode = FORWARD;
			codegen(SKIP_, mode==FORWARD? 1 : 0x81);
			codegen(LOCATE_, 0x80 | mode);
			break; }
		case REINDEX_CMD: {
			int qsort = 0;
			get_token();
			if (curr_tok) {
				kw = kw_search();
				if (kw == USING) {
					get_token();
					kw = kw_search();
					if (kw != QSORT) syn_err(MISSING_PARM);
				}
				if (kw == QSORT) {
					qsort = 1;
					get_token();
				}
			}
			codegen(REINDEX_, qsort);
			break; }
		case PACK_CMD:
			get_token();
			codegen(PACK_, 0);
			break;
		case DIRLST_CMD: {
			int typ = 0;
			get_token();
			if (!curr_tok) typ = F_ALL;
			else if (kw_search() == TO) {
				typ = F_ALL;
				goto to_dbf;
			}
			else do {
				char typstr[8];
				if (curr_tok == TCOMMA) {
					if (!typ) syn_err(ILL_SYNTAX);
					get_token();
				}
				if (currtok_len > EXTLEN) syn_err(INVALID_TYPE);
				sprintf(typstr, "%.*s", currtok_len, &curr_cmd[currtok_s]);
				chgcase(typstr, 1);
				get_token();
				if (!strcmp(typstr, "DBF")) typ |= F_DBF;
				else if (!strcmp(typstr, "TBL")) typ |= F_TBL;
				else if (!strcmp(typstr, "IDX")) typ |= F_IDX;
				else if (!strcmp(typstr, "NDX")) typ |= F_NDX;
				else if (!strcmp(typstr, "DBT")) typ |= F_DBT;
				else if (!strcmp(typstr, "TXT")) typ |= F_TXT;
				else if (!strcmp(typstr, "MEM")) typ |= F_MEM;
				else if (!strcmp(typstr, "PRG")) typ |= F_PRG;
				else if (!strcmp(typstr, "PRO")) typ |= F_PRO;
				else if (!strcmp(typstr, "SER")) typ |= F_SER;
				else if (!strcmp(typstr, "DIR")) typ |= F_DIR;
				else if (!strcmp(typstr, "ALL")) typ = F_ALL;
				else break;
			} while (curr_tok==TCOMMA);
			if (curr_tok && kw_search() == TO) {
to_dbf:
				get_tok();
				if (curr_tok == TMAC)
					codegen(MACRO_FULFN, loadstring());
				else
					codegen(PUSH, loadfilename(FALSE));
				typ |= DIR_TO_DBF;
				get_token();
			}
			codegen(PUSH, loadint(typ));
			codegen(DIR_, OPND_ON_STACK);
			break; }
		case CHDIR_CMD: {
			int n = 0;
			get_tok();
			if (curr_tok) {
				if (curr_tok == TMAC) codegen(MACRO_FULFN, loadstring());
				else codegen(PUSH, loadfilename(FALSE));
				++n;
				get_token();
			}
			codegen(PUSH_N, n);
			codegen(SET_TO, PATH_TO);
			break; }
		case EXPORT_CMD: {
			int n = 0;
			get_token();
			if (!curr_tok || kw_search() == TO) syn_err(MISSING_ARG);
			expression();
			++n;
			if (curr_tok && kw_search() == TO) {
				get_token();
				expression();
				++n;
			}
			codegen(EXPORT_, n);
			break; }
		case SU_CMD:
			get_token();
			if (curr_tok == TMAC) {
				codegen(MACRO_ID, loadstring());
				codegen(SU_, OPND_ON_STACK);
				get_token();
			}
			else if (curr_tok == TID) {
				codegen(SU_, loadstring());
				get_token();
			}
			else syn_err(MISSING_VAR);
			break;
		case READ_CMD: {
			int set_stamp = 0;
			get_token();
			if (curr_tok) {
				if (kw_search() == STAMP) {
					set_stamp = 1;
					get_token();
				}
				else syn_err(ILL_SYNTAX);
			}
			codegen(IN_READ, set_stamp);
			break; }
#ifdef HAVE_HBX
		case HBX_CMD:
			get_token();
			if (!curr_tok) syn_err(MISSING_PARM);
			expression();
			if (curr_tok) syn_err(ILL_SYNTAX);
			codegen(HBX_, 0);
			break;
#endif
		case MKDIR_CMD: {
			int n = 0;
			do {
				get_tok();
				if (curr_tok) {
					if (curr_tok == TMAC)
						codegen(MACRO_FN, loadstring());
					else
						codegen(PUSH, loadfilename(TRUE));
					++n;
					get_token();
				}
			} while ((curr_tok == TCOMMA) && (n < MAXARGS));
			if (n == 0) syn_err(MISSING_ARG);
			if (n >= MAXARGS) syn_err(TOOMANYARGS);
			codegen(MKDIR_, n);
			break; }
		case RENAME_CMD: {
			int n = 0;
			get_token();
			if (kw_search() != FILE_) syn_err(MISSING_FILE);
			get_token();
			if (!curr_tok) syn_err(MISSING_ARG);
			expression();
			++n;
			if (kw_search() != TO) syn_err(MISSING_TO);
			get_token();
			if (!curr_tok) syn_err(MISSING_ARG);
			expression();
			++n;
			if (curr_tok) {
				if (kw_search() != OWNER) syn_err(ILL_SYNTAX);
				get_token();
				expression();
				++n;
			}
			codegen(RENFILE, n);
			break; }
		case SLEEP_CMD:
			get_token();
			if (!curr_tok) syn_err(MISSING_PARM);
			expression();
			if (curr_tok) syn_err(ILL_SYNTAX);
			codegen(SLEEP_, OPND_ON_STACK);
			break;
		case SERIAL_CMD:
		case TRANSAC_CMD: {
			int mode;
			get_token();
			if (!curr_tok) syn_err(MISSING_ARG);
			else switch (curr_tok) {
			case TID:
				codegen(PUSH, loadstring());
				break;
			case TMAC:
				codegen(MACRO_ID, loadstring());
				break;
			default: 
				syn_err(INVALID_ARG);
			}
			get_token();
			if (cmd == SERIAL_CMD) {
				if (!curr_tok) mode = 0;
				else if (kw_search() == DELETE) {
					mode = 1;
					get_token();
				}
				else {
					expression();
					mode = 2;
				}
				codegen(SERIAL_, mode);
			}
			else {
				if (!curr_tok) syn_err(MISSING_ARG);
				switch (kw_search()) {
				case START:
					mode = TRANSAC_START;
					break;
				case CANCEL:
					mode = TRANSAC_CANCEL;
					break;
				case COMMIT:
					mode = TRANSAC_COMMIT;
					break;
				case ROLLBACK:
					mode = TRANSAC_ROLLBACK;
					break;
				default:
					syn_err(ILL_KEYWORD);
				}
				get_token();
				codegen(TRANSAC_, mode); //trans_id is always on stack
			}
			break; }
		case ASSIGN_CMD:
		default:
			if (curr_tok == TID) {
				int vname = loadstring();
				int isarray;
				TOKEN tk2;
				save_token(&tk);
				get_token();
				if (curr_tok!=TEQUAL && curr_tok!=TLBRKT) syn_err(ILL_SYNTAX);
				isarray = curr_tok == TLBRKT;
				while (curr_tok && (curr_tok != TEQUAL)) get_token();
				if (!curr_tok) {
					rest_token(&tk);
					syn_err(COMMAND_UNKNOWN);
				}
				get_token();
				expression();
				save_token(&tk2);
				if (isarray) {
					rest_token(&tk);
					process_array();
					codegen(VSTORE,OPND_ON_STACK);
				}
				else codegen(VSTORE, vname);
				rest_token(&tk2);
				break;
			}
			else syn_err(COMMAND_UNKNOWN);
	}
	chk_nulltok();
	if (!exe && pbuf) emit(pbuf, size_max);
}
