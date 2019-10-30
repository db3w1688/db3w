/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hberrno.h
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

#define NOMEMORY           1
#define BAD_COMMUNICATION  2
#define SESS_MISMATCH      3
#define LOGIN_NOSESS       4
#define LOGIN_NOUSER       5
#define LOGIN_SUID         6
#define LOGIN_SGID         7
#define LOGIN_MULTI        8
#define LOGIN_CRYPT        9
#define LOGIN_NOAUTH       10
#define LOGIN_CHROOT       11
#define LOGIN_CDROOT       12
#define DISK_WRITE         13
#define STACK_FAULT        14
#define TOK_OVERFLOW       15
#define STACK_OVERFLOW     16
#define STACK_UNDERFLOW    17
#define USR_NOTINSTALLED   18
#define VARTBL_OVERFLOW    19
#define DCTBL_OVERFLOW     20
#define MEMBUF_OVERFLOW    21
#define CODEBLOCK_OVERFLOW 22

#define SYN_ERR            23

#define ILL_SYMBOL         SYN_ERR+0
#define UNBAL_QUOTE        SYN_ERR+1
#define TYPE_MISMATCH      SYN_ERR+2
#define INVALID_TYPE       SYN_ERR+3
#define INVALID_OPERATOR   SYN_ERR+4
#define INVALID_VALUE      SYN_ERR+5
#define INVALID_PARMCNT    SYN_ERR+6
#define MISSING_RPAREN     SYN_ERR+7
#define STR_TOOLONG        SYN_ERR+8
#define BAD_EXP            SYN_ERR+9
#define BAD_VARNAME        SYN_ERR+10
#define UNDEF_VAR          SYN_ERR+11
#define VAR_LOCAL          SYN_ERR+12
#define UNDEF_FCN          SYN_ERR+13
#define COMMAND_UNKNOWN    SYN_ERR+14
#define MISSING_VAR        SYN_ERR+15
#define MISSING_TO         SYN_ERR+16
#define MISSING_LIT        SYN_ERR+17
#define EXP_TOOLONG        SYN_ERR+18
#define ILL_KEYWORD        SYN_ERR+19
#define ILL_SELECT         SYN_ERR+20
#define ILL_VALUE          SYN_ERR+21
#define BAD_ALIAS          SYN_ERR+22
#define BAD_FILENAME       SYN_ERR+23
#define BAD_SKEL           SYN_ERR+24
#define EXCESS_SYNTAX      SYN_ERR+25
#define OUT_OF_RANGE       SYN_ERR+26
#define BAD_DATE           SYN_ERR+27
#define MISSING_RBRKT      SYN_ERR+28
#define MISSING_LBRKT      SYN_ERR+29
#define MISSING_FLDNAME    SYN_ERR+30
#define MISSING_WITH       SYN_ERR+31
#define INVALID_SET_PARM   SYN_ERR+32
#define MISSING_ON_OFF     SYN_ERR+33
#define ILL_COND           SYN_ERR+34
#define MISSING_ON         SYN_ERR+35
#define NULL_VALUE         SYN_ERR+36
#define TOOMANY_LEVELS     SYN_ERR+37
#define UNKNOWN_CONTROL    SYN_ERR+38
#define MISSING_MODE       SYN_ERR+39
#define ILL_SYNTAX         SYN_ERR+40
#define ILL_DLMTR          SYN_ERR+41
#define MISSING_FROM       SYN_ERR+42
#define MISSING_FILENAME   SYN_ERR+43
#define BAD_DRIVE          SYN_ERR+44
#define UNBAL_DOWHILE      SYN_ERR+45
#define UNBAL_ENDDO        SYN_ERR+46
#define UNBAL_IF           SYN_ERR+47
#define MISSING_IF         SYN_ERR+48
#define UNBAL_ENDIF        SYN_ERR+49
#define UNBAL_CASE         SYN_ERR+50
#define MISSING_CASE       SYN_ERR+51
#define UNBAL_ENDCASE      SYN_ERR+52
#define UNEXPECTED_END     SYN_ERR+53
#define MISSING_PARM       SYN_ERR+54
#define NO_PRIVATE         SYN_ERR+55
#define BAD_DIM            SYN_ERR+56
#define MISSING_DIM        SYN_ERR+57
#define NOTARRAY           SYN_ERR+58
#define TOOMANY_DIM        SYN_ERR+59
#define NAMECONFLICT       SYN_ERR+60
#define MISSING_INTO       SYN_ERR+61
#define ILL_FCN            SYN_ERR+62
#define UNBAL_LOOP         SYN_ERR+63
#define MISSING_TYPE       SYN_ERR+64
#define MISSING_PROC       SYN_ERR+65
#define MISSING_RETURN     SYN_ERR+66
#define MULTI_DEF          SYN_ERR+67
#define BAD_PRGNAME        SYN_ERR+68
#define PROC_NOLIB         SYN_ERR+69
#define PROC_NOTFOUND      SYN_ERR+70
#define PROC_MISMATCH      SYN_ERR+71
#define ILL_OPCODE         SYN_ERR+72
#define NORLOCK            SYN_ERR+73
#define NOFLOCK            SYN_ERR+74
#define INVALID_CALL       SYN_ERR+75
#define TOOMANYPROCS       SYN_ERR+76
#define ARRAY_TOOBIG       SYN_ERR+77
#define MISSING_ALIAS      SYN_ERR+78
#define MISSING_REPLACE    SYN_ERR+79
#define NOSUCHALIAS        SYN_ERR+80
#define NOSUCHFIELD        SYN_ERR+81
#define MISSING_ARG        SYN_ERR+82
#define TOOMANYARGS        SYN_ERR+83
#define MISSING_FILE       SYN_ERR+84
#define INVALID_ARG        SYN_ERR+85
#define NAME_TOOLONG       SYN_ERR+86

#define DBERROR            SYN_ERR+87

#define DBCREATE           DBERROR+0
#define DBOPEN             DBERROR+1
#define DBCLOSE            DBERROR+2
#define DBREAD             DBERROR+3
#define DBWRITE            DBERROR+4
#define DBSEEK             DBERROR+5
#define BADFLDNAME         DBERROR+6
#define BADFLDTYPE         DBERROR+7
#define OUTOFRANGE         DBERROR+8
#define NTDBASE            DBERROR+9
#define NTNDX              DBERROR+10
#define DUPKEY             DBERROR+11
#define NOTINUSE           DBERROR+12
#define NOINDEX            DBERROR+13
#define TOOMANYRECS        DBERROR+14
#define TOOMANYDIGITS      DBERROR+15
#define NODBT              DBERROR+16
#define BADFILENAME        DBERROR+17
#define FILENOTEXIST       DBERROR+18
#define FILEEXIST          DBERROR+19
#define OPENFILE           DBERROR+20
#define NONAMESPACE        DBERROR+21
#define DB_INVALID_TYPE    DBERROR+22
#define DB_TYPE_MISMATCH   DBERROR+23
#define UNDEF_FIELD        DBERROR+24
#define NO_CTX             DBERROR+25
#define FLTROVERFLOW       DBERROR+26
#define RELAOVERFLOW       DBERROR+27
#define CYCLIC_REL         DBERROR+28
#define TOOMANYNDX         DBERROR+29
#define INTERN_ERR         DBERROR+30
#define NDXOPEN            DBERROR+31
#define NDXREAD            DBERROR+32
#define MEMOCREATE         DBERROR+33
#define MEMOREAD           DBERROR+34
#define MEMOWRITE          DBERROR+35
#define MEMOCOPY           DBERROR+36
#define KEYTOOLONG         DBERROR+37
#define NOTNDXCMBK         DBERROR+38
#define INVALID_PATH       DBERROR+39
#define PROC_ERR           DBERROR+40
#define PROC_UNRSLVD       DBERROR+41
#define DB_READONLY        DBERROR+42
#define DB_ACCESS          DBERROR+43
#define NDXNOTALLOW        DBERROR+44
#define DUP_ALIAS          DBERROR+45
#define MKDIR_FAILED       DBERROR+46
#define DELFIL_FAILED      DBERROR+47
#define RENAME_FAILED      DBERROR+48
#define NOTDIR             DBERROR+49
#define DELDIR_FAILED      DBERROR+50
#define INVALID_USER       DBERROR+51
#define INVALID_GRP        DBERROR+52
#define STILL_OPEN         DBERROR+53
#define NOFORM             DBERROR+54
#define NONDXLOCK          DBERROR+55
#define RBK_INVAL          DBERROR+56
#define INVALID_REC        DBERROR+57
#define TRANSAC_ALRDY_ON   DBERROR+58
#define TRANSAC_EXCL       DBERROR+59
#define TRANSAC_EXIST      DBERROR+60
#define TRANSAC_NOTFOUND   DBERROR+61
#define TRANSAC_NOEND      DBERROR+62
#define TRANSAC_FAIL       DBERROR+63
#define TRANSAC_NORBK      DBERROR+64
#define TBLLOCKED          DBERROR+65
#define UNKNOWNLOCK        DBERROR+66
#define OFFSET_TOO_SMALL   DBERROR+67
#define INVALID_ENTRY      DBERROR+68
#define NOTEMPTY           DBERROR+69
#define BAD_HEADER         DBERROR+70

#define LAST_DBERROR       DBERROR+70
#define USR_ERROR          200
