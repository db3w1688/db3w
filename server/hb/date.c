/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/date.c
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
#include "hb.h"
#include "dbe.h"

static char daysofmonth[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int verify_date(int year, int month, int day)
{
   if (day < 1 || year < 0) return(0);
   if (year < 100) {
   	if (year < epoch % 100) year += ((epoch + 99) / 100) * 100;
   	else year += (epoch / 100) * 100;
   }
   switch (month) {
      case 1:
      case 3:
      case 5:
      case 7:
      case 8:
      case 10:
      case 12:
         if (day > 31) return(0);
         break;
      case 2:
         if (day > (isleap(year) ? 29 : 28)) return(0);
         break;
      case 4:
      case 6:
      case 9:
      case 11:
         if (day > 30) return(0);
         break;
      default: return(0);
   }
   return(1);
}

static date_t yeartoday(int year)
{
   if (year < 0) return((date_t)0);
   return(((date_t)year / 4) * 1461 + (year % 4) * 365);
}

date_t date_to_number(int day, int month, int year)
{
   if (year < 100) {
   	if (year < epoch % 100) year += ((epoch + 99) / 100) * 100;
   	else year += (epoch / 100) * 100;
   }
   while (--month) day += month==2? (isleap(year) ? 29 : 28) : daysofmonth[month];
   return(1721410 + yeartoday(year - 1) + day);
}

void n_to_mdy(date_t d, int *mm, int *dd, int *yy)
{
   date_t td = d - 1721410;
   if ((int)td <= 0) {
      *dd = *mm = *yy = 0;
      return;
   }
   *yy = (int)((double)td / 365.25 + 1);
   *dd = td - yeartoday(*yy - 1);
	if (!*dd) *dd = td - yeartoday(--(*yy) - 1);
   for (*mm=1;;++(*mm)) {
		int days = *mm == 2? (isleap(*yy) ? 29 : 28) : daysofmonth[*mm];
		if (*dd <= days) break;
		*dd -= days;
	}
}

#define MAXDATEFMT	15
#define DEFDATEFMT	"MM/DD/YYYY"

char datespec[MAXDATEFMT + 1] = DEFDATEFMT;

static int is_separator(char c)
{
	if (c == '/' || c == '.' || c == '-' || c == ',') return(1);
	return(0);
}

void datefmt(char *spec, char *fmt, char *seq, int *year)
{
	char *tp;
	char *sfmt[3] = {NULL, NULL, NULL}, **sp = sfmt, *fp = seq;
	int sptr1 = 0, sptr2 = 0, y = 0;
	memset(seq, 0, 4);
	if (!spec) spec = datespec;
	for (tp=spec; *tp; ++tp) {
		if (toupper(*tp) == 'M') {
			if (!*sp) *sp = "%d";
			else if (!strcmp(*sp, "%d")) *sp = "%02d";
			else syn_err(BAD_DATE);
			*fp = 'm';
		}
		else if (toupper(*tp) == 'D') {
			if (!*sp) *sp = "%d";
			else if (!strcmp(*sp, "%d")) *sp = "%02d";
			else syn_err(BAD_DATE);
			*fp = 'd';
		}
		else if (toupper(*tp) == 'Y') {
			if (!*sp) *sp = "%d";
			else if (!strcmp(*sp, "%d")) {
				*sp = "%02d";
				y = 2; //2 digit year
			}
			else if (!strcmp(*sp, "%02d") && toupper(*(tp+1)) == 'Y') {
		        *sp = "%04d";
		        y = 4; //4 digit year
		        ++tp;
			}
			else syn_err(BAD_DATE);
			*fp = 'y';
		}
		else if (is_separator(*tp)) {
			if (!sptr1) sptr1 = *tp;
			else if (!sptr2) sptr2 = *tp;
			else syn_err(BAD_DATE);
			++sp; ++fp;
		}
		else syn_err(BAD_DATE);
	}
	if (!strchr(seq, 'm') || !strchr(seq, 'd') || !strchr(seq, 'y')) syn_err(BAD_DATE);
	if (fmt) sprintf(fmt, "%s%c%s%c%s", sfmt[0], sptr1, sfmt[1], sptr2, sfmt[2]);
	if (year && y == 2) *year = *year % 100;
}

void set_datefmt(int n)
{
	if (!n) strcpy(datespec, DEFDATEFMT);
	else {
		HBVAL *v;
		char dspec[MAXDATEFMT + 1], fmt[MAXDATEFMT + 1], seq[4];
		v = pop_stack(CHARCTR);
		if (v->valspec.len > MAXDATEFMT - 1) syn_err(STR_TOOLONG);
		sprintf(dspec, "%.*s", v->valspec.len, v->buf);
		datefmt(dspec, fmt, seq, NULL);
		strcpy(datespec, dspec);
	}
}

void number_to_date(int dbdate, date_t d, char *dd)
{
	int month,day,year;
	
	n_to_mdy(d, &month,&day, &year);
	if (dbdate) sprintf(dd, "%04d%02d%02d", year, month, day);
	else {
		char fmt[MAXDATEFMT + 1], seq[4];
		int mdy[3], i;
		datefmt(datespec, fmt, seq, &year);
		for (i=0; i<3; ++i) {
			char c = seq[i];
			mdy[i] = c == 'd'? day : (c == 'm'? month : year);
		}
		sprintf(dd, fmt, mdy[0], mdy[1], mdy[2]);
	}
}
