/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/startup.c
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

void dbe_end_session(HBCONN *conn)
{
	if (conn && *conn != HBCONN_INVAL) {
		HBREGS regs;
		HBCONN conn2 = *conn;
		*conn = HBCONN_INVAL;
		regs.ax = DBE_LOGOUT;
		regs.cx = 0;
		regs.ex = 0;
		hb_int(conn2,&regs);	/** don't care about errors **/
		*conn = HBCONN_INVAL;
	}
	/*
	else exit(1);
	*/
}

HBCONN dbe_init_session(HBSSL_CTX ctx, char *login, int hb_port, int stamp, int maxndx, int  *sid)
/** NULL ctx for unsecured link **/
{
	HBREGS regs;
	HBCONN conn;
	int new_session = *sid == -1;

	regs.ax = DBE_LOGIN;
	regs.bx = login;
	regs.cx = 0; //xlen set by hb_int()
	if (new_session) {
		if (maxndx < 0) maxndx = MIN_NIBUF;
		else if(maxndx == 0) maxndx = DEFA_NIBUF;
		regs.dx = maxndx;
	}
	else regs.dx = stamp;
	regs.ex = hb_port;
	regs.fx = new_session? -1 : 0x7fff & *sid;
	if (hb_int((HBCONN)ctx, &regs)) {
		*sid = regs.ax == DBE_LOGIN? BAD_COMMUNICATION : regs.ax; //error code
		return(HBCONN_INVAL);
	}
	if (new_session) *sid = regs.ex;
	conn = regs.dx;
	return(conn);
}

int dbe_inst_usr(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_INST_USR;
	regs.cx = 0;
	return(hb_int(conn, &regs));
}

void dbe_sleep(HBCONN *conn, int timeout)
{
	/** similar to dbe_end_session() **/
	if (conn && *conn != HBCONN_INVAL) {
		HBREGS regs;
		HBCONN conn2 = *conn;
		*conn = HBCONN_INVAL;
		dbe_clr_err(conn2, 1);	/** clr server error **/
		regs.ax = DBE_SLEEP;
		regs.cx = 0;
		regs.ex = timeout;
		hb_int(conn2, &regs);	/** don't care about errors **/
	}
}

char *dbe_get_ver(HBCONN conn, char *buf)
{
	HBREGS regs;

	regs.ax = DBE_WHOAREYOU;
	regs.bx = buf;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(NULL);
	return(buf);
}

char *dbe_get_user(HBCONN conn, char *user)
{
	HBREGS regs;

	regs.ax = DBE_GET_USER;
	regs.bx = user;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(NULL);
	return(regs.cx? user : NULL);
}

DWORD dbe_get_stamp(HBCONN conn)
{
	HBREGS regs;

	regs.ax = DBE_GET_STAMP;
	regs.cx = 0;
	if (hb_int(conn, &regs)) return(-1);
	return(regs.dx);
}

void hbusr_return(HBCONN *conn) /** never returns **/
{
	HBREGS usr_regs;

	usr_regs.ax = 0;
	usr_regs.cx = 0;
	usr_return(conn, &usr_regs, 0);
}
