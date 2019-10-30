/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/serial.c
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
#include "hberrno.h"
#include "dbe.h"

long hb_get_serial(char *sername)
{
	char serbuf[sizeof(long)], serfn[FNLEN + 8] ,*fserfn;
	int serfd,i;
	long serial;
	i = strlen(sername);
	if (i<1 || i>FNLEN) syn_err(INVALID_VALUE);
	strcpy(serfn, sername);
	if (!strchr(serfn, '.')) strcat(serfn, ".ser");
	fserfn = form_fn(serfn, 1);
	serfd = s_open(fserfn, OLD_FILE, 0);
	for (i=0; i<=LRETRY && rlock(serfd, (off_t)0, (off_t)sizeof(long), HB_WRLCK)<0; ++i) {
		sleep(1);
	}
	if (i>LRETRY) {
		close(serfd);
		db_error0(NORLOCK);
	}
	if (read(serfd, serbuf, sizeof(long)) != sizeof(long)) db_error(DBREAD,fserfn);
	serial = read_long(serbuf);
	write_long(serbuf, serial + 1);
	lseek(serfd, (off_t)0, 0);
	if (write(serfd, serbuf, sizeof(long)) != sizeof(long)) db_error(DBWRITE,fserfn);	
	close(serfd);
	return(serial);
}

void serial(int parm_cnt)
{
	HBVAL *v;
	char sername[FNLEN+8];
/*
	if (!parm_cnt) syn_err(INVALID_PARMCNT);
*/
	v = pop_stack(CHARCTR);
	if (v->valspec.len < 1 || v->valspec.len > FNLEN) syn_err(INVALID_VALUE);
	sprintf(sername, "%.*s", v->valspec.len, v->buf);
	push_int(hb_get_serial(sername));
}

int hb_set_serial(char *sername, long serial)
{
	char serbuf[sizeof(long)], serfn[FNLEN + 8], *fserfn;
	int serfd,i;
	i = strlen(sername);
	if (i<1 || i>FNLEN) syn_err(INVALID_VALUE);
	strcpy(serfn, sername);
	if (!strchr(serfn, '.')) strcat(serfn, ".ser");
	fserfn = form_fn(serfn, 1);
	if (serial < 0) unlink(fserfn);
	else {
		serfd = s_open(fserfn, NEW_FILE | OVERWRITE, 0);
		write_long(serbuf, serial);
		if (write(serfd, serbuf, sizeof(long)) != sizeof(long)) db_error(DBWRITE,fserfn);
		close(serfd);
	}
	return(0);
}
