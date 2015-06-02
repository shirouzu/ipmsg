/*	@(#)Copyright (C) H.Shirouzu 2013-2014   logmng.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Log Manager
	Create					: 2013-03-03(Sun)
	Update					: 2014-04-14(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGMNG_H
#define LOGMNG_H

#define LOG_ENC0		0x0001
#define LOG_ENC1		0x0002
#define LOG_ENC2		0x0004
#define LOG_SIGN_OK		0x0008
#define LOG_SIGN_NOKEY	0x0010
#define LOG_SIGN_ERR	0x0020
#define LOG_UNAUTH		0x0040

class LogMng {
protected:
	Cfg		*cfg;
	BOOL	Write(LPCSTR str);

public:
	LogMng(Cfg *_cfg);

	BOOL	WriteSendStart(void);
	BOOL	WriteSendHead(LPCSTR head);
	BOOL	WriteSendMsg(LPCSTR msg, ULONG command, int opt, ShareInfo *shareInfo=NULL);
	BOOL	WriteRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo=NULL);
	BOOL	WriteStart(void);
	BOOL	WriteMsg(LPCSTR msg, ULONG command, int opt, ShareInfo *shareInfo=NULL);

static void StrictLogFile(char *path);
};

enum {	STI_NONE=0, STI_ENC, STI_SIGN, STI_FILE, STI_CLIP, STI_ENC_FILE, STI_ENC_CLIP,
		STI_SIGN_FILE, STI_SIGN_CLIP, STI_MAX };

#endif

