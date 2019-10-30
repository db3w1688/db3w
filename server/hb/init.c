/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/init.c
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
#include <string.h>
#include <memory.h>
#include "hb.h"
#include "hbpcode.h"
#include "dbe.h"
#define KW_MAIN
#include "hbkwdef.h"

jmp_buf env;

PCODE *pcode, *code_buf;
BYTE *pdata, *data_buf;
int nextcode, nextdata, pc;

char *cmd_line, *curr_cmd;

char *exp_stack, *stack_limit;
HBVAL *topstack;

int curr_tok, currtok_s, currtok_len;
TOKEN last_tok;

int exe = 0, hb_errcode = 0, restricted = 0, escape = 0;

int nwidth, ndeci, epoch = 1950;

int onoff_flags[N_ONOFF_FLAGS];

HBREGS usr_regs;

int usrcmd;       /** user defined command installed or not **/
int ext_ndx;		/** user supplied indexing routine **/

int *usr_conn;

DBF *dbf_buf, *curr_dbf;

struct ndx *ndx_buf, *curr_ndx;
 
FCTL memfctl = {NULL, -1, NULL}, frmfctl = {NULL, -1, NULL}, profctl= {NULL, -1, NULL};

CTX table_ctx[MAXCTX + 1], *curr_ctx;

char rootpath[FULFNLEN], defa_path[FULFNLEN];

#ifdef EMBEDDED
struct mem_pool fltr_buf, rela_buf, loc_buf, fn_buf;
#endif

int maxbuf, maxndx;

void stack_init(void)
{
	exp_stack = stack_limit + STACKSZ - 1;
	topstack = (HBVAL *)exp_stack;
}

void hb_release_mem(void)
{
	release_proc(LIB_ALL);	/** release procedure mem, if any **/
	memvar_free();
	if (stack_limit) free(stack_limit);
	if (data_buf) free(data_buf);
	if (code_buf) free(code_buf);
	if (cmd_line) free(cmd_line);
}

int hb_init(void)
{
	cmd_line = malloc(STRBUFSZ);
	code_buf = (PCODE *)malloc(sizeof(PCODE) * CODE_LIMIT);
	data_buf = (BYTE *)malloc(SIZE_LIMIT);
	stack_limit = malloc(STACKSZ);
	if (!cmd_line || !code_buf || !data_buf || !stack_limit) return(-1);
	stack_init();
	if (var_init0()) return(-1);
	memset(cmd_line, 0, STRBUFSZ);
	onoff_flags[EXCL_ON] = onoff_flags[NULL_ON] = onoff_flags[DELETE_ON] = onoff_flags[CENTURY_ON] = onoff_flags[DFPI_ON] = S_ON;
	onoff_flags[READONLY_ON] = S_AUTO;
	return(0);
}

void restore_pcode_ctl(PCODE_CTL *save_ctl)
{
	if (save_ctl) {
		curr_cmd = save_ctl->cmd_line;
		rest_token(&save_ctl->tok);
		pcode = save_ctl->pcode;
		pdata = save_ctl->pdata;
		nextcode = save_ctl->nextcode;
		nextdata = save_ctl->nextdata;
		pc = save_ctl->pc;
	}
}

void cmd_init(char *cmd_line, PCODE *code_buf, BYTE *data_buf, PCODE_CTL *save_ctl)
{
	if (save_ctl) {
		save_ctl->pcode = pcode;
		save_ctl->pdata = pdata;
		save_ctl->nextcode = nextcode;
		save_ctl->nextdata = nextdata;
		save_ctl->pc = pc;
		save_ctl->cmd_line = curr_cmd;
		save_token(&save_ctl->tok);
	}
	curr_cmd = cmd_line;
	curr_tok = TNULLTOK;
	currtok_s = currtok_len = 0;
	nextcode = nextdata = 0;
	pcode = code_buf;
	pdata = data_buf;
}
