/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/display.c
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
#include "hbpcode.h"
#include "hbapi.h"

struct _pcodetab {
	char *name;
	int code;
} pcodetab[] = {
{ "NOP",               NOP               },
{ "PUSH",              PUSH              },
{ "UOP",               UOP               },
{ "BOP",               BOP               },
{ "NRETRV",            NRETRV            },
{ "VSTORE",            VSTORE            },
{ "PARAM",             PARAM             },
{ "FCN",               FCN               },
{ "MACRO",             MACRO             },
{ "MACRO_FULFN",       MACRO_FULFN       },
{ "MACRO_FN",          MACRO_FN          },
{ "MACRO_ALIAS",       MACRO_ALIAS       },
{ "MACRO_ID",          MACRO_ID          },
{ "SC_SET",            SC_SET            },
{ "SC_FOR",            SC_FOR            },
{ "SC_WHILE",          SC_WHILE          },
{ "PRNT",              PRNT              },
{ "USE_",              USE_              },
{ "SELECT_",           SELECT_           },
{ "GO_",               GO_               },
{ "SKIP_",             SKIP_             },
{ "LOCATE_",           LOCATE_           },
{ "DISPREC",           DISPREC           },
{ "DEL_RECALL",        DEL_RECALL        },
{ "REPL",              REPL              },
{ "PUTREC",            PUTREC            },
{ "APBLANK",           APBLANK           },
{ "SET_ONOFF",         SET_ONOFF         },
{ "SET_TO",            SET_TO            },
{ "INDEX_",            INDEX_            },
{ "REINDEX_",          REINDEX_          },
{ "SCOPE_F",           SCOPE_F           },
{ "R_CNT",             R_CNT             },
{ "INP",               INP               },
{ "FIND_",             FIND_             },
{ "MPRV",              MPRV              },
{ "MREL",              MREL              },
{ "DISPSTR",           DISPSTR           },
{ "DISPMEM",           DISPMEM           },
{ "IN_READ",           IN_READ           },
{ "MPUB",              MPUB              },
{ "TYPECHK",           TYPECHK           },
{ "PUSH_TF",           PUSH_TF           },
{ "PUSH_N",            PUSH_N            },
{ "RESET",             RESET             },
{ "QUIT_",             QUIT_             },
{ "DRETRV",            DRETRV            },
{ "ARRAYCHK",          ARRAYCHK          },
{ "ARRAYDEF",          ARRAYDEF          },
{ "REF",               REF               },
{ "PACK_",             PACK_             },
{ "DIR_",              DIR_              },
{ "AP_INIT",           AP_INIT           },
{ "AP_REC",            AP_REC            },
{ "AP_NEXT",           AP_NEXT           },
{ "AP_DONE",           AP_DONE           },
{ "CP_INIT",           CP_INIT           },
{ "CP_REC",            CP_REC            },
{ "CP_NEXT",           CP_NEXT           },
{ "CP_DONE",           CP_DONE           },
{ "CREAT_",            CREAT_            },
{ "UPD_INIT",          UPD_INIT          },
{ "UPD_NEXT",          UPD_NEXT          },
{ "UPD_DONE",          UPD_DONE          },
{ "MKDIR_",            MKDIR_            },
{ "DISPFILE",          DISPFILE          },
{ "DELFILE",           DELFILE           },
{ "RENFILE",           RENFILE           },
{ "DELDIR",            DELDIR            },
{ "SU_",               SU_               },
{ "EXPORT_",           EXPORT_           },
{ "SLEEP_",            SLEEP_            },
{ "APNDFIL",           APNDFIL           },
{ "DISPNDX",           DISPNDX           },
{ "TRANSAC_",          TRANSAC_          },
{ "DISPSTAT",          DISPSTAT          },
{ "SCOPE_B",           SCOPE_B           },
{ "PBRANCH",           PBRANCH           },
{ "CALL",              CALL              },
{ "RETURN",            RETURN            },
{ "JUMP",              JUMP              },
{ "JMP_FALSE",         JMP_FALSE         },
{ "JMP_EOF_0",         JMP_EOF_0         },
{ "FCNRTN",            FCNRTN            },
{ "USRLDD",            USRLDD            },
{ "USRHB",             USRHB             },
{ "USRFCN",            USRFCN            },
{ "EXEC_",             EXEC_             },
{ "HBX_",              HBX_              },
{ "STOP",              STOP              },
{ NULL,                -1                }
};

#define LAST_REGULAR_OPCODE	EXPORT_

static char *pcode2name(int opcode)
{
	int i;
	if (opcode <= LAST_REGULAR_OPCODE) return(pcodetab[opcode].name);
	else for (i=LAST_REGULAR_OPCODE+1; pcodetab[i].name; ++i) if (pcodetab[i].code == opcode) return(pcodetab[i].name);
	return("UNKNOWN");
}

static int pcode_has_pdata(opcode, opnd) {
	return((opcode==PUSH || opcode==MACRO || opcode==MACRO_FN || opcode==MACRO_FULFN ||\
				opcode==MACRO_ID || opcode == NRETRV || opcode == VSTORE ||\
				opcode==MPUB || opcode==PARAM || opcode==ARRAYDEF || opcode==REPL) && (opnd != OPND_ON_STACK));
}

#define MAXERRLEN	128

void display_error(HBCONN conn, char *msg, char *ptr, int is_cmd, int p1, int p2)
{
	char msgbuf[4 * STRBUFSZ], *msg2 = NULL, *tp;
	if (is_cmd) {
		if (ptr) {
			int cmdlen = strlen(ptr), start = 0, carret = p1 + p2;
			if (cmdlen > MAXERRLEN) while (carret > 30) { carret -= 10; start += 10; }
			if (p1 != -1) {
				sprintf(msgbuf, "%s\n%s%.*s%s\n%*s", msg, start? "...":"", MAXERRLEN, ptr + start, cmdlen - start > MAXERRLEN? "...":"", carret, "^");
			}
			else sprintf(msgbuf, "%s\n%s%.*s", msg, start? "...":"", MAXERRLEN, ptr + start);
		}
		else strcpy(msgbuf, msg);
	}
	else {
		HBVAL *v = NULL;
		char pdata[STRBUFSZ];
		if (ptr) {
			if (p1 != -1) {
				sprintf(msgbuf, "%s\n%.*s%s", msg, MAXERRLEN, ptr, strlen(ptr) > MAXERRLEN? "..." : "");
				msg2 = msgbuf + strlen(msgbuf) + 1;
				sprintf(msg2, "HB*pcode@(%s,%02xh)", pcode2name(p1), p2);
				tp = msg2 + strlen(msg2);
				if (conn != HBCONN_INVAL && pcode_has_pdata(p1, p2)) v = dbe_pcode_data(conn, p2, pdata);
				if (v) { strcpy(tp, "->"); hb_val2str(conn, v, tp + 2); }
			}
			else {
				strcpy(msgbuf, msg);
				msg2 = ptr;
			}
		}
		else {
			if (p1 != -1) {
				strcpy(msgbuf, msg);
				msg2 = msgbuf + strlen(msgbuf) + 1;
				sprintf(msg2, "HB*pcode@(%s,%02xh)", pcode2name(p1), p2);
				tp = msg2 + strlen(msg2);
				if (conn != HBCONN_INVAL && pcode_has_pdata(p1, p2)) v = dbe_pcode_data(conn, p2, pdata);
				if (v) { strcpy(tp, "->"); hb_val2str(conn, v, tp + 2); }
			}
			else strcpy(msgbuf, msg);
		}
	}
	if (conn != HBCONN_INVAL && !dbe_stackempty(conn)) {
		char vbuf[STRBUFSZ];
		if (dbe_peek_stack(conn, 0, DONTCARE, vbuf, STRBUFSZ)) {
			if (msg2) {
				tp = msg2 + strlen(msg2);
				strcpy(tp, "\nTOS->"); 
			}
			else {
				msg2 = tp = msgbuf + strlen(msgbuf) + 1;
				strcpy(tp, "TOS->");
			}
			tp += strlen(tp);
			hb_val2str(conn, (HBVAL *)vbuf, tp);
		}
	}
			
	display_msg(msgbuf, msg2);
}

void display_comm_err(int errno, char *errmsg, char *msg)
{
	char msgbuf[200];
	if (errno) sprintf(msgbuf, "Communication Error (%d): %s\n%s", errno, errmsg? errmsg:"", msg);
	else sprintf(msgbuf, "Communication Error: %s", msg);
	display_msg(msgbuf, NULL);
}
