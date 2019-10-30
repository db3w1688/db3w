/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - cgi/console.c
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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define SOCK_NAME	"db3w_console"
#define BUFSIZE		4096

#define SIGNON		"DB3W Console v1.0 Copyright (C) 2019 Charles C. Chou. All rights reserved."

void ctl_c(int sig)
{
	exit(0);
}

#define VTIMEOUT	180 //3 min

int main() 
{
	struct sockaddr_un addr;
	char buf[BUFSIZE];
	int fd, cl, rc;
	time_t curr_time;
	struct tm *now;

	printf("%s\n", SIGNON);

	signal(SIGINT, ctl_c);
	signal(SIGHUP, ctl_c);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, ctl_c);

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		exit(1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	sprintf(addr.sun_path, "%c%s", '\0', SOCK_NAME);
	
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("bind()");
		exit(1);
	}
	
	if (listen(fd, 5) < 0) {
		perror("listen()");
		exit(1);
	}
	printf("Listening for connections...\n");

	while (1) {
		fd_set ready;
		struct timeval to;
		FD_ZERO(&ready);
		FD_SET(fd, &ready);
		to.tv_sec = VTIMEOUT;
		to.tv_usec = 0;
		rc = select(fd+1, &ready, (fd_set *)0, (fd_set *)0, &to);
		if (rc < 0) {
			perror("select()");
			exit(1);
		}
		if (rc == 0) {
			printf("Connection timed out, exiting...\n");
			exit(0);
		}
		if ( (cl = accept(fd, NULL, NULL)) < 0) {
			perror("accept()");
			continue;
		}
		curr_time = time((time_t *)NULL);
		now = localtime(&curr_time);
		printf("Connection #%d started at %d-%02d-%02d %02d:%02d:%02d %s\n", cl, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, now->tm_zone);
		while ((rc = read(cl, buf, sizeof(buf))) > 0) {
				printf("%.*s", rc, buf);
		}
		if (rc < 0) {
			perror("read()");
			exit(1);
		}
		curr_time = time((time_t *)NULL);
		now = localtime(&curr_time);
		printf("Connection #%d ended at %d-%02d-%02d %02d:%02d:%02d %s\n", cl, now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, now->tm_zone);
		close(cl);
	}


	return 0;
}
