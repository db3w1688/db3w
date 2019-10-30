/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- hb/sock.c
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
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#define __USE_GNU
#include <signal.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <shadow.h>
#include <pthread.h>

#ifdef isset
#undef isset
#endif
#include "hb.h"
#include "hbapi.h"
#include "hbtli.h"
#include "dbe.h"
#include "hbusr.h"

#define SRV_PORT    htons(HB_PORT)
#define SRV_ADDR	0
#define MAX_CIND	1
#define VTIMEOUT	300

#define NOBODY		"nobody"

#define STAT_SESS_AUTH		0x80000001
#define STAT_SESS_FAULT		0x00000002
#define STAT_SESS_MULTIOK	0x00000004
#define multi_login_ok(sid)	!!(hb_session[sid].status & STAT_SESS_MULTIOK)

int hb_uid=-1, hb_euid = -1, hb_gid=-1;
char hb_user[LEN_USER + 1];

extern char progid[],*_progid;
extern int errno;

static FILE *logfp = NULL;
int verbose = 0;

struct _client {
	struct sockaddr_in peer; /** may be different than client **/
	char userspec[256];
};

struct hb_se {
	int pid;
	int connection;
	int sfd[2];	/** for ipc to send fd's **/
	struct _client client;
	int stamp;
	int status;
} *hb_session;

pthread_mutex_t *shm_mutex;

int get_session_id(void)
{
	int i, pid = getpid();
	pthread_mutex_lock(shm_mutex);
	for (i=0; i<MAXUSERS; ++i) if (hb_session[i].pid == pid) break;
	pthread_mutex_unlock(shm_mutex);
	return(i < MAXUSERS? i : -1);
}

int hb_session_stamp(int setit)
{
	int sid = get_session_id();
	if (sid < 0) return(-1);
	if (setit) {
		pthread_mutex_lock(shm_mutex);
		hb_session[sid].stamp = rand();
		pthread_mutex_unlock(shm_mutex);
	}
	return(hb_session[sid].stamp);
}

static int already_logged_in(char *user, int sid)
{
	int i;
	char userspec[256], *tp;
	pthread_mutex_lock(shm_mutex);
	for (i=0; i<MAXUSERS; ++i) {
		if (i == sid) continue;
		if (hb_session[i].pid == -1) continue;
		strcpy(userspec, hb_session[i].client.userspec);
		tp = strchr(userspec, '@');
		if (tp) *tp = '\0';
		if (!strcmp(user, userspec)) break;
	}
	pthread_mutex_unlock(shm_mutex);
	if (i >= MAXUSERS) return(-1);
	return(i);
}

int hb_authenticate(char *epasswd);

void log_printf(char *fmt, ...),log_error();

int set_session_user(char *userspec, char *spath)
{
	struct passwd *pwe;
	int sid = get_session_id(), sid2;
	int flags, do_auth = 0, suid = 0, is_admin = 0, multi_ok = 0;
	struct hb_se *pse;
	time_t curr_time;
	char homedir[FULFNLEN], *subdir = NULL, *tp;
	if (sid < 0) return(-1);
	flags = *userspec++;
	if (flags & HB_SESS_FLG_SUID) suid = 1;
	if (flags & HB_SESS_FLG_AUTH) do_auth = 1;
	if (flags & HB_SESS_FLG_MULTI) multi_ok = 1;

	pthread_mutex_lock(shm_mutex);
	pse = &hb_session[sid];
	strcpy(pse->client.userspec, userspec);
	if (tp = strchr(userspec,'@')) {
		*tp = '\0';
		if (tp = strchr(userspec, SWITCHAR)) {
			*tp = '\0';
			subdir = tp + 1;
		}
	}
	else {
		tp = pse->client.userspec + strlen(pse->client.userspec);
		sprintf(tp, "@%s", inet_ntoa(pse->client.peer.sin_addr));
	}
	pthread_mutex_unlock(shm_mutex);

	if (setgid(hb_gid)) sys_err(LOGIN_SGID);

	strcpy(hb_user, userspec);
	pwe = getpwnam(hb_user); //username only
	if (!pwe) sys_err(LOGIN_NOUSER);
	strcpy(homedir, pwe->pw_dir);
	if (suid) {
		if (hb_euid == -1 && pwe->pw_uid != hb_uid) sys_err(LOGIN_SUID);
		if (pwe->pw_uid == hb_uid && hb_euid != -1) is_admin = 1;
		else hb_uid = pwe->pw_uid;
		sid2 = already_logged_in(hb_user, sid);
		if (!is_admin && ((!multi_ok && sid2 != -1) || sid2 != -1 && !multi_login_ok(sid2))) sys_err(LOGIN_MULTI);
		if (multi_ok) pse->status |= STAT_SESS_MULTIOK;
		if (do_auth) {
			char *epasswd;
			if (strcmp(pwe->pw_passwd, "x")) { //no shadow passwd
				epasswd = pwe->pw_passwd;
			}
			else { //shadow passwd
				char *tp;
				struct spwd *spwe = getspnam(hb_user);
				if (!spwe) sys_err(LOGIN_NOUSER);
				epasswd = spwe->sp_pwdp;
			}
			if (hb_authenticate(epasswd)) sys_err(LOGIN_NOAUTH);
			pthread_mutex_lock(shm_mutex);
			pse->status |= STAT_SESS_AUTH;
			pthread_mutex_unlock(shm_mutex);
		}
		else if (flags & HB_SESS_FLG_SETAUTH) pse->status |= STAT_SESS_AUTH;
	}
	hb_session_stamp(1);
	curr_time = time((time_t *)NULL);
	if (is_admin || hb_euid == -1) {
		if (subdir) sprintf(rootpath, "%s/%s", homedir, subdir);
		else strcpy(rootpath, homedir);
	}
	else {
		if (chroot(homedir) < 0) sys_err(LOGIN_CHROOT);
		if (subdir) sprintf(rootpath, "/%s", subdir);
		else rootpath[0] = '\0';
	}
	strcpy(defa_path, rootpath);
	if ((is_admin && seteuid(hb_uid)) || (!is_admin && setuid(hb_uid))) sys_err(LOGIN_SUID); //seteuid() so we can get back to root
	//rootpath does not have trailing '/'
	if (chdir(rootpath[0]? rootpath : "/")) sys_err(LOGIN_CDROOT);
	tp = spath + strlen(spath);
	if (*(tp - 1) == SWITCHAR) *(tp - 1) = '\0';
	tp = defa_path + strlen(defa_path);
	if (spath[0] != SWITCHAR) *tp++ = SWITCHAR;
	if (strlen(spath) > 0) sprintf(tp, "%s%c", spath, SWITCHAR); else *tp = '\0';
	//defa_path does have trailing '/'
	log_printf("[%d]Session #%d from %s stamp=%d started on Connection #%d at %s\n", pse->pid, sid, pse->client.userspec, pse->stamp, pse->connection, timestr_(&curr_time));
	return(is_admin);
}

char *get_session_user(int sid)
{
	if (sid < 0 || sid >= MAXUSERS || hb_session[sid].pid < 0) return(NULL);
	return(hb_session[sid].client.userspec);
}

void hb_shutdown(void)
{
	int i = get_session_id();
	if (i>=0) {
		struct hb_se *pse = &hb_session[i];
		int rc;
		close(pse->sfd[0]);
		close(pse->sfd[1]);
		shutdown(pse->connection, 2);
		rc = close(pse->connection);
		log_printf("[%d]Connection #%d released, rc=%d\n", pse->pid, pse->connection, rc);
		pthread_mutex_lock(shm_mutex);
		//pse->sfd[0] = pse->sfd[1] = -1;
		//pse->connection = -1; do not set to -1 here. sig_child in parent needs it
		pthread_mutex_unlock(shm_mutex);
	}
	exit(0);
}

void hb_end_session(void)
{
	HBREGS hb_regs;
	do_onexit_proc();
	hb_regs.ax = DBE_LOGOUT;
	hb_regs.cx = 0;
	hb_regs.ex = 1;
	dbesrv(&hb_regs);
}

#ifdef BSDRENO
static struct cmsghdr *cmptr = NULL;
#define CONTROLLEN (sizeof(struct cmsghdr) + sizeof(int))
#ifndef CMSG_DATA
#define CMSG_DATA(cmptr) cmptr->cmsg_data
#endif
#endif

int wait_fd(int fd, int timeout)
{
	fd_set ready;
	struct timeval to;
	int rc;
	FD_ZERO(&ready);
	FD_SET(fd,&ready);
	to.tv_sec = timeout;
	to.tv_usec = 0;
	errno = 0;
	rc = select(fd+1, &ready, (fd_set *)0, (fd_set *)0, &to);
	return(rc);
}

int is_session_fault(void)
{
	int sid = get_session_id();
	return(sid < 0? 1 : !!(hb_session[sid].status & STAT_SESS_FAULT));
}

int is_session_authenticated(void)
{
	int sid = get_session_id();
	return(sid < 0? 0 : !!(hb_session[sid].status & STAT_SESS_AUTH));
}

void hb_sleep(int sid, int vtimeout)
{
	struct hb_se *pse = &hb_session[sid];
	struct iovec iov[1];
	struct msghdr msg;
	char msgbuf[256], *tp;
	int s, rc;
	if (vtimeout <= 0) vtimeout = VTIMEOUT;
	shutdown(pse->connection, 2);
	rc = close(pse->connection); //do not set pse->conneciton to -1
	if (rc < 0) {
		log_error("close()");
		goto end;
	}
	log_printf("[%d]Connection #%d released, rc=%d\n", pse->pid, pse->connection, rc);
	log_printf("[%d]Session #%d suspended\n", pse->pid, sid);
	/** read from pipe for new socket, block till available **/
	iov[0].iov_base = msgbuf;
	iov[0].iov_len = sizeof(msgbuf);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	rc = -1;
#ifdef BSDRENO
        if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL) {
        	log_error("out of memory");
        	goto end;
        }
        msg.msg_control = (caddr_t)cmptr;
        msg.msg_controllen = CONTROLLEN;
#else
	msg.msg_accrights = (caddr_t)&s;
	msg.msg_accrightslen = sizeof(s);
#endif
	if (wait_fd(pse->sfd[0], vtimeout)<=0) {
		hb_end_session();
		log_error("session timeout");
		goto end;
	}
	if (recvmsg(pse->sfd[0], &msg, 0) < 0) {
		log_error("recvmsg()");
		goto end;
	}
#ifdef BSDRENO
        pse->connection = *(int *)CMSG_DATA(cmptr);
#else
	pse->connection = s;
#endif
	tp = msgbuf;
	if (get_int32(tp) != pse->stamp || strcmp(tp + sizeof(int), pse->client.userspec)) pse->status |= STAT_SESS_FAULT; //fault
	log_printf("[%d]Session #%d resumed on Connection %d, stamp=%d, userspec=%s, status=%d\n", pse->pid, sid, pse->connection, get_int32(tp), tp + sizeof(int), pse->status);
	rc = 0;
end:
#ifdef BSDRENO
	if (cmptr) free(cmptr);
	cmptr = NULL;
#endif
	if (rc < 0) exit(1);
}

char *peer_ip_address(int sid)
{
	if (sid < 0 || sid >= MAXUSERS || hb_session[sid].pid < 0) return(NULL);
	return(inet_ntoa(hb_session[sid].client.peer.sin_addr));
}

int hb_wakeup(int sid, int stamp, char *userspec)
{
	struct iovec iov[1];
	struct msghdr msg;
	char msgbuf[256], *tp;
	int i, s, rc = -1;
	time_t curr_time;
	if (sid < 0 || sid >= MAXUSERS 
		|| (i = get_session_id()) < 0	//also sync hb_session[]
		|| hb_session[sid].pid < 0 
		|| i == sid) goto end;
	tp = msgbuf;
	put_int32(tp, stamp); tp += sizeof(int);
	if (!strchr(userspec, '@')) sprintf(tp, "%s@%s", userspec, peer_ip_address(i));
	else strcpy(tp, userspec);
	iov[0].iov_base = msgbuf;
	iov[0].iov_len = sizeof(int) + strlen(tp) + 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
#ifdef BSDRENO
        if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL) sys_err(NOMEMORY);
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        cmptr->cmsg_len = CONTROLLEN;
        *(int *)CMSG_DATA(cmptr) = (int)hb_session[i].connection;
        msg.msg_control = (caddr_t)cmptr;
        msg.msg_controllen = CONTROLLEN;
#else
	s = (int)hb_session[i].connection;
	msg.msg_accrights = (caddr_t)&s;
	msg.msg_accrightslen = sizeof(s);
#endif
	if (sendmsg(hb_session[sid].sfd[1], &msg, 0) < 0) goto end;
	curr_time = time((time_t *)NULL);
	log_printf("[%d]Session #%d ended at %s\n", hb_session[i].pid, i, timestr_(&curr_time));
	sleep(2);
	exit(0);
end:
	if (cmptr) {
		free(cmptr);
		cmptr = NULL;
	}
	return(-1);
}

void hb_do_session();

void log_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	if (logfp) vfprintf(logfp, fmt, ap);
	else {
		char msgbuf[1024];
		vsprintf(msgbuf, fmt, ap);
		syslog(LOG_NOTICE, "%s", msgbuf);
	}
}

void log_error(char *msg)
{
	/** fseek not neccessary, logfp is in append mode. see man pages...
	fseek(logfp,0L,2);
	**/
	if (errno) log_printf("[%d]%s error (%d): %s\n", getpid(), msg, errno, strerror(errno));
	else log_printf("[%d]%s error\n", getpid(), msg);
}

void hb_session_reset(int i)
{
	pthread_mutex_lock(shm_mutex);
	hb_session[i].sfd[0] = hb_session[i].sfd[1] = -1;
	hb_session[i].pid = hb_session[i].connection = -1;
	hb_session[i].status = 0;
	memset(&hb_session[i].client, 0, sizeof(struct _client));
	pthread_mutex_unlock(shm_mutex);
}

void sig_child(int sig)
{
	int pid,status,i;
	if (pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) {
		for (i=0; i<MAXUSERS; ++i) if (hb_session[i].pid == pid) {
			close(hb_session[i].sfd[0]);
			close(hb_session[i].sfd[1]);
			close(hb_session[i].connection);
			hb_session_reset(i);
			if (WIFEXITED(status)) {
				time_t curr_time = time((time_t *)NULL);
				log_printf("[%d]Session #%d ended at %s\n", pid, i, timestr_(&curr_time));
				log_printf("[%d]Session %d reset\n", pid, i);
			}
			else {
				log_printf("[%d]Session %d exited abnormally\n", pid, i);
			}
			break;
		}
	}
}

void sig_term(int sig)
{
	log_printf("[%d]signal %d received, exiting...\n", getpid(), sig);
	hb_end_session();
	exit(1);
}

void sig_term2(int sig)
{
	int i;
	log_printf("[%d]signal %d received, exiting...\n", getpid(), sig);
	for (i=0; i<MAXUSERS; ++i) if (hb_session[i].pid != -1) kill(hb_session[i].pid, SIGTERM);
	exit(1);
}

void usage(char *prgname)
{
	fprintf(stderr,"Usage: %s [-admin <user>] [-port <port#>] [-log <logfn>]\n", prgname);
	exit(1);
}

main(int argc, char *argv[])
{
	char logfn[FULFNLEN], dbe_user[MAXUSERLEN + 1], *tp, *shm;
	int i, pid, shm_id, sock, hb_port = 0, length, opt;
	struct sockaddr_in server;
	struct passwd *pwe = NULL;
	time_t curr_time;
	sighandler_t old_sig_child;
	pthread_mutexattr_t shm_attr;

	logfn[0] = '\0';
	if ((i = geteuid()) != 0) { //dbe not running as root
		pwe = getpwuid(i);
		strcpy(dbe_user, pwe->pw_name);
		hb_uid = pwe->pw_uid; //hb_euid is -1
		hb_gid = pwe->pw_gid;
	}
	else dbe_user[0] = '\0';
	for (i=1; i<argc; ++i) {
		if (!strcmp(argv[i], "-verbose")) verbose = 1; //not used
		else if (!strcmp(argv[i], "-log")) {
			++i;
			if (i < argc) {
				if (strlen(argv[i]) >= FULFNLEN) {
					fprintf(stderr, "Filename too long: %s\n", argv[i]);
					exit(1);
				}
				strcpy(logfn, argv[i]);
			}
			else usage(argv[0]);
		}
		else if (!strcmp(argv[i], "-port")) {
			++i;
			if (i < argc) hb_port = atoi(argv[i]);
			else usage(argv[0]);
		}
		else if (!strcmp(argv[i], "-admin")) {
			++i;
			if (pwe) {
				fprintf(stderr, "Not run as root\n");
				exit(1);
			}
			if (i < argc) {
				if (strlen(argv[i]) > MAXUSERLEN) {
					fprintf(stderr, "Username too long: %s\n", argv[i]);
					exit(1);
				}
				strcpy(dbe_user, argv[i]);
			}
			else usage(argv[0]);
		}
		else usage(argv[0]);
	}
	if (hb_port <= 0) hb_port = HB_PORT;
	if (!pwe) {
		if (!dbe_user[0]) {
			fprintf(stderr, "Admin user must be specified with -admin\n", dbe_user);
			exit(1);
		}
		pwe = getpwnam(dbe_user);
		if (pwe) { hb_uid = hb_euid = pwe->pw_uid; hb_gid = pwe->pw_gid; }
		else {
			fprintf(stderr, "Unknow user: %s\n", dbe_user);
			exit(1);
		}
	}
	if (logfn[0]) {
		if (logfn[0] != '/') {
			char tmpfn[1024];
			if (strlen(logfn) + strlen(pwe->pw_dir) >= 1024) {
				fprintf(stderr, "Filename too long: %s/%s\n", pwe->pw_dir, logfn);
				exit(1);
			}
			sprintf(tmpfn, "%s/%s", pwe->pw_dir, logfn);
			strcpy(logfn, tmpfn);
		}
		logfp = fopen(logfn, "a");
		if (!logfp) {
			fprintf(stderr, "%s: %s\n", logfn, strerror(errno));
			exit(1);
		}
		else setbuf(logfp, NULL);
	}
	else openlog("HB*Engine", 0, LOG_USER);

	sprintf(progid,_progid, "UNIX/Linux", MAXUSERS);
	fprintf(stderr, "%s\n", progid);
	fprintf(stderr, "Copyright (C) 1997-2019 Kimberlite Software. All rights reserved.\n");

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Cannot start process\n");
		exit(1);
	}
	if (pid > 0) exit(0);

#ifndef BSD
	setpgrp();
#else
	setpgrp(getpid(), 0);
#endif
	umask(027);
	shm_id = shmget(IPC_PRIVATE, sizeof(pthread_mutex_t) + (MAXUSERS + 1) * sizeof(struct hb_se), IPC_CREAT | 0660);
	if (shm_id < 0) {
		log_error("Failed to get shared memory");
		exit(1);
	}
	shm = shmat(shm_id, NULL, 0);
	if (shm == (void *)-1) {
		log_error("Failed to attach shared memory");
		exit(1);
	}
	shm_mutex = (pthread_mutex_t *)shm;
	hb_session = (struct hb_se *)(shm + sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&shm_attr);
	pthread_mutexattr_setpshared(&shm_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(shm_mutex, &shm_attr);

	for (i=0; i<=MAXUSERS; ++i) hb_session_reset(i);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		log_error("stream socket open");
		exit(1);
		}
	opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(int)) < 0) {
		close(sock);
		log_error("set stream socket option");
		exit(1);
	}
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(hb_port);
	length = sizeof(server);
	if (bind(sock, (struct sockaddr *)&server, length)<0) {
		close(sock);
		log_error("stream socket bind");
		exit(1);
		}
	if (getsockname(sock, (struct sockaddr *)&server, &length)<0) {
		close(sock);
		log_error("get stream socket name");
		exit(1);
		}

	if (server.sin_port != htons(hb_port)) {
		close(sock);
		fprintf(stderr,"Bound to wrong address\n");
		exit(1);
		}

	fprintf(stderr, "Listening for connections at port %d as %s\n", hb_port, dbe_user);
	log_printf("[%d]Listening for connections at port %d as %s\n", getpid(), hb_port, dbe_user);

	/* make sure that stdin, stdout an stderr don't stuff things up (library functions, for example) */
	for (i=0; i<3; i++) {
		close(i);
		open("/dev/null", O_RDWR);
	}
	/* catch signals */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, sig_term2);
	signal(SIGTERM, sig_term2);
	signal(SIGTSTP, SIG_IGN);
	old_sig_child = signal(SIGCHLD, sig_child);

	listen(sock, 5);

	int mutex_locked = 0;
	for (;;) {
		int conn_fh;
		struct sockaddr_in peer;

		length = sizeof(peer);
retry:
		conn_fh = accept(sock, (struct sockaddr *)&peer, &length);
		if (conn_fh < 0) {
			if (errno==EINTR) goto retry;
			log_error("stream socket accept");
			break;
			}
		opt = 1;
		if (setsockopt(conn_fh, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(int)) < 0) {
			log_error("set stream socket option");
			break;
			}

		pthread_mutex_lock(shm_mutex);
		mutex_locked = 1;
		for (i=0; i<MAXUSERS; ++i) if (hb_session[i].connection == -1) break;
		if (i >= MAXUSERS) {
			log_error("maximum connections reached");
			close(conn_fh);
			break;
			}
		memset(&hb_session[i].client, 0, sizeof(struct _client));
		hb_session[i].connection = conn_fh;
		memcpy(&hb_session[i].client.peer, &peer, sizeof(peer));
		hb_session[i].status = 0;
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, hb_session[i].sfd) < 0) {
			log_error("ipc create");
			break;
			}
		pthread_mutex_unlock(shm_mutex);
		mutex_locked = 0;

		if (!(pid = fork())) {
			hb_session[i].pid = getpid();
			srand(hb_session[i].pid);
			signal(SIGCHLD, old_sig_child);
			signal(SIGINT, sig_term);
			signal(SIGTERM, sig_term);
			hb_do_session(i);
			}
		else if (pid == -1) {
			fprintf(stderr,"Error spawning new process, error code=%d\n", errno);
			close(conn_fh);
			break;
			}
		else {
			hb_session[i].pid = pid;
			}

		}
	if (mutex_locked) pthread_mutex_unlock(shm_mutex);
	pthread_mutexattr_destroy(&shm_attr);
	pthread_mutex_destroy(shm_mutex);
	shmdt(shm);
	shmctl(shm_id, IPC_RMID, NULL);
	exit(1);
}

#define SENDERR		"Sending data to client"
#define RCVERR		"Receiving data from client"

int tli_send(int s, BYTE *buf, int len)
{
	int rc = send(s, buf, len, 0);
	if (rc < 0) {
		log_error("stream socket write");
	}
	if (rc > 0 && rc != len) {
		log_error("##### rc!=len");
	}
	return(rc);
}

int tli_recv(int s, BYTE *buf, int len)
{
	if (wait_fd(s, VTIMEOUT) <= 0) {	/** 300 secs timeout here **/
		hb_end_session();
		log_error("stream socket timeout during read");
		return(-1);
	}
	len = recv(s, buf, len, 0);
	if (len < 0) {
		log_error("stream socket read");
	}
	return(len);
}

int dbesrv_dispatch(int *conn_fh, BYTE *t_buf)
{
	int len, xlen, special;
	char *tp;
	HBREGS hb_regs;
	packet2regs(t_buf, &hb_regs, 1);
	special = hb_regs.ax == DBE_LOGIN;
/*
printf("dbesrv_dispatch, &hb_regs=%lx, ax=%x, cx=%x, dx=%lx, ex=%x\n",&hb_regs,hb_regs.ax,hb_regs.cx,hb_regs.dx,hb_regs.ex);
*/
	if (dbesrv(&hb_regs)) {
		regs2packet(&hb_regs, HBERROR, t_buf);
		if (hb_regs.cx > 0) memcpy(t_buf + DATA_OFFSET, hb_regs.bx, hb_regs.cx);
/*
printf("dbesrv error! ax=%x, cx=%x, dx=%lx, ex=%x\n",hb_regs.ax,hb_regs.cx,hb_regs.dx,hb_regs.ex);
*/
		if (tli_send(*conn_fh, t_buf, hb_regs.cx + PKTHDRSZ) == -1) {
			log_error(SENDERR);
			return(-1);
		}
		if (special) return(1);	/** go into disconnect **/
	}
	else {
/*
printf("return from dbesrv(), ax=%x, cx=%x, dx=%lx, ex=%x\n",hb_regs.ax,hb_regs.cx,hb_regs.dx,hb_regs.ex);
*/
		if (hb_regs.ax == DBE_LOGOUT && !hb_regs.ex) {	/** from client, end session. **/
			usr_conn = NULL; //disable usr calls
			return(1);
		}
		regs2packet(&hb_regs, HBNORMAL, t_buf);
		if (hb_regs.cx >0) {
			int more = 0;
			tp = hb_regs.bx;
			xlen = hb_regs.cx;
			while (xlen > 0) {
				len = xlen > TRANSBUFSZ? TRANSBUFSZ : xlen;
				memcpy(t_buf + DATA_OFFSET, tp, len);
				if (more) PKT_CX_PUT(t_buf, len);
				PKT_FLAGS_PUT(t_buf, UPSTREAM);
				if (tli_send(*conn_fh, t_buf, len + PKTHDRSZ) == -1) {
					log_error(SENDERR);
					return(-1);
				}
				tp += len;
				xlen -= len;
				more = 1;
			}
		}
		else {
			xlen = -hb_regs.cx;
			if (xlen) tp = hb_regs.bx;	/* setup transfer address */
			if (tli_send(*conn_fh, t_buf, PKTHDRSZ) == -1) {
				log_error(SENDERR);
				return(-1);
			}
			while (xlen > 0) {
				int len2;
				len = tli_recv(*conn_fh, t_buf, (xlen>TRANSBUFSZ? TRANSBUFSZ : xlen) + PKTHDRSZ);
				if (len == -1) {
					log_error(RCVERR);
					return(-1);
				}
				len2 = PKT_CX_GET(t_buf);
				if (len < PKTHDRSZ + len2 || PKT_FLAGS_GET(t_buf) != HBNORMAL
					|| PKT_AX_GET(t_buf) != DOWNSTREAM) return(-1);
				memcpy(tp, t_buf + DATA_OFFSET, len2);
				tp += len2;
				xlen -= len2;
			}
		}
	}
	PKT_FLAGS_PUT(t_buf, HBNORMAL);
	return(hb_regs.ax == DBE_LOGOUT? 1 : 0);
}

int usr_tli(void)
{
	char t_buf[TLIPACKETSIZE];
	int rc, len;
	if (!usr_conn) return(-1);
	regs2packet(&usr_regs, USRNORMAL, t_buf);
	if (usr_regs.cx) memcpy(t_buf + DATA_OFFSET, usr_regs.bx, usr_regs.cx);
	/* usr_regs.cx must not exceed TRANSBUFSZ, currently 0 always */
/*
printf("usr call, &t_buf=%lx, ax=%x, cx=%x, dx=%lx, ex=%x\n",&t_buf,usr_regs.ax,usr_regs.cx,usr_regs.dx,usr_regs.ex);
*/
	if (tli_send(*usr_conn, t_buf, PKTHDRSZ + usr_regs.cx) == -1) {
		usr_regs.ax = BAD_COMMUNICATION;
		return(-1);
	}
	do {
		len = tli_recv(*usr_conn, t_buf, TLIPACKETSIZE);
		if (len == -1 || len != PKT_CX_GET(t_buf) + PKTHDRSZ) {
			usr_regs.ax = BAD_COMMUNICATION;
			return(-1);
		}
		if (PKT_FLAGS_GET(t_buf) == HBNORMAL) {
			if (rc = dbesrv_dispatch(usr_conn, t_buf)) {
				if (rc < 0) {
					usr_regs.ax = BAD_COMMUNICATION;
					return(-1);
				}
				hb_shutdown();
			}
		}
	} while (PKT_FLAGS_GET(t_buf) == HBNORMAL);
	packet2regs(t_buf, &usr_regs, 1);
	if (usr_regs.cx) usr_regs.bx = t_buf+DATA_OFFSET;
/*
printf("usr return, ax=%x, cx=%x, dx=%lx, ex=%x\n",usr_regs.ax,usr_regs.cx,usr_regs.dx,usr_regs.ex);
*/
	return(PKT_FLAGS_GET(t_buf) != USRNORMAL);
}

void hb_do_session(int i)
{
	struct hb_se *pse = &hb_session[i];
	int rc, len, *conn_fh;
	char t_buf[TLIPACKETSIZE];

	log_printf("[%d]Connection #%d accepted\n", pse->pid, pse->connection);
	/** *pse->connection may change due to sleep and wake-up **/
	conn_fh = &pse->connection;
	while ((len = tli_recv(*conn_fh, t_buf, TLIPACKETSIZE)) >= 0) {
		if (i == MAXUSERS || len != PKT_CX_GET(t_buf) + PKTHDRSZ
			|| PKT_FLAGS_GET(t_buf) != HBNORMAL) {
			PKT_FLAGS_PUT(t_buf, HBERROR);
			PKT_AX_PUT(t_buf, BAD_COMMUNICATION);
			PKT_CX_PUT(t_buf, 0);
			if (tli_send(*conn_fh, t_buf, PKTHDRSZ) == -1) log_error(SENDERR);
			break;
		}
		if (PKT_AX_GET(t_buf) == DBE_LOGIN) usr_conn = &pse->connection; //needed for client authentication
		else if (PKT_AX_GET(t_buf) == DBE_INST_USR) {
			usr_conn = &pse->connection;
			PKT_CX_PUT(t_buf, 0);
			if (tli_send(*conn_fh, t_buf, PKTHDRSZ) == -1) {
				log_error(SENDERR);
				break;
			}
			continue;
		}
		if (rc = dbesrv_dispatch(conn_fh, t_buf)) {
			if (rc < 0) hb_end_session();
			break;
		}
	}
	hb_shutdown();
}

int hb_authenticate(char *epasswd)
{
	usr_regs.ax = USR_AUTH;
	usr_regs.bx = epasswd;
	usr_regs.cx = strlen(epasswd) + 1;
	if (usr_tli()) return(-1);
	return(usr_regs.ax);
}
