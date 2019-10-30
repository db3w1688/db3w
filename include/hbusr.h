/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hbusr.h
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

#define USR_FCN            0x0001
#define USR_PRNT_VAL       0x0002
#define USR_DISPREC        0x0003
#define USR_DISPSTR        0x0004
#define USR_READ           0x0005
#define USR_DISPFILE       0x0006
#define USR_EXPORT         0x0007
#define USR_DISPNDX        0x0008
#define USR_CREAT          0x0009
#define USR_INPUT          0x000a

/****** autentication and access ******************/
#define USR_AUTH           0x0010
#define USR_GETACCESS      0x0011

/************ USR_INPUT dx value *********/
#define USR_GETC           0x00
#define USR_GETS           0x01
#define USR_GETS0          0x02

/************* front end keywords ************/
#define MAXHBKWLEN	64
#define HBKWTABSZ       20

#define HBKW_CMDLST		0x101
#define HBKW_DISPLAY		0x102
#define HBKW_FCNLST		0x013
#define HBKW_HELP		0x104
#define HBKW_KWLST		0x105
#define HBKW_MEMORY		0x106
#define HBKW_CTLLST		0x107
#define HBKW_QUIT		0x108
#define HBKW_RECORD		0x109
#define HBKW_RESUME		0x10a
#define HBKW_SCOPE		0x10b
#define HBKW_SELECT		0x10c
#define HBKW_SERIAL		0x10d
#define HBKW_SHELL		0x10e
#define HBKW_SLEEP		0x10f
#define HBKW_STACK		0x110
#define HBKW_STRUCTURE		0x111
#define HBKW_TAKEOVER		0x112
#define HBKW_WAIT		0x113
#define HBKW_WAKEUP		0x114

#define HBKW_START		HBKW_CMDLST
struct hb_kw {
	char *name;
	WORD kw_val;
};

extern struct hb_kw hb_keywords[];

#define MAXUSERLEN	64 //maximum login user ID len

/******* password encryption method *************/
#define CRYPT_DES	0
#define CRYPT_MD5	1
#define CRYPT_BLOWFISH	2
#define CRYPT_SHA256	5
#define CRYPT_SHA512	6

#ifdef _HBCLIENT_
int hb_disprec(HBCONN conn, int argc);
int hb_dispstru(HBCONN conn);
char *hb_gets(char *s, int len);
int hb_input(HBCONN conn, int mode);
int hb_prnt_val(HBCONN conn, HBVAL *v, int fchar);
int hb_table_create(HBCONN conn, int cnt);
char *hb_uppcase(char *str);
char *hb_val2str(HBCONN conn, HBVAL *v, char *outbuf);
#endif
