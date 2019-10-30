/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/w3util.c
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "w3util.h"
#include "db3w.h"

#define LF 10
#define CR 13

void getword(char *word, char *line, char stop)
{
    int x = 0,y;

    for(x=0;((line[x]) && (line[x] != stop));x++)
        word[x] = line[x];

    word[x] = '\0';
    if(line[x]) ++x;
    while (isspace(line[x])) ++x;
    y=0;

    while(line[y++] = line[x++]);
}

char *makeword(char *line, char stop)
{
    int x = 0,y;
    char *word = (char *) malloc(sizeof(char) * (strlen(line) + 1));

    for(x=0;((line[x]) && (line[x] != stop));x++)
        word[x] = line[x];

    word[x] = '\0';
    if(line[x]) ++x;
    y=0;

    while (isspace(line[x])) ++x;
    while(line[y++] = line[x++]);
    return word;
}

#define WSIZE	1024

char *fmakeword(FILE *f, char stop, int *cl)
{
    int wsize;
    char *word;
    int ll;

    wsize = WSIZE;
    ll=0;
    word = (char *) malloc(sizeof(char) * (wsize + 1));

    while(1) {
        word[ll] = (char)fgetc(f);
        if(ll==wsize) {
            word[ll+1] = '\0';
            wsize+=WSIZE;
            word = (char *)realloc(word,sizeof(char)*(wsize+1));
        }
        --(*cl);
        if((word[ll] == stop) || (feof(f)) || (!(*cl))) {
            if(word[ll] != stop) ll++;
            word[ll] = '\0';
            return word;
        }
        ++ll;
    }
}

char x2c(char *what) 
{
    char digit;

    digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
    return(digit);
}

void unescape_url(char *url)
{
    int x,y;

    for(x=0,y=0;url[y];++x,++y) {
        if((url[x] = url[y]) == '%') {
            url[x] = x2c(&url[y+1]);
            y+=2;
        }
    }
    url[x] = '\0';
}

void plustospace(char *str) 
{
    int x;

    for(x=0;str[x];x++) if(str[x] == '+') str[x] = ' ';
}

int rind(char *s, char c)
{
    int x;
    for(x=strlen(s) - 1;x != -1; x--)
        if(s[x] == c) return x;
    return -1;
}

/*
int getline(s, n, f)
char *s;
int n;
FILE *f;
{
    int i=0;

    while(1) {
        s[i] = (char)fgetc(f);

        if(s[i] == CR)
            s[i] = fgetc(f);

        if((s[i] == 0x4) || (s[i] == LF) || (i == (n-1))) {
            s[i] = '\0';
            return (feof(f) ? 1 : 0);
        }
        ++i;
    }
}
*/

void send_fd(FILE *f, FILE *fd)
{
    int num_chars=0;
    char c;

    while (1) {
        c = fgetc(f);
        if(feof(f))
            return;
        fputc(c,fd);
    }
}

int ind(char *s, char c)
{
    int x;

    for(x=0;s[x];x++)
        if(s[x] == c) return x;

    return -1;
}

void escape_shell_cmd(char *cmd)
{
    int x,y,l;

    l=strlen(cmd);
    for(x=0;cmd[x];x++) {
        if(ind("&;`'\"|*?~<>^()[]{}$\\",cmd[x]) != -1){
            for(y=l+1;y>x;y--)
                cmd[y] = cmd[y-1];
            l++; /* length has been increased */
            cmd[x] = '\\';
            x++; /* skip the character */
        }
    }
}

char *skipblank(char *line)
{
	char *tp = line;
	while (*tp && isspace(*tp)) ++tp;
	return(tp);
}

char *space2plus(char *str)
{
	char *tp = str;
	while (*tp) { if (*tp==' ') *tp = '+'; ++tp; }
	return(str);
}

char *plus2space(char *str)
{
	char *tp = str;
	while (*tp) { if (*tp=='+') *tp = ' '; ++tp; }
	return(str);
}

#define MAXNOUT		8
#define STDOUT		0
#define MAILOUT		1
#define DBGOUT		2

#define MAILPROG	"/usr/lib/sendmail -t"

static FILE *htmloutp[MAXNOUT] = { NULL };
static int htmloutv[MAXNOUT] = { 0 };
static int headerprinted[MAXNOUT] = { 0 };

static void _print_header(int out)
{
	if (!headerprinted[out]) {
		if (htmloutp[out] && htmloutv[out]) {
			fprintf(htmloutp[out],"Content-type: text/html\n\n");
			headerprinted[out] = 1;
		}
	}
	/*
	fprintf(htmloutp[DBGOUT],"%d %d, %d %d\n",htmloutv[STDOUT],htmloutv[MAILOUT],headerprinted[STDOUT],headerprinted[MAILOUT]);
	*/
}

void print_header(void)
{
	int i;
	for (i=0; i<MAXNOUT; ++i) if (i != DBGOUT) _print_header(i);
}

int isbreakchar(char c)
{
	return(!c || c==' ' || c==',' || c=='!' || c=='?' || c==';' || c=='<' || c=='>');
}

void html_encode(char *src, char *dest, int srclen, int destlen, int flags)
{
	char *tp1, *tp2, *tp3;
	char tail[MAXLINELEN], token[MAXLINELEN];
	int destlen2 = destlen;
	for (tp1=src,tp2=dest; tp1-src<srclen && destlen>1; tp1++) {
		char c = *tp1;
		if (c == '\n') {
			if (flags & NL2BR) {
				if (destlen <= 4) break;
				strcpy(tp2, "<BR>");
				tp2 += 4; destlen -= 4;
			}
			else {
				if (destlen <= 2) break;
				strcpy(tp2, "\n");
				tp2 += 2; destlen -= 2;
			}
		}
		else if (c == '\t') { *tp2++ = c; --destlen; }
		else if (isspace(c)) {
			*tp2++ = ' ';
			--destlen;
		}
		else if (!isprint(c)) continue;
		else if (isalpha(c) || isdigit(c)) { *tp2++ = c; --destlen; }
		else if (c == '&') {
			if (destlen <= 5) break;
			strcpy(tp2, "&amp;");
			tp2 += 5;
			destlen -= 5;
		}
		else if (c == '<') {
			if (destlen <= 4) break;
			strcpy(tp2, "&lt;");
			tp2 += 4;
			destlen -= 4;
		}
		else if (c == '>') {
			if (destlen <= 4) break;
			strcpy(tp2, "&gt;");
			tp2 += 4;
			destlen -= 4;
		}
		else if (c == '\"') {
			if (destlen <= 6) break;
			strcpy(tp2, "&quot;");
			tp2 += 6;
			destlen -= 6;
		}
		else if (c == '\'') {
			if (destlen <= 6) break;
			strcpy(tp2, "&apos;");
			tp2 += 6;
			destlen -= 6;
		}
		else {
			char ccode[16];
			int len;
			sprintf(ccode, "&#%d;", (int)*tp1);
			len = strlen(ccode);
			if (destlen <= len) break;
			strcpy(tp2, ccode);
			tp2 += len;
			destlen -= len;
		}
	}
	*tp2 = '\0';
	/** now look for special tokens such as e-mail address and url's **/
	destlen = destlen2;
	for (tp1=dest; *tp1; tp1=tp2+1) {
		while (*tp1 && *tp1==' ') ++tp1;
		if (!*tp1) break;
		if (*tp1=='\"') {
			tp2 = ++tp1;
			while (*tp2 && *tp2!='\"') ++tp2;
		}
		else for (tp2=tp1; !isbreakchar(*tp2); ++tp2);
		sprintf(token,"%.*s",tp2-tp1, tp1);
		strcpy(tail, tp2);
		destlen2 = destlen - (tp1 - dest);
		tp3 = strchr(token,'@');
		if (tp3) {
		/** we found a potential e-mail address, if it contains
		a '.' after '@' we will assume it is a valid address **/
			if (tp3 > token && strchr(tp3, '.') && destlen2 > 2*strlen(token) + 22) {
				sprintf(tp1, "<A HREF=\"mailto:%s\">%s</A>", token, token);
				tp2 = tp1 + strlen(tp1);
				strcpy(tp2, tail);
			}
		}
		else {
			tp3 = strchr(token,'/');
			/** see if we have a protocol specifier, if we do
			we will assume it is a hyper link **/
			if (tp3 && *(tp3-1)==':' && *(tp3+1)=='/' 
				&& (!strncmp(token,"http",4) || !strncmp(token,"ftp",3)) && destlen2 > 2*strlen(token) + 15) {
				sprintf(tp1,"<A HREF=\"%s\">%s</A>",token, token);
				tp2 = tp1 + strlen(tp1);
				strcpy(tp2, tail);
			}
		}
		//*tp2 is either a breakchar or \"
	}
}

/** a unified html output routine **/
extern char *root;

void html_out(int flags, char *fmt, ...)
{
	char tmp[MAXLINELEN],outbuf[4*MAXLINELEN];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(tmp,fmt,ap);
	html_encode(tmp, outbuf, strlen(tmp), 4*MAXLINELEN, flags);
	if (!headerprinted[STDOUT]) {	/* nothing outputed so far */
		_print_header(STDOUT);
		fputs(outbuf,stdout);
			
	}
	else {	/** already in the middle of output **/
		fputs(outbuf,stdout);
	}
}

void print_line(char *line)
{
	int i;
	for (i=0; i<MAXNOUT; ++i) 
		if (htmloutp[i] && htmloutv[i]) 
			fputs(line, htmloutp[i]);
}

void print_footer(void)
{
	if (htmloutp[MAILOUT]) {
		fprintf(htmloutp[MAILOUT],".%c%c",10,10);
		pclose(htmloutp[MAILOUT]);
		htmloutp[MAILOUT] = NULL;
		headerprinted[MAILOUT] = 0;	/** allow more SENDMAIL forms **/
	}
}

int set_http_cookie(char *cookie)
{
	if (headerprinted[STDOUT]) return(0);
	printf("Set-Cookie: %s\n",cookie);
	fflush(stdout);
	return(1);
}

void set_stdout(void)
{
	htmloutp[STDOUT] = stdout;
	htmloutv[STDOUT] = 1;
}

#define is_debug_on()	(htmloutv[DBGOUT] && htmloutp[DBGOUT])

int set_sendmail(char *to, char *from, char *subj)
{
	char mailcmd[1024];
	if (headerprinted[MAILOUT]) return(0);
	sprintf(mailcmd, "%s -f %s", MAILPROG, from);
	htmloutp[MAILOUT] = popen(mailcmd,"w");
	if (!htmloutp[MAILOUT]) return(0);
	if (is_debug_on()) fprintf(htmloutp[DBGOUT],"To: %s\nFrom: %s\nSubject: %s\n", to, from, subj);
	fprintf(htmloutp[MAILOUT],"To: %s\nSubject: %s\n", to, subj);
	fflush(htmloutp[MAILOUT]);
	htmloutv[MAILOUT] = 1;
	return(1);
}

int is_sendmail(void)
{
	return(!!htmloutp[MAILOUT]);
}

int set_output(int kw)
{
	htmloutv[STDOUT] = htmloutv[MAILOUT] = 0;
	if (kw==KW_ALL) htmloutv[STDOUT] = htmloutv[MAILOUT] = 1;
	else if (kw==KW_HTTP) htmloutv[STDOUT] = 1;
	else if (kw==KW_SENDMAIL) htmloutv[MAILOUT] = 1;
	return(0);
}

int set_debug(int onoff)
{
	if (onoff) {
		char *cwd = getcwd(NULL, 0);
		htmloutp[DBGOUT] = fopen("tmp/db3w_dbg.out", "a"); //under DOCUMENT_ROOT
		htmloutv[DBGOUT] = 1;
	}
	else {
		htmloutv[DBGOUT] = 0;
		if (htmloutp[DBGOUT]) fclose(htmloutp[DBGOUT]);
		htmloutp[DBGOUT] = NULL;
	}
}
