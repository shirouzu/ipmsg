/*	@(#)Copyright (C) H.Shirouzu 2013-2017   logmng.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Log Manager
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGMNG_H
#define LOGMNG_H

#define LOG_ENC0		0x0001
#define LOG_ENC1		0x0002
#define LOG_ENC2		0x0004
#define LOG_SIGN_OK		0x0008
#define LOG_SIGN2_OK	0x0100
#define LOG_SIGN_NOKEY	0x0010
#define LOG_SIGN_ERR	0x0020
#define LOG_UNAUTH		0x0040

#include "logdb.h"

enum CopyMsgFlags { HEADMSG_COPY=1, BODYMSG_COPY=2, ALLMSG_COPY=3, SELFHEAD_COPY=4,
	ALLMSGEX_COPY=7 };

class TMainWin;

class LogMng {
protected:
	Cfg			*cfg;
	BOOL		Write(LPCSTR str);
	LogMsg		*logMsg;
	LogDb		*logDb;
	BOOL		RegisterUpLog();

public:
	LogMng(Cfg *_cfg);
	~LogMng();

	void	InitDB();
	BOOL	WriteSendStart(void);
	BOOL	WriteSendHead(Host *host);
	BOOL	WriteSendMsg(ULONG packetNo, LPCSTR msg, ULONG command, int opt, time_t t,
				ShareInfo *shareInfo=NULL, int64 *msg_id=NULL);
	BOOL	WriteRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo=NULL,
				const std::vector<HostSub> *recvList=NULL, int64 *msg_id=NULL);
	BOOL	GetRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo,
				const std::vector<HostSub> *recvList, U8str *u);
	BOOL	WriteStart(void);
	BOOL	WriteMsg(ULONG packetNo, LPCSTR msg, ULONG command, int opt, time_t t,
				ShareInfo *shareInfo=NULL, int64 *msg_id=NULL);

	BOOL	InitLogDb(BOOL with_import=FALSE);
	BOOL	IsInitLogDb();
	BOOL	IsLogDbExists();
	LogDb	*GetLogDb() { return logDb; }
	BOOL	ReadCheckStatus(MsgIdent *mi, BOOL is_recv, BOOL unread_tmp);

	BOOL	MakeMsgStr(LogMsg *msg, Wstr *wbuf, CopyMsgFlags flags);
	int		MakeMsgHead(LogMsg *msg, WCHAR *wbuf, int max_len, BOOL with_self=FALSE);

	static void StrictLogFile(char *path);
};

enum {	STI_NONE=0, STI_ENC, STI_SIGN, STI_FILE, STI_CLIP, STI_ENC_FILE, STI_ENC_CLIP,
		STI_SIGN_FILE, STI_SIGN_CLIP, STI_MAX };

#endif

