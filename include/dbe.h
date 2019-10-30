/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/dbe.h
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

typedef LWORD     pageno_t; //64 bit index pageno

#define PRIMARY   0

#define MINBUF    3      /* mininum number of dbf buffers required */
#define MAXCACHE  24
#define MINCACHE  2
 
#define MINSTACK  16384   /* 16K minimum system stack space */
 
#define MAXSDIG   10     /* MAX significant digits */
#define DB3NUMSZ  8      /* double data type size */

#define MAXKEXPLEN  1024    /* MAXIMUM LENGTH FOR INDEX KEY EXPRESSION */
#define HBPAGESIZE  4096 /* Index page size, should be the same as OS PAGESIZE */

#define MAXBUFSZ  65536
 
#define CLOSED    0
#define IN_USE    1
#define SWAPPED   2

#define FNBUFSZ   2048

/*  FILE TYPE, LOW BYTE USED BY 'USE' COMMAND */

#define D_B_F     0x100
#define N_D_X     0x200  //no longer supported
#define F_R_M     0x300
#define L_B_L     0x400
#define M_E_M     0x500
#define T_X_T     0x600
#define F_M_T     0x800
#define P_R_G     0x900
#define P_R_O     0xa00
#define T_B_L     0xb00
#define I_D_X     0xc00
#define A_N_Y     0xf00

#define get_file_type(mode)	(0xf00 & mode)

#define LRETRY    100	/** retry locking attempts **/

/************** rlock mode for save_rec() ***********/
#define R_APPEND      1
#define R_UPDATE      2
#define R_PACK        3
#define R_DELRCAL     4

/***************************************************************************
 
                * MISCELLANEOUS DEFINE *
 
***************************************************************************/

#define EXP_RELA	0
#define EXP_FILTER	1
#define EXP_LOCATE	2

#define MAXPROCLIB	16		//only 2 are used currently
#define LIB_SYS		0
#define LIB_USER	1
#define LIB_ALL		-1
#define ON_EXIT_PROC	"_onexit"	//prg proc name to be called at exit

/*** s_open() mode ***/
#define OLD_FILE      0x00
#define NEW_FILE      0x01
#define OVERWRITE     0x10
#define EXCL          0x20
#define RDONLY        0x40
#define DENYWRITE     0x80
 
/************** for ndxpage **************/
#define DIRTY         0x8000
#define NEWPAGE       0x4000

/******** pcode block size limit for filter, locate and index ****/
#define FLTRPBUFSZ         8192
#define LOCPBUFSZ          8192
#define NDXPBUFSZ          2048

/************** for various expressions, e.g., locate, filter **********/
#define EXPBUFSZ      16384

/************** for composite index key ***/
#define CMBK          0x100   /* combo key type for index header */
#define MAXCMBK       16

#define MAXARGS       100

/************** text mode append/copy *****/
#define TXT_STRU_EXP                  0x7f  //special mode for structure export

#define txt_get_dlmtr(mode)           (0x7f & mode)
#define txt_set_dlmtr(mode, dlmtr)    (mode |= dlmtr)
#define txt_is_set(mode)              (0x80 & mode)
#define txt_set_mode(mode)            (mode |= 0x80)
#define txt_is_struct_exp(mode)       (txt_get_dlmtr(mode)==TXT_STRU_EXP)

/*************** data structures **********/
typedef struct {
	BYTE nparts;
	BYTE flags; //not used
	WORD keylen;
	struct cmbk {
		BYTE typ;
		BYTE flags;
		WORD len;
	} keys[MAXCMBK];
} NDXCMBK;

typedef struct {
	pageno_t rootno;
	pageno_t pagecnt;
	int keylen;
	int norecpp;
	int keytyp;
	int entry_size; //entry size in page:keylen + sizeof(recno_t) + sizeof(pageno_t) + alignment
	char ndxexp[MAXKEXPLEN];
	char pbuf[NDXPBUFSZ];
	NDXCMBK cmbkey;
} NDX_HDR;

#define NDXEXP_OFFSET	0x20 //new .idx key expression offset

typedef struct {
	pageno_t pno; //in little endian order
	recno_t recno; //in little endian order
	BYTE key[MAXKEYSZ];
} NDX_ENTRY;

typedef struct {
	WORD entry_cnt;
	WORD pad; //4-byte alignment
	NDX_ENTRY entries[];
} __attribute__((packed)) NDX_PAGE;

typedef struct {
	int fd;
	recno_t recno;
	DWORD flags;
	BYTE *iobuf; //sizeof(crc32) + sizeof(recno_t) + byprec
} RBK;

#define rbk_iobufsize(byprec)	(sizeof(DWORD) + sizeof(recno_t) + byprec)
#define RBK_MAGIC		123456789
#define RBK_RNO_OFF		sizeof(DWORD)
#define RBK_BUF_OFF		(RBK_RNO_OFF + sizeof(recno_t))

typedef struct dbf {
	int ctx_id;
	struct dbf *nextdb;
	TABLE *dbf_h; //header buffer
	char *dbf_b;  //record buffer
} DBF;

typedef struct ndx {
	int ctx_id;
	int seq;
	int age;
	FCTL *fctl;
	struct ndx *nextndx;
	NDX_HDR ndx_h;
	int w_hdr;
	int fsync;
	pageno_t page_no; //the following 3 fields are for btree traversal
	int entry_no;
	BYTE *pagebuf;
} NDX;

struct flags {
	unsigned e_o_f:		1;
	unsigned t_o_f:		1;
	unsigned fextd:		1;
	unsigned dbmdfy:	1;
	unsigned rec_found: 	1;
	unsigned exclusive:	1;
	unsigned readonly:	1;
	unsigned denywrite:     1;
	unsigned setndx:	1;
	unsigned fsync:         1;
};

struct _lock {
	int type; //HB_FLCK,HB_RDLCK,HBWRLCK,HB_UNLCK
	int mode; //record lock mode R_APPEND,R_UPDATE,etc.
	recno_t recno;
};

struct _scope {
	int dir;
	recno_t total;
	recno_t count;
	recno_t nextrec;
};

typedef struct { //Context control block
	FCTL dbfctl;
	FCTL ndxfctl[MAXNDX];
	int dbtfd;//for memo file
	int exfd; //for export and other file I/O
	char alias[VNAMELEN + 1];
	DBF *dbp;
	NDX *ndxp;
	struct _lock lock;
	struct _scope scope;
	recno_t crntrec;
	recno_t record_cnt;
	int order;	  /** index order **/
	int rela;
	char *filter, *relation, *locate;
	CODE_BLOCK *loc_pbuf, *fltr_pbuf;
	struct flags db_flags;
	RBK rbkctl;  //backup record control
} CTX;

#define clr_stack(n) while (n--) pop_stack(0xff)
#define imax(a,b)    ((a)>(b) ? (a) : (b))
#define imin(a,b)    ((a)>(b)? (b) : (a))
#define is_addop(op) ((0xf0 & (op)) == ADD_OP)
#define is_mulop(op) ((0xf0 & (op)) == MUL_OP)
#define is_relop(op) ((0xf0 & (op)) == REL_OP)
#define isleap(year) (!((year)%400) || (!((year)%4) && (year)%100))
#define stack_empty() ((char *)topstack == exp_stack)

#define is_db_err(code)   (0x8000 & code)
#define is_usr_err(code)  ((0x7fff & code) >= USR_ERROR)
#define errno2code(errno) (errno | 0x8000)
#define code2errno(code)  (code & 0x7fff)

#include <setjmp.h>
extern jmp_buf env;
#define syn_err(code) longjmp(env,code)
#define sys_err(code) longjmp(env,code)

#define get_ctx_id() _get_ctx_id(curr_ctx)
#define blkswap(p,q,n) swapmem(p,q,n)
#define index_active() (curr_ctx->order != -1)
#define _index_active(ctx) (ctx != -1 && table_ctx[ctx].order != -1)
#define chkdb() _chkdb(get_ctx_id())
#define clrflag(flag) curr_ctx->db_flags.flag = FALSE
#define filterisset() (!!curr_ctx->filter)
#define flagset(flag) curr_ctx->db_flags.flag
#define get_aux_buf() table_ctx[AUX].dbp->dbf_b
#define get_db_alias() (curr_ctx->alias)
#define get_db_hdr() (curr_dbf->dbf_h)
#define get_db_recno() (curr_ctx->crntrec)
#define get_db_rccnt() (curr_dbf->dbf_h->reccnt)
#define get_dbt_fd() (curr_ctx->dbtfd)
#define get_fcount() ((int)((curr_dbf->dbf_h->hdlen-HD3SZ)/sizeof(FIELD)))
#define get_fdlist(hdr) (&hdr->fdlist[0])
#define get_field(n) (&curr_dbf->dbf_h->fdlist[n])
#define get_hdr_len(hdr) (sizeof(TABLE) + (hdr->fldcnt * sizeof(FIELD)))
#define get_rec_sz() (curr_dbf->dbf_h->byprec)
#define get_rec_buf() (curr_dbf->dbf_b)
#define get_rbk_buf() (curr_ctx->rbkctl.iobuf + RBK_BUF_OFF)
#define ne_get_key(p,n) ((get_entry(p,n))->key)
#define ne_get_pageno(p,n) (read_pageno(&(get_entry(p,n))->pno))
#define ne_get_recno(p,n) (read_recno(&(get_entry(p,n))->recno))
#define ne_put_pageno(p,n,pageno) (write_pageno(&(get_entry(p,n))->pno, pageno))
#define ne_put_recno(p,n,rec_no) (write_recno(&(get_entry(p,n))->recno, rec_no))
#define isdelete() (*get_rec_buf() == DEL_MKR)
#define iseof() (curr_ctx->db_flags.e_o_f)
#define isfound() (curr_ctx->db_flags.rec_found)
#define is_leaf(p) !ne_get_pageno(p, 0)
#define isntype() (curr_ndx->ndx_h.keytyp)
#define istof() (curr_ctx->db_flags.t_o_f)
#define setflag(flag) curr_ctx->db_flags.flag = TRUE
#define is_rec_saved() (curr_ctx->rbkctl.recno == curr_ctx->crntrec)
#define clr_rec_saved() (curr_ctx->rbkctl.recno = 0)
#define set_onoff(what, onoff) (onoff_flags[what] = !!(onoff))
#define is_scope_end() (curr_ctx->scope.count >= curr_ctx->scope.total)
#define is_scope_set() (curr_ctx->scope.total > 0)
#define unset_scope() (curr_ctx->scope.total = 0)
#define relation_isset() (curr_ctx->rela != -1 && curr_ctx->relation)
#define unset_relation() { curr_ctx->rela = -1; del_exp(EXP_RELA); }

extern int db3w_admin, hb_uid, hb_euid;

extern char hb_user[];

extern HBKW keywords[];

extern PCODE *pcode, *code_buf;
extern BYTE *pdata, *data_buf;
extern int nextcode, nextdata, pc;

extern char *cmd_line, *curr_cmd;

extern char *exp_stack, *stack_limit;
extern HBVAL *topstack;

extern int curr_tok,currtok_s, currtok_len;
extern TOKEN last_tok;

extern int exe, hb_errcode, restricted, escape;

extern int nwidth, ndeci, epoch;

extern int onoff_flags[N_ONOFF_FLAGS];

extern HBREGS usr_regs;

extern int usrcmd;       /** user defined command installed or not **/
extern int ext_ndx;		/** user supplied indexing routine **/

extern int *usr_conn;

extern DBF *dbf_buf, *curr_dbf;

extern NDX *ndx_buf, *curr_ndx;

extern FCTL memfctl, frmfctl, profctl;
 
extern CTX table_ctx[], *curr_ctx;

extern char rootpath[], defa_path[];

#ifdef EMBEDDED
struct mem_pool {
	WORD next_avail;
	char *buf;
};
extern struct mem_pool fltr_buf, rela_buf, loc_buf, fn_buf;
#endif

extern int maxbuf, maxndx;

void _usr(int fcn);
void addr(int vname);
void apdone(void);
void apinit(int mode);
void apnext(void);
void aprec(void);
void arraychk(int aname);
void arraydef(int aname);
void chk_nulltok(void);
void cleandb(void);
void clear_proc_stack(void);
void clretof(int dir);
void cnt_message(char *msg, int mode);
int compile_proc(char *infn, int dbg);
void cpinit(int mode);
void cpnext(int dir);
void cprec(void);
void cpdone(void);
date_t date_to_number(int day, int month, int year);
void ctx_select(int ctx);
void dbpack(void);
void dispmemo(void);
void dispstru(void);
void disprec(int argc);
void do_proc(int pcount);
void do_onexit_proc(void);
int evaluate(char *exp_buf, int catch_error);
void execute(CODE_BLOCK *b);
int execute_proc(char *proc_name, int type, int pcount);
void expression(void);
void num2str(number_t num, int len, int deci, char *buf);
number_t str2num(char *numbuf);
int function_id(void);
int get_command(int allow_redir);
char *get_proc_context_str();
int getaccess(char *name, int type);
int alias2ctx(HBVAL *v);
char *get_dirent(int firsttime, int typ);
int go_rel(void);
int hash_(char *cpool, char *str, int len, char *hashstr, int hashlen);
void hb_cmd(int cmd, BYTE *pbuf, int size_max);
int isset(int parm);
int istrue(void);
int kw_bin_srch(int start, int end);
int kw_search(void);
void load_proc(int lib_id, int dbg);
int loaddate(date_t date);
int loadexpr(int isndx);
int loadfilename(int simple);
int loadnumber(void);
int loadint(int n);
int loadskel(int typ);
void n_to_mdy(date_t d, int *mm, int *dd, int *yy);
int namecmp(char *s, char *p, int len, int mode);
int ndx_get_key(int clr_stack, NDX_ENTRY *target);
void ndx_put_key(void);
void ndx_select(int seq);
int num_rel(number_t n1, number_t n2, int op);
void number_to_date(int dbdate, date_t d, char *dd);
number_t pop_date(void);
int pop_int(int low, int high);
int pop_logic(void);
int64_t pop_long(void);
number_t pop_number(void);
double pop_real(void);
HBVAL *pop_stack(int typ);
int64_t power10(int exp);
void private(int mode);
void process_array(void);
int process_cmd(int cmd, BYTE *pbuf, int size_max);
date_t process_date(void);
void prnt_val(HBVAL *v, int fchar);
void prnt_str(char *fmt, ...);
void public(int name);
void push(HBVAL *v);
void push_date(date_t d);
void push_int(int64_t n);
void push_logic(int tf);
void push_number(number_t num);
void push_opnd(int opnd);
void push_real(double r);
void push_string(char *s, int len);
void put_cond(char *exp, CODE_BLOCK *pbuf, int size_max);
void quit(void);
int r_lock(recno_t recno, int type, int do_retry);
void release(int mode);
void release_proc(int lib_id);
void restore_pcode_ctl(PCODE_CTL *ctL);
void rest_token(TOKEN *tok);
void retriev(int name);
void save_token(TOKEN *tok);
void setetof(int dir);
HBVAL *stack_chk_set(int size);
HBVAL *stack_peek(int level);
void sys_reset(void);
char *timestr_(time_t *tp);
void typechk(int typ);
void updinit(int mode);
void updrec(void);
void updnext(void);
void upddone(void);
int val_size(HBVAL *v);
int verify_date(int year, int month, int day);
void vstore(int name);
int read_db_hdr(TABLE *hdr, char *xlat_buf, int buf_size, int fixed);
void _apblank(int mode);
recno_t btsrch(pageno_t pno, int mode, NDX_ENTRY *target);
int btsrdel(pageno_t pno, NDX_ENTRY *target);
int btsrin(pageno_t pno, NDX_ENTRY *target);
pageno_t btxrch(int dir, pageno_t pn, NDX_ENTRY *target);
void _index(int quick);
char *addfn(char *filename);
char *chgcase(char *str, int up);
char *get_root_path(char *buf, int len);
void closeindex(void);
void db_error(int code, char *msg1);
void db_error0(int code);
void db_error1(int code, HBVAL *v);
void db_error2(int code, int n);
size_t dbread(char *buf, off_t offset, size_t nbytes);
void dbwrite(char *buf, off_t offset, size_t nbytes);
void find(int mode);
char *form_fn(char *fn, int mode);
char *form_fn1(int mode, char *fn);
char *form_memo_fn(char *memofn, int max);
char *form_tmp_fn(char *tmpfn, int max);
char *form_rbk_fn(char *tid, char *basefn);
BYTE *get_db_rec(void);
char *get_ndx_exp(int n);
NDX_ENTRY *get_entry(NDX_PAGE *page, int item);
NDX_PAGE *getpage(pageno_t n, BYTE *buf, int exclusive);
void insert_entry(NDX_PAGE *p, pageno_t pno, int r, NDX_ENTRY *target);
int is_rec_locked(recno_t recno, int type);
NDX_ENTRY *lfind(int dir, NDX_ENTRY *ne);
int read_page(pageno_t pno, BYTE *buf);
void reindex(int quick);
void set_to(int what);
recno_t skip(int64_t n);
void save_rec(int mode);
int store_rec(void);
void update_hdr(int do_lock);
void use(int m_ode);
char *val2str(HBVAL *v, char *outbuf, int maxlen);
int write_page(pageno_t pno, BYTE *buf);
char *get_dbpath(void);
blkno_t get_next_memo(int fd);
blkno_t memo_copy(int fdbt, int tdbt, blkno_t fblkno, blkno_t tblkno);
int put_next_memo(int memofd, blkno_t blkno);
int get_session_id(void);
char *peer_ip_address(int pid);
char *get_session_user(int pid);
int rbk_open(char *tid, RBK *rbk, char *dbf_fn, int mode);
void rbk_write(char *tid, RBK *rbk, int byprec);
int rbk_read_last(char *tid, RBK *rbk, int byprec);
int rbk_read_prev(char *tid, RBK *rbk, int byprec);
void transaction_start(char *tid);
void transaction_cancel(char *tid);
void transaction_commit(char *tid);
void transaction_rollback(char *tid);
void transaction_add_table(char *tid, char *path);
char *transaction_get_id(void);
void scope_init(recno_t count, int dir);
void datefmt(char *spec, char *fmt, char *seq, int *year);
recno_t nextndx(int dir, int push_key);
void push_ndx_entry(NDX_ENTRY *ne);
int file_open(int mode, char *ext);
