/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbcmd/hbscrn.c
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
#ifdef HBSCRN_CURSES
#include <curses.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#endif
#include <stdarg.h>
#define MAXLINELEN	2048

#ifdef HBSCRN_CURSES
#define EOL	'\n'
#else
static int            terminal_descriptor = -1;
static struct termios terminal_original;
static struct termios terminal_settings;
static const char *CLEAR_SCREEN_ANSI = "\e[1;1H\e[2J";

void clr_scr(void)
{
	if (terminal_descriptor != -1)
		write(terminal_descriptor, CLEAR_SCREEN_ANSI, 10);
}
 
/* Restore terminal to original settings
*/
static void terminal_done(void)
{
	if (terminal_descriptor != -1)
		tcsetattr(terminal_descriptor, TCSANOW, &terminal_original);
}
 
/* "Default" signal handler: restore terminal, then exit.
*/
static void terminal_signal(int signum)
{
	if (terminal_descriptor != -1)
		tcsetattr(terminal_descriptor, TCSANOW, &terminal_original);
}
 
/* Initialize terminal for non-canonical, non-echo mode,
 * that should be compatible with standard C I/O.
 * Returns 0 if success, nonzero errno otherwise.
*/
static int terminal_init(void)
{
	struct sigaction act;
	
	/* Already initialized? */
	if (terminal_descriptor != -1)
	return errno = 0;
	
	/* Which standard stream is connected to our TTY? */
	if (isatty(STDERR_FILENO))
		terminal_descriptor = STDERR_FILENO;
	else if (isatty(STDIN_FILENO))
		terminal_descriptor = STDIN_FILENO;
	else if (isatty(STDOUT_FILENO))
		terminal_descriptor = STDOUT_FILENO;
	else
		return errno = ENOTTY;
	
	/* Obtain terminal settings. */
	if (tcgetattr(terminal_descriptor, &terminal_original) ||
		tcgetattr(terminal_descriptor, &terminal_settings))
		return errno = ENOTSUP;
	
	/* Disable buffering for terminal streams. */
	if (isatty(STDIN_FILENO))
		setvbuf(stdin, NULL, _IONBF, 0);
	if (isatty(STDOUT_FILENO))
		setvbuf(stdout, NULL, _IONBF, 0);
	if (isatty(STDERR_FILENO))
		setvbuf(stderr, NULL, _IONBF, 0);
	
	/* At exit() or return from main(),
	* restore the original settings. */
	if (atexit(terminal_done))
		return errno = ENOTSUP;
	
	/* Set new "default" handlers for typical signals,
	* so that if this process is killed by a signal,
	* the terminal settings will still be restored first. */
	sigemptyset(&act.sa_mask);
	act.sa_handler = terminal_signal;
	act.sa_flags = 0;
	if (sigaction(SIGHUP,  &act, NULL) ||
		sigaction(SIGINT,  &act, NULL) ||
		sigaction(SIGQUIT, &act, NULL) ||
		sigaction(SIGTERM, &act, NULL) ||
#ifdef SIGXCPU
		sigaction(SIGXCPU, &act, NULL) ||
#endif
#ifdef SIGXFSZ    
		sigaction(SIGXFSZ, &act, NULL) ||
#endif
#ifdef SIGIO
		sigaction(SIGIO,   &act, NULL) ||
#endif
		sigaction(SIGPIPE, &act, NULL) ||
		sigaction(SIGALRM, &act, NULL))
			return errno = ENOTSUP;

	/* Let BREAK cause a SIGINT in input. */
	terminal_settings.c_iflag &= ~IGNBRK;
	terminal_settings.c_iflag |=  BRKINT;
	
	/* Ignore framing and parity errors in input. */
	terminal_settings.c_iflag |=  IGNPAR;
	terminal_settings.c_iflag &= ~PARMRK;
	
	/* Do not strip eighth bit on input. */
	terminal_settings.c_iflag &= ~ISTRIP;
	
	/* Do not do newline translation on input. */
	terminal_settings.c_iflag &= ~(INLCR | IGNCR | ICRNL);
	
	#ifdef IUCLC
	/* Do not do uppercase-to-lowercase mapping on input. */
	terminal_settings.c_iflag &= ~IUCLC;
	#endif

	/* Use 8-bit characters. This too may affect standard streams,
	* but any sane C library can deal with 8-bit characters. */
	terminal_settings.c_cflag &= ~CSIZE;
	terminal_settings.c_cflag |=  CS8;
	
	/* Enable receiver. */
	terminal_settings.c_cflag |=  CREAD;
	
	/* Let INTR/QUIT/SUSP/DSUSP generate the corresponding signals. */
	terminal_settings.c_lflag |=  ISIG;
	
	/* Enable noncanonical mode.
	* This is the most important bit, as it disables line buffering etc. */
	terminal_settings.c_lflag &= ~ICANON;
	
	/* Disable echoing input characters. */
	terminal_settings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	
	/* Disable implementation-defined input processing. */
	terminal_settings.c_lflag &= ~IEXTEN;
	
	/* To maintain best compatibility with normal behaviour of terminals,
	* we set TIME=0 and MAX=1 in noncanonical mode. This means that
	* read() will block until at least one byte is available. */
	terminal_settings.c_cc[VTIME] = 0;
	terminal_settings.c_cc[VMIN] = 1;
	
	/* Set the new terminal settings.
	* Note that we don't actually check which ones were successfully
	* set and which not, because there isn't much we can do about it. */
	tcsetattr(terminal_descriptor, TCSANOW, &terminal_settings);
	
	/* Done. */
	return errno = 0;
}

static void noecho(void)
{
	terminal_settings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	tcsetattr(terminal_descriptor, TCSAFLUSH, &terminal_settings);
}

static int timeout(int dsecs)
{
	int old_timeout = terminal_settings.c_cc[VTIME];
	if (old_timeout == 0 && terminal_settings.c_cc[VMIN]) old_timeout = -1;
	if (dsecs < 0) {
		terminal_settings.c_cc[VTIME] = 0;
		terminal_settings.c_cc[VMIN] = 1;
	}
	else {
		terminal_settings.c_cc[VTIME] = dsecs;
		terminal_settings.c_cc[VMIN] = 0;
	}
	tcsetattr(terminal_descriptor, TCSANOW, &terminal_settings);
	return(old_timeout);
}

static void refresh(void)
{
}

#define getch()	getchar()
#define ERR	EOF
#define EOL	'\r'
#endif

void hbscrn_end(void)
{
#ifdef HBSCRN_CURSES
	refresh();
	endwin();
#else
	terminal_done();
#endif
}

void hbscrn_init(void)
{
#ifdef HBSCRN_CURSES
	initscr();
	cbreak();
	block_breaks();
	setbuf(stdout, (char *)NULL);
	atexit(hbscrn_end);
	refresh();
#else
	terminal_init();
	block_breaks();
	clr_scr();
#endif
}

int hbscrn_nrows(void)
{
#ifdef HBSCRN_CURSES
	int nh, nw;
	getmaxyx(stdstr, nh, nw);
	return(nh);
#else
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	return(w.ws_row);
#endif
}

int hbscrn_ncols(void)
{
#ifdef HBSCRN_CURSES
	int nh, nw;
	getmaxyx(stdstr, nh, nw);
	return(nw);
#else
	struct winsize w;
	ioctl(0, TIOCGWINSZ, &w);
	return(w.ws_col);
#endif
}

void hbscrn_puts(char *str)
{
	char *tp;
	for (tp=str; *tp; ++tp) {
		if (*tp=='\n') putchar('\r');
		putchar(*tp);
	}
	refresh();
}

void hbscrn_printf(char *fmt, ...)
{
	char tmp[MAXLINELEN];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, MAXLINELEN, fmt, ap);
	hbscrn_puts(tmp);
}

#ifdef HBSCRN_CURSES
#define HBSCRN_TIMEOUT	24000 // 24 secs in msecs
#else
#define HBSCRN_TIMEOUT	240   // 24 secs in tenth of a sec NOTE: must be <= 255 to fit unsigned char
#endif
#define BACKSPACE	0x08
#define BK_DELETE	0x7f

#define ESC_KEY		0x1b

void hbscrn_clr_input(void)
{
	char c;
	int old_timeout = timeout(0);
	while ((c = getch()) != ERR);
	timeout(old_timeout);
}

#define HIST_MAX	100

static char *hist_list[HIST_MAX] = { NULL };
static int hist_next = -1, hist_last = -1;

static char *hist_next_entry(void)
{
	if (hist_next < 0) hist_next = 0;
	if (hist_next + 1 >= HIST_MAX || hist_list[hist_next + 1] == NULL) return("");
	return(hist_list[++hist_next]);
}

static char *hist_prev_entry(void)
{
	if (hist_next < 0) return(NULL);
	return(hist_list[hist_next--]);
}

static void hist_add_entry(char *str)
{
	char *ep = malloc(strlen(str) + 1);
	if (ep) {
		strcpy(ep, str);
		if (hist_last + 1 >= HIST_MAX) {
			int i;
			free(hist_list[0]);
			for (i=0; i<HIST_MAX-1; ++i) hist_list[i] = hist_list[i+1];
		}
		else ++hist_last;
		hist_list[hist_next = hist_last] = ep;
	}
}

void hist_free_entries(void)
{
	int i;
	for (i=0; i<HIST_MAX; ++i) if (hist_list[i]) {
		free(hist_list[i]);
		hist_list[i] = NULL;
	}
}

/* UTF-8 : BYTE_BITS*/

/* B0_BYTE : 0XXXXXXX */

/* B1_BYTE : 10XXXXXX */

/* B2_BYTE : 110XXXXX */

/* B3_BYTE : 1110XXXX */

/* B4_BYTE : 11110XXX */

/* B5_BYTE : 111110XX */

/* B6_BYTE : 1111110X */

#define B0_BYTE 0x00

#define B1_BYTE 0x80

#define B2_BYTE 0xC0

#define B3_BYTE 0xE0

#define B4_BYTE 0xF0

#define B5_BYTE 0xF8

#define B6_BYTE 0xFC

#define B7_BYTE 0xFE

int utf8_bytes(char *tp)
{
	if ((*tp & B1_BYTE) == B0_BYTE) return(1);
	if ((*tp & B7_BYTE) == B6_BYTE) return(6);
	if ((*tp & B6_BYTE) == B5_BYTE) return(5);
	if ((*tp & B5_BYTE) == B4_BYTE) return(4);
	if ((*tp & B4_BYTE) == B3_BYTE) return(3);
	if ((*tp & B3_BYTE) == B2_BYTE) return(2);
	return(-1);
}

int utf8_disp_bytes(char *tp)
{
	int nbytes = utf8_bytes(tp);
	if (nbytes <= 0) return(-1);
	if (nbytes <= 2) return(1);
	return(2);
}

int utf8_disp_count(char *s, char *tp)
{
	int count = 0;
	while (s < tp && *s) {
		int nb = utf8_bytes(s);
		int ndb = utf8_disp_bytes(s);
		s += nb;
		count += ndb;
	}
	return(count);
}

char *utf8_next(char *tp)
{
	int nbytes = utf8_bytes(tp);
	if (nbytes <= 0) return(NULL);
	return(tp +nbytes);
}

char *utf8_prev(char *s, char *tp)
{
	char *tp2, *t;
	if (!s || tp == s) return(NULL);
	for (t = s, tp2 = s; tp2 != tp  && *tp2; t = tp2, tp2 = utf8_next(tp2));
	if (tp2 == tp) return(t);
	return(NULL);
}

int hbscrn_gets(char *str, int len, int do_timeout)
{
	char c = 0, *tp, *ep, *ptp;
	int len2, i, nb, ndb;
	noecho();
	if (do_timeout) {
		tp = str + strlen(str); //str must be null terminated
		len -= (int)(tp - str);
		timeout(HBSCRN_TIMEOUT);
	}
	else {
		tp = str;
		timeout(-1); //block indefinitely
	}
	memset(tp, 0, len);
	while (len > 1 && (c > 0 || (c = getch()))) {
		if (c == ERR) { //timeout occurred
			while (*tp) putchar(*tp++); //move to end-of-line
			return(1);
		}
		if (c == EOL) {
			tp += strlen(tp);
			break;
		}
		if (c == BACKSPACE || c == BK_DELETE) { /* backspace */
			if (tp == str) {
				c = 0;
				continue;
			}
			len2 = strlen(tp);
			ptp = utf8_prev(str, tp);
			if (!ptp) return(-1);
			nb = utf8_bytes(ptp), ndb = utf8_disp_bytes(ptp);
			if (nb < 0 || ndb < 0) return(-1);
			for (i=0; i<ndb; ++i) putchar(BACKSPACE);
			for (i=0; i<len2; ++i) {
				ptp[i] = tp[i];
				putchar(tp[i]);
			}
			ptp[i] = '\0';
			for (i=0; i<ndb; ++i) putchar(' '); //clear the tail
			for (i=0; i<ndb; ++i) putchar(BACKSPACE);
			ndb = utf8_disp_count(ptp, ptp + strlen(ptp));
			for (i=0; i<ndb; ++i) putchar(BACKSPACE);
			refresh();
			tp -= nb; len += nb;
		}
		else if (c == ESC_KEY) {
			int old_timeout = timeout(0);
			char c2 = getch();
			if (c2 == '[') { //escape sequences we are interested in
				int c3 = getch();
				if (c3 == 'D') { //left arrow
					if (tp > str) {
						ptp = utf8_prev(str, tp);
						ndb = utf8_disp_bytes(ptp);
						for (i=0; i<ndb; ++i) putchar(BACKSPACE);
						tp = ptp;
					}
				}
				else if (c3 == 'C') { //right arrow
					if (*tp) {
						nb = utf8_bytes(tp);
						for (i=0; i<nb; ++i) putchar(*tp++);
					}
				}
				else if (c3 == 'A' || c3 == 'B') { //up or down arrow
					char *ep = c3 == 'A'? hist_prev_entry() : hist_next_entry();
					if (ep) {
						ndb = utf8_disp_count(str, tp);
						for (i=0; i<ndb; ++i) putchar(BACKSPACE); //move cursor to beginning of line
						ndb = utf8_disp_count(str, str + strlen(str));
						for (i=0; i<ndb; ++i) putchar(' '); //clear the line
						for (i=0; i<ndb; ++i) putchar(BACKSPACE);
						len += strlen(str);
						strcpy(str, ep);
						len -= strlen(str);
						hbscrn_puts(str);
						tp = str + strlen(str);
						memset(tp, 0, len);
					}
				}
				else if (c3 == '1' || c3 == 'H') { //home
					nb = utf8_disp_count(str, tp);
					while (nb--) putchar(BACKSPACE);
					tp = str;
				}
				else if (c3 == '4' || c3 == 'F') { //end
					while (*tp) putchar(*tp++);
				}
				else if (c3 == '3') { //delete
					if (*tp) {
						nb = utf8_bytes(tp);
						for (i=0; i<nb; ++i) putchar(tp[i]);
						tp += nb;
						c = BACKSPACE;
					}
				}
				//else { putchar(c2); putchar(c3); c = 0; }
				else c = 0;
			}
			else c = 0;
			timeout(old_timeout);
			hbscrn_clr_input();
			continue;
		}
		else {
			len2 = strlen(tp);
			for (i=len2+1; i>0; --i) tp[i] = tp[i-1];
			*tp++ = c;
			putchar(c);
			for (i=0; i<len2; ++i) putchar(tp[i]);
			for (i=0; i<len2; ++i) putchar(BACKSPACE);
			refresh();
			--len;
		}
		c = 0;
	}
	if (strlen(str) > 0) hist_add_entry(str);
	putchar('\r');
	putchar('\n');
	refresh();
	return(0);
}

int hbscrn_getpasswd(char *passwd, int len)
{
	char c, *tp = passwd;
	noecho();
	timeout(HBSCRN_TIMEOUT);
	hbscrn_puts("Password: ");
	while (len > 1 && ((c = getch()) != EOL)) {
		if (c == ERR) {
			*tp == '\0';
			return(1);
		}
		if ((c == 0x08 || c == 0x7f) && tp > passwd) { /* backspace */
			putchar(c);
			putchar(' ');
			putchar(c);
			refresh();
			--tp; ++len;
		}
		else if (!isprint(c)) continue;
		else {
			*tp++ = c;
			putchar('*');
			refresh();
			--len;
		}
	}
	*tp = '\0';
	putchar('\r');
	putchar('\n');
	refresh();
	return(0);
}
	
char hbscrn_wait(char *str, int time_out)
{
	char c;
	int old_timeout, n;
	noecho();
	hbscrn_printf("%s", str);
	if (time_out >= 0) {
		n = time_out / HBSCRN_TIMEOUT;
		old_timeout = timeout(HBSCRN_TIMEOUT);
		do {
			c = getch();
		} while (n-- > 0 && c == ERR);
	}
	else {
		old_timeout = timeout(-1);
		c = getch();
	}
	timeout(old_timeout);
	if (c == ERR) {
		hbscrn_printf("timed out\n");
		return(-1);
	}
	hbscrn_printf("\r%*s\r", strlen(str), "");
	return(c);
}

void display_msg(char *msg, char *msg2)
{
	hbscrn_printf("%s", msg);
	if (msg2) hbscrn_printf("\n%s", msg2);
	hbscrn_printf("\n");
}

int hb_yesno(char *what)
{
	char c;
	hbscrn_printf("%s? (Y/N)", what);
	refresh();
	noecho();
	c = getch();
	if (c == 'Y' || c == 'y') {
		hbscrn_puts("Y\n");
		refresh();
		return(1);
	}
	hbscrn_puts("N\n");
	refresh();
	return(0);
}

int hb_chk_escape(void)
{
#ifdef HBSCRN_CURSES
	return(0);
#else
	int escape = 0;
	int old_timeout = timeout(0);
	char c;
	while ((c = getch()) != ERR) if (c == ESC_KEY) escape = 1;
	timeout(old_timeout);
	if (escape) hbscrn_puts("<ESC>\n");
	return(escape);
#endif
}
