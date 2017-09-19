/* static char *inet_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		inet.h	Ver4.70"; */
/* ========================================================================
	Project  Name			: Internet access
	Create					: 2017-08-10(Thr)
	Update					: 2017-08-10(Thr)
	Copyright				: H.Shirouzu
	======================================================================== */
#ifndef INET_H
#define INET_H

#include <wininet.h>

#define INETREQ_GET		0x0001
#define INETREQ_SECURE	0x0002
#define INETREQ_NOCERT	0x0004

#define MAX_URLBUF	16384

struct InetReqReply {
	int64	id;
	DWORD	code;
	DynBuf	reply;
	U8str	errMsg;
};

struct InetReqParam { // 内部利用
	U8str	host;
	U8str	path;
	DynBuf	data;
	DWORD	flags;

	int64	id;
	HWND	hWnd;
	UINT	uMsg;
};

DWORD InetRequest(LPCSTR host, LPCSTR path, BYTE *data, int data_len, DynBuf *reply,
	U8str *errMsg, DWORD flags=0);
void InetRequestAsync(LPCSTR host, LPCSTR path, BYTE *data, int data_len,
	HWND hWnd, UINT uMsg, int64 id=0, DWORD flags=0);

void InetAsync(LPCSTR host, LPCSTR path, HWND hWnd, UINT uMsg, int64 id=0);

void SlackMakeJson(LPCSTR chan, LPCSTR _user, LPCSTR _body, LPCSTR _icon, U8str *json);
BOOL SlackRequest(LPCSTR host, LPCSTR path, LPCSTR json, DynBuf *reply, U8str *errMsg);
void SlackRequestAsync(LPCSTR _host, LPCSTR _path, LPCSTR _json, HWND hWnd, UINT uMsg, int64 id=0);

BOOL CrackUrl(LPCSTR url, U8str *host, U8str *path, int *port, BOOL *is_sec);

#endif

