/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/viewsource.c
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
#include <string.h>

#include "w3util.h"

extern char *getenv();

main()
{
	char *pinfo = getenv("PATH_INFO");
	char *path = getenv("PATH_TRANSLATED");
	char *qs = getenv("QUERY_STRING"), *qs2;
	char line[2048], *cmt = NULL;
	FILE *fp;
	int lineno;

	set_stdout();
	if (!pinfo || !path) {
		html_out(NL2BR, "ERROR: No file specified.\n");
		exit(1);
	}
	fp = fopen(path, "r");
	if (!fp) {
		html_out(NL2BR, "ERROR: Cannot open %s\n", path);
		exit(1);
	}
	if (qs) {
		char *tp = strchr(qs, '=');
		if (tp) {
			if (!strncasecmp(qs, "comments", 8)) cmt = tp + 1;
		}
	}
	print_header();
	fputs("<html>\n<body background=\"data:image/png;base64,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\" dir=\"ltr\">\n", stdout);
	sprintf(line, "<h3>%s</h3>\n", path);
	fputs(line, stdout);
	if (cmt) {
		sprintf(line, "<p><a href=\"#\" onClick=\"window.open('%s','comments','toolbar=0,location=0,directories=0,status=no,scrollbars=yes,menubar=0,resizable=yes,width=800,height=600'); return false;\">Click here for line-by-line comments</a></p>\n", cmt);
		fputs(line, stdout);
	}
	fputs("<hr>\n<pre>\n", stdout);
	lineno = 1;
	while (1) {
		char *tp;
		sprintf(line, "%4d  ", lineno++);
		tp = line + strlen(line);
		if (!fgets(tp, 2048 - (int)(tp - line), fp)) break;
		html_out(0, line);
	}
	fputs("</pre>\n", stdout);
	fputs("</body>\n</html>\n", stdout);
	fclose(fp);
	exit(0);
}
