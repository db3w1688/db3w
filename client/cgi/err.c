/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/err.c
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
#include <setjmp.h>
#include "db3w.h"
#include "w3util.h"

extern jmp_buf hb_env;

void syn_error(line, code)
char *line;
int code;
{
	char outstr[MAXLINELEN], *tp, *tp2, *errstr;
	switch (code) {
	case DB3W_NOMEMORY:
		errstr = "Not enough memory";
		break;
	case DB3W_MISSING_NAME:
		errstr = "Missing NAME identifier";
		break;
	case DB3W_MISSING_EQUAL:
		errstr = "'=' expected";
		break;
	case DB3W_MISSING_ACTION:
		errstr = "ACTION keyword expected";
		break;
	case DB3W_MISSING_LIT:
		errstr = "Literal expected";
		break;
	case DB3W_UNKNOWN_ACTION:
		errstr = "Unknown Action";
		break;
	case DB3W_MISSING_SRC:
		errstr = "Source file expected";
		break;
	case DB3W_UNBAL_MAC:
		errstr = "Unbalanced Cookie macro";
		break;
	case DB3W_MISSING_PARAM:
		errstr = "Missing required parameter(s)";
		break;
	case DB3W_MISSING_TAG:
		errstr = "Missing Tag identifier";
		break;
	case DB3W_MISSING_RBRKT:
		errstr = "Missing '>'";
		break;
	case DB3W_TOOMANY_LEVELS:
		errstr = "Too many nested control structures";
		break;
	case DB3W_STACK_FAULT:
		errstr = "Control structure corrupted";
		break;
	case DB3W_UNKNOWN_CONTROL:
		errstr = "Unknown control structure";
		break;
	case DB3W_UNBAL_ENDWHILE:
		errstr = "Unbalanced ENDWHILE";
		break;
	case DB3W_UNBAL_ENDIF:
		errstr = "Unbalanced ENDIF";
		break;
	case DB3W_UNBAL_ENDCASE:
		errstr = "Unbalanced ENDCASE";
		break;
	case DB3W_UNBAL_CASE:
		errstr = "Unbalanced CASE";
		break;
	case DB3W_UNBAL_IF:
		errstr = "Unbalanced IF";
		break;
	case DB3W_UNBAL_WHILE:
		errstr = "Unbalanced WHILE";
		break;
	case DB3W_MISSING_CASE:
		errstr = "Missing CASE keyword";
		break;
	case DB3W_MISSING_IF:
		errstr = "Missing IF keyword";
		break;
	case DB3W_ILL_SYNTAX:
		errstr = "Illegal syntax";
		break;
	case DB3W_MISSING_DB3WEND:
		errstr = "Missing '}'";
		break;
	case DB3W_ILL_EXPR:
		errstr = "Bad HB*Engine Expression";
		break;
	case DB3W_SET_SENDMAIL:
		errstr = "Error setting up sendmail";
		break;
	case DB3W_MISSING_ONOFF:
		errstr = "Missing ON/OFF keyword";
		break;
	default:
		errstr = "Unknown Error Code!";
	}
	sprintf(outstr, "DB3W ERROR (%d): %s", code, errstr);
	display_msg(outstr, line);
	longjmp(hb_env, code);
}
