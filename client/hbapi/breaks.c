/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/breaks.c
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
#include "hb.h"
#include "hbapi.h"

extern int hb_sigs;

int sys_break(void)
{
	return(!!hb_sigs);
}

static void _ctl_c(int sig)
{
	signal(SIGINT, _ctl_c);
	hb_sigs |= S_INTERRUPT;
	ctl_c();
}

void stop(int sig)
{
	hb_sigs |= S_STOP;
}

void block_breaks(void)
{
	signal(SIGINT, _ctl_c);
	signal(SIGTSTP, stop);
}

void unblock_breaks(void)
{
	signal(SIGTSTP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
}

void clr_break(void)
{
	hb_sigs = 0;
}
