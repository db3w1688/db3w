/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/hb_sock.c
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
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hb.h"
#include "hbapi.h"
#include "hberrno.h"
#include "hbtli.h"

#define INVALID_SOCKET	(-1)

extern int errno, sys_nerr;

#define hb_comm_err(msg)	display_comm_err(errno,strerror(errno),msg)
#ifdef HBSSL
#define SHUTDOWN(conn)		{ close(SSL_get_fd(conn)); SSL_free(conn); }
#else
#define SHUTDOWN(conn)		close((int)conn)
#endif

#ifdef HBSSL
#define SSL_read(a,b,c)		SSL2_read(a,b,c)
#define SSL_write(a,b,c)	SSL2_write(a,b,c)
extern int SSL_get_fd(),SSL2_read(),SSL2_write();
extern HBCONN SSL_new();
extern void SSL_free();
#endif

long power10(int exp)
{
	long result = 1;
	while (exp) { result *= 10; --exp; }
	return result;
}

int usr(HBCONN conn, HBREGS *regs);

void sigpipe(int sig)
{
	signal(SIGPIPE,sigpipe);
	hb_sigs |= S_PIPE;
}

int tli_send(HBCONN conn, char *buf, int len)
{
	int rc;
	do {
#ifdef HBSSL
		rc = SSL_write(conn, buf, len);
#else
		rc = send((int)conn, buf, len, 0);
#endif
		if (hb_sigs & S_PIPE) return(-1);
	} while (rc < 0 && (errno == EWOULDBLOCK || errno == EINTR));
	return(rc);
}

int tli_recv(HBCONN conn, char *buf, int len, int exact)
{
	int rc,rc2;

	do {
#ifdef HBSSL
		rc = SSL_read(conn,buf,len);
#else
		rc = recv((int)conn,buf,len,0);
#endif
		if (hb_sigs & S_PIPE) return(-1);
	} while (rc < 0 && (errno == EWOULDBLOCK || errno == EINTR));
	if (rc<0 || !exact || rc==len) return(rc);
	else {
		rc2 = tli_recv(conn, buf+rc, len-rc, 1);
		if (rc2 < 0) return(-1);
		return(rc + rc2);
	}
}

int hb_response(HBCONN conn, HBREGS *regs, char *t_buf)
{
	char *tp;
	int xlen, rc, islogin = regs->ax == DBE_LOGIN;
again:
	rc = tli_recv(conn, t_buf, TLIPACKETSIZE, 0);
	if (rc < 0) {
		hb_comm_err("stream socket receive");
		return(-1);
	}
	if (rc < PKTHDRSZ) {
		hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
		return(-1);
	}
	if (PKT_FLAGS_GET(t_buf) == HBERROR) {
		packet2regs(t_buf, regs, 1);
		if (!islogin) dbe_error(conn, regs); //conn already shutdown by server if login failed, let _msghtml display the message
		return(-1);
	}
	if (PKT_FLAGS_GET(t_buf) == USRNORMAL) {
		HBREGS usr_regs;
		int flags;
		packet2regs(t_buf, &usr_regs, 1);
/*
printf("usr call, ax=%x, cx=%x, dx=%lx, ex=%x\n",usr_regs.ax,usr_regs.cx,usr_regs.dx,usr_regs.ex);
*/
		flags = usr(conn, &usr_regs)? USRERROR : USRNORMAL;
		regs2packet(&usr_regs, flags, t_buf);
		if (usr_regs.cx > 0) memcpy(t_buf + DATA_OFFSET, usr_regs.bx, usr_regs.cx);
		tli_send(conn, t_buf, PKTHDRSZ + usr_regs.cx);
		/*
		return(hb_response(conn,regs,t_buf));
		*/
		goto again;
	}
/*
printf("packet received, ax=%x, cx=%x, dx=%lx, ex=%x\n",read_short(t_buf+AX_OFFSET),read_short(t_buf+CX_OFFSET),read_long(t_buf+DX_OFFSET),read_short(t_buf+EX_OFFSET));
getchar();
*/
	packet2regs(t_buf, regs, 0); //don't set bx

	if (regs->ax == DBE_LOGOUT) return(1); //server already finished

	if (regs->cx > 0) { //server is sending data
		xlen = regs->cx;
/*
printf("%d bytes to be transfered...\n",xlen);
*/
		if (xlen <= TRANSBUFSZ) {
			if (rc != xlen + PKTHDRSZ) {
				hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
				return(-1);
			}
			if (regs->bx) memcpy(regs->bx, t_buf + DATA_OFFSET, xlen);
			else regs->bx = t_buf + DATA_OFFSET;
		}
		else if (regs->bx) { //client buffer must be set before the hbapi call
			tp = regs->bx;
			while (xlen > 0) {
				int len;
				if ((xlen > TRANSBUFSZ && rc != TLIPACKETSIZE)
				|| (xlen <= TRANSBUFSZ && (xlen != PKT_CX_GET(t_buf) || rc != xlen + PKTHDRSZ))) {
					hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
					return(-1);
				}
				len = xlen > TRANSBUFSZ? TRANSBUFSZ : xlen;
				memcpy(tp, t_buf + DATA_OFFSET, len);
				tp += len;
				xlen -= len;
				if (xlen > 0) {
					rc = tli_recv(conn, t_buf, xlen>TRANSBUFSZ? TLIPACKETSIZE : xlen + PKTHDRSZ, 1);
					if (rc < 0) {
						hb_comm_err("stream socket receive");
						return(-1);
					}
				}
			}
		}
		else hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
	}
	else if (regs->cx < 0) { //server needs data
		xlen = -regs->cx;
		tp = regs->bx;
		while (xlen > 0) {
			int size;
			PKT_AX_PUT(t_buf, DOWNSTREAM);
			/* if receiver is not expecting it, it will trigger an error */
			size = xlen > TRANSBUFSZ? TRANSBUFSZ : xlen;
			PKT_CX_PUT(t_buf, size);
			memcpy(t_buf + DATA_OFFSET, tp, size);
			rc = tli_send(conn, t_buf, PKTHDRSZ + size);
			if (rc < 0) {
				hb_comm_err("stream socket send");
				return(-1);
			}
			if (rc != PKTHDRSZ + size) {
				hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
			}
			xlen -= size; //NEEDSWORK: need to check source (regs->bx) buffer size
			tp += size;
		}
	}
	return(0);
}

int hb_int(HBCONN conn, HBREGS *regs)
{
	int islogin, islogoff, s, rc, xlen;
	struct sockaddr_in server;
	char t_buf[TLIPACKETSIZE], *tp;

	if (conn == HBCONN_INVAL) return(-1);
	islogin = regs->ax == DBE_LOGIN;
	islogoff = regs->ax == DBE_LOGOUT || regs->ax == DBE_SLEEP;
	if (islogin) {
		char *dbsrvr = regs->bx;
		int hb_port = regs->ex;
		if (!dbsrvr) {
			display_msg("No Login specified.", NULL);
			return(-1);
		}
#ifdef HBSSL
		if (!conn) {
			display_msg("No SSL Context defined.", NULL);
			return(-1);
		}
		if (!(conn = SSL_new(conn))) {
			display_msg("Cannot allocate SSL structure.", NULL);
			return(-1);
		}
#endif
		if (hb_port <= 0) hb_port = HB_PORT;
		tp = regs->bx += strlen(regs->bx) + 1;	/** *tp@dbpath **/
		tp += strlen(tp) + 1;	/** *tp@suid byte **/
		++tp; //*tp@addr
		tp += strlen(tp) + 1;
		xlen = (int)((BYTE *)tp - regs->bx);

		if (!dbsrvr[0]) {
			dbsrvr = getenv("DBSRVR");
			if (!dbsrvr)
				dbsrvr = LOCALHOST;
		}
		if (!atoi(dbsrvr)) {
			struct hostent *hp;
			if (!(hp = gethostbyname(dbsrvr))) {
				hb_comm_err("Unknown server");
				goto xx;
			}
			bcopy((char *)hp->h_addr, (char *)&server.sin_addr, hp->h_length);
		}
		else server.sin_addr.s_addr = inet_addr(dbsrvr);
		if (server.sin_addr.s_addr == -1) {
			hb_comm_err("Invalid IP address");
			goto xx;
		}
		server.sin_family = AF_INET;
		server.sin_port = htons((unsigned short)hb_port);
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET) {
			hb_comm_err("stream socket open");
xx:
			return(-1);
		}

		rc = connect(s, (struct sockaddr *)&server, sizeof(server));
		if (rc) {
			close(s);
			hb_comm_err("stream socket connect");
			goto xx;
		}
#ifdef HBSSL
		if (hbssl_start(conn, s) < 0) {
			hb_comm_err("SSL connect");
			SHUTDOWN(conn);
			goto xx;
		}
#else
		conn = s;
#endif
		rc = 1;
		ioctl(s, FIONBIO, &rc);
		regs->cx = xlen;
		if (regs->cx > TRANSBUFSZ) hb_error(conn, BAD_COMMUNICATION, NULL, -1, -1);
	}
	regs2packet(regs, 0, t_buf);
	if (regs->cx > 0) memcpy(t_buf + DATA_OFFSET, regs->bx, regs->cx);
	xlen = PKTHDRSZ + regs->cx;
	rc = tli_send(conn, t_buf, xlen);
/*
printf("packet sent, ax=%x, cx=%x, dx=%lx, ex=%x\n",regs->ax,regs->cx,regs->dx,regs->ex);
getchar();
*/
	if (rc != xlen) {
		hb_comm_err("stream socket send");
		return(-1);
	}
	if (!islogoff) {
		if (regs->bx && regs->cx > 0) regs->bx = NULL; //prevent over-run with unexpected data
		if (hb_response(conn, regs, t_buf) < 0) {
			if (islogin) {
				dbe_clr_err(conn, 0);
				SHUTDOWN(conn);
			}
			return(-1);
		}
		regs->ax = PKT_AX_GET(t_buf); /** could be DBE_LOGOUT from QUIT command **/
		regs->ex = PKT_EX_GET(t_buf);
		if (islogin) {
			regs->dx = conn;
		}
		else {
			regs->dx = PKT_DX_GET(t_buf);
		}
	}
	if (regs->ax == DBE_LOGOUT) { //may come from server after a non-logout call
		if (!!regs->ex) {
			dbe_clr_err(conn, 0);
		}
		SHUTDOWN(conn);
	}
	return(0);
}

void usr_return(HBCONN *conn, HBREGS *usr_regs, int iserr)
{
	char t_buf[TLIPACKETSIZE];
	int rc;

	regs2packet(usr_regs, iserr? USRERROR : USRNORMAL, t_buf);
	if (usr_regs->cx > 0) memcpy(t_buf + DATA_OFFSET, usr_regs->bx, usr_regs->cx);
	rc = tli_send(*conn, t_buf, PKTHDRSZ + usr_regs->cx);
	if (rc < 0) hb_comm_err("stream socket send");
	else if (hb_response(*conn, usr_regs, t_buf)) goto end; //>0: server already exited so just exit
	/** the engine does not know a previous session has ended, so wait
	for the response. we are still in prg processing **/
	dbe_end_session(conn); //no need, server has already exited
end:
	exit(0);
}

#include <sys/un.h>
extern char *homedir, hb_user[];

static int ipc_send(int fd, char *buf, int count)
{
	int rc = write(fd, buf, count);
	if (rc < 0 || rc != count) {
		hb_comm_err("AF_UNIX socket write() failed");
		rc = -1;
	}
	return(rc);
}

static int ipc_recv(int fd, char *buf, int size)
{
	int rc = read(fd, buf, size);
	if (rc < 0) hb_comm_err("AF_UNIX socket read() failed");
	return(rc);
}

int hb_ipc_wait(char *sockname, int sid, int timeout)
{
	struct sockaddr_un addr;
	char buf[100];
	int fd = -1, cl = -1, rc = -1;
	fd_set ready;
	struct timeval to;
	
	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		hb_comm_err("Failed to create AF_UNIX socket");
		goto end;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%c%s", '\0', sockname);
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		hb_comm_err("AF_UNIX socket bind() failed");
		goto end;
	}
	
	if (listen(fd, 5) < 0) {
		hb_comm_err("AF_UNIX socket listen() failed");
		goto end;
	}
new_sess:	
	FD_ZERO(&ready);
	FD_SET(fd, &ready);
	to.tv_sec = timeout;
	to.tv_usec = 0;
	rc = select(fd+1, &ready, (fd_set *)0, (fd_set *)0, &to);
	if (rc < 0) {
		hb_comm_err("AF_UNIX socket select() failed");
		goto end;
	}
	if (rc == 0) {
		hb_comm_err("AF_UNIX socket accept() timed out");
		goto end;
	}
	if ( (cl = accept(fd, NULL, NULL)) < 0) {
		hb_comm_err("AF_UNIX socket accept() failed");
		rc = -1;
		goto end;
	}
		
	while (1) {
		int ipc_cmd;

		FD_ZERO(&ready);
		FD_SET(cl, &ready);
		to.tv_sec = timeout;
		to.tv_usec = 0;
		rc = select(cl+1, &ready, (fd_set *)0, (fd_set *)0, &to);
		if (rc < 0) {
			hb_comm_err("AF_UNIX socket select() failed");
			break;
		}
		if (rc == 0) {
			hb_comm_err("AF_UNIX socket read() timed out");
			break;
		}

		rc = ipc_recv(cl, buf, sizeof(buf));
		if (rc < 0) break;
		if (rc == 0) { //connection closed, go wait for a new one
			close(cl);
			goto new_sess;
		}
		ipc_cmd = ntohl(get_dword(buf));
		switch (ipc_cmd) {
		case IPC_GET_SID:
			put_dword(buf, htonl(sid));
			rc = ipc_send(cl, buf, sizeof(uint32_t));
			break;
		case IPC_GET_USER:
			strcpy(buf, hb_user);
			rc = ipc_send(cl, buf, strlen(buf) + 1);
			break;
		case IPC_RESUME:
			rc = 0;
			break;
		default:
			hb_comm_err("Unknown IPC command");
			break;
		}
		if (rc <= 0) break;
	}
end:
	if (cl != -1) close(cl);
	if (fd != -1) close(fd);
	return(rc);
}

int hb_ipc_resume(char *sockname)
{
	struct sockaddr_un addr;
	char buf[100];
	int fd, rc = 0;
	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		hb_comm_err("Failed to create AF_UNIX socket");
		return(-1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%c%s", '\0', sockname);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		hb_comm_err("AF_UNIX socket connect() failed");
		return(-1);
	}
	
	put_dword(buf, htonl(IPC_GET_USER));
	rc = ipc_send(fd, buf, sizeof(uint32_t));
	if (rc < 0) goto end;
	rc = ipc_recv(fd, buf, sizeof(buf));
	if (rc <= 0) goto end;
	if (strcmp(buf, hb_user)) {
		hb_comm_err("User mismatch!");
		rc = -1;
		goto end;
	}
	put_dword(buf, htonl(IPC_RESUME));
	rc = ipc_send(fd, buf, sizeof(uint32_t));
end:
	close(fd);
	return(rc);
}

int hb_ipc_get_sid(char *sockname)
{
	struct sockaddr_un addr;
	char buf[100];
	int fd, rc;
	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		hb_comm_err("Failed to create AF_UNIX socket");
		return(-1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%c%s", '\0', sockname);
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		hb_comm_err("AF_UNIX socket connect() failed");
		rc = -1;
		goto end;
	}
	
	put_dword(buf, htonl(IPC_GET_USER));
	rc = ipc_send(fd, buf, sizeof(uint32_t));
	if (rc < 0) goto end;
	rc = ipc_recv(fd, buf, sizeof(buf));
	if (rc <= 0) goto end;
	if (strcmp(buf, hb_user)) {
		hb_comm_err("User mismatch!");
		rc = -1;
		goto end;
	}
	put_dword(buf, htonl(IPC_GET_SID));
	rc = ipc_send(fd, buf, sizeof(uint32_t));
	if (rc < 0) goto end;
	rc = ipc_recv(fd, buf, sizeof(buf));
	if (rc != sizeof(uint32_t)) {
		rc = -1;
		goto end;
	}
	rc = ntohl(get_dword(buf));
end:
	close(fd);
	return(rc);
}
