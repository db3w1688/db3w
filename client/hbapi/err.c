/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/err.c
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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include "hb.h"
#include "hbapi.h"
#include "hbusr.h"
#include "hberrno.h"

char *error_msgs[] = {
"Not enough memory",
"Communication error",
"Session stamp mismatch",
"Invalid session handle",
"Invalid username",
"Failed to set UID",
"Failed to set GID",
"Multiple logins not allowed",
"Password encryption error",
"Authentication failed",
"Failed to set root directory",
"Failed to change to root directory",
"Disk write error, possibly full",
"System stack fault",
"Too many tokens",
"Expression stack overflow",
"Expression stack underflow",
"User Callback functions not installed",
"Variable table overflow",
"Declaration table overflow",
"Memory variable buffer overflow",
"Code block overflow",
"Illegal symbol",
"Unbalanced quotes",
"Type mismatch",
"Invalid type",
"Invalid operator",
"Invalid value",
"Invalid number of parameters",
"Missing right parenthesis",
"String too long",
"Bad expression",
"Bad variable name",
"Undefined variable",
"Variable already local or hidden",
"Undefined function",
"Unknown command",
"Missing NAME identifier",
"Missing 'TO' keyword",
"Missing literal",
"Expression too long",
"Illegal keyword",
"Illegal 'SELECT' value",
"Illegal value",
"Bad alias name",
"Bad or illegal filename",
"Bad skeleton",
"Excessive syntax",
"Record out of range or not valid",
"Invalid date",
"Missing right bracket",
"Missing left bracket",
"Missing field name",
"Missing 'WITH' keyword",
"Invalid 'SET' parameter",
"Missing 'ON' or 'OFF' keyword",
"Illegal condition",
"Missing 'ON' keyword or expression",
"Variable does not contain value",
"Too many nested levels",
"Unknown or illegal control structure",
"Missing or illegal mode",
"Illegal syntax",
"Illegal delimiter",
"Missing 'FROM' keyword",
"Missing filename",
"Bad drive specifier",
"Unbalanced 'DO WHILE'",
"Unbalanced 'ENDDO'",
"Unbalanced 'IF'",
"'ELSE' without 'IF'",
"Unbalanced 'ENDIF'",
"Unbalanced 'DO CASE'",
"Missing 'DO CASE' keyword",
"Unbalanced 'ENDCASE'",
"Unexpected end of input",
"Missing parameter specification",
"Private variables not allowed at this level",
"Bad array element specifier",
"Missing array element specifier",
"The named variable is not an array",
"Too many dimensions",
"Array name must not conflict with other visible variable name",
"Missing 'INTO' keyword",
"Illegal function",
"'LOOP' or 'EXIT' without a WHILE loop",
"Missing 'TYPE' Keyword",
"Missing 'PROCEDURE' or 'FUNCTION' Keyword",
"Missing 'RETURN' or Keyword",
"Procedure/Function Module multiply defined",
"Bad Procedure/Function name",
"No Procedure Library loaded",
"Procedure not found",
"Procedure/Function mismatch",
"Illegal Opcode",
"Cannot obtain record lock",
"Cannot obtain file lock",
"Invalid HB*Engine CALL ID",
"Too many procedure modules",
"Array too big",
"Missing ALIAS specifier",
"Missing REPLACE specifier",
"No such ALIAS",
"No such Field",
"Missing argument",
"Too many arguments",
"Missing FILE keyword",
"Invalid argument",
"Name too long",
"Can not create file",
"Can not open file",
"Can not close file",
"Can not read file",
"Can not write file",
"Can not seek file",
"Bad field name",
"Bad field type",
"Record out of range",
"File format not recognized",
"Record not in index",
"Duplicate entry in index",
"No database table in use",
"No index in use",
"Too many records",
"Too many digits",
"Cannot open MEMO file",
"Bad filename",
"File or directory does not exist",
"File or directory already exists",
"File already open",
"File name space exhausted",
"Invalid Field Type",
"Field Type mismatch",
"Undefined field",
"No more available Table Context",
"Filter buffer overflow",
"Relation buffer overflow",
"Cyclic relation not allowed",
"Too many indexes",
"Internal error",
"No such index",
"Error reading index",
"Error creating MEMO file",
"Error reading MEMO file",
"Error writing MEMO file",
"Error copying MEMO field",
"Index Key too long",
"Not a composite index key",
"Invalid path",
"Procedure file error",
"Procedure not defined in library",
"Table is READONLY",
"Access permission denied",
"No Indexes allowed during this operation",
"Duplicate Alias",
"Failed to make directory",
"Failed to delete file",
"Failed to rename file",
"Not a directory",
"Failed to delete directory",
"Invalid user",
"Invalid group",
"File is still open",
"No Form file specified",
"Failed to get index lock",
"Invalid RBK file",
"Invalid database record",
"Another transaction already started",
"Record update failed because it is part of an active transaction",
"The named transaction already exists",
"Transaction does not exist",
"Transaction has modified records. Use ROLLBACK to cancel",
"Transaction failed",
"Record backup file does not exist or is damaged",
"Table locked by another session",
"Unknown lock type",
"Offset too small",
"Invalid index page entry number"
};

int dbe_errno = 0;

#define is_sys_err(code)        (code<SYN_ERR)

static char *usrerr2str(int code)
{
	switch (code - USR_ERROR) {
	case USR_FCN:
		return("USR_FCN");
	case USR_PRNT_VAL:
		return("USR_PRNT_VAL");
	case USR_DISPREC:
		return("USR_DISPREC");
	case USR_DISPSTR:
		return("USR_DISPSTR");
	case USR_READ:
		return("USR_READ");
	case USR_DISPFILE:
		return("USR_DISPFILE");
	case USR_EXPORT:
		return("USR_EXPORT");
	case USR_DISPNDX:
		return("USR_DISPNDX");
	case USR_CREAT:
		return("USR_CREAT");
	case USR_INPUT:
		return("USR_INPUT");
	case USR_AUTH:
		return("USR_AUTH");
	case USR_GETACCESS:
		return("USR_GETACCESS");
	default:
		return("UNKNOWN");
	}
}

char *dbe_errmsg(int code, char *msgbuf)
{
	sprintf(msgbuf,"HB*Engine ERROR (%d)", code);
	char *tp = msgbuf + strlen(msgbuf);
	if (code >= USR_ERROR) sprintf(tp, ": %s", usrerr2str(code));
	else if (code <= LAST_DBERROR) sprintf(tp, ": %s", error_msgs[code - 1]);
	else sprintf(tp, ": UNKNOWN ERROR");
	return(msgbuf);
}

void dbe_error(HBCONN conn, HBREGS *regs)
{
	char msgbuf[STRBUFSZ];
	if (dbe_errno) {	/** no multiple errors **/
		return;
	}
	dbe_errno = regs->ax;
	if (!dbe_errno) return;
	dbe_errmsg(dbe_errno, msgbuf);
	if (regs->ex == 0) { //we have command line
		char *cmd_line = regs->bx;
		display_error(conn, msgbuf, cmd_line, 1, LO_DWORD(regs->dx), HI_DWORD(regs->dx));
	}
	else { //procedure context or other
		int opcode = regs->dx < 0? -1 : LO_DWORD(regs->dx);
		int oprnd = HI_DWORD(regs->dx);
		display_error(conn, msgbuf, regs->cx? regs->bx : NULL, 0, opcode, oprnd);
	}
}

void dbe_clr_err(HBCONN conn, int sendtoserver)
{
	if (sendtoserver) {
		HBREGS regs;

		regs.ax = DBE_CLR_ERR;
		regs.cx = 0;
		hb_int(conn, &regs);	/** don't check for -1 return **/
	}
	dbe_errno = 0;
}

void hb_error(HBCONN conn, int code, char *cmd_line, int p1, int p2)
{
	char msgbuf[STRBUFSZ];
	if (dbe_errno) return;	/** avoid multiple errors **/
	display_error(conn, dbe_errmsg(code, msgbuf), cmd_line, 1, p1, p2);
}
