/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hbpcode.h
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

typedef int32_t	cpos_t;	//32 bit code position for ljmp instructions
typedef int32_t lno_t;	//32 bit line number

//for compiled object file, platform dependent
#define get_cpos(tp)		get_dword(tp)
#define put_cpos(tp,pos)	put_dword(tp,pos)
#define get_lno(tp)		get_dword(tp)
#define put_lno(tp,lno)		put_dword(tp,lno)

#define CBLOCKSZ           16384 //CODE_BLOCK size limit
#define CODE_LIMIT         128   //each pcode is 2 bytes, code_sz in CODE_BLOCK is one byte
#define DATA_LIMIT         256
#define SIZE_LIMIT         (CBLOCKSZ - CODE_LIMIT - sizeof(CODE_BLOCK)) //data size limit

#define MAXPCODE           0x5e
#define PBRANCH            6     /* CALL, RETURN, JUMP change pc */

#define CALL               0x80
#define RETURN             0x81
#define JUMP               0x82
#define JMP_FALSE          0x83
#define JMP_EOF_0          0x84
#define FCNRTN             0x85  /** UDF RETURN **/

#define NOP                0x00
#define PUSH               0x01
#define UOP                0x02
#define BOP                0x03
#define NRETRV             0x04
#define VSTORE             0x05
#define PARAM              0x06
#define FCN                0x07
#define MACRO              0x08
#define MACRO_FULFN        0x09
#define MACRO_FN           0x0a
#define MACRO_ALIAS        0x0b
#define MACRO_ID           0x0c
#define SC_SET             0x0d
#define SC_FOR             0x0e
#define SC_WHILE           0x0f
#define PRNT               0x10
#define USE_               0x11
#define SELECT_            0x12
#define GO_                0x13
#define SKIP_              0x14
#define LOCATE_            0x15
#define DISPREC            0x16
#define DEL_RECALL         0x17
#define REPL               0x18
#define PUTREC             0x19
#define APBLANK            0x1a
#define SET_ONOFF          0x1b
#define SET_TO             0x1c
#define INDEX_             0x1d
#define REINDEX_           0x1e
#define SCOPE_F            0x1f
#define R_CNT              0x20
#define INP                0x21
#define FIND_              0x22
#define MPRV               0x23
#define MREL               0x24
#define DISPSTR            0x25
#define DISPMEM            0x26
#define IN_READ            0x27
#define MPUB               0x28
#define TYPECHK            0x29
#define PUSH_TF            0x2a
#define PUSH_N             0x2b
#define RESET              0x2c
#define QUIT_              0x2d
#define DRETRV             0x2e
#define ARRAYCHK           0x2f
#define ARRAYDEF           0x30
#define REF                0x31
#define PACK_              0x32
#define DIR_               0x33
#define AP_INIT            0x34
#define AP_REC             0x35
#define AP_NEXT            0x36
#define AP_DONE            0x37
#define CP_INIT            0x38
#define CP_REC             0x39
#define CP_NEXT            0x3a
#define CP_DONE            0x3b
#define CREAT_             0x3c
#define UPD_INIT           0x3d
#define UPD_REC            0x3e
#define UPD_NEXT           0x3f
#define UPD_DONE           0x40
#define MKDIR_             0x41
#define DISPFILE           0x42
#define DELFILE            0x43
#define RENFILE            0x44
#define DELDIR             0x45
#define SU_                0x46
#define EXPORT_            0x47
#define SLEEP_             0x48
#define APNDFIL            0x49
#define DISPNDX            0x4a
#define TRANSAC_           0x4b
#define DISPSTAT           0x4c
#define SCOPE_B            0x4d
#define SERIAL_            0x4e

/*********** usr callback routines **************/
#define USRLDD             0x50
#define USRHB              0x51
#define USRFCN             0x52

/*********** procedure call ********************/
#define EXEC_              0x53

/*********** external command exchange *********/
#define HBX_               0x54

#define STOP               0xff

/*********************** macro() ****************/
#define MAC_ID             0x0100      /* only ID allowed in result of mac */
#define MAC_ALIAS          0x0200
#define MAC_FULFN          0x0300      /* only full filename allowed */
#define MAC_FN             0x0400      /* only simple filename allowed */

/******* operand ********/
/******* for SELECT *****/
#define NEXT_AVAIL         0xff

/******* on stack *******/
#define OPND_ON_STACK      0xff

#define P_DBG              0x80  //to be OR'd with PUSH_N oprnd
#define P_SYS              0x40

#define PROC_FLG_LNO       0x0001 //retain line number

#define TRANSAC_CANCEL     0x00
#define TRANSAC_START      0x01
#define TRANSAC_COMMIT     0x02
#define TRANSAC_ROLLBACK   0x03
