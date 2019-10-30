/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hb.h
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

#define HB
#include <stdio.h>
#include <stdint.h>
#define __USE_ISOC11
#include <stdlib.h>
#include <sys/types.h>

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t LWORD;
typedef DWORD date_t;
typedef LWORD blkno_t;	//64 bit memo block number
typedef LWORD recno_t;	//64 bit record number

#ifndef MAXUSERS
#define MAXUSERS	2
#endif

#ifndef FALSE
#define FALSE	0
#define TRUE	1
#endif

#include "hberrno.h"

#define VERS_LEN	4 // on-disk header version length: byte0:id, byte1-3: not used
#define MKR_MASK	0x07
#define DB3_MKR		0x03
#define DB5_MKR		0x05 //new header format
#define DB3_MEMO	0x80
#define DB3_BLOB	0x40
#define is_db3(hdr)	(MKR_MASK & hdr->id == DB3_MKR)
#define is_db5(hdr)	(MKR_MASK & hdr->id == DB5_MKR)
#define table_set_mkr(hdr, mkr)	(hdr->id=(hdr->id & ~(MKR_MASK)) | mkr)
#define table_has_memo(hdr) (DB3_MEMO & hdr->id)
#define table_has_blob(hdr) (DB3_BLOB & hdr->id)

#define EOF_MKR         0x1a
#define VAL_MKR         ' ' //record valid
#define DEL_MKR         '*' //record deleted
#define TRANS_MKR       '!' //record in on-going transaction

#define MAXCTX		100     /* maximum number of table contexts */
#define AUX		MAXCTX  /* aux table context, for append, copy, etc */
#define MAXNDX		16      /* maximun number of indexes in each area */
//#define DEFA_NDBUF	10      //open table buffer
#define DEFA_NDBUF	MAXCTX  //set to be the same ax MAXCTX since DBF struct only contain pointers to
                                //header and record buffers
#define DEFA_NIBUF	16	//open index control buffers
#define MIN_NDBUF	3
#define MIN_NIBUF	7

#define SWITCHAR	'/'
#define ROOT_DIR	"/"
#define CURRENT_DIR	"./"
#define PARENT_DIR	"../"

#define FULFNLEN	1024

/* #define FNLEN		8	*/
#define EXTLEN		3
#define FLDNAMELEN  20  // fieldname length, db5 format
#define FLD3NAMLEN	10  // fieldname length, db3 format
#define VNAMELEN	20	/* memory variable name length */
#define FNLEN		16	//VNAMELEN-EXTLEN-1
#define STRBUFSZ	2048
#define MAXSTRLEN	2046	/* STRBUFSZ - 2 */
#define MAXKEYSZ	1024	/* Maximum index key value size */
#define STACKSZ		65536
#define MAXCMDLEN	4096
#define TRANSID_LEN	VNAMELEN

#define EOH          0x0d   /* end-of-header marker */
#define MAXFLD       1023   /* maximum number of fields (MAXHDLEN - HDRSZ) / FLDWD, db5 format */
#define HD3SZ        32     /* fixed size portion of db header, db3 format */
#define HDRSZ        64     /* fixed size portion of db header, db5 format */
#define FLD3WD       32     /* header field width, db3 format */
#define FLDWD        64     /* header field width, db5 format */
#define MAXHDLEN     65536  /* use db5 format as the max*/
#define EOM          0x7f   /* end-of-meta marker */
#define META_HDR_ALIGN   (sizeof(DWORD)-1) //table header EOH
#define META_ENTRY_OFF   sizeof(DWORD)
#define META_ENTRY_LEN   32
#define MIN_META_LEN (META_ENTRY_OFF+META_ENTRY_LEN+1) //+EOH
#define META_NDX     0x01
#define is_meta_ndx(meta) (meta[0] == META_NDX)
#define meta_entry_cnt(meta) (meta[1])
#define meta_entry_start(meta) (meta + META_ENTRY_OFF)


#define BLKSIZE      512    /* .DBT BLOCK SIZE */

#ifdef  DEMO
#define MAXNRECS     500
#else
#define MAXNRECS     (UINTMAX_MAX - 1) //2^64 - 2)
#endif

#define MAXRECSZ     65536   /* MAX. RECORD SIZE */
#define MAXMEMOSZ    1048576
#define MINRECSZ     2

#define MAXDIGITS 	20 //ULONG_MAX has 20 decimal digits
#define DATELEN 	8
#define MEMOLEN		10
#define LEN_PASSWD	16
#define LEN_USER	32

/************ val.typ *********************/
/** simple type ******/
#define NULLTYP		0x00
#define LOGIC		0x01
#define NUMBER		0x02
#define DATE		0x03
#define CHARCTR		0x04

/** complex type *****/
#define ADDR		0x08 /*** passing by reference **/
#define ARRAYTYP	0x09
#define MEMO		0x0a
#define BLOB		0x0b

/**** see HBVAL below for val.typ==NUMBER, val.numspec.deci ***/
#define DECI_MAX	18	//better than double precision which has 15.95 decimal digits of precision.
#define REAL		0x3f
#define num_is_null(num)        (num.deci == (BYTE)-1)
#define num_set_null(num)       (num.deci = (BYTE)-1)
#define num_is_real(num)	(num.deci == REAL)
#define num_is_integer(num)	(num.deci == 0)
#define val_is_real(val)	(val->numspec.deci == REAL)
#define val_is_integer(val)	(val->numspec.deci == 0)

#define UNDEF		0x0f
#define DONTCARE	0xff /*** for pop stack ***/

#define MINWIDTH	4
#define MAXWIDTH	MAXDIGITS //including deci and .
#define MAXDCML		DECI_MAX
#define DEFAULTDCML	2
#define DEFAULTWIDTH	10

/************ for PRIVATE,RELEASE commands ******/
#define R_ALL		0x80
#define R_LIKE		0x40
#define R_EXCEPT	0x20
#define R_PROC		0x10 /*** procecure end ***/

/*********** for loadskel() ********/
#define SKEL_VARNAME		0
#define SKEL_FILENAME		1

/*********** for DBE_GET_ID *********/
#define ID_ALIAS		0
#define ID_DBF_FN		1
#define ID_NDX_FN		2
#define ID_FRM_FN		3

/*********** for DBE_GET_STAT *******/
#define STAT_CTX_CURR	0
#define STAT_CTX_NEXT	1
#define STAT_IN_USE	2
#define STAT_ORDER	3
#define STAT_N_NDX	4
#define STAT_RO		5

/************ for namecmp() *****************/
#define KEYWORD      0
#define ID_VNAME     1 //var name, fld name
#define ID_FNAME     2 //filename, alias
#define ID_PARTIAL   3

/*********** for arrays *************/
#define MAXADIM		13
#define MAXAELE		10000

/*********** token type *********************/
#define TNULLTOK		0x0000
#define TID			0x0001
#define TLOGIC			0x0002
#define TINT			0x0003
#define TNUMBER			0x0004
#define TLIT			0x0005
#define TCOMMA			0x0006
#define TSEMICOLON		0x0007
#define TLPAREN			0x0008
#define TRPAREN			0x0009
#define TAT			0x000a
#define TLBRKT			0x000b
#define TRBRKT			0x000c
#define TCOLON			0x000d
#define TARROW			0x000e
#define TPOUND                  0x000f
#define TPERIOD                 0x0010

/********  the following are also used as operators, must fit in 1 BYTE **/
#define TAND         0x0011
#define TOR          0x0012
#define TNOT         0x0013

#define REL_OP       0x0020
#define TEQUAL       0x0021
#define TGTREQU      0x0022
#define TGREATER     0x0023
#define TLESSEQU     0x0024
#define TNEQU        0x0025
#define TLESS        0x0026
#define TQUESTN      0x0027

#define ADD_OP       0x0030
#define TPLUS        0x0031
#define TMINUS       0x0032

#define MUL_OP       0x0040
#define TMULT        0x0041
#define TDIV         0x0042

#define TPOWER       0x0050

#define TMAC         0x1000
#define TTOK         0x1001

#define TEOF         0x7fff

/***** Set ON/OFF parameters *************/
#define N_ONOFF_BITS                    2
#define ONOFF_MASK                      0x03
#define S_OFF                           0
#define S_ON                            1
#define S_AUTO                          2

#define CENTURY_ON                      0x00 //deprecated
#define DELETE_ON                       0x01
#define EXACT_ON                        0x02
#define EXCL_ON                         0x03
#define TALK_ON                         0x04
#define UNIQUE_ON                       0x05
#define DEBUG_ON                        0x06
#define NULL_ON                         0x07
#define READONLY_ON                     0x08
#define DFPI_ON                         0x09 //Decimal Floating Point

#define N_ONOFF_FLAGS                   0x10  //for future expansion
#define is_onoff(setting_id)            (setting_id < N_ONOFF_FLAGS)

/***** Set TO parameters *****************/
#define DECI_TO                            0x10
#define DEFA_TO                            0x11
#define FILTER_TO                          0x12
#define FORM_TO                            0x13
#define INDEX_TO                           0x14
#define ORDER_TO                           0x15
#define PATH_TO                            0x16
#define PROC_TO                            0x17
#define RELA_TO                            0x18
#define WIDTH_TO                           0x19
#define EPOCH_TO                           0x1a
#define DATEFMT_TO                         0x1b

/*************** reset mask *********************/
#define RST_ALL      0xff
#define RST_UNLOCK   0x01
#define RST_SYS      0x80

/*************** table open bit mask ************/
/** for the USE pcode mode, must fit in 1 byte *****/
#define U_USE           0x08
#define U_INDEX         0x01 // has index(es)
#define U_DEFA_NDX      0x02
#define U_ALIAS         0x04
#define U_READONLY      0x10
#define U_EXCL          0x20
#define U_DENYW         0x40
#define U_SHARED        0x80
#define use_index(mode) (mode & 0x03)

/** for the dbe_use() or internal direct use() call **/
#define U_CREATE        0x0100
#define U_DBT           0x0200

/** for get_fn() *************/
#define GETFN_MASK      0xf000
#define U_NO_CHK_EXIST  0x1000
#define U_CHK_DUP       0x2000

/**************** locate direction *******/
#define FORWARD       1
#define BACKWARD      2

/************** lock types ***************/
#define HB_UNLCK      0
#define HB_RDLCK      1
#define HB_WRLCK      2
#define HB_FWLCK      3
#define HB_FRLCK      4

/************** file types for DIR command **************/
#define F_ALL         0x7fff
#define F_DBF         0x0001
#define F_NDX         0x0002
#define F_DBT         0x0004
#define F_MEM         0x0008
#define F_TXT         0x0010
#define F_PRG         0x0020
#define F_PRO         0x0040
#define F_SER         0x0080
#define F_DIR         0x0100
#define F_ZIP         0x0200
#define F_TBL         0x0400
#define F_IDX         0x0800

#define DIR_TO_DBF    0x8000 //output to DBF

/************** directory entry blob field offsets *********/
#define DIRENTNAME    0
#define DIRENTEXT    18
#define DIRENTTYPE   24
#define DIRENTPERM   25
#define DIRENTMTIME  26
#define DIRENTSIZ    (DIRENTMTIME+sizeof(time_t))
#define DIRENTLEN    64

/************** procedure stuff *********/
#define  MAXPROCSIZE (1048576*32) //32MB
#define  MAXMCOUNT   32767
#define  PRG_REG     0x00
#define  PRG_PROC    0x01
#define  PRG_FUNC    0x02

/************** access types & rights ***********/
#define HBDB         0x01
#define HBTABLE      0x02
#define HBFIELD      0x03

/** HBMODIFY, HBCREATE, HBAPPEND, HBDELETE, HBALL valid for HBTABLE only **/
#define HBNONE       0x00
#define HBREAD       0x01
#define HBMODIFY     0x02
#define HBCREATE     0x04
#define HBAPPEND     0x08
#define HBDELETE     0x10
#define HBALL        0xff

/* for obsolete 3-byte date field in db header */
#define LEGACY_YY	80
#define LEGACY_MM	1
#define LEGACY_DD	1

/*************** data structures ****************/
typedef struct {
	union {
		struct {
			unsigned typ : 4;	//must be the same as numspec.typ
			unsigned len : 12;	//0-4095
		} __attribute__((packed)) valspec;
		struct {
			unsigned typ : 4;
			unsigned len : 6;	//max 64 bytes. double is 8
			unsigned deci : 6;	//0..62 digits, DECI_MAX is 18, sentinel REAL is 0x3f or 63
		} __attribute__((packed)) numspec;	//for NUMBER type
	};
	BYTE buf[];
} __attribute__((packed)) HBVAL; //2 bytes

typedef struct {
	BYTE deci;
	union {
		double r;
		int64_t n;	//64 bits 
	};
} number_t;

typedef struct {
	DWORD fd;
	blkno_t blkno;
	blkno_t blkcnt;
} __attribute__((packed)) memo_t;

typedef struct {
	int fd;
	blkno_t blkno;
	DWORD size;
} __attribute__((packed)) blob_t;

typedef struct token {
   int tok_typ;
   int tok_s;
   int tok_len;
} TOKEN;

typedef struct {
   WORD block_sz;
   BYTE code_sz;
   BYTE flags;  //not used
   BYTE buf[];
} __attribute__((packed, aligned(1))) CODE_BLOCK;

typedef struct {
   BYTE opcode;
   BYTE opnd;
} __attribute__((packed, aligned(1))) PCODE;

typedef struct {
   char *name;
   int kw_val;
   int cmd_id;
   int fcn_id;
   int setting_id;
} HBKW;

typedef struct {
	char *filename;
	int fd;
	FILE *fp;
} FCTL;

typedef struct {
	int32_t ax; //input: HBAPI call id, return: status
	BYTE *bx;
	int32_t cx; //transfer byte count
	int32_t dx; //32-bit parameter
	int32_t ex; //flags, additional parameter
	int32_t fx;	//dx + fx == 64-bit parameter
} HBREGS;

typedef struct {
	char name[FLDNAMELEN + 1]; /* null BYTE */
	int ftype;
	int tlength;
	int dlength;
	int offset;
	int x0;  /** reserved, used for access control, offset calc, etc.. **/
	int x1;  /** reserved, used for access control, offset calc, etc.. **/
} FIELD;
 
typedef struct {
	int id;
	recno_t reccnt;
	int hdlen; //on-disk header length (including EOH)
	int doffset; //data start (>=hdlen)
	int meta_len;
	int byprec;
	int access;
	int fldcnt;
	FIELD fdlist[];
} TABLE;

typedef struct {
	PCODE *pcode;
	BYTE *pdata;
	int nextcode;
	int nextdata;
	int pc;
	char *cmd_line;
	TOKEN tok;
} PCODE_CTL;

#ifdef UNALIGNED_MEM
#define	get_short(tp)		(*(short *)(tp))
#define get_word(tp)		(*(WORD *)(tp))
#define get_long(tp)		(*(int64_t *)(tp))
#define get_dword(tp)		(*(DWORD *)(tp))
#define get_lword(tp)		(*(LWORD *)(tp))
#define get_foff(tp)		(*(off_t *)(tp))
#define get_double(tp)		(*(double *)(tp))
#define put_short(tp,i)		*(short *)(tp)=i
#define	put_long(tp,l)		*(int64_t *)(tp)=l
#define put_dword(tp,i)		*(DWORD *)(tp)=i
#define put_lword(tp,l)		*(LWORD *)(tp)=l
#define put_foff(tp,o)		*(off_t *)(tp)=o
#define	put_double(tp,df)	*(double *)(tp)=df
#else
short get_short(char *tp);
unsigned short get_word(char *tp);
int64_t get_long(char *tp);
off_t get_off_t(char *tp);
double get_double(char *tp);
void put_short(char *tp, short n);
void put_long(char *tp, int64_t n);
void put_foff(char *tp, off_t n);
void put_double(char *tp, double n);
#endif

//On disk format is little endian
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	read_short(tp)		get_short(tp)
#define read_word(tp)		get_word(tp)
#define read_long(tp)		get_long(tp)
#define read_dword(tp)		get_dword(tp)
#define read_lword(tp)		get_lword(tp)
#define read_double(tp)		get_double(tp)
#define read_date(tp)		read_dword(tp)
#define write_short(tp,i)	put_short(tp,i)
#define write_long(tp,l)	put_long(tp,l)
#define write_dword(tp,n)	put_dword(tp,n)
#define write_lword(tp,n)	put_lword(tp,n)
#define	write_double(tp,df)	put_double(tp,df)
#define write_date(tp,dd)   put_dword(tp,dd)
#else
short read_short(char *tp);
int64_t read_long(char *tp);
off_t read_foff(char *tp);
double read_double(char *tp);
date_t read_date(char *tp);
void write_short(char *tp, short n);
void write_long(char *tp, long n);
void write_foff(char *tp, off_t n);
void write_double(char *tp, double n);
void write_date(char *tp, date_t dd);
#endif

#define get_int32(tp)		get_dword(tp)
#define get_recno(tp)		get_dword(tp)
#define get_pageno(tp)		get_dword(tp)
#define get_blkno(tp)		get_dword(tp)
#define get_date(tp)		get_dword(tp)
#define put_word(tp,n)		put_short(tp,n)
#define put_int32(tp,n)		put_dword(tp,n)
#define put_recno(tp,n)		put_dword(tp,n)
#define put_pageno(tp,n)	put_dword(tp,n)
#define put_blkno(tp,n)		put_dword(tp,n)
#define put_date(tp,dd)		put_dword(tp,dd)

#define write_word(tp,n)	write_short(tp,n)
#define read_recno(tp)		read_lword(tp)
#define read_pageno(tp)		read_lword(tp)
#define write_recno(tp,n)	write_lword(tp,n)
#define write_pageno(tp,n)	write_lword(tp,n)
#define read_blkno(tp)		read_lword(tp)
#define write_blkno(tp,blk)	write_lword(tp,blk)
#define read_ftime(tp)		read_long(tp)
#define write_ftime(tp,n)	write_long(tp,n)
#define read_fsize(tp)		read_long(tp)
#define write_fsize(tp,n)	write_long(tp,n)

/****** int16 to BYTE macros  ****************/
#define MAKE_WORD(low, high)  ((((uint16_t)((BYTE)(high))) << 8) | (((BYTE)(low))))
#define LO_BYTE(w)            ((BYTE)(uint16_t)(w))
#define HI_BYTE(w)            ((BYTE)(((uint16_t)(w)) >> 8))

/****** int32 to int16 macros ****************/
#define MAKE_DWORD(low, high) ((((uint32_t)((uint16_t)(high))) << 16) | (((uint16_t)(low))))
#define LO_DWORD(l)           ((uint16_t)(uint32_t)(l))
#define HI_DWORD(l)           ((uint16_t)(((uint32_t)(l)) >> 16))

/****** int64 to int32 macros ****************/
#define MAKE_LWORD(low, high) ((((uint64_t)((uint32_t)(high))) << 32) | (((uint32_t)(low))))
#define LO_LWORD(l)           ((uint32_t)(uint64_t)(l))
#define HI_LWORD(l)           ((uint32_t)(((uint64_t)(l)) >> 32))
