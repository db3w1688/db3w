/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hbapi.h
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

#include <stdint.h>
/** abstract security context handle must be wide enough to cast into complex connection types such as SSL_CTX handle **/
typedef void* HBSSL_CTX;

/** abstract connection handle, must be wide enough to cast into complex connection types such as SSL handle **/
typedef uint64_t		HBCONN;
#define HBCONN_INVAL	(HBCONN)-1L

/****** Session Control Calls ******/
#define  DBE_LOGIN          0x7000
#define  DBE_LOGOUT         0x7001
#define  DBE_RESET          0x00
#define  DBE_WHOAREYOU      0x01
#define  DBE_CLR_ERR        0x02
#define  DBE_SLEEP          0x03
#define  DBE_GET_USER       0x04
#define  DBE_GET_STAMP      0x05
#define  DBE_INST_USR       0x06

/****** Regular Calls **********/
#define  DBE_CMD            0x10
#define  DBE_GET_HDR        0x11
#define  DBE_GET_REC        0x12
#define  DBE_GET_HDRLEN     0x13
#define  DBE_GET_RECSZ      0x14
#define  DBE_GET_RECNO      0x15
#define  DBE_GET_RECCNT     0x16
#define  DBE_GO             0x17
#define  DBE_GO_TOP         0x18
#define  DBE_GO_BOT         0x19
#define  DBE_IS_EOF         0x1a
#define  DBE_IS_BOF         0x1b
#define  DBE_IS_TRUE        0x1c
#define  DBE_IS_DELETE      0x1d
#define  DBE_IS_FOUND       0x1e
#define  DBE_IS_SET         0x1f
#define  DBE_GET_STAT       0x20
#define  DBE_GET_ID         0x21
#define  DBE_SET            0x22
#define  DBE_GET_PATH       0x23
#define  DBE_GET_FILTER     0x24
#define  DBE_GET_NDX        0x25
#define  DBE_GET_WDECI      0x26
#define  DBE_SELECT         0x27
#define  DBE_SKIP           0x28
#define  DBE_APBLANK        0x29
#define  DBE_SEEK           0x2a
#define  DBE_DELRCAL        0x2b
#define  DBE_USE            0x2c
#define  DBE_LOCATE         0x2d
#define  DBE_GET_FLDCNT     0x2f
#define  DBE_GET_FLDNO      0x31
#define  DBE_EVALUATE       0x32
#define  DBE_INDEX          0x33
#define  DBE_RW_NDX_HDR     0x34
#define  DBE_GET_NDX_STAT   0x35
#define  DBE_GET_NDX_KEY    0x36
#define  DBE_GET_DBT_FD     0x37
#define  DBE_DATE_TO_CHAR   0x38
#define  DBE_NUM_TO_CHAR    0x39
#define  DBE_CHAR_TO_NUM    0x3a
#define  DBE_IS_ONLYCOPY    0x3b
#define  DBE_REC_MARK       0x3c
#define  DBE_CHECK_OPEN     0x3d
#define  DBE_GET_LOCATE_EXP 0x3e
#define  DBE_GET_NDX_EXPR   0x3f
#define  DBE_PUT_REC        0x40
#define  DBE_PUT_HDR        0x41
#define  DBE_READ_HDR       0x42
#define  DBE_STORE_REC      0x43
#define  DBE_UNLOCK         0x44
#define  DBE_RLOCK          0x45
#define  DBE_FLOCK          0x46
#define  DBE_IS_READONLY    0x47
#define  DBE_GET_REC_BATCH  0x48
#define  DBE_DISPMEMO       0x49
#define  DBE_DISPSTRU       0x4a
#define  DBE_DISPREC        0x4b
#define  DBE_GET_PDATA      0x4c
#define  DBE_GET_EPOCH      0x4d
#define  DBE_SET_ESCAPE     0x4e
#define  DBE_FORM_REWIND    0x4f
#define  DBE_FORM_SEEK      0x50
#define  DBE_FORM_TELL      0x51
#define  DBE_FORM_RDLN      0x52
#define  DBE_VSTORE         0x53
#define  DBE_GET_FTIME      0x54
#define  DBE_GET_META_LEN   0x55
#define  DBE_GET_META_HDR   0x56
#define  DBE_GET_TRANS_ID   0x57
#define  DBE_SET_WDECI      0x58
#define  DBE_DEL_FILE       0x59
#define  DBE_REN_FILE       0x5a
#define  DBE_SET_EPOCH      0x5b

/****** PCode Execute Call *****/
#define  DBE_PCODE_EXE      0x80

/****** Compiler Control Calls *****/
#define  DBE_CMD_INIT       0x100
#define  DBE_CMD_END        0x101
#define  DBE_GET_COMMAND    0x102
#define  DBE_GET_TOKEN      0x103
#define  DBE_GET_TOK        0x104
#define  DBE_EXPRESSION     0x105
#define  DBE_STACK_OP       0x106
#define  DBE_KWSEARCH       0x107
#define  DBE_CMD2           0x108
#define  DBE_CODEGEN        0x109
#define  DBE_EMIT           0x10a
#define  DBE_LEXLOAD        0x10b
#define  DBE_SYNC_TOKEN     0x10c
#define  DBE_MOD_CODE       0x10d
#define  DBE_GET_NEXTCODE   0x10e
#define  DBE_UNGET_TOKEN    0x10f
#define  DBE_PROC_LEVEL     0x110

/**** for DBE_LEXLOAD **************/
#define  L_STRING       0x01
#define  L_NUMBER       0x02
#define  L_EXPR         0x03
#define  L_FULFN        0x04
#define  L_FN           0x05

/****** File Operation Calls *****/
#define	DBE_FILE_DIR    0x800
#define DBE_FILE_OPEN   0x801
#define	DBE_FILE_SEEK   0x802
#define	DBE_FILE_READ   0x803
#define	DBE_FILE_WRITE  0x804
#define DBE_FILE_CLOSE  0x805

/****** Serial Number Generation ***/
#define  DBE_SET_SERIAL 0x810
#define  DBE_GET_SERIAL 0x811

#define dbe_loadfilename(conn,simple,tok)	dbe_lexload(conn,simple? L_FN:L_FULFN,0,tok)
#define dbe_loadstring(conn,tok)		dbe_lexload(conn,L_STRING,0,tok)
#define dbe_loadnumber(conn,sz,tok)		dbe_lexload(conn,L_NUMBER,sz,tok)
#define dbe_loadexpr(conn,isndx,tok)		dbe_lexload(conn,L_EXPR,isndx,tok)

#define dbe_del_rec(conn)	dbe_mark_rec(conn,'*')
#define dbe_rest_rec(con_fh)	dbe_mark_rec(conn,' ')

/******* HB*Engine Stack Op flags *******/
#define  STACK_OUT_MASK 0xf000
#define  STACK_LVL_MASK 0x4000
#define  STACK_RM_MASK  0x8000
#define  N_MASK         0x00ff
#define  PUSH_TYP_MASK  0x0f00
#define  PUSH_VAL       0x0100
#define  PUSH_STRING    0x0200
#define  PUSH_INT       0x0300
#define  PUSH_REAL      0x0400
#define  PUSH_LOGIC     0x0500
#define  PUSH_DATE      0x0600

/****** for GET_NDX_STAT *****/
#define  NDX_FD         0x001
#define  NDX_FN         0x002
#define  NDX_TYPE       0x003
#define  NDX_EXP        0x004

/****** for identify usr_err source ********/
#define FROM_APPEND     0x100
#define FROM_COPY       0x200

/****** file timestamp ******/
#define FTIME_A		0 //access
#define FTIME_M		1 //modify
#define FTIME_C		2 //change

/****** HBREGS struct *********************/
#ifndef HB		/* hb.h not included */
typedef struct _HBREGS {
	int ax;
	BYTE *bx;
	int cx;
	int dx;
	int ex;
	int fx;
} HBREGS;
#endif

/****** Call Monitor support **************/
/****** message type **********************/
#define HBMON_CONN		0x0001
#define HBMON_CALL		0x0002
#define HBMON_RET		0x0003
#define HBMON_DBG		0x0004

/****** connect messages ******************/
#define CONTACT			0x0000
#define CONNECT			0x0001
#define LOGIN_TO_SERVER		0x0002
#define LOGOUT_FROM_SERVER	0x0003

/****** client IPC commands ***************/
#define IPC_GET_SID		0x0001
#define IPC_RESUME		0x0002
#define IPC_GET_USER		0x0003

#define DBE_LOGIN_LEN		1024

#define dbe_set_login(login,dbsrvr,dbpath,user,addr,flags) snprintf(login,DBE_LOGIN_LEN,"%s%c%s%c%c%s%c%s",dbsrvr,'\0',dbpath,'\0',flags,user,addr[0]? '@':'\0',addr)

#define imin(a,b)    ((a)>(b)? (b) : (a))

/**************** general dbesrv functions  **********/
int dbe_apblank(HBCONN conn, int inc);
int dbe_authenticate(HBCONN conn, char *user, char *passwd);
int dbe_char_to_num(HBCONN conn, HBVAL *v);
int dbe_check_open(HBCONN conn, char *fn);
int dbe_close_file(HBCONN conn, int fd);
int dbe_clr_stack(HBCONN conn, int n);
int dbe_cmd(HBCONN conn, char *cmd_line);
int dbe_cmd_init(HBCONN conn, char *cmd_line);
int dbe_cnt_message(HBCONN conn, char *msg, int mode);
int dbe_codegen(HBCONN conn, int opcode, int opnd);
int hb_create_tbl(HBCONN conn, TABLE *hdr );
char *dbe_date_to_char(HBCONN conn, date_t d, int is_dbdate);
int dbe_del_user(HBCONN conn, char *user);
int dbe_emit(HBCONN conn, char *b, int size);
int dbe_eval(HBCONN conn, char *exp_buf, int catch_error);
int dbe_execute(HBCONN conn, char *pbuf);
int dbe_expression(HBCONN conn, struct token *tok);
int dbe_flock(HBCONN conn, int exclusive);
int dbe_get_command(HBCONN conn, struct token *tok);
int dbe_get_dbt_fd(HBCONN conn);
char *dbe_get_dirent(HBCONN conn, int firsttime, int type, char *buf);
int dbe_get_fcount(HBCONN conn);
int dbe_get_fieldno(HBCONN conn, char *name);
char *dbe_get_filter(HBCONN conn, char *buf, int bufsz);
char *dbe_get_hdr(HBCONN conn, char *buf, int bufsz);
int dbe_get_hdr_len(HBCONN conn);
char *dbe_get_id(HBCONN conn, int what, int n, char *buf, int bufsz);
char *dbe_get_locate_exp(HBCONN conn, char *buf, int bufsz);
blkno_t dbe_get_memo(HBCONN conn, char *fldname, char *memobuf, size_t bufsz);
char *dbe_get_ndx_exp(HBCONN conn, char *ndxname, char *buf, int bufsz);
int dbe_get_ndx_fd(HBCONN conn);
char *dbe_get_ndx_filename(HBCONN conn, char *buf);
char *dbe_get_ndx_key(HBCONN conn, char *buf);
char *dbe_get_ndx_name(HBCONN conn, int n, char *buf, int bufsz);
int dbe_get_ndx_type(HBCONN conn);
blkno_t dbe_get_next_memo(HBCONN conn, int memofd);
int dbe_get_nextcode(HBCONN conn);
char *dbe_get_path(HBCONN conn, int mode, char *buf, int bufsz);
recno_t dbe_get_reccnt(HBCONN conn);
char *dbe_get_rec(HBCONN conn, char *buf, int bufsize, int skip);
int dbe_get_rec_batch(HBCONN conn, char *buf, int n, int dir, int recsiz, char *rec_arr[], recno_t recno_arr[]);
recno_t dbe_get_recno(HBCONN conn);
int dbe_get_rec_sz(HBCONN conn);
int dbe_get_stat(HBCONN conn, int type);
int dbe_get_token(HBCONN conn, char *cmd_line, struct token *tok);
int dbe_get_tok(HBCONN conn, char *cmd_line, struct token *tok);
char *dbe_get_user(HBCONN conn, char *user);
int dbe_get_wdeci(HBCONN conn, int *width, int *deci);
int dbe_go(HBCONN conn, recno_t n);
int dbe_go_bottom(HBCONN conn);
int dbe_go_top(HBCONN conn);
int dbe_gorec(HBCONN conn, recno_t n);
int dbe_hb_cmd(HBCONN conn, int cmd, char *pbuf, struct token *tok);
int dbe_index(HBCONN conn, char *ndxfn, char *ndxexp);
int dbe_inst_usr(HBCONN conn);
int dbe_is_only_copy(HBCONN conn);
int dbe_isdelete(HBCONN conn);
int dbe_iseof(HBCONN conn);
int dbe_isreadonly(HBCONN conn);
int dbe_isset(HBCONN conn, int what);
int dbe_isbof(HBCONN conn);
int dbe_istrue(HBCONN conn);
int dbe_kw_search(HBCONN conn, int *kw_val);
int dbe_lextload(HBCONN conn, int typ, int x, struct token *tok);
int dbe_locate(HBCONN conn, char *loc_cond, int mode);
int dbe_mark_rec(HBCONN conn, int mark);
int dbe_memvar_release(HBCONN conn, int mode);
int dbe_mod_code(HBCONN conn, int which, int what);
char *dbe_num_to_char(HBCONN conn, HBVAL *v, int len, int deci);
int dbe_open_file(HBCONN conn, int mode, char *ext);
HBVAL *dbe_pcode_data(HBCONN conn, int idx, char *buf);
int dbe_pop_logic(HBCONN conn);
double dbe_pop_real(HBCONN conn);
HBVAL *dbe_pop_stack(HBCONN conn, int type, char *buf, int bufsz);
int dbe_pop_int(HBCONN conn, int low, int high, int *err);
char *dbe_pop_string(HBCONN conn, char *buf, int bufsz);
int dbe_popstack(HBCONN conn, char *buf, int bufsz);
int dbe_pop_memo(HBCONN conn, int *fdp, blkno_t *start_blkno);
int dbe_pro_level(HBCONN conn, int lvl);
int dbe_push_logic(HBCONN conn, int tf);
int dbe_push_date(HBCONN conn, date_t d);
int dbe_push_int(HBCONN conn, int i);
int dbe_push_real(HBCONN conn, double r);
int dbe_push_string(HBCONN conn, char *buf, int len);
int dbe_put_hdr(HBCONN conn, char *buf, int len);
int dbe_read_hdr(HBCONN conn);
blkno_t dbe_put_memo(HBCONN conn, char *memobuf, int size, int memofd, blkno_t blkno);
int dbe_put_next_memo(HBCONN conn, int memofd, blkno_t blkno);
int dbe_put_rec(HBCONN conn, char *buf, int mode);
int dbe_read_file(HBCONN conn, int fd, char *buf, int nbytes, int isbin);
int dbe_rec_found(HBCONN conn);
int dbe_rec_mark(HBCONN conn);
int dbe_reindex(HBCONN conn);
int dbe_rlock(HBCONN conn, int exclusive);
int dbe_rw_ndx_hdr(HBCONN conn, int mode, char *buf);
int dbe_seek(HBCONN conn, char *str, int mode);
off_t dbe_seek_file(HBCONN conn, int fd, off_t pos);
int dbe_select(HBCONN conn, int ctx);
int dbe_set_datefmt(HBCONN conn, char *fmt);
int dbe_set_talk(HBCONN conn, int onoff);
int dbe_set_filter(HBCONN conn, char *fltr);
int dbe_set_ndx(HBCONN conn, char *p);
int dbe_set_order(HBCONN conn, int i);
int dbe_set_passwd(HBCONN conn, char *user, char *passwd);
int dbe_set_path(HBCONN conn, char *p);
int dbe_set_relation(HBCONN conn, char *rel, int wa);
recno_t dbe_skip(HBCONN conn, int64_t n);
int dbe_stackempty(HBCONN conn);
HBVAL *dbe_peek_stack(HBCONN conn, int lvl, int type, char *buf, int bufsz);
int dbe_store_rec(HBCONN conn);
int dbe_sync_token(HBCONN conn, struct token *tok);
int dbe_sys_reset(HBCONN conn);
int dbe_unget_token(HBCONN conn, struct token *tok);
int dbe_unlock(HBCONN conn);
int dbe_use(HBCONN conn, int mode);
int dbe_write_file(HBCONN conn, int fd, char *buf, int nbytes);
char *hbcrypt(char *usr, char *passwd, char *outstr);
TABLE *hb_get_hdr(HBCONN conn);
DWORD hb_get_stamp(HBCONN conn);
int hb_get_value(HBCONN conn, char *expr, char *buf, int len, int catch_error);
int hb_put_hdr(HBCONN conn, TABLE *hdr);
int dbe_set_serial(HBCONN conn,char *name, uint64_t inival);
uint64_t dbe_get_serial(HBCONN conn, char *name);
int dbe_get_epoch(HBCONN conn);
void display_msg(char *msg, char *msg2);
int dbe_form_rewind(HBCONN conn);
int dbe_form_seek(HBCONN conn, long offset);
off_t dbe_form_tell(HBCONN conn);
char *dbe_form_rdln(HBCONN conn, char *line, int max);
time_t dbe_get_ftime(HBCONN conn, int which);
char *dbe_get_meta_hdr(HBCONN conn, char *buf, int buf_size);
char *dbe_transaction_get_id(HBCONN conn, char *buf);
int dbe_delete_file(HBCONN conn, char *fn);
int dbe_rename_file(HBCONN conn, char *srcfn, char *tgtfn, char *user);

/******************** gate keeper **************************/
int hb_int(HBCONN conn, HBREGS *regs);
int usr(HBCONN conn, HBREGS *regs);

/******************** necessary if sleep during an usr() ***/
void usr_return(HBCONN *conn, HBREGS *usr_regs, int iserr);
void hbusr_return(HBCONN *conn);

/******************* startup functions *********************/
void dbe_end_session(HBCONN *conn);
char *dbe_get_ver(HBCONN conn, char *buf);
HBCONN dbe_init_session(HBSSL_CTX ctx, char *login, int hb_port, int maxbuf, int maxcache, int *sid);
void dbe_sleep(HBCONN *conn, int timeout);

/******************* error handling ************************/
void dbe_clr_err(HBCONN conn, int send_to_srvr);
char *dbe_errmsg(int code, char *msgbuf);
void dbe_error(HBCONN conn, HBREGS *regs);
int dbe_error_handler(HBCONN conn, int preempt, void (*handler)(int errcode));
int dbe_get_error(HBCONN conn);
void hb_error(HBCONN conn, int errcode, char *cmd_line, int p1, int p2);
int hb_yesno(char *what);

/******** message display ****************/
void display_error(HBCONN conn, char *msg, char *ptr, int is_cmd, int p1, int p2);
void display_comm_err(int, char *errmsg, char *msg);
void monitor_msg(int msgtyp, int msgid, void *buf);

char *hb_uppcase(char *s);
char *hb_lwrcase(char *s);

/******** Ctrl-C Ctrl-Break and other signal handler functions ****/
#include <signal.h>

extern int hb_sigs;

#define S_INTERRUPT     0x01
#define S_STOP          0x02
#define S_PIPE          0x04

extern void block_breaks(void);
extern int sys_break(void);
extern void unblock_breaks(void);
extern void ctl_c(void);
extern void sigpipe(int sig);

extern int dbe_errno;
