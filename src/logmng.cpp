static char *logmng_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   logmng.cpp	Ver3.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Log Manager
	Create					: 1996-08-18(Sun)
	Update					: 2011-04-20(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "tlib/tlib.h"
#include "resource.h"
#include "ipmsg.h"

LogMng::LogMng(Cfg *_cfg)
{
	cfg = _cfg;
}

BOOL LogMng::WriteSendStart()
{
	return	WriteStart();
}

BOOL LogMng::WriteSendHead(LPCSTR head)
{
	char	buf[MAX_LISTBUF];

	wsprintf(buf, " To: %s\r\n", head);
	return	Write(buf);
}

BOOL LogMng::WriteSendMsg(LPCSTR msg, ULONG command, int opt, ShareInfo *shareInfo)
{
	return	WriteMsg(msg, command, opt, shareInfo);
}

BOOL LogMng::WriteRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo)
{
	if (msg->command & IPMSG_NOLOGOPT)
		return	FALSE;

	WriteStart();
	char	buf[MAX_PATH_U8] = " From: ";

	MakeListString(cfg, &msg->hostSub, hosts, buf + strlen(buf), TRUE);
	strcat(buf, "\r\n");
	Write(buf);

	return	WriteMsg(msg->msgBuf, msg->command, opt, shareInfo);
}

BOOL LogMng::WriteStart(void)
{
	return	Write("=====================================\r\n");
}

BOOL LogMng::WriteMsg(LPCSTR msg, ULONG command, int opt, ShareInfo *shareInfo)
{
	char	buf[MAX_BUF * 2] = "  at ";
	int		id;

	strcat(buf, Ctime()); 
	strcat(buf, " ");

	if (command & IPMSG_BROADCASTOPT)
		strcat(buf, GetLoadStrU8(IDS_BROADCASTLOG));

	if (command & IPMSG_AUTORETOPT)
		strcat(buf, GetLoadStrU8(IDS_AUTORETLOG));

	if (command & IPMSG_MULTICASTOPT)
		strcat(buf, GetLoadStrU8(IDS_MULTICASTLOG));

	if (command & IPMSG_ENCRYPTOPT) {
		if      (opt & LOG_SIGN_OK)   id = IDS_ENCRYPT2_SIGNED;
		else if (opt & LOG_SIGN_ERR)  id = IDS_ENCRYPT2_ERROR;
		else if (opt & LOG_ENC2)      id = IDS_ENCRYPT2;
		else                          id = IDS_ENCRYPT;
		strcat(buf, GetLoadStrU8(id));
	}
	else if (opt & LOG_UNAUTH) {
		strcat(buf, GetLoadStrU8(IDS_UNAUTHORIZED));
	}

	if (command & IPMSG_SECRETOPT)
	{
		if (command & IPMSG_PASSWORDOPT)
			strcat(buf, GetLoadStrU8(IDS_PASSWDLOG));
		else
			strcat(buf, GetLoadStrU8(IDS_SECRETLOG));
	}

	if (shareInfo && (command & IPMSG_FILEATTACHOPT))
	{
		int	i,	clip_num = 0, noclip_num = 0;
		for (i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD)	clip_num++;
			else																	noclip_num++;
		}
		strcat(buf, "\r\n  ");
		if (clip_num || noclip_num) {
			int id = clip_num ? noclip_num ? IDS_FILEWITHCLIP : IDS_WITHCLIP : IDS_FILEATTACH;
			strcat(buf, GetLoadStrU8(id));
			strcat(buf, " ");
		}
		char	fname[MAX_PATH_U8], *ptr = buf + strlen(buf);
		for (i=0; i < shareInfo->fileCnt && ptr-buf < sizeof(buf)-MAX_PATH_U8; i++)
		{
			ForcePathToFname(shareInfo->fileInfo[i]->Fname(), fname);
			ptr += wsprintf(ptr, "%s%s", fname, i+1 == shareInfo->fileCnt ? "" : ", ");
		}
	}
	strcat(buf, "\r\n-------------------------------------\r\n");

	if (Write(buf) && Write(msg) && Write("\r\n\r\n"))
		return	TRUE;
	else
		return	FALSE;
}

inline int bit_cnt(unsigned char c)
{
	int bit;
	for (bit=0; c; c>>=1)
		if (c&1)
			bit++;
	return	bit;
}

inline char make_key(char *password)
{
	char	key = 0;

	while (*password)
		key += *password++;

	return	key;
}

BOOL LogMng::Write(LPCSTR str)
{
	BOOL	ret = FALSE;

	if (!cfg->LogCheck || *cfg->LogFile == 0)
		return	TRUE;

	HANDLE		fh;
	DWORD		size;

	if ((fh = CreateFileU8(cfg->LogFile, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE)
	{
		::SetFilePointer(fh, 0, 0, FILE_END);
		str = cfg->LogUTF8 ? str : U8toA(str);
		ret = ::WriteFile(fh, str, strlen(str), &size, NULL);
		::CloseHandle(fh);
	}

	return	ret;
}

void LogMng::StrictLogFile(char *logFile)
{
	if (strncmp(logFile, "\\\\", 2) == 0 || logFile[0] == 0 || logFile[1] == ':')
		return;

	char	orgPath[MAX_PATH_U8], buf[MAX_PATH_U8], *tmp=NULL;

	strcpy(orgPath, logFile);

	if (strchr(logFile, '\\') == NULL) {
		TRegistry	reg(HKEY_CURRENT_USER);
		if (reg.OpenKey(REGSTR_SHELLFOLDERS)) {
			if (reg.GetStr(REGSTR_MYDOCUMENT, buf, sizeof(buf))) {
				MakePath(logFile, buf, orgPath);
				return;
			}
		}
	}

	GetFullPathNameU8(orgPath, MAX_PATH_U8, logFile, &tmp);
	return;
}

