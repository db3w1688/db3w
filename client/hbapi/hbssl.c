/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name - hbapi/hbssl.c
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

#ifdef _WINDOWS
#include <winsock.h>
#else
#define CALLBACK
#define MS_CALLBACK
#define FAR
#define LPSTR char *
#endif
#ifdef HBSSL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define USE_SOCKETS
#include "apps.h"
#include "x509.h"
#include "ssl.h"
#include "err.h"
#include "pem.h"
#include "s_apps.h"

#include "hbapi.h"

#define DEFA_DEPTH	4

extern int verify_depth;
extern int verify_error;

void CALLBACK HBSSLEnd( HBSSLctx )
HBSSL_CTX HBSSLctx;
{
	SSL_CTX_free( ( SSL_CTX * )HBSSLctx );
}

void hbssl_end()
{
	if ( bio_err ) BIO_free( bio_err );
	bio_err = NULL;
}

static void MS_CALLBACK client_info_callback(s,where,ret)
SSL *s;
int where;
int ret;
{
#ifdef DEBUG
	if (where == SSL_CB_CONNECT_LOOP) {
		display_msg(SSL_state_string(s));
	}
	else 
#endif
	if (where == SSL_CB_CONNECT_EXIT) {
		if (ret == 0)
			display_msg(SSL_state_string(s));
		else if (ret < 0)
			display_msg(SSL_state_string(s));
		}
}

extern void _far _pascal HBMonPutStr();

static int MS_CALLBACK monitor_write(b,buf,len)
BIO *b;
char *buf;
int len;
{
	char *str,*tp;
	for (str=buf; str-buf<len; str=tp+1) {
		int newline=0;
		for (tp=str; *tp && *tp!='\n' && tp-buf<len; ++tp);
		if (*tp=='\n') newline = 1;
		*tp = '\0';
		HBMonPutStr(str);
		if (newline) HBMonPutStr("\r\n");
	}
	return(len);
}

HBSSL_CTX CALLBACK HBSSLInit( keyfile, certfile, CAfile, CApath, cipher, passwd_cb, verify_cb )
char *keyfile,*certfile,*CAfile,*CApath,*cipher;
int (*passwd_cb)(),(*verify_cb)();
{
	SSL_CTX *ctx;

	apps_startup();

	if (bio_err == NULL) {
		BIO_METHOD *bio;
		/** bio_err is never used for input, output is to the monitor window **/
		bio = BIO_s_file();
		if (bio) bio->bwrite = monitor_write;
		bio_err = BIO_new(bio);
		if (!bio_err) {
			display_msg("Cannot allocate BIO structure.");
			return(NULL);
		}
		bio_err->init = 1;
	}

	ctx = SSL_CTX_new();
	if (ctx == NULL) {
		display_msg("Cannot allocate SSL Context structure.");
		return(NULL);
	}
	verify_depth = DEFA_DEPTH;
	verify_error = VERIFY_OK;
	SSL_CTX_set_info_callback(ctx,client_info_callback);
	SSL_CTX_set_cipher_list(ctx,cipher);
	SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,verify_cb? verify_cb:verify_callback);
	SSL_CTX_set_default_passwd_cb(ctx,passwd_cb);
	if (!set_cert_stuff(ctx,certfile,keyfile)) {
		display_msg("Cannot use specified certificate.");
		return(NULL);
	}
	SSL_load_error_strings();
	if ((!SSL_load_verify_locations(ctx,CAfile,CApath)) || (!SSL_set_default_verify_paths(ctx))) {
		BIO_printf(bio_err,"error seting default verify locations\n");
		ERR_print_errors(bio_err);
		SSL_CTX_free(ctx);
		return(NULL);
		}

	BIO_printf(bio_err,"SSL initialized\n");
	return((HBSSL_CTX)ctx);
}

int hbssl_start(conn,s)
HBCONN conn;
int s;
{
	int rc;
	X509 *peer;

	SSL_set_fd((SSL *)conn,s);
	BIO_printf(bio_err,"Establishing SSL connection with server...\n");
	rc = SSL_connect((SSL *)conn);
	if (rc<=0) {
		BIO_printf(bio_err,"SSL_connect ERROR\n");
		if (verify_error != VERIFY_OK)
			BIO_printf(bio_err,"verify error: %s\n",X509_cert_verify_error_string(verify_error));
		else {
			BIO_printf(bio_err,"SSL_connect was in %s\n",SSL_state_string_long((SSL *)conn));
			ERR_print_errors(bio_err);
			BIO_printf(bio_err,"(%d) error=%d\n",rc,WSAGetLastError());
		}
		SHUTDOWN(SSL_get_fd((SSL *)conn));
		return(-1);
	}
	BIO_printf(bio_err,"SSL connection established.\n");
	peer = SSL_get_peer_certificate((SSL *)conn);
	if (peer != NULL) {
		char *str;
		BIO_printf(bio_err,"Server Certificate:\n");
		PEM_write_bio_X509(bio_err,peer);
		str = X509_NAME_oneline(X509_get_subject_name(peer));
		BIO_printf(bio_err,"subject=%s\n",str);
		Free(str);
		str = X509_NAME_oneline(X509_get_issuer_name(peer));
		BIO_printf(bio_err,"issuer=%s\n",str);
		Free(str);
		X509_free(peer);
	}
	SSL_SESSION_print(bio_err,SSL_get_session((SSL *)conn));
	return(0);
}

#else
#include <stdio.h>
#include "hbapi.h"

void CALLBACK HBSSLEnd( HBSSLctx )
HBSSL_CTX HBSSLctx;
{
/** dummy function **/
}

void hbssl_end()
{
}

HBSSL_CTX CALLBACK HBSSLInit( lpKeyfile, lpCertfile, lpCAfile, lpCApath, lpCipher, passwd_callback, verify_callback, client_info_callback )
LPSTR lpKeyfile, lpCertfile, lpCAfile, lpCApath, lpCipher;
int ( FAR *passwd_callback )(), ( FAR *verify_callback )(), ( FAR *client_info_callback )();
{
	return( NULL );
}
#endif
