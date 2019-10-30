/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/db3w.h
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

/** Max. line length **/
#define MAXLINELEN	4096
#define MAXPATHLEN	2048

/** Max. nested control structs **/
#define MAXLEVEL	16

/** Subset of HTML tokens **/
#define HT_NULLTOK	0
#define HT_EOF		0xffff
#define HT_LIT		0x01
#define HT_TAG		0x02
#define HT_TAG_END	0x03
#define HT_NUMBER	0x05
#define HT_ID		0x06
#define HT_LBRKT	0x07
#define HT_RBRKT	0x08
#define HT_PERIOD	0x09
#define HT_COMMA	0x0a
#define HT_AT		0x0b
#define HT_SEMICOLON	0x0c
#define HT_LPAREN	0x0d
#define HT_RPAREN	0x0e
#define HT_COLON	0x0f
#define HT_EQUAL	0x10
#define HT_EXCLAMATION	0x11
#define HT_DOLLAR	0x11
#define HT_MACRO	0x12
#define HT_DB3W		0x13
#define HT_DB3WEND	0x14
#define HT_EXPR		0x15
#define HT_LBRACE	0x16

#define HT_UNBAL_QUOTE	0xf1
#define HT_UNBAL_TAG	0xf2

#define HT_UNKNOWN	0xff

#define KW_WHILE	0x00
#define KW_ENDWHILE	0x01
#define KW_IF		0x02
#define KW_ELSE		0x03
#define KW_ENDIF	0x04
#define KW_BEGINCASE	0x05
#define KW_CASE		0x06
#define KW_OTHERWISE	0x07
#define KW_ENDCASE	0x08

#define KW_GO		0x09
#define KW_SELECT	0x0a
#define KW_SKIP		0x0b
#define KW_STORE	0x0c
#define KW_DB3W_SESSION	0x0d
#define KW_COOKIE	0x0e
#define KW_SENDMAIL	0x0f
#define KW_FROM		0x10
#define KW_TO		0x11
#define KW_SUBJ		0x12
#define KW_OUTPUT	0x13
#define KW_ALL		0x14
#define KW_HTTP		0x15
#define KW_DB3W_ERROR	0x16
#define KW_FOR		0x17
#define KW_RETURN	0x18
#define KW_PRNT		0x19
#define KW_SHOWVARS	0x1a
#define KW_DEBUG	0x1b
#define KW_ON		0x1c
#define KW_OFF		0x1d
#define KW_SLEEP	0x1e

#define MACCHAR		'^'

/*** Syntax Errors ***/
#define DB3W_NOMEMORY		1
#define DB3W_MISSING_NAME	2
#define DB3W_MISSING_EQUAL	3
#define DB3W_MISSING_ACTION	4
#define DB3W_MISSING_LIT	5
#define DB3W_MISSING_SRC	6
#define DB3W_UNKNOWN_ACTION	7
#define DB3W_UNBAL_MAC		8
#define DB3W_MISSING_PARAM	9
#define DB3W_MISSING_TAG	10
#define DB3W_MISSING_RBRKT	11
#define DB3W_TOOMANY_LEVELS	12
#define DB3W_STACK_FAULT	13
#define DB3W_UNKNOWN_CONTROL	14
#define DB3W_UNBAL_ENDWHILE	15
#define DB3W_UNBAL_ENDIF	16
#define DB3W_UNBAL_ENDCASE	17
#define DB3W_UNBAL_CASE		18
#define DB3W_UNBAL_IF		19
#define DB3W_UNBAL_WHILE	20
#define DB3W_MISSING_CASE	21
#define DB3W_MISSING_IF		22
#define DB3W_ILL_SYNTAX		23
#define DB3W_MISSING_DB3WEND	24
#define DB3W_ILL_EXPR		25
#define DB3W_SET_SENDMAIL	26
#define DB3W_MISSING_ONOFF	27

typedef struct {
	char *line;
	int maxlen;
	int token;
	int tok_s;
	int tok_len;
} HT_TOKEN;

typedef struct {
    char *name;
    char *val;
} HTTP_VAR;

