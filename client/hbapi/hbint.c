/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/hbint.c
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
#include <stdarg.h>
#include <pwd.h>
#include <setjmp.h>
#include "hb.h"
#include "hbapi.h"

//Note: this module is only used when server code is linked directly to client, no transport

char hb_user[LEN_USER + 1];

extern char rootpath[], defa_path[];

extern jmp_buf env;

extern int dbesrv(), usr();

int get_session_id(void)
{
	return(0);
}

int hb_uid = -1, hb_euid = -1, hb_gid = -1;

int set_session_user(char *userspec, char *spath)
{
	struct passwd *pwe;
	char *tp;
	hb_uid = hb_euid = geteuid();
	pwe = getpwuid(hb_uid);
	if (!pwe) return(-1);
	strcpy(hb_user, pwe->pw_name);
	hb_gid = pwe->pw_gid;
	strcpy(rootpath, pwe->pw_dir);
	if (chdir(rootpath)) return(-1);
	strcpy(defa_path,rootpath);
	tp = defa_path + strlen(defa_path);
	if (spath[0] != '/') *tp++ = '/';
	if (strlen(spath) > 0) sprintf(tp, "%s/", spath);
}

void hb_wakeup(sid)
int sid;
{
}

void hb_sleep(sid)
int sid;
{
}

void log_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr,fmt,ap);
}

int hb_int(HBCONN conn, HBREGS *regs)
{
	HBREGS hb_regs;
	int islogin = regs->ax == DBE_LOGIN;
	memcpy(&hb_regs, regs, sizeof(HBREGS));
/*
printf("ax=%x bx=%x cx=%x dx=%x\n",hb_regs.ax,hb_regs.bx,hb_regs.cx,hb_regs.dx);
if (hb_regs.cx) printf("%s\n",(char *)hb_regs.bx);
*/
	if (islogin) {
		BYTE *tp;
		tp = hb_regs.bx += strlen(hb_regs.bx) + 1;
		tp += strlen(tp) + 1;
		tp += strlen(tp) + 1;
		hb_regs.cx = (int)(tp - hb_regs.bx);
	}
	if (dbesrv(&hb_regs)) {	/** we are linked with engine, no need for conn **/
		dbe_error(conn, &hb_regs);
		if (islogin) dbe_clr_err(conn, 0);
		return(-1);
	}
	else if (islogin) {
		hb_regs.ax = LO_DWORD(hb_regs.dx);
		hb_regs.ex = HI_DWORD(hb_regs.dx);
	}
	else if (regs->ax == DBE_LOGOUT) goto logoff;
	if (hb_regs.cx > 0) memcpy(regs->bx, hb_regs.bx, hb_regs.cx);
	else if (hb_regs.cx < 0) memcpy(hb_regs.bx, regs->bx, -hb_regs.cx);
	if (hb_regs.ax == DBE_LOGOUT) {
logoff:
		if (!!regs->ex) dbe_clr_err(conn, 0);
	}
	memcpy(regs, &hb_regs, sizeof(HBREGS));
	return(0);
}

extern int *usr_conn;
extern HBREGS usr_regs;

int usr_tli(void)
{
	if (!*usr_conn) return(-1);
	return(usr(*usr_conn, &usr_regs));
}

void usr_return(HBCONN *conn, HBREGS *regs, int iserr)
{
}

int hb_session_stamp(int isset)
{
	return(0);
}

void sigpipe(int sig)	/* included only to resolve name, no such signal happens for standalone */
{
}

int is_session_fault(void)
{
	return(0);
}

int is_session_authenticated(void)
{
	return(1);
}

char *peer_ip_address(int sid)
{
	return("_self");
}

char *get_session_user(int sid)
{
	return("_self");
}

int hb_ipc_wait(char *sockname, int sid, int timeout)
{
	return(-1);
}

int hb_ipc_resume(char *sockname)
{
	return(-1);
}

int hb_ipc_get_sid(char *sockname)
{
	return(-1);
}
