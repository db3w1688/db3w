/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/code.c
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

#include <memory.h>
#include "hb.h"
#include "hbpcode.h"
#include "dbe.h"

void (*hbint[MAXPCODE - PBRANCH])(int opnd);

void codegen(int opcode, int opnd)
{
	PCODE *curr_code = &pcode[nextcode];
	if (nextcode == CODE_LIMIT) sys_err(EXP_TOOLONG);
	if (exe) pc = nextcode; //set pc so that error handling can correctly identify the pcode
	curr_code->opcode = opcode;
	curr_code->opnd = opnd;
	if (!(opcode & 0x80) && exe) {
		(*hbint[opcode])(opnd);
	}
	nextcode++;
}

void emit(CODE_BLOCK *b, int size_max)
{
	int blocksz, codesz, datasz = cb_data_offset(nextdata);

	codesz = nextcode * sizeof(PCODE);
	blocksz = codesz + datasz + sizeof(CODE_BLOCK); //sizeof(CODE_BLOCK)==4
	/* nextdata == datasz */
	if (blocksz > size_max) sys_err(CODEBLOCK_OVERFLOW);
	b->code_sz = nextcode;
	b->block_sz = blocksz;
	b->flags = 0;
	memcpy(&b->buf[0], pcode, codesz);
	memcpy(&b->buf[codesz], pdata, datasz);
}

static void _execute(int start, int limit)
{
	exe = TRUE;
	pc = start;
//	escape = 0;
	while (pc < limit) {
		int op_code = pcode[pc].opcode;
		int op_nd = pcode[pc].opnd;
		switch (op_code) {
		case JMP_EOF_0:
			if (escape || etof(0) || is_scope_end()) {
				unset_scope();
				pc = op_nd;
				continue;
			}
			break;
		case JMP_FALSE:
			if (pop_logic()) break;
		case JUMP:
			pc = op_nd;
			continue;
		case FCNRTN:
			syn_err(ILL_OPCODE);
		case RETURN:
			return;
		case CALL: {
			int save_pc = pc;
			_execute(op_nd, limit);
			pc = save_pc;
			break; }
		default:
			(*hbint[op_code])(op_nd);
		}
		++pc;
	}
}

#define cb_get_lineno(b)	get_lno((BYTE *)b + b->block_sz - sizeof(lno_t))
extern int dbg_flags, lineno;
void cb_set_lineno(int lno);

void execute(CODE_BLOCK *b)
{
	PCODE *savecode = pcode;
	BYTE *savedata = pdata;
	int save_pc = pc;
	pcode = (PCODE *)b->buf;
	pdata = &b->buf[b->code_sz * sizeof(PCODE)];
	if (dbg_flags & PROC_FLG_LNO) lineno = cb_get_lineno(b);
	_execute(0, b->code_sz);
	pcode = savecode;
	pdata = savedata;
	pc = save_pc;
}
