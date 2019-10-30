/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbcmd/hbhlp.c
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

#define HLPLINELEN	4096
#define NTAB_COL	4

#define ESC_BOLD	"\x1b[1m"
#define ESC_UNDRLN	"\x1b[4m"
#define ESC_BLNK	"\x1b[5m"
#define ESC_RVSV	"\x1b[7m"
#define ESC_STRKTR	"\x1b[9m"
#define ESC_RED		"\x1b[31m"
#define ESC_END		"\x1b[0m"

void hbscrn_printf(char *fmt, ...);
int hbscrn_wait(char *prompt, int timeout);

int isword(char c)
{
	return(isalnum(c) || c == '_' || c == '.');
}

char *format_line(char *line, int np)
{
	int i, j, bullet_start = 0, bullet_stop = 0;
	static int bullet_cnt;
	static char bullet[64];
	char tail[HLPLINELEN], word[1024];
	for (i=0; line[i];) {
		int c = line[i];
		if (c == '$' || c == '%' || c == '~') {
			char *esc_code;
			switch (c) {
			case '$':
				esc_code = ESC_UNDRLN;
				break;
			case '%':
			default:
				esc_code = ESC_BOLD;
				break;
			case '~':
				esc_code = ESC_RVSV;
				break;
			}
			if (line[i+1] == c) {
				for (j=i+2; line[j]; ++j) if (line[j] == c && line[j-1] != '\\') break;
				if (line[j]) strcpy(tail, &line[j + 1]);
				snprintf(word, 1024, "%s%.*s%s", esc_code, j-i-2, &line[i+2], ESC_END);
			}
			else {
				for (j=i+1; line[j]; ++j) if (!isword(line[j])) break;
				strcpy(tail, &line[j]);
				snprintf(word, 1024,  "%s%.*s%s", esc_code, j-i-1, &line[i+1], ESC_END);
			}
			snprintf(&line[i], HLPLINELEN - i, "%s%s", word, tail);
			i += strlen(word);
		}
		else if (c == '#') {
			if (!i && np) {
				if (line[i+1] == '#') bullet_cnt = 1;
				else if (line[i+1] == '!') {
					bullet_stop = 1;
					bullet_cnt++;
				}
				else bullet_cnt++;
				sprintf(bullet, "%2d. ", bullet_cnt);
				j = i + 1;
				if (bullet_cnt == 1 || bullet_stop) j++;
				if (isspace(line[j])) j++;
				strcpy(tail, &line[j]);
				snprintf(&line[i], HLPLINELEN - i, "%s%s", bullet, tail);
				i += strlen(bullet);
				bullet_start = 1;
			}
			else ++i;
		}
		else if (c == '\\') {
			strcpy(tail, &line[i+1]);
			strcpy(&line[i], tail);
			++i;
		}
		else ++i;
	}
	if (np && !bullet_start) bullet_cnt = 0;
	if (!bullet_start && bullet_cnt) {
		strcpy(tail, line);
		sprintf(line, "%s%s", "    ", tail);
	}
	if (bullet_stop) bullet[0] = '\0';
	return(line);
}
	
static int help_printline(char *line, int ncols, int nrows, int timeout, int *lncnt)
{
	char *start, *tp, *tp2;
	int n, ntabs, i, len, np = 1; //new paragraph
	for (start=tp=line, n=0, ntabs=0; *tp; ++tp) {
		if (n >= ncols) {
			char outline[HLPLINELEN];
			tp2 = start + (n - ntabs * NTAB_COL);
			while (tp2 > start && *tp2 != ' ') --tp2;
			if (tp2 == start) tp2 = start + (ntabs * NTAB_COL) + (ncols - ntabs);
			for (i=0; i<ntabs; ++i) hbscrn_printf("%*s", NTAB_COL, " ");
			sprintf(outline, "%.*s", (int)(tp2 - start), start);
			hbscrn_printf("%s\n", format_line(outline, np));
			np = 0;
			++(*lncnt);
			if (*lncnt >= nrows - 1) {
				int c = hbscrn_wait("---- more ----", timeout);
				if (c < 0 || c == 0x1b ||  c == 'q' || c == 'Q') return(1);
				*lncnt = 0;
			}
			if (*tp2 == ' ') start = tp2;
			if (*start == ' ') ++start;
			tp = start;
			n = ntabs * NTAB_COL;
		}
		if (*tp == '\t') {
			if (tp == start) {
				n += NTAB_COL; ++ntabs; ++start;
				continue;
			}
			else *tp = ' ';
		}
		++n;
	}
	for (i=0; i<ntabs; ++i) hbscrn_printf("%*s", NTAB_COL, " ");
	if (!*start || *start == '\n') hbscrn_printf("\n");
	else hbscrn_printf("%s\r", format_line(start, np));
	++(*lncnt);
	return(0);
}

static int help_is_comment(char *line)
{
	return(line[0] == '/' && line[1] == '/');
}

int _hb_help(char *hlpfn, int timeout)
{
	char hlpmsg[HLPLINELEN];
	FILE *hlpfp = fopen(hlpfn, "r");
	if (!hlpfp) return(-1);
	else {
		int ncols, nrows, lncnt;
		ncols = hbscrn_ncols();
		nrows = hbscrn_nrows();
		for (lncnt=0; fgets(hlpmsg, HLPLINELEN, hlpfp);) {
			if (help_is_comment(hlpmsg)) continue;
			if (help_printline(hlpmsg, ncols - 4, nrows, timeout, &lncnt)) break;
			if (lncnt >= nrows - 1) {
				int c = hbscrn_wait("---- more ----", timeout);
				if (c < 0 || c == 0x1b || c == 'q' || c == 'Q') break;
				lncnt = 0;
			}
		}
		hbscrn_printf("\n");
		fclose(hlpfp);
	}
	return(0);
}

#define DPATH "/share/doc/db3w/"

char *get_docpath(char *buf, int bufsiz)
{
	char *tp;
	int len = readlink("/proc/self/exe", buf, bufsiz);
	if (len < 0) return(NULL);
	buf[len] = '\0';
	tp = strrchr(buf, '/');
	if (!tp) return(NULL);
	*tp = '\0';
	tp = strrchr(buf, '/');
	if (!tp || strcmp(tp + 1, "bin")) return(NULL);
	if ((tp - buf) + strlen(DPATH) + 1 > bufsiz) return(NULL);
	strcpy(tp, DPATH);
	return(buf);
}

#ifdef HLP_MAIN
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
	
int hb_sigs = 0;
static char *prgname = "hbhlp";
void unblock_breaks(void);

void usage(void)
{
	printf("Usage: %s <topic/subtopic>\n", prgname);
	exit(0);
}

main(int argc, char *argv[])
{
	DIR *dirp = NULL;
	struct dirent *dp;
	int len, size, found = 0;
	char docpath[PATH_MAX], *end, *tp;
	if (argc != 2) usage();
	if (!get_docpath(docpath, PATH_MAX)) strcpy(docpath, "./");
	end = docpath + strlen(docpath);
	size = PATH_MAX - (end - docpath);
	tp = strrchr(argv[1], '/');
	if (tp) {
		++tp;
		snprintf(end, size, "%.*s", tp - argv[1], argv[1]);
		end += strlen(end);
		size = PATH_MAX - (end - docpath);
	}
	else tp = argv[1];
	dirp = opendir(docpath);
	if (!dirp) {
		fprintf(stderr, "Cannot open directory %s\n", argv[1]);
		exit(1);
	}
	len = strlen(tp);
	if (len > 0) while (!found && (dp = readdir(dirp))) {
		struct stat f_stat;
		char *tp2;
		if (strlen(dp->d_name) + 1 > size) {
			fprintf(stderr, "Path too long: %s\n", dp->d_name);
			exit(1);
		}
		strcpy(end, dp->d_name);
		if (stat(docpath, &f_stat) < 0) {
			fprintf(stderr, "Failed to get stat: %s\n", docpath);
			exit(1);
		}
		if (!S_ISREG(f_stat.st_mode)) continue;
		if (strncasecmp(tp, dp->d_name, len)) continue;
		tp2 = strchr(dp->d_name, '.');
		if (!tp2 || strcmp(tp2+1, "hlp")) continue;
		found = 1;
	}
	if (!found) fprintf(stderr, "No entry found\n");
	else {
		hbscrn_init();
		atexit(unblock_breaks);
		_hb_help(docpath, -1);
	}
	closedir(dirp);
}
#endif