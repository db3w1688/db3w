/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/proc.c
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include "hb.h"
#include "hbkwdef.h"
#include "hbpcode.h"
#include "hberrno.h"
#include "dbe.h"

/*************** pseudo code object file type *********************/
#define OBJ_REG      0x86
#define OBJ_LIB      0x87

static void write_block(CODE_BLOCK *b, int dbg);
static void cgen_ljmp(int jcode, cpos_t pos);
static void push_addr(cpos_t pos);
static cpos_t pop_addr(void);
static void mod_addr(void);
static void mod_jmp(void);
static void jmp_codegen(void);
static void cond_codegen(BYTE *pbuf, int size_max, int dbg);
static void push_call_addr(cpos_t pos);
static cpos_t pop_call_addr(void);

#define MAXLEVEL  32
#define JBLOCKSZ  (2 + sizeof(cpos_t))  /* 0 + jcode + sizeof(cpos_t) see cgen_ljmp() */

#define PLACE_HOLDER	(cpos_t)-1

int curr_lvl = 0; //procedure call level
int lineno = 0;

static int mcount;
static BYTE *pbuf_base;
static FILE *srcfp, *outfp;	  /* source and output files for compile_proc */
static off_t last_pos;
static cpos_t jmp_addr[MAXLEVEL], call_addr[MAXLEVEL]; //call_addr[] is support tracking of proc context at run-time
static int top_addr = 0, top_caddr = 0;
static int pcount[MAXLEVEL];

#define init_stack()	neststack[0] = 0
#define zerolevel()	(!top)
#define crntlevel()	(0x01ff & neststack[top])
#define checklevel()	if (++top == MAXLEVEL) sys_err(TOOMANY_LEVELS)
#define pushlevel(what)	checklevel(); neststack[top] = what
#define poplevel()	--top
#define chglevel(what)	neststack[top] = what
#define set_nf()	neststack[top] |= 0x2000
#define nf_case()	(0x2000 & neststack[top])
#define set_s()		neststack[top] |= 0x1000
#define s_case()	(0x1000 & neststack[top])
#define nestlevel(n)	(0x01ff & neststack[n])
#define s_case1(n)	(0x1000 & neststack[n])
#define nf_case1(n)	(0x2000 & neststack[n])

#define get_pcount(pcount)	(0x7f & pcount)
#define isset_final(pcount)	(0x80 & pcount)
#define set_final(pcount)	(pcount |= 0x80)

struct do_entry {
	char proc_name[VNAMELEN+1];
	char type;
	cpos_t faddr;
	int len;
} *do_tab = (struct do_entry *)NULL;

struct _do_list {
	struct do_entry de;
	struct _do_list *next;
} *do_list = (struct _do_list *)NULL;

struct jblock {
	BYTE zero;
	BYTE jmpcode;
	BYTE jaddr[sizeof(cpos_t)];
};

struct _proc_lib {
	int mcount;
	struct do_entry *do_tab;
	BYTE *pbuf_base;
} proc_lib[MAXPROCLIB] = {{0, NULL, NULL}, {0, NULL, NULL}};

void push_pcount(int n)
{
	if (++curr_lvl > MAXLEVEL) sys_err(TOOMANY_LEVELS);
	pcount[curr_lvl] = n;
}

int pop_pcount(void)
{
	if (!curr_lvl) sys_err(STACK_FAULT);
	return(pcount[curr_lvl--]);
}

void clear_proc_stack(void)
{
	while (curr_lvl) {
		release(R_PROC);
		--curr_lvl;
	}
	top_addr = 0;
}

/*****************************************************/

int ln_init(char *line)
{
	int comm_len = 0;
	char *p, c;
	int bufsz = STRBUFSZ;

	memset(p = curr_cmd = line, 0, STRBUFSZ);
	last_pos = ftell(srcfp);
	do {
		if (!fgets(p, bufsz, srcfp)) break;
		comm_len = strlen(p);
		p += comm_len;
		c = *(p - 1);
		while (isspace(c)) { /* get rid of the trailing spaces including CRLF */
			--p;
			--comm_len;
			c = *(p - 1);
		}
		if (c == '\\') { --p; --comm_len; }
		*p = '\0';
		bufsz -= comm_len;
	} while (c == '\\');
	return(!feof(srcfp) && !ferror(srcfp));
}

struct _do_list *add_do_list(char *name, int type, cpos_t faddr)
{
	struct _do_list *dl, *dr;

	for (dl=NULL,dr=do_list; dr; dl=dr,dr=dr->next) {
		if (!strcasecmp(dr->de.proc_name, name)) break;
	}
	if (!dr) {			
		dr = (struct _do_list *)malloc(sizeof(struct _do_list));
		if (!dr) sys_err(NOMEMORY);
		if (faddr < 0) {	/** unresolved, referenced @(-faddr) **/
			dr->de.faddr = 0;
			fseek(outfp, -(off_t)faddr, 0);
			fwrite(&dr->de.faddr, sizeof(cpos_t), 1, outfp);
		}
		strcpy(dr->de.proc_name, name);
		dr->de.type = (char)type;
		dr->de.len = 0;
		dr->de.faddr = faddr;
		if (dl) {
			dr->next = dl->next;	/** should be NULL **/
			dl->next = dr;
		}
		else {
			dr->next = NULL;
			do_list = dr;
		}
	}
	else {
		cpos_t tmp;
		if (faddr < 0) {	/** unresolved, referenced @(-faddr) **/
			fseek(outfp, -(off_t)faddr, 0);
			if (dr->de.faddr < 0) {
				fwrite(&dr->de.faddr, sizeof(cpos_t), 1, outfp);
				dr->de.faddr = faddr;
			}
			else {
				tmp = dr->de.faddr;
				fwrite(&tmp, sizeof(cpos_t), 1, outfp);
			}
		}
		else {
			if (dr->de.faddr > 0) syn_err(MULTI_DEF);
			if (dr->de.type != type) syn_err(PROC_MISMATCH);
			tmp = faddr;
			do {
				fseek(outfp, -(off_t)dr->de.faddr, 0);
				fread(&dr->de.faddr, sizeof(cpos_t), 1, outfp);
				fseek(outfp,-(off_t)sizeof(cpos_t), 1);
				fwrite(&tmp, sizeof(cpos_t), 1, outfp);
			} while (dr->de.faddr != 0);
			dr->de.faddr = faddr;
		}
	}
	fseek(outfp, (off_t)0, 2);
	return(dr);
}

static void _free_do_list(do_ent)
struct _do_list *do_ent;
{
	if (!do_ent) return;
	_free_do_list(do_ent->next);
	free((char *)do_ent);
}

static void free_do_list(void)
{
	_free_do_list(do_list);
	do_list = (struct _do_list *)NULL;
}

static int write_rtable(int flags) //only PROC_FLG_LNO is supported
{
	struct _do_list *de;
	off_t pos = ftell(outfp);	/** start of symbol/proc table **/
	cpos_t faddr;
	int mcount = 0, ecount = 0, spec;

	fwrite(&mcount, sizeof(int), 1, outfp);
	for (de=do_list; de; de=de->next) {
		++mcount;
		if (de->de.faddr < 0) {
			++ecount;
			faddr = de->de.faddr;
			do {
				fseek(outfp, -(off_t)faddr, 0);
				fread(&faddr, sizeof(cpos_t), 1, outfp);
			} while (faddr != 0);
			fseek(outfp, -(off_t)sizeof(cpos_t), 1);
			fwrite(&ecount, sizeof(cpos_t), 1, outfp);
			fseek(outfp, 0, 2);
		}
		fwrite(&de->de, sizeof(struct do_entry), 1, outfp);
		if (ferror(outfp)) sys_err(DISK_WRITE);
	}
	spec = MAKE_DWORD(mcount, flags);
	fseek(outfp, pos, 0);
	fwrite(&spec, sizeof(int), 1, outfp);
	fseek(outfp, (off_t)2, 0); //to 1st OBJ_LIB JBLOCK faddr
	fwrite(&pos, sizeof(cpos_t), 1, outfp);
	return(ecount);
}

void process_index(void);
void process_set(void);
void process_append(void);
void process_copy(void);
void process_input(int cmd);
void process_display(int islist);
void process_del_rcal(int cmd);
void process_count(void);
void process_create(void);
void process_replace(void);
void process_update(void);

int process_cmd(int cmd, BYTE *pbuf, int size_max)
{
	int _exe = exe, unknown = 0, errcode = 0;
	jmp_buf sav_env;
	memcpy(sav_env, env, sizeof(jmp_buf));
	if (errcode = setjmp(env)) {
		exe = _exe;
		memcpy(env, sav_env, sizeof(jmp_buf));
		longjmp(env, errcode);
	}
	exe = 0; //compile to pcode only
	memset(pbuf, 0, size_max);
	switch(cmd) {
	case SET_CMD:
		process_set();
		break;
	case DISPLAY_CMD:
		process_display(0);
		break;
	case RECALL_CMD:
	case DELETE_CMD:
		process_del_rcal(cmd);
		break;
	case APPEND_CMD:
		process_append();
		break;
	case INDEX_CMD:
		process_index();
		break;
	case COUNT_CMD:
		process_count();
		break;
	case ACCEPT_CMD:
		process_input(cmd);
		break;
	case COPY_CMD:
		process_copy();
		break;
	case REPLACE_CMD:
		process_replace();
		break;
	case CREATE_CMD:
		process_create();
		break;
	case UPDATE_CMD:
		process_update();
		break;
	case DO_CTL: {		/** only accessible from level 0 **/
		int kw;
		get_token();
		kw = kw_search();
		if (kw == WHILE || kw == CASE) syn_err(ILL_SYNTAX);
		else {
			int pname = loadstring();
			int pcount = 0;
			get_token();		/** read pass the file name ***/
			while (curr_tok) if (kw_search() == WITH) {
				if (get_pcount(pcount)) syn_err(ILL_SYNTAX); //already set
				else do {
					get_token();
					if (!curr_tok) syn_err(MISSING_PARM);
					if (curr_tok == TAT) {	/** passing by reference **/
						int isarray;
						TOKEN tk;
						get_token();
						if (curr_tok != TID) syn_err(MISSING_VAR);
						save_token(&tk);
						get_token();
						isarray = curr_tok == TLBRKT;
						rest_token(&tk);
						if (isarray) {
							process_array();
							codegen(REF, 0xff);
						}
						else codegen(REF, loadstring());
						get_token();
						if (curr_tok && curr_tok != TCOMMA) syn_err(ILL_SYNTAX);
					}
					else expression();
					++pcount;
				} while (curr_tok == TCOMMA);
			}
			else if (kw_search() == FINAL) {
				if (isset_final(pcount)) syn_err(ILL_SYNTAX);
				set_final(pcount);
				get_token();
			}
			else if (curr_tok == TPOUND) break;
			else syn_err(EXCESS_SYNTAX);
			codegen(PUSH, pname);
			codegen(EXEC_, pcount);
		}
		break; }
	default:
		unknown = 1;
		break;
	}
	memcpy(env, sav_env, sizeof(jmp_buf));
	exe = _exe;
	if (unknown) return(0);
	chk_nulltok();
	emit(pbuf, size_max);
	if (exe) {
		escape = 0;
		execute((CODE_BLOCK *)pbuf); //if execute flag was set previously than execute the code block just generated
	}
	return(1);
}

int ec(int type, char *name, int dbg)	/** returns stopping cmd. types: REG,PROC,FCN **/
{
	BYTE pbuf[CBLOCKSZ];
	int isfcn = type == PRG_FUNC;
	int cmd = 0;
	int stop = FALSE, seen_ret = FALSE;
	int neststack[MAXLEVEL];		  /* to implement control structures */
	int top = 0;
	unsigned ln_cnt = 0;
	cpos_t pos;

	init_stack();
	if (isset(TALK_ON)) {
		prnt_str("%s %c%s\n",isfcn ? "Function":"Procedure", 0x7f&name[0], name+1);
	}
	while (!stop) {
		memset(pbuf, 0, CBLOCKSZ);
		if (stop = !ln_init(cmd_line)) {
			if (!zerolevel()) switch (crntlevel()) {
				case IF_CTL:	syn_err(UNBAL_IF);
				case WHILE_CTL:	syn_err(UNBAL_DOWHILE);
				case CASE_CTL:	syn_err(UNBAL_CASE);
				default:	sys_err(UNKNOWN_CONTROL);
			}
			cmd = 0;
			continue;
		}
		cmd_init(cmd_line, code_buf, data_buf, NULL);
		lineno++;
		cmd = get_command(0);
		if (cmd == KW_UNKNOWN) syn_err(COMMAND_UNKNOWN);
		if (cmd == PROCEDURE_CTL || cmd == FUNCTION_CTL) {
			if (type == PRG_REG) syn_err(ILL_SYNTAX);
			if (isfcn && !seen_ret) syn_err(MISSING_RETURN);
			stop = TRUE;
			continue;
		}
		/*
		printf("\rLINE:%u",++ln_cnt);
		*/
		switch (cmd) {
		case NULL_CMD:
		case KW_COMMENT:
			break;
		case EOF:
			stop = TRUE;
			continue;
		case DO_CTL: {
			int kw;
			get_token();
			kw = kw_search();
			if (kw == WHILE) {
				pushlevel(WHILE_CTL);
				push_addr((cpos_t)ftell(outfp)); /* save jump addr for enddo */
				cond_codegen(pbuf, CBLOCKSZ, dbg);
			}
			else if (kw == CASE) {
				get_token();
				chk_nulltok();
				pushlevel(CASE_CTL);
			}
			else {
				int pcount = 0;
				char procname[VNAMELEN  + 1];
				memset(procname, 0, VNAMELEN+1);
				memcpy(procname, &curr_cmd[currtok_s], imin(currtok_len, VNAMELEN));
				get_token();		/** read pass the file name ***/
				if (kw_search() == WITH) do {
					get_token();
					if (!curr_tok) syn_err(MISSING_PARM);
					if (curr_tok == TAT) {	/** passing by reference **/
						int isarray;
						TOKEN tk;
						get_token();
						if (curr_tok != TID) syn_err(MISSING_VAR);
						save_token(&tk);
						get_token();
						isarray = curr_tok == TLBRKT;
						rest_token(&tk);
						if (isarray) {
							process_array();
							codegen(REF, 0xff);
						}
						else codegen(REF, loadstring());
						get_token();
						if (curr_tok && curr_tok != TCOMMA) syn_err(ILL_SYNTAX);
					}
					else expression();
					++pcount;
				} while (curr_tok == TCOMMA);
				else chk_nulltok();
				codegen(PUSH_N, pcount);
				emit(pbuf, CBLOCKSZ);
				write_block((CODE_BLOCK *)pbuf, dbg);
				cgen_ljmp(CALL, PLACE_HOLDER);
				add_do_list(procname, PRG_PROC, -(cpos_t)(ftell(outfp) - sizeof(cpos_t)));
			}
			break; }
		case RETURN_CTL:
			get_token();
			if (curr_tok) {
				if (!isfcn && curr_tok != TPOUND) syn_err(EXCESS_SYNTAX);
				expression();
				emit(pbuf, CBLOCKSZ);
				write_block((CODE_BLOCK *)pbuf, dbg);
				cgen_ljmp(FCNRTN, (cpos_t)0);
			}
			else {
				if (isfcn) syn_err(BAD_EXP);
				cgen_ljmp(RETURN, (cpos_t)0);
			}
			seen_ret = TRUE;
			break;
		case EXIT_CTL:
		case LOOP_CTL: {
			int sp_lvl = top;
			int sp_addr = top_addr;
			int found = FALSE;
			while (sp_lvl && !found) {
				switch (nestlevel(sp_lvl)) {
					case IF_CTL:
					case ELSE_CTL:
						--sp_lvl;
						--sp_addr;
						break;
					case ELSEIF_CTL:
						--sp_lvl;
						--sp_addr; --sp_addr;
						break;
					case CASE_CTL:
					case OTHERWISE_CTL:
						if (s_case1(sp_lvl)) --sp_addr;
						if (nf_case1(sp_lvl)) --sp_addr;
						--sp_lvl;
						break;
					case WHILE_CTL:
						found = TRUE;
						if (cmd == LOOP) cgen_ljmp(JUMP, jmp_addr[--sp_addr]);
						else {
							off_t curr_pos = ftell(outfp);
							fseek(outfp, (off_t)jmp_addr[sp_addr], 0);
							fwrite(&curr_pos, sizeof(cpos_t), 1, outfp);
							fseek(outfp, curr_pos, 0);
							cgen_ljmp(JUMP, PLACE_HOLDER);
							jmp_addr[sp_addr] = (cpos_t)(ftell(outfp) - sizeof(cpos_t));
						}
						break;
					default:
						syn_err(UNBAL_LOOP);
				}
			}
			if (!found) syn_err(UNBAL_LOOP);
			get_token();
			break; }
		case ENDDO_CTL:
			if (crntlevel() != WHILE_CTL) syn_err(UNBAL_ENDDO);
			poplevel();
			pos = pop_addr();			 /* get modify addr */
			cgen_ljmp(JUMP, pop_addr());  /* get jump addr */
			push_addr(pos);	 /* push modify addr back on the stack */
			mod_addr();
			get_token();
			break;
		case IF_CTL:
			pushlevel(IF_CTL);
			cond_codegen(pbuf, CBLOCKSZ, dbg);
			get_token();
			break;
		case ELSEIF_CTL:
			if (crntlevel() == IF_CTL) {
				jmp_codegen();
				chglevel(ELSEIF_CTL);
			}
			else if (crntlevel() == ELSEIF_CTL) {
				mod_jmp();
				jmp_codegen();
			}
			else syn_err(MISSING_IF);
			cond_codegen(pbuf, CBLOCKSZ, dbg);
			get_token();
			break;
		case ELSE_CTL:
			if (crntlevel() == IF_CTL) jmp_codegen();
			else if (crntlevel() == ELSEIF_CTL) {
				mod_jmp();
				jmp_codegen();
			}
			else syn_err(MISSING_IF);
			chglevel(ELSE_CTL);		/* to prevent more ELSE */
			get_token();
			break;
		case ENDIF_CTL:
			if ((crntlevel() == IF_CTL) || (crntlevel() == ELSE_CTL)) mod_addr();
			else if (crntlevel() == ELSEIF_CTL) {
				mod_jmp();
				mod_addr();
			}
			else syn_err(UNBAL_ENDIF);
			poplevel();
			get_token();
			break;
		case CASE_CTL:
			if (crntlevel() != CASE_CTL) syn_err(MISSING_CASE);
			if (nf_case()) {
				if (s_case()) mod_jmp();
				set_s();
				jmp_codegen();
			}
			set_nf();
			cond_codegen(pbuf, CBLOCKSZ, dbg);
			get_token();
			break;
		case OTHERWISE_CTL: {
			int j = FALSE;
			if (crntlevel() != CASE_CTL) syn_err(MISSING_CASE);
			if (nf_case()) {
				if (s_case()) mod_jmp();
				jmp_codegen();
				j = TRUE;
			}
			chglevel(OTHERWISE_CTL);	 /* to prevent more OTHERWISE */
			if (j) set_nf();
			get_token();
			break; }
		case ENDCASE_CTL:
			if ((crntlevel() != CASE_CTL) && (crntlevel() != OTHERWISE_CTL))
				syn_err(UNBAL_ENDCASE);
			if (nf_case()) {
				if (s_case()) mod_jmp();
				mod_addr();
			}
			poplevel();
			get_token();
			break;
		default:
			if (!process_cmd(cmd, pbuf, CBLOCKSZ)) hb_cmd(cmd, pbuf, CBLOCKSZ);
			write_block((CODE_BLOCK *)pbuf, dbg);
		}
	}
	/*
	if (type == PRG_PROC && !seen_ret) cgen_ljmp(RETURN, 0L);
	*/
	cgen_ljmp(RETURN, (cpos_t)0);	/** safety code, in case no explicit RETURN issued **/
	return(cmd);
}

int is_dcln(void)
{
	int kw = kw_search();
	switch (kw) {
	case K_CHARCTR: return(CHARCTR);
	case K_NUMBER: return(NUMBER);
	case DATE_: return(DATE);
	case K_LOGIC: return(LOGIC);
	default: return(NULLTYP);
	}
}

int compile_proc(char *infn, int dbg)
{
	char outfn[FULFNLEN];
	struct _do_list *dl;
	int _exe = exe, errcode = 0;
	int mcount = 0, ecount;
	jmp_buf sav_env;

	if (do_list) free_do_list();
	strcpy(outfn,infn);
	strcpy(strrchr(outfn,'.'),".pro");
	if (!(srcfp = fopen(infn, "r"))) db_error(DBOPEN, infn);
	if (!(outfp = fopen(outfn, "w+"))) db_error(DBCREATE, outfn);
	memcpy(sav_env, env, sizeof(jmp_buf));
	if (errcode = setjmp(env)) {
		exe = _exe;
		memcpy(env, sav_env, sizeof(jmp_buf));
		longjmp(env, errcode);
	}
	exe = FALSE;
	lineno = 0;
	cgen_ljmp(OBJ_LIB, (cpos_t)0);
	while (ln_init(cmd_line)) {
		int cmd;
		char name[VNAMELEN + 1];
		cmd_init(cmd_line, code_buf, data_buf, NULL);
		lineno++;
		cmd = get_command(0);
		while (cmd) switch (cmd) {
		case 0:
		case KW_COMMENT:
			break;
		case PROCEDURE_CTL:
		case FUNCTION_CTL:
			get_token();
			if (curr_tok != TID) syn_err(BAD_PRGNAME);
			if (mcount >= MAXMCOUNT) syn_err(TOOMANYPROCS);
			memcpy(name, &curr_cmd[currtok_s], currtok_len);
			name[currtok_len] = '\0';
			//chgcase(name, 1);
			get_token();
			chk_nulltok();
			if ((cpos_t)ftell(outfp) < 0) { //overflow, file too large
				ecount = -1;
				goto end;
			}
			dl = add_do_list(name, cmd == FUNCTION_CTL? PRG_FUNC : PRG_PROC, (cpos_t)ftell(outfp));
			cmd = ec(cmd == FUNCTION_CTL? PRG_FUNC : PRG_PROC, name, dbg);
			dl->de.len = (int)((cpos_t)ftell(outfp) - dl->de.faddr);
			++mcount;
			break;
		default:
			syn_err(MISSING_PROC);
		}
	}
	ecount = write_rtable(dbg);
	if (isset(TALK_ON)) {
		prnt_str("Modules compiled: %d\n",mcount);
	}
end:
	fclose(outfp);
	fclose(srcfp);
	free_do_list();
	exe = _exe;
	return(ecount);
}

static void write_block(CODE_BLOCK *b, int dbg)
{
	if (dbg) { //currently only lineno is added
		BYTE *lnop = (BYTE *)b + b->block_sz;
		put_lno(lnop, lineno);
		b->block_sz += sizeof(lno_t);
	}
	fwrite(b, b->block_sz, 1, outfp);
	if (ferror(outfp)) sys_err(DISK_WRITE);
}

static void cgen_ljmp(int jcode, cpos_t pos)
{
   char jblock[JBLOCKSZ];
   int jsize = jcode == RETURN || jcode == FCNRTN ? 2 : JBLOCKSZ;
   jblock[0] = 0;    /** indicate long jump **/
   jblock[1] = jcode;
   if (jsize == JBLOCKSZ) put_cpos(&jblock[2], pos);
   fwrite(jblock, jsize, 1, outfp);
   if (ferror(outfp)) sys_err(DISK_WRITE);
}

static void push_addr(cpos_t pos)
{
   if (++top_addr >= MAXLEVEL) sys_err(STACK_FAULT); //0th entry not used
   jmp_addr[top_addr] = pos;
}

static cpos_t pop_addr(void)
{
   if (top_addr <= 0) sys_err(STACK_FAULT);
   return(jmp_addr[top_addr--]);
}

static void push_call_addr(cpos_t pos)
{
   if (++top_caddr >= MAXLEVEL) sys_err(STACK_FAULT); //0th entry not used
   call_addr[top_caddr] = pos;
}

static cpos_t pop_call_addr(void)
{
   if (top_caddr <= 0) sys_err(STACK_FAULT);
   return(call_addr[top_caddr--]);
}

static void cond_codegen(BYTE *pbuf, int size_max, int dbg)
{
   CODE_BLOCK *b = (CODE_BLOCK *)pbuf;
   get_token();
   expression();
   emit(b, size_max);
   write_block(b, dbg);
   cgen_ljmp(JMP_FALSE, PLACE_HOLDER);
   push_addr((cpos_t)(ftell(outfp) - sizeof(cpos_t)));
}

static void mod_addr(void)
{
   cpos_t curr_pos = (cpos_t)ftell(outfp);
   fseek(outfp, (off_t)pop_addr(), 0);
   fwrite(&curr_pos, sizeof(cpos_t), 1, outfp);
   fseek(outfp, (off_t)curr_pos, 0);
}

static void mod_jmp(void)
{
   /* top of stack is location to contain addr for jmp_false in case cond */
   /* the one below is that for true-exec-jump */
   cpos_t pos = pop_addr();
   mod_addr();
   push_addr(pos);
}

static void jmp_codegen(void)
{
   cgen_ljmp(JUMP, PLACE_HOLDER);
   mod_addr();
   push_addr((cpos_t)ftell(outfp) - sizeof(cpos_t));
}

#define is_ljump(pbuf)	(!pbuf[0])

static void exec_(cpos_t faddr)
{
	BYTE *next_pbuf;
	if (!pbuf_base) return;		/** NEEDSWORK error condition **/
	next_pbuf = pbuf_base + faddr;
	for (;;) {
		if (is_ljump(next_pbuf)) {
			struct jblock *jb = (struct jblock *)next_pbuf;
			char vbuf[STRBUFSZ];
			HBVAL *rtv = (HBVAL *)vbuf;	/** udf return value **/
			cpos_t newpos;
			int jmpcode = jb->jmpcode;
			switch (jmpcode) {
			case JMP_FALSE:
				if (istrue()) {
					next_pbuf += JBLOCKSZ;
					break;
				}
			case JUMP:
			case CALL:
				if (escape) 
					return;
				newpos = get_cpos(jb->jaddr);
				if (jmpcode == CALL) {
					push_addr(next_pbuf + JBLOCKSZ - pbuf_base);
					push_pcount(pop_int(0, 255));
					push_call_addr(newpos);
				}
			case FCNRTN:
				if (jmpcode == FCNRTN) {
					memcpy(rtv, topstack, topstack->valspec.len + 2);
					pop_stack(DONTCARE);
				}
			case RETURN:
				if ((jmpcode == RETURN) || (jmpcode == FCNRTN)) {
					int pcount;
					newpos = pop_addr();
					release(R_ALL | R_PROC);
					pcount = pop_pcount();
					while (!stack_empty() && pcount--) pop_stack(DONTCARE);
					pop_call_addr();
				}
				if (jmpcode == FCNRTN) push(rtv);
				if (newpos == (cpos_t)-1) return;	/** interactive level **/
				next_pbuf = pbuf_base + newpos;
			default:
				break;
			}
		}
		else {
			CODE_BLOCK *b = (CODE_BLOCK *)next_pbuf;
			next_pbuf += b->block_sz;
			execute(b);
		}
	}
}

static struct do_entry *proc_bin_srch(char *proc_name, int start, int end)
{
	int mid, i;
	if (start > end) return((struct do_entry *)NULL);
	mid = (start + end) / 2;
	i = namecmp(do_tab[mid].proc_name, proc_name, strlen(proc_name), ID_FNAME);
	if (!i) return(&do_tab[mid]);
	return(i < 0? proc_bin_srch(proc_name, mid + 1, end) : proc_bin_srch(proc_name, start, mid - 1));
}

static int set_proc_lib(char *proc_name)
{
	if (*proc_name == '_') {
		mcount = proc_lib[LIB_SYS].mcount;
		do_tab = proc_lib[LIB_SYS].do_tab;
		pbuf_base = proc_lib[LIB_SYS].pbuf_base;
	}
	else {
		mcount = proc_lib[LIB_USER].mcount;
		do_tab = proc_lib[LIB_USER].do_tab;
		pbuf_base = proc_lib[LIB_USER].pbuf_base;
	}
	return(0);
}

int dbg_flags = 0;

int execute_proc(char *proc_name, int type, int pcount)
{
	char *sav_curr_cmd = curr_cmd;
	struct do_entry *proc;
	set_proc_lib(proc_name);
	if (!do_tab) return(-1);
	proc = proc_bin_srch(proc_name, 0, mcount - 1);
	if (!proc || proc->type != type) return(-1);
	push_addr((cpos_t)-1);
	push_pcount(pcount);
	push_call_addr(proc->faddr);
	curr_cmd = NULL;
	escape = 0;
	exec_(proc->faddr);
	curr_cmd = sav_curr_cmd;	/** restore it as we may be calling usrfcn() **/
	return(0);
}

void do_proc(int pcount)
{
	HBVAL *v = pop_stack(CHARCTR);
	char proc_name[VNAMELEN + 1];
	sprintf(proc_name, "%.*s", v->valspec.len, v->buf);
	if (execute_proc(proc_name, PRG_PROC, get_pcount(pcount))) syn_err(do_tab? PROC_NOTFOUND : PROC_NOLIB);
	if (isset_final(pcount)) quit();
}

void release_proc(int lib_id)
{
	int i;
	for (i=0; i<MAXPROCLIB; ++i) {
		if (lib_id == i || lib_id == LIB_ALL) {
			struct _proc_lib *lib = &proc_lib[i];
			if (lib->pbuf_base) free(lib->pbuf_base);
			lib->pbuf_base = NULL;
			if (lib->do_tab) free(lib->do_tab);
			lib->do_tab = NULL;
		}
	}
}

void load_proc(int lib_id, int dbg)
{
	char profn[FULFNLEN], prgfn[FULFNLEN], *tp;
	time_t prgtime,protime;
	struct stat f_stat;
	BYTE id[2];
	int proc_size = -1;
	int i, j, spec, rc = -1;
	struct do_entry de;
	struct _proc_lib *lib;

	strcpy(profn, form_fn(profctl.filename, 1)); //guaranteed to have .pro extension
	strcpy(prgfn, profn);
	tp = strrchr(prgfn, '.');
	strcpy(tp, ".prg");
	if (lib_id > MAXPROCLIB) db_error(PROC_ERR, profn);
	lib = &proc_lib[lib_id];

	if (stat(prgfn, &f_stat) < 0) prgtime = 0;
	else prgtime = f_stat.st_mtime;
	if (stat(profn,&f_stat)<0) protime = 0;
	else protime = f_stat.st_mtime;
	if (prgtime >= protime) {
		if (!prgtime) db_error(DBOPEN, prgfn);
recomp:
		if (compile_proc(prgfn, dbg? 1: isset(DEBUG_ON)) < 0) db_error(PROC_ERR, prgfn);
	}

	profctl.fp = fopen(profn, "r");
	if (!profctl.fp) goto err;
	if (fread(id, 2, 1, profctl.fp) != 1 || id[1] != OBJ_LIB) goto err;
	if (fread(&proc_size, sizeof(cpos_t), 1, profctl.fp) != 1 || proc_size <= 0) goto err;
	if (proc_size <= 0 || proc_size > MAXPROCSIZE || fseek(profctl.fp, (off_t)proc_size, 0) == -1) goto err;
	if (fread(&spec, sizeof(DWORD), 1, profctl.fp) != 1) goto err;
	lib->mcount = LO_DWORD(spec); dbg_flags = HI_DWORD(spec);
	if (lib->mcount > MAXMCOUNT) goto err;
	if ((dbg && !dbg_flags) || (!dbg_flags && isset(DEBUG_ON)) || (!dbg && dbg_flags && !isset(DEBUG_ON))) {
		fclose(profctl.fp);
		profctl.fp = NULL;
		goto recomp; //to force a re-compile
	}
	lineno = 0;
	release_proc(lib_id);
	lib->do_tab = (struct do_entry *)malloc(lib->mcount * sizeof(struct do_entry));
	if (!lib->do_tab) goto err;
	do_tab = lib->do_tab;
	for (i=0; i<lib->mcount; ++i) {
		if (fread(&de, sizeof(struct do_entry), 1, profctl.fp) != 1) goto err;
		if (de.faddr <= (cpos_t)0) {	/** unresolved **/
			strcpy(profn, de.proc_name);
			db_error(PROC_UNRSLVD, profn);
		}
		for (j=i; j; --j) {
			if (strcmp(do_tab[j-1].proc_name, de.proc_name) > 0) memcpy(&do_tab[j], &do_tab[j-1], sizeof(struct do_entry));
			else break;
		}
		memcpy(&do_tab[j], &de, sizeof(struct do_entry));
	}
	lib->pbuf_base = (BYTE *)malloc(proc_size);
	if (!lib->pbuf_base) sys_err(NOMEMORY);
	fseek(profctl.fp, (off_t)0, 0);
	if (fread(lib->pbuf_base, (int)proc_size, 1, profctl.fp) != 1) goto err;
	rc = 0;
err:
	fclose(profctl.fp);
	profctl.fp = NULL;
	if (rc < 0) {
		release_proc(lib_id);
		db_error(PROC_ERR, profn);
	}
}

static char *get_proc_name(cpos_t faddr)
{
	int i;
	for (i=0; i<mcount; ++i) if (do_tab[i].faddr == faddr) return(do_tab[i].proc_name);
	return(NULL);
}

char *get_proc_context_str(void) //only called at pro run-time
{
	static char ctx_str[STRBUFSZ];
	char *proc_name, *tp;
	int i;
	if (!do_tab || !top_addr) return(NULL);
	tp = ctx_str;
	if (isset(FORM_TO)) {
		snprintf(tp, STRBUFSZ, "Form:%s ", frmfctl.filename);
		tp += strlen(tp);
	}
	snprintf(tp, STRBUFSZ - (int)(tp - ctx_str), "Procedure:%s", form_fn1(0, profctl.filename));
	tp += strlen(tp);
	for (i=1; i<=top_caddr; ++i) { //call_addr[] starts with 1
		if (proc_name = get_proc_name(call_addr[i])) {
			int len = strlen(proc_name);
			if (tp + len + 2 >= ctx_str + STRBUFSZ) break;
			snprintf(tp, STRBUFSZ - (int)(tp - ctx_str), "%s%s", i == 1? "." : "->", proc_name);
			tp += strlen(tp);
		}
	}
	if (dbg_flags & PROC_FLG_LNO) snprintf(tp, STRBUFSZ - (int)(tp - ctx_str), " at line %d", lineno);
	return(ctx_str);
}

static int _onexit = 0;	//ensure that ON_EXIT_PROC is executed only once
void do_onexit_proc(void)
{
	if (!_onexit) execute_proc(ON_EXIT_PROC, PRG_PROC, 0); //ignore error return
	_onexit = 1;
}
