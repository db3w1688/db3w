/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/usrfcn.c
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "hb.h"
#include "hbapi.h"

#define  PARMCHK  /** turn on parameter check **/
#define  NFCNS    0x0a

static char *nullstr = "";

static char *fcn_name[NFCNS]={
"GETENV",	"GETLOGIN",	"HBCURL",	"REGEX",	"SYSTEM",	"URL2DOMAIN",
"URLDECODE",	"URLENCODE",	"VERSION"
};

int fcn_bin_srch(char *fcn, int start, int end)
{
	if (start > end) return(-1);
	{
		int i, mid = (start + end)/2;
		i = strcmp(fcn_name[mid], fcn); 
		return(!i ? mid : 
						(i > 0 ? fcn_bin_srch(fcn, start, mid - 1) :
								 fcn_bin_srch(fcn, mid + 1, end)));
	}
}

int usr_getfcn(HBCONN conn)
{
	char *name, vbuf[STRBUFSZ];
	HBVAL *v = (HBVAL *)vbuf;
	int i;
	if (!dbe_pop_stack(conn, CHARCTR, vbuf, STRBUFSZ)) return(-1);
	name = v->buf;
	i = imin(v->valspec.len, FLDNAMELEN);
	name[i] = '\0';
	i = fcn_bin_srch(hb_uppcase(name), 0, NFCNS - 1);
	return(i);
}


static int getenv_(HBCONN conn, int parm_cnt)
{
	char estr[STRBUFSZ], *ep;
#ifdef PARMCHK
	if (parm_cnt != 1)  {
		hb_error(conn,INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_string(conn, estr, STRBUFSZ)) return(-1);
	ep = getenv(hb_uppcase(estr));
	if (!ep) ep = nullstr;
	return(dbe_push_string(conn, ep, strlen(ep)));
}

#define CURLCMD		"/usr/bin/curl"

static int hbcurl(HBCONN conn, int parm_cnt)
{
	FILE *fpcurl = NULL;
	char vbuf[STRBUFSZ], url[STRBUFSZ], data[STRBUFSZ], cmd[MAXCMDLEN], *tp;
#ifdef PARMCHK
	if (parm_cnt != 2)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		goto out;
	}
#endif
	if (!dbe_pop_string(conn, data, STRBUFSZ)) goto out;
	if (!dbe_pop_string(conn, url, STRBUFSZ)) goto out;
	if (strlen(data) + strlen(url) + strlen(CURLCMD) > MAXCMDLEN - 10) {
		hb_error(conn, STR_TOOLONG, NULL, 0, 0);
		goto out;
	}
	sprintf(cmd,"%s %s -s -d \"%s\"", CURLCMD, url, data);
//strcpy(cmd,"/bin/cat /tmp/test.txt");
	fpcurl = popen(cmd, "r");
	if (!fpcurl) goto out;
	if (!fgets(data, MAXSTRLEN, fpcurl)) goto out;
	tp = data + strlen(data) - 1;
	if (*tp == '\n') *tp = '\0';
	if (strcmp(data, "SUCCESS") && strcmp(data, "FAIL")) goto out;
	sprintf(url, "STORE \"%s\" TO hbcurl_result", data);
	dbe_cmd(conn, url);
	while (fgets(data, MAXSTRLEN, fpcurl)) {
		tp = data + strlen(data) - 1;
		if (*tp == '\n') *tp = '\0';
		tp = strchr(data,'=');
		if (tp) {
			sprintf(url, "STORE \"%s\" TO %.*s", tp + 1, tp - data, data);
			dbe_cmd(conn, url);
		}
	}


out:
	if (fpcurl) {
		pclose(fpcurl);
		dbe_push_logic(conn, 1);
	}
	else dbe_push_logic(conn, 0);
	return(0);
}

static int version(HBCONN conn, int parm_cnt)
{
	char ver[STRBUFSZ];
	dbe_get_ver(conn, ver);
#ifdef PARMCHK
	if (parm_cnt)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	return(dbe_push_string(conn, ver, strlen(ver)));
}

extern char *getlogin();

static int getlogin_(HBCONN conn, int parm_cnt)
{
	char *username = getlogin();
#ifdef PARMCHK
	if (parm_cnt)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	return(dbe_push_string(conn, username, strlen(username)));
}

#define MAXDNLEN	128

static char *topdomains[] = {
".ac",
".ad",
".ae",
".af",
".ag",
".ai",
".al",
".am",
".an",
".ao",
".aq",
".ar",
".arpa",
".as",
".at",
".au",
".aw",
".az",
".ba",
".bb",
".bd",
".be",
".bf",
".bg",
".bh",
".bi",
".bj",
".bm",
".bn",
".bo",
".br",
".bs",
".bt",
".bv",
".bw",
".by",
".bz",
".ca",
".cc",
".cf",
".cg",
".ch",
".ci",
".ck",
".cl",
".cm",
".cn",
".co",
".com",
".cr",
".cs",
".cu",
".cv",
".cx",
".cy",
".cz",
".de",
".dj",
".dk",
".dm",
".do",
".dz",
".ec",
".edu",
".ee",
".eg",
".eh",
".er",
".es",
".et",
".fi",
".firm",
".fj",
".fk",
".fm",
".fo",
".fr",
".fx",
".ga",
".gb",
".gd",
".ge",
".gf",
".gh",
".gi",
".gl",
".gm",
".gn",
".gov",
".gp",
".gq",
".gr",
".gs",
".gt",
".gu",
".gw",
".gy",
".hk",
".hm",
".hn",
".hr",
".ht",
".hu",
".id",
".ie",
".il",
".in",
".int",
".io",
".iq",
".ir",
".is",
".it",
".jm",
".jo",
".jp",
".ke",
".kg",
".kh",
".ki",
".km",
".kn",
".kp",
".kr",
".kw",
".ky",
".kz",
".la",
".lb",
".lc",
".li",
".lk",
".lr",
".ls",
".lt",
".lu",
".lv",
".ly",
".ma",
".mc",
".md",
".mg",
".mh",
".mil",
".mk",
".ml",
".mm",
".mn",
".mo",
".mp",
".mq",
".mr",
".ms",
".mt",
".mu",
".mv",
".mw",
".mx",
".my",
".mz",
".na",
".nato",
".nc",
".ne",
".net",
".nf",
".ng",
".ni",
".nl",
".no",
".nom",
".np",
".nr",
".nt",
".nu",
".nz",
".om",
".org",
".pa",
".pe",
".pf",
".pg",
".ph",
".pk",
".pl",
".pm",
".pn",
".pr",
".pt",
".pw",
".py",
".qa",
".re",
".ro",
".ru",
".rw",
".sa",
".sb",
".sc",
".sd",
".se",
".sg",
".sh",
".si",
".sj",
".sk",
".sl",
".sm",
".sn",
".so",
".sr",
".st",
".store",
".su",
".sv",
".sy",
".sz",
".tc",
".td",
".tf",
".tg",
".th",
".tj",
".tk",
".tm",
".tn",
".to",
".tp",
".tr",
".tt",
".tv",
".tw",
".tz",
".ua",
".ug",
".uk",
".um",
".us",
".uy",
".uz",
".va",
".vc",
".ve",
".vg",
".vi",
".vn",
".vu",
".web",
".wf",
".ws",
".ye",
".yt",
".yu",
".za",
".zm",
".zr",
".zw",
NULL
};

static int istopdomain(char *str)
{
	int i;
	for (i=0; topdomains[i]; ++i) if (!strcmp(str, topdomains[i])) return 1;
	return 0;
}

char *url2dn(char *url)
{
	char url2[MAXDNLEN], *tp, *tp2, *tt, *tt2;
	static char dn[MAXDNLEN];
	tp = strchr(url, ':');
	if (tp) {
		if (*(tp + 1) != '/' || *(tp + 2) != '/') return NULL;
		tp += 3;
	}
	else tp = url;
	if (!isalnum(*tp) || strchr(tp, ':')) return NULL;
	if (tp2 = strchr(tp, '/')) sprintf(url2, "%.*s", (int)(tp2 - tp), tp);
	else if (tp2 = strchr(tp, '?')) sprintf(url2, "%.*s", (int)(tp2 - tp), tp);
	else {
		if (strlen(tp) >= MAXDNLEN) return NULL;
		strcpy(url2, tp);
	}
	tp = strrchr(url2, '.');
	if (!tp) return NULL;
	tp2 = tp+1;
	while (isalpha(*tp2)) ++tp2;
	if (tp2 - tp >= MAXDNLEN) return NULL;
	sprintf(dn,"%.*s", (int)(tp2 - tp), tp);
	if (!istopdomain(dn)) return NULL;
	tt = tp - 1;
	while (tt>=url2 && *tt!='.') --tt;
	if (tt <= url2) return NULL;
	if ((int)(tp2 - tt) >= MAXDNLEN) return NULL;
	sprintf(dn, "%.*s", (int)(tp - tt), tt);
	if (!istopdomain(dn)) {
		sprintf(dn, "%.*s", (int)(tp2 - tt - 1), tt + 1);
		return dn;
	}
	// if (strcasecmp(dn,".net") && strcasecmp(dn,".com") && strcasecmp(dn,".co")) return NULL;
	tt2 = tt - 1;
	while (tt2>=url2 && *tt2!='.') --tt2;
	if (tt2 <= url2) return NULL;
	if ((int)(tp2 - tt2) >= MAXDNLEN) return NULL;
	sprintf(dn, "%.*s", (int)(tp2 - tt2 - 1), tt2 + 1);
	return dn;
}

static int url2domain(HBCONN conn, int parm_cnt)
{
	char *dn, url[MAXSTRLEN];
#ifdef PARMCHK
	if (parm_cnt != 1)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_string(conn, url, MAXSTRLEN)) return(-1);
	if (!(dn = url2dn(url))) {
		hb_error(conn, INVALID_VALUE, url, -1, -1);
		return(-1);
	}
	return(dbe_push_string(conn, dn, strlen(dn)));
}

static int urlencode(HBCONN conn, int parm_cnt)
{
	char vbuf[STRBUFSZ], eurl[STRBUFSZ], *tp1, *tp2;
	HBVAL *v = (HBVAL *)vbuf;
	int i;
#ifdef PARMCHK
	if (parm_cnt != 1)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_stack(conn, CHARCTR, vbuf, STRBUFSZ)) return(-1);
	for (i=0,tp1=v->buf,tp2=eurl; i<v->valspec.len; ++i,++tp1) {
		char c = *tp1;
		if ((c <= '9' && c >= '0') 
			|| (c <= 'Z' && c >= 'A')
			|| (c <= 'z' && c >= 'a')
			|| (c == '-' || c == '_' || c == '.' || c == '~')) {
				if (tp2 - eurl >= STRBUFSZ - 1) break;
				*tp2++ = c;
		}
		else {
			if (tp2 - eurl >= STRBUFSZ - 3) break;
			sprintf(tp2, "%%%02X", (BYTE)c);
			tp2 += 3;
		}
	}
	*tp2 = '\0';
	return(dbe_push_string(conn, eurl, (int)(tp2 - eurl)));
}

static int urldecode(HBCONN conn, int parm_cnt)
{
	char vbuf[STRBUFSZ], url[STRBUFSZ],*tp1,*tp2;
	HBVAL *v = (HBVAL *)vbuf;
	int i;
#ifdef PARMCHK
	if (parm_cnt != 1)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_stack(conn, CHARCTR, vbuf, STRBUFSZ)) return(-1);
	for (i=0,tp1=v->buf,tp2=url; i<v->valspec.len; ++i) {
		if (*tp1 == '+') {
			*tp2++ = ' ';
			++tp1;
		}
		else if (*tp1 == '%') {
			char numbuf[16];
			int n;
			sprintf(numbuf, "%.*s", 2, tp1 + 1);
			sscanf(numbuf, "%x", &n);
			*tp2++ = n;
			tp1 += 3; i += 2;
		}
		else *tp2++ = *tp1++;
	}
	*tp2 = '\0';
	return(dbe_push_string(conn, url, (int)(tp2 - url)));
}

#include <regex.h>

static int regex_match(HBCONN conn, int parm_cnt)
{
	char string[STRBUFSZ], pattern[STRBUFSZ];
	int status, errcode;
	regex_t re;
	int cflags = REG_EXTENDED|REG_NOSUB;
#ifdef PARMCHK
	if (parm_cnt != 2)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_string(conn, string, STRBUFSZ)) return(-1);
	if (!dbe_pop_string(conn, pattern, STRBUFSZ)) return(-1);
	if ((errcode = regcomp(&re, pattern, cflags)) != 0) {
		char errbuf[STRBUFSZ];
		regerror(errcode, &re, errbuf, STRBUFSZ);
		hb_error(conn, BAD_EXP, errbuf, -1, -1);
		return(-1);
	}
	status = regexec(&re, string, (size_t) 0, NULL, 0);
	regfree(&re);
	if (dbe_push_int(conn, (DWORD)!!status)) return(-1);
	return(0);
}

#define MAXLINELEN	1024
#define HBPARAM		"HBPARAM:"
#define HBPARAMLEN	8
extern FILE *rptfp;

static int system_(HBCONN conn, int parm_cnt)
{
	FILE *pcmd;
	char vbuf[STRBUFSZ], cmd[STRBUFSZ], line[MAXLINELEN];
	HBVAL *v = (HBVAL *)vbuf;
	int status;
#ifdef PARMCHK
	if (parm_cnt != 1)  {
		hb_error(conn, INVALID_PARMCNT, NULL, 0, 0);
		return(-1);
	}
#endif
	if (!dbe_pop_stack(conn, CHARCTR, vbuf, STRBUFSZ)) return(-1);
	sprintf(cmd, "%.*s 2>&1", v->valspec.len, v->buf);
	pcmd = popen(cmd, "r");
	if (!pcmd) {
		hb_error(conn, BAD_COMMUNICATION, NULL, errno, 0);
		return(-1);
	}
	if (rptfp) fprintf(rptfp, "%s\n", cmd);
	while (fgets(line, MAXLINELEN, pcmd)) {
		if (!strncmp(line, HBPARAM, HBPARAMLEN)) {
			char *tp = line + HBPARAMLEN, *tp2 = strchr(line,'=');
			if (tp2) {
				*tp2 = '\0';
				setenv(hb_uppcase(tp), tp2 + 1, 1);
			}
		}
		else if (rptfp) fputs(line, rptfp);
	}
	status = pclose(pcmd);
	if (dbe_push_int(conn, (DWORD)WEXITSTATUS(status))) return(-1);
	return(0);
}

int (*hb_usrfcn[NFCNS])() = {
getenv_,	getlogin_,	hbcurl,		regex_match,	system_,	url2domain,
urldecode,	urlencode,	version
};

