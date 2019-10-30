/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/error.c
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

#include <string.h>
#include "hb.h"
#include "dbe.h"

void db_error0(int errno)
{
	if (!hb_errcode) {
		char *dbfn = curr_ctx->dbfctl.filename;
		if (dbfn) push_string(dbfn, strlen(dbfn) + 1);
		longjmp(env, errno2code(errno));
	}
}

void db_error(int errno, char *msg1)
{
	if (!hb_errcode) {
		push_string( msg1, strlen(msg1) + 1);
		longjmp(env, errno2code(errno));
	}
}
 
void ndx_err(int seq, int errno)
{
	if (!hb_errcode) {
		char *errfn = form_fn(curr_ctx->ndxfctl[seq].filename, 1);
		push_string( errfn, strlen(errfn) + 1);
		longjmp(env, errno2code(errno));
	}
}

void db_error1(int errno, HBVAL *v)
{
	if (!hb_errcode) {
		push(v);
		longjmp(env, errno2code(errno));
	}
}

void db_error2(int errno, int n)
{
	if (!hb_errcode) {
		push_int((long)n);
		longjmp(env, errno2code(errno));
	}
}

void clr_db_err(int errno)
{
	int ctx_id = get_ctx_id();
	switch (errno) {
		case DBCREATE:
		case DBOPEN:
		case DBCLOSE:
		case DBREAD:
		case DBWRITE:
		case DBSEEK:
		case INTERN_ERR:
		default:
			cleandb();
			break;
		case OUTOFRANGE:
			setetof(0);
			break;
		case NTDBASE:
		case NODBT:
		case TOOMANYRECS:
			cleandb();
			break;
		case NTNDX:
		case DUPKEY:
			closeindex();
			break;
		case FILEEXIST:
		case FILENOTEXIST:
		case OPENFILE:
		case INVALID_PATH:
		case PROC_ERR:
		case PROC_UNRSLVD:
		case DB_READONLY:
		case DB_ACCESS:
			//do nothing
			break;
	}
	if (ctx_id != AUX) {
		ctx_select(AUX);
		cleandb();	/** close any copy or append operation**/
		ctx_select(ctx_id);
	}
}
