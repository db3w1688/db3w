/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/usr.c
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
#include "hb.h"
#include "hbapi.h"

//minimum usr() support. linked in only when no usr() is defined in client (e.g., dbetc.c).

#define USR_PRNT_VAL       0x0003

void hb_prnt_val(HBCONN conn, HBVAL *v, char fchar)
{
	char outbuf[STRBUFSZ], *tp;
	hb_val2str(conn, v, outbuf);
	if (fchar != -1) {
		tp = outbuf + strlen(outbuf);
		if (fchar) sprintf(tp, "%c", fchar);
		display_msg(outbuf, NULL);
	}
	else dbe_push_string(conn, outbuf, strlen(outbuf));
}

int usr(HBCONN conn, HBREGS *regs)
{
	char *bx = regs->bx;
	int ax = regs->ax, ex = regs->ex;
	int cx = regs->cx;
	int dx = regs->dx;
	int fx = regs->fx;
	switch (ax) {
	case USR_PRNT_VAL:
		hb_prnt_val(conn, (HBVAL *)bx, ex);
		break;
	default:
		return(-1);
	}
	regs->ax = ax;
	regs->bx = bx;
	regs->cx = cx;
	regs->dx = dx;
	regs->ex = ex;
	regs->fx = fx;
	return(0);
}
