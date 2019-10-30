/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- dbe/fn.c
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

#include <memory.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "hb.h"
#include "dbe.h"
 
#define EXTLEN	 3		  /* length of file extension excluding '.' */
#define DRVLEN	 1		  /* only one char allowed for drive */

static char fulfn[FULFNLEN];

char *addfn(char *filename)
{
	int len = strlen(filename) + 1;
#ifdef EMBEDDED
	char *p = fn_buf.buf + fn_buf.next_avail;
	if (fn_buf.next_avail + len > FNBUFSZ) db_error(NONAMESPACE, filename);
	memcpy(p, filename, len);
	fn_buf.next_avail += len;
	return(p);
#else
	char *fn = malloc(len);
	if (!fn) sys_err(NOMEMORY);
	memcpy(fn, filename, len);
	return(fn);
#endif
}

void delfn(FCTL *fctl)
{
   char *p = fctl->filename;
   int i,j,len;
 
   if (!p) return;
   if (fctl->fd != -1 || fctl->fp != NULL) db_error(STILL_OPEN, p);
#ifdef EMBEDDED
   len = strlen(p) + 1;
   for (i=0; i<MAXCTX + 1; ++i) {
      CTX *q = &table_ctx[i];
      if (q->dbfctl.filename > p) q->dbfctl.filename -= len;
      for (j=0; j<MAXNDX; ++j)
         if (q->ndxfctl[j].filename > p) q->ndxfctl[j].filename -= len;
   }
   if (frmfctl.filename > p) frmfctl.filename -= len;
   if (profctl.filename > p) profctl.filename -= len;
   if (memfctl.filename > p) memfctl.filename -= len;
   memcpy(p, p + len, (int)(fn_buf.buf + fn_buf.next_avail - p) - len);
   fn_buf.next_avail -= len;
#else
   free(fctl->filename);
#endif
   fctl->filename = NULL;
}

extern char *getcwd();

char *get_root_path(char *buf, int len)
{
	if (strlen(rootpath) + 1 >= len) db_error0(STR_TOOLONG);
	sprintf(buf, "%s/", rootpath);
	return(buf);
}

char *form_fn1(int mode, char *fn)
{
	char *p,*q;

	strcpy(fulfn, fn);
	q = fulfn;
	while (p = strchr(q, '/')) q = ++p;
	if (mode) {
		if (q > fulfn) *q = '\0';
		return(fulfn);
	}
	*(p = strchr(q, '.')) = '\0';		 /* must have ext. */
	return(q);
}

char *form_fn(char *fn, int mode)
{
	char *p, *q;
	char tmp[FULFNLEN];

	q = mode && chkdb()!=CLOSED? strcpy(tmp, form_fn1(1, curr_ctx->dbfctl.filename)) : defa_path;
	if (fn[0] != '/') strcpy(fulfn, q);
	else {
		strcpy(fulfn, get_root_path(tmp, FULFNLEN));
		++fn;
	}
	//fulfn has trailing '/'
	if (strlen(fulfn) + strlen(fn) >= FULFNLEN) db_error(BADFILENAME, fn);
	strcat(fulfn, fn);
	for (p=strchr(fulfn, '/'); p; p=strchr(p+1, '/')) {  /** get rid of relative dir **/
		if (!strncmp(p + 1, "./", 2)) {
			for (q=p+2; *q; ++q) *(q - 2) = *q;
			*(q - 2) = '\0';
			--p;
		}
		else if (!strncmp(p + 1, "../", 3)) {
			int i;
			q = p + 3;
			if (p > fulfn) while (*(--p) != '/');
			for (i=0; *q; ++q,++i) p[i] = *q;
			p[i] = '\0';
			--p;
		}
	}
	return(fulfn);

}

char *form_memo_fn(char *memofn, int max)
{
	char *tp;
	if (strlen(curr_ctx->dbfctl.filename) >= max) return(NULL);
	strcpy(memofn, curr_ctx->dbfctl.filename);
	tp = strrchr(memofn, SWITCHAR);
	if (!tp) tp = memofn;
	tp = strrchr(tp, '.');
	if (!tp) return(NULL);
	strcpy(tp + 1, "dbt");
	return(memofn);
}

char *form_tmp_fn(char *tmpfn, int max)
{
	char *tp;
	if (strlen(curr_ctx->dbfctl.filename) >= max) return(NULL);
	strcpy(tmpfn, curr_ctx->dbfctl.filename);
	tp = strrchr(tmpfn, SWITCHAR);
	if (!tp) tp = tmpfn;
	tp = strrchr(tp, '.');
	if (!tp) return(NULL);
	strcpy(tp + 1, "tmp");
	return(tmpfn);
}

int fileexist(char *fn)
{
	 return(!euidaccess(fn, F_OK));
}

int checkopen(char *fn)
{
	int i, j;
	char *db_fn, *ndx_fn, *pro_fn, *frm_fn;
	CTX *save_ctx;
	int found;

	if (profctl.filename) {
		pro_fn = form_fn(profctl.filename, 1);
		if (!strcmp(fn, pro_fn)) return(TRUE);
	}
	if (frmfctl.filename) {
		frm_fn =  form_fn(frmfctl.filename, 1);
		if (!strcmp(fn, frm_fn)) return(TRUE);
	}
	save_ctx = curr_ctx; found = 0;
	found = FALSE;
	for (i=PRIMARY; i<MAXCTX; ++i) {
		curr_ctx = &table_ctx[i];
		db_fn = curr_ctx->dbfctl.filename;
		if (db_fn) {
			if (!strcmp(fn, db_fn)) { found = TRUE; goto done; }
			if (curr_ctx->dbtfd != -1) {
				char memofn[FULFNLEN];
				if (form_memo_fn(memofn, FULFNLEN) && !strcmp(fn, memofn)) { found = 1; goto done; }
			}
		}
		for (j=0; j<MAXNDX; ++j) {
			ndx_fn = curr_ctx->ndxfctl[j].filename;
			if (ndx_fn) {
				ndx_fn = form_fn(ndx_fn, 1);
				if (!strcmp(fn, ndx_fn)) { found = TRUE; goto done; }
			}
		}
	}
done:
	curr_ctx = save_ctx;
	return(found);
}

void get_fn(int m, FCTL *fctl)
{
	char *s, *t;
	int i = get_file_type(m);
	char tempfn[FULFNLEN], tempfn1[FULFNLEN];
	typechk(CHARCTR);
	sprintf(tempfn, "%.*s", topstack->valspec.len, topstack->buf); //don't pop yet, may need to retry
	t = strrchr(tempfn, SWITCHAR);
	if (!t) t = tempfn; //partial or no path
	else ++t;
	if (!strchr(t, '.')) {
		switch (i) {
			  case D_B_F : s = ".dbf"; break;
			  case N_D_X : s = ".ndx"; break; //no longer used
			  case L_B_L : s = ".lbl"; break;
			  case F_R_M : s = ".frm"; break;
			  case M_E_M : s = ".mem"; break;
			  case T_X_T : s = ".txt"; break;
			  case P_R_G : s = ".prg"; break;
			  case P_R_O : s = ".pro"; break;
			  case T_B_L : s = ".tbl"; break; //db5 table file
			  case I_D_X : s = ".idx"; break; //db5 index file
			  default: s = "";
		}
		strcat(t, s);
	}
	strcpy(tempfn1, form_fn(tempfn, i != D_B_F && i != T_B_L)); //full path
	if (U_NO_CHK_EXIST & m) {
		if (checkopen(tempfn1)) db_error(OPENFILE, tempfn1);
		if (U_CHK_DUP & m) {
			if (fileexist(tempfn1)) db_error(FILEEXIST, tempfn1);
		}
	}
	else if (!fileexist(tempfn1)) db_error(FILENOTEXIST, tempfn1);
	fctl->filename = addfn((i == D_B_F || i == T_B_L) ? tempfn1 : tempfn);
	pop_stack(CHARCTR);
}

int get_default_ndx(void) //not used
{
	char tempfn[FULFNLEN], *s, *t;
	int i;
	strcpy(tempfn, curr_ctx->dbfctl.filename);
	s = tempfn + strlen(tempfn);
	while (*(s-1) != SWITCHAR) --s;	/** must have path **/
	t = strchr(s, '.');
	for (i=0; i<MAXNDX; ++i) {
		sprintf(t + 1, "%d", i + 1);
		if (!fileexist(tempfn)) break;
		curr_ctx->ndxfctl[i].filename = addfn(s);
	}
	return(i);
}
