/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/port.c
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

#include <unistd.h>
#include <sys/types.h>

#ifndef UNALIGNED_MEM

short get_short(char *tp)
{
    unsigned short n;
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(short); ++i) *ip++ = *tp++;
    return(n);
}

long get_long(char *tp)
{
    long n;
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(long); ++i) *ip++ = *tp++;
    return(n);
}

off_t get_foff(char *tp)
{
    off_t n;
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(off_t); ++i) *ip++ = *tp++;
    return(n);
}

double get_double(char *tp)
{
    double n;
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(double); ++i) *ip++ = *tp++;
    return(n);
}

void put_short(char *tp, short n)
{
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(short); ++i) *tp++ = *ip++;
}

void put_long(char *tp, long n)
{
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(long); ++i) *tp++ = *ip++;
}

void put_foff(char *tp, off_t n)
{
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(off_t); ++i) *tp++ = *ip++;
}

void put_double(char *tp, double n)
{
    int i;
    char *ip = (char *)&n;
    for (i=0; i<sizeof(double); ++i) *tp++ = *ip++;
}

#endif

#if __BYTE_ORDER == __BIG_ENDIAN

short read_short(char *tp)
{
    unsigned short n;
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(short)-1; i>=0; --i) *ip++ = tp[i];
    return(n);
}

long read_long(char *tp)
{
    long n;
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(long)-1; i>=0; --i) *ip++ = tp[i];
    return(n);
}

off_t read_foff(char *tp)
{
    off_t n;
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(off_t)-1; i>=0; --i) *ip++ = tp[i];
    return(n);
}

double read_double(char *tp)
{
    double n;
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(double)-1; i>=0; --i) *ip++ = tp[i];
    return(n);
}

void write_short(char *tp, short n)
{
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(short)-1; i>=0; --i) *tp++ = ip[i];
}

void write_long(char * tp, long n)
{
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(long)-1; i>=0; --i) *tp++ = ip[i];
}

void write_foff(char *tp, off_t n)
{
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(off_t)-1; i>=0; --i) *tp++ = ip[i];
}

void write_double(char *tp, double n)
{
    int i;
    char *ip = (char *)&n;
    for (i=sizeof(double)-1; i>=0; --i) *tp++ = ip[i];
}

#endif

#ifdef NO_RENAME
extern int errno;
int rename(char *path1, char * path2)
{
    char cmd[1024];
    sprintf(cmd,"mv %s %s",path1,path2);
    system(cmd);
    return(errno);
}
#endif

#ifdef NO_CHSIZE
#ifdef MS_DOS
int chsize(int fd, off_t length)
{
	lseek(fd, length, 0);
	write(fd, &length, 0);
	return(0);
}
#else
int chsize(int fd, off_t length)
{
	return(ftruncate(fd, length));
}
#endif
#endif
