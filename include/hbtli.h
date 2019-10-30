/*
 *  DB3W -- A database application development and deployment platform
 *  HB*Engine -- A 64-bit dBASE/xBase compatible database server
 *  HB*Commander -- A command line HB*Engine client
 *  DB3W.CGI -- A Web application delivery client for HB*Engine
 *
 *  Module Name -- include/hbtli.h
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

#define HB_PORT		1703		/* iana@isi.edu registered port number */
#define LOCALHOST	"127.0.0.1"	/* local host (self) */

/* session flags */
#define HB_SESS_FLG_SUID	0x01	//set uid
#define HB_SESS_FLG_AUTH	0x02	//do authentication
#define HB_SESS_FLG_MULTI	0x04	//allow multiple logins
#define HB_SESS_FLG_SYSPRG	0x08	//load _sys.prg
#define HB_SESS_FLG_SETAUTH 0x10    //already authenticated

/* TLI flags */
#define HBNORMAL	0
#define HBERROR		1
#define USRNORMAL	2
#define USRERROR	3

#define PKTHDRSZ	64 //large enough for HBREGS ax,bx,cx,dx,ex and future expansion

#define UPSTREAM	0xffd		/* return from dbesrv(), cx>0 */
#define DOWNSTREAM	0xffe		/* cx<0 situation */

#define TLIPACKETSIZE	4096

#define FLAGS_OFFSET	0	//32-bit
#define AX_OFFSET	4	//32-bit
#define CX_OFFSET	8	//32-bit
#define DX_OFFSET	12	//32-bit
#define EX_OFFSET	16	//32-bit
#define FX_OFFSET	20	//32-bit
#define DATA_OFFSET	PKTHDRSZ //where bx points to
#define TRANSBUFSZ	(TLIPACKETSIZE - PKTHDRSZ)

/* packet logical structure, for reference only. Physical layout is using above offsets
static struct tli_packet {
	uint32_t hb_flags;
	uint32_t hb_ax;
	int32_t hb_cx;
	uint32_t hb_dx;
	uint32_t hb_ex;
	char buf[TRANSBUFSZ];
};
*/
#define PKT_AX_GET(t_buf)	ntohl(get_int32(t_buf+AX_OFFSET))
#define PKT_CX_GET(t_buf)	ntohl(get_int32(t_buf+CX_OFFSET))
#define PKT_DX_GET(t_buf)	ntohl(get_int32(t_buf+DX_OFFSET))
#define PKT_EX_GET(t_buf)	ntohl(get_int32(t_buf+EX_OFFSET))
#define PKT_FX_GET(t_buf)	ntohl(get_int32(t_buf+FX_OFFSET))
#define PKT_FLAGS_GET(t_buf)	ntohl(get_int32(t_buf+FLAGS_OFFSET))

#define PKT_AX_PUT(t_buf,ax)	put_int32(t_buf+AX_OFFSET, htonl(ax))
#define PKT_CX_PUT(t_buf,cx)	put_int32(t_buf+CX_OFFSET, htonl(cx))
#define PKT_DX_PUT(t_buf,dx)	put_int32(t_buf+DX_OFFSET, htonl(dx))
#define PKT_EX_PUT(t_buf,ex)	put_int32(t_buf+EX_OFFSET, htonl(ex))
#define PKT_FX_PUT(t_buf,fx)	put_int32(t_buf+FX_OFFSET, htonl(fx))
#define PKT_FLAGS_PUT(t_buf,flags)	put_int32(t_buf+FLAGS_OFFSET, htonl(flags))

#define packet2regs(t_buf, regs, set_bx) \
{ \
	(regs)->ax = PKT_AX_GET(t_buf); \
	(regs)->cx = PKT_CX_GET(t_buf); \
	(regs)->dx = PKT_DX_GET(t_buf); \
	(regs)->ex = PKT_EX_GET(t_buf); \
	(regs)->fx = PKT_FX_GET(t_buf); \
	if (set_bx) (regs)->bx = (regs)->cx? t_buf+DATA_OFFSET : NULL; \
}

#define regs2packet(regs, flags, t_buf) \
{ \
	PKT_AX_PUT(t_buf, (regs)->ax); \
	PKT_CX_PUT(t_buf, (regs)->cx); \
	PKT_DX_PUT(t_buf, (regs)->dx); \
	PKT_EX_PUT(t_buf, (regs)->ex); \
	PKT_FX_PUT(t_buf, (regs)->fx); \
	PKT_FLAGS_PUT(t_buf, flags); \
}

