static char *logmng_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   logmng.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Log Manager
	Create					: 1996-08-18(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "logdbconv.h"

// Todo:
// ログフォーマッタ、file用/view用二重もちをいずれ解消（メモリベースに）

#define LOGMSG_HEAD_TOP	"=====================================\r\n"
#define LOGMSG_HEAD_END	"-------------------------------------\r\n"
#define LOGMSG_FROM		" From: "
#define LOGMSG_CC		"   Cc: "
#define LOGMSG_TO		" To: "
#define LOGMSG_AT		"  at "

LogMng::LogMng(Cfg *_cfg)
{
	cfg   = _cfg;
	logDb = NULL;
	logMsg = new LogMsg;
}

LogMng::~LogMng()
{
	delete logMsg;
	delete logDb;
}

void LogMng::InitDB()
{
	if (IsLogDbExists()) {
		InitLogDb();
	}
}

void SetLogMsgUser(Cfg *cfg, HostSub *hostSub, THosts *hosts, LogMsg *logmsg)
{
	Host	*host = hostSub->addr.IsEnabled() ? hosts->GetHostByAddr(hostSub) :
		 hosts->GetHostByName(hostSub);

	LogHost	log_host;
	log_host.uid  = hostSub->u.userName;
	log_host.addr = hostSub->addr.S();
	log_host.host = hostSub->u.hostName;

	if (host && IsSameHost(hostSub, &host->hostSub)
		 || GetUserNameDigestField(hostSub->u.userName) &&
		 	(host = cfg->priorityHosts.GetHostByName(hostSub))) {
		log_host.gname = host->groupName;
		log_host.nick  = host->nickName;
		if (!hostSub->addr.IsEnabled()) {
			log_host.addr = host->hostSub.addr.S();
		}
	}

	if (!log_host.nick) {
		log_host.nick = hostSub->u.userName;
	}

	logmsg->host.push_back(log_host);
}

void SetLogMsgUser(Host *host, LogMsg *logmsg)
{
	LogHost	log_host;

	log_host.uid  = host->hostSub.u.userName;
	log_host.addr = host->hostSub.addr.S();
	log_host.host = host->hostSub.u.hostName;
	log_host.gname = host->groupName;
	log_host.nick  = host->nickName;

	if (!log_host.nick) {
		log_host.nick = host->hostSub.u.userName;
	}

	logmsg->host.push_back(log_host);
}

BOOL LogMng::WriteSendStart()
{
	return	WriteStart();
}

BOOL LogMng::WriteSendHead(Host *host)
{
	char	buf[MAX_BUF];
	char	*p = buf;

	p += strcpyz(p, LOGMSG_TO);
	p += MakeListString(cfg, host, p, TRUE);
	p += strcpyz(p, "\r\n");

	BOOL ret = Write(buf);

	SetLogMsgUser(host, logMsg);

	return	ret;
}

BOOL LogMng::WriteSendMsg(ULONG packetNo, LPCSTR msg, ULONG command, int opt, time_t t,
	ShareInfo *shareInfo, int64 *msg_id)
{
	return	WriteMsg(packetNo, msg, command, opt, t, shareInfo, msg_id);
}

BOOL LogMng::WriteRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo,
	const std::vector<HostSub> *recvList, int64 *msg_id)
{
	if (msg->command & IPMSG_NOLOGOPT)
		return	FALSE;

	WriteStart();
	logMsg->flags |= DB_FLAG_FROM;
	SetLogMsgUser(cfg, &msg->hostSub, hosts, logMsg);

	char	buf[MAX_PATH_U8];
	char	*p = buf;

	p += strcpyz(p, LOGMSG_FROM);
	p += MakeListString(cfg, &msg->hostSub, hosts, p, TRUE);
	p += strcpyz(p, "\r\n");
	Write(buf);

	if (recvList && recvList->size() >= 1) {
		for (auto &h: *recvList) {
			p = buf;
			p += strcpyz(p, LOGMSG_CC);
			p += MakeListString(cfg, (HostSub *)&h, hosts, p, TRUE);
			p += strcpyz(p, "\r\n");
			Write(buf);

			SetLogMsgUser(cfg, (HostSub *)&h, hosts, logMsg);
		}
	}

	return	WriteMsg(msg->packetNo, msg->msgBuf.s(), msg->command, opt, msg->timestamp,
		shareInfo, msg_id);
}

BOOL LogMng::GetRecvMsg(MsgBuf *msg, int opt, THosts *hosts, ShareInfo *shareInfo,
	const std::vector<HostSub> *recvList, U8str *u)
{
	U8str	buf(MAX_UDPBUF);
	char	*p = buf.Buf();

	p += strcpyz(p, LOGMSG_HEAD_TOP);
	p += strcpyz(p, LOGMSG_FROM);
	p += MakeListString(cfg, &msg->hostSub, hosts, p, TRUE);
	p += strcpyz(p, "\r\n");

	if (recvList && recvList->size() >= 1) {
		for (auto &h: *recvList) {
			p += strcpyz(p, LOGMSG_CC);
			p += MakeListString(cfg, (HostSub *)&h, hosts, p, TRUE);
			p += strcpyz(p, "\r\n");
		}
	}

	p += strcpyz(p, LOGMSG_AT);
	p += strcpyz(p, Ctime(&msg->timestamp)); 
	p += strcpyz(p, " ");

	int	command = msg->command;

	if (command & IPMSG_BROADCASTOPT) {
		p += strcpyz(p, LoadStrU8(IDS_BROADCASTLOG));
	}
	if (command & IPMSG_AUTORETOPT) {
		p += strcpyz(p, LoadStrU8(IDS_AUTORETLOG));
	}
	if (command & IPMSG_MULTICASTOPT) {
		p += strcpyz(p, LoadStrU8(IDS_MULTICASTLOG));
	}
	if (command & IPMSG_ENCRYPTOPT) {
		int	id = 0;
		if (opt & LOG_SIGN2_OK) {
			id = IDS_ENCRYPT2_SIGNED2;
		}
		else if (opt & LOG_SIGN_OK) {
			id = IDS_ENCRYPT2_SIGNED;
		}
		else if (opt & LOG_SIGN_ERR) {
			id = IDS_ENCRYPT2_ERROR;
		}
		else if (opt & LOG_ENC2) {
			id = IDS_ENCRYPT2;
		}
		else {
			id = IDS_ENCRYPT;
		}
		p += strcpyz(p, LoadStrU8(id));
	}
	else if (opt & LOG_UNAUTH) {
		p += strcpyz(p, LoadStrU8(IDS_UNAUTHORIZED));
	}

	if (command & IPMSG_SECRETOPT) {
		if (command & IPMSG_PASSWORDOPT)
			p += strcpyz(p, LoadStrU8(IDS_PASSWDLOG));
		else
			p += strcpyz(p, LoadStrU8(IDS_SECRETLOG));
	}

	if (shareInfo && (command & IPMSG_FILEATTACHOPT)) {
		int	clip_num = 0;
		int	noclip_num = 0;

		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				clip_num++;
			}
			else {
				noclip_num++;
			}
		}
		p += strcpyz(p, "\r\n  ");
		if (clip_num || noclip_num) {
			int id = clip_num ? noclip_num ? IDS_FILEWITHCLIP : IDS_WITHCLIP : IDS_FILEATTACH;
			p += strcpyz(p, LoadStrU8(id));
			p += strcpyz(p, " ");
		}

		for (int i=0; i < shareInfo->fileCnt && p - buf.Buf() < MAX_BUF; i++)
		{
			char	fname[MAX_PATH_U8];
			ForcePathToFname(shareInfo->fileInfo[i]->Fname(), fname);
			p += snprintfz(p, MAX_BUF, "%s%s", fname, i+1 == shareInfo->fileCnt ? "" : ", ");

			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				LogClip	clip;
				clip.fname = fname;
				SetClipDimension(cfg, &clip);
			}
		}
	}
	p += strcpyz(p, "\r\n");
	p += strcpyz(p, LOGMSG_HEAD_END);
	p += strcpyz(p, msg->msgBuf.s());
	p += strcpyz(p, "\r\n\r\n");

	*u = buf.s();
	return	TRUE;
}

BOOL LogMng::WriteStart(void)
{
	logMsg->Init();

	return	Write(LOGMSG_HEAD_TOP);
}

#ifdef IPMSG_PRO
#define LOGMNG_FUNCS
#include "miscext.dat"
#undef  LOGMNG_FUNCS
#endif

BOOL LogMng::WriteMsg(ULONG packetNo, LPCSTR msg, ULONG command, int opt, time_t t,
	ShareInfo *shareInfo, int64 *msg_id)
{
	U8str	buf(MAX_UDPBUF);
	char	*p = buf.Buf();

	if (msg_id) {
		*msg_id = 0;
	}

	p += strcpyz(p, LOGMSG_AT);
	p += strcpyz(p, Ctime(&t)); 
	p += strcpyz(p, " ");

	logMsg->packet_no = packetNo;
	logMsg->date = t;

	if (command & IPMSG_BROADCASTOPT) {
		p += strcpyz(p, LoadStrU8(IDS_BROADCASTLOG));
	}

	if (command & IPMSG_AUTORETOPT) {
		p += strcpyz(p, LoadStrU8(IDS_AUTORETLOG));
		logMsg->flags |= DB_FLAG_AUTOREP;
	}

	if (command & IPMSG_MULTICASTOPT) {
		p += strcpyz(p, LoadStrU8(IDS_MULTICASTLOG));
		logMsg->flags |= DB_FLAG_MULTI;
	}

	if (command & IPMSG_ENCRYPTOPT) {
		int		id = 0;
		if (opt & LOG_SIGN2_OK) {
			id = IDS_ENCRYPT2_SIGNED2;
			logMsg->flags |= DB_FLAG_SIGNED2|DB_FLAG_RSA2;
		}
		else if (opt & LOG_SIGN_OK) {
			id = IDS_ENCRYPT2_SIGNED;
			logMsg->flags |= DB_FLAG_SIGNED|DB_FLAG_RSA2;
		}
		else if (opt & LOG_SIGN_ERR) {
			id = IDS_ENCRYPT2_ERROR;
			logMsg->flags |= DB_FLAG_SIGNERR|DB_FLAG_RSA2;
		}
		else if (opt & LOG_ENC2) {
			id = IDS_ENCRYPT2;
			logMsg->flags |= DB_FLAG_RSA2;
		}
		else {
			id = IDS_ENCRYPT;
			logMsg->flags |= DB_FLAG_RSA;
		}
		p += strcpyz(p, LoadStrU8(id));
	}
	else if (opt & LOG_UNAUTH) {
		p += strcpyz(p, LoadStrU8(IDS_UNAUTHORIZED));
		logMsg->flags |= DB_FLAG_UNAUTH;
	}

	if (command & IPMSG_SECRETOPT)
	{
		if (command & IPMSG_PASSWORDOPT) {
			p += strcpyz(p, LoadStrU8(IDS_PASSWDLOG));
		}
		else {
			p += strcpyz(p, LoadStrU8(IDS_SECRETLOG));
		}
		logMsg->flags |= DB_FLAG_SEAL;

		// 未開封フラグのセット
		for (auto itr=logMsg->host.begin(); itr != logMsg->host.end(); itr++) {
			if ((logMsg->flags & DB_FLAG_FROM) == 0 || itr == logMsg->host.begin()) {
				itr->flags = DB_FLAGMH_UNOPEN;
			}
		}
		if ((logMsg->flags & DB_FLAG_FROM)) {
			logMsg->flags |= DB_FLAG_UNOPENR;
		}
	}
	if (opt & LOG_DELAY) {
		p += strcpyz(p, LoadStrU8(IDS_DELAYSEND));
		logMsg->flags |= DB_FLAG_DELAY;
	}

	if (shareInfo && (command & IPMSG_FILEATTACHOPT))
	{
		int	clip_num = 0;
		int	noclip_num = 0;

		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				clip_num++;
			}
			else {
				noclip_num++;
			}
		}
		p += strcpyz(p, "\r\n  ");
		if (clip_num || noclip_num) {
			int id = clip_num ? noclip_num ? IDS_FILEWITHCLIP : IDS_WITHCLIP : IDS_FILEATTACH;
			p += strcpyz(p, LoadStrU8(id));
			p += strcpyz(p, " ");
		}

		for (int i=0; i < shareInfo->fileCnt && p - buf.Buf() < MAX_BUF; i++)
		{
			char	fname[MAX_PATH_U8];
			ForcePathToFname(shareInfo->fileInfo[i]->Fname(), fname);
			p += snprintfz(p, MAX_BUF, "%s%s", fname, i+1 == shareInfo->fileCnt ? "" : ", ");

			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				LogClip	clip;
				clip.fname = fname;
				SetClipDimension(cfg, &clip);
				logMsg->clip.push_back(clip);
				logMsg->flags |= DB_FLAG_CLIP;
			}
			else {
				logMsg->files.push_back(fname);
				logMsg->flags |= DB_FLAG_FILE;
			}
		}
	}
	p += strcpyz(p, "\r\n");
	p += strcpyz(p, LOGMSG_HEAD_END);

	Wstr	wmsg(msg);
	LocalNewLineToUnixW(wmsg.s(), wmsg.Buf(), wmsg.Len()+1);
	logMsg->body = wmsg;
	logMsg->lines = get_linenum_n(logMsg->body.s(), logMsg->body.StripLen());

	BOOL ret = Write(buf.s()) && Write(msg) && Write("\r\n\r\n");

#ifdef IPMSG_PRO
#define LOGMNG_WRITEMSG
#include "miscext.dat"
#undef  LOGMNG_WRITEMSG
#endif

	if (logDb && cfg->LogCheck) {
		logDb->InsertOneData(logMsg);
		PostMessage(GetMainWnd(), WM_LOGVIEW_UPDATE,
			MakeMsgIdHigh(logMsg->msg_id), MakeMsgIdLow(logMsg->msg_id));
		if (msg_id) {
			*msg_id = logMsg->msg_id;
		}
	}
	logMsg->Init();

	return	ret;
}

BOOL LogMng::ReadCheckStatus(MsgIdent *mi, BOOL is_recv, BOOL unread_tmp)
{
	if (!logDb) {
		return	FALSE;
	}

	int64	msg_id = mi->msgId ? mi->msgId : logDb->SelectMsgIdByIdent(mi, is_recv);
	if (!msg_id) {
		return	FALSE;
	}

	LogMsg	msg;
	if (!logDb->SelectOneData(msg_id, &msg)) {
		return	FALSE;
	}

	for (auto &h: msg.host) {
		if (mi->uid != h.uid || mi->host != h.host) {
			continue;
		}
		if (h.flags & DB_FLAGMH_UNOPEN) {
			h.flags = time_to_msgid(time(NULL));
			h.flags &= ~DB_FLAGMH_UNOPEN;
			logDb->UpdateOneMsgHost(msg_id, &h);
			if (msg.flags & DB_FLAG_UNOPENR) {
				logDb->UpdateOneData(msg_id,
					(msg.flags| (unread_tmp ? DB_FLAG_UNOPENRTMP : 0)) & ~DB_FLAG_UNOPENR);
			}
			PostMessage(GetMainWnd(), WM_LOGVIEW_UPDATE,
				MakeMsgIdHigh(msg_id), MakeMsgIdLow(msg_id));

//			PostMessage(GetMainWnd(), WM_LOGVIEW_RESETCACHE, 0, 0);
			return	TRUE;
		}
		return	FALSE;
	}

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

	if ((fh = CreateFileWithDirU8(cfg->LogFile, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE) {
		::SetFilePointer(fh, 0, 0, FILE_END);
		str = cfg->LogUTF8 ? str : U8toAs(str);
		ret = ::WriteFile(fh, str, (DWORD)strlen(str), &size, NULL);
		::CloseHandle(fh);
	}

	return	ret;
}

int MakeHostFormat(LogHost *host, WCHAR *wbuf, int max_len)
{
	int		len = 0;

	len += wcsncpyz(wbuf + len, host->nick ? host->nick.s() : host->uid.s(), max_len - len);
	len += wcsncpyz(wbuf + len, L" (", max_len - len);
	if (host->gname) {
		len += wcsncpyz(wbuf + len, host->gname.s(), max_len - len);
		len += wcsncpyz(wbuf + len, L"/", max_len - len);
	}
	len += wcsncpyz(wbuf + len, host->host.s(), max_len - len);
	if (host->addr) {
		len += wcsncpyz(wbuf + len, L"/", max_len - len);
		len += wcsncpyz(wbuf + len, host->addr.s(), max_len - len);
	}
	len += wcsncpyz(wbuf + len, L"/", max_len - len);
	len += wcsncpyz(wbuf + len, host->uid.s(), max_len - len);
	len += wcsncpyz(wbuf + len, L")", max_len - len);
	return	len;
}

int LogMng::MakeMsgHead(LogMsg *msg, WCHAR *wbuf, int max_len, BOOL with_self)
{
	int			len = 0;
	BOOL		is_memo = (msg->flags & DB_FLAG_MEMO) ? TRUE : FALSE;
	const WCHAR	*from_to = (msg->flags & DB_FLAG_FROM) ? L" From: " : L" To: ";
	const WCHAR	*next_to = (msg->flags & DB_FLAG_FROM) ? L"   Cc: " : NULL;

	len = U8toW(LOGMSG_HEAD_TOP, wbuf, max_len);

	if (is_memo) {
		len += wcsncpyz(wbuf + len, msg->host[0].nick.s(), max_len - len);
		len += wcsncpyz(wbuf + len, L"\r\n", max_len - len);
	}
	else {
		for (auto host=msg->host.begin(); host != msg->host.end(); host++) {
			WCHAR	host_str[MAX_LISTBUF];

			if (host == msg->host.begin() || !with_self || !cfg->selfHost.IsSameUidHost(*host)) {
				len += wcsncpyz(wbuf + len, from_to, max_len - len);
				MakeHostFormat(&(*host), host_str, wsizeof(host_str));
				len += wcsncpyz(wbuf + len, host_str, max_len - len);
				len += wcsncpyz(wbuf + len, L"\r\n", max_len - len);
			}

			if (next_to) {
				from_to = next_to;
			}
		}
		if (with_self) {
			const WCHAR	*self_from_to = (msg->flags & DB_FLAG_FROM) ? L" To:   " : L" From: ";
			WCHAR	host_str[MAX_LISTBUF];

			len += wcsncpyz(wbuf + len, self_from_to, max_len - len);
			MakeHostFormat(&cfg->selfHost, host_str, wsizeof(host_str));
			len += wcsncpyz(wbuf + len, host_str, max_len - len);
			len += wcsncpyz(wbuf + len, L"\r\n", max_len - len);
		}
	}
	len += wcsncpyz(wbuf + len, L"  at ", max_len - len);
	len += wcsncpyz(wbuf + len, CtimeW(msg->date), max_len - len);

#ifdef IPMSG_PRO
#define LOGMNG_MAKEMSGHEAD
#include "miscext.dat"
#undef  LOGMNG_MAKEMSGHEAD
#endif

	len += wcsncpyz(wbuf + len, L"\r\n", max_len - len);

	if (msg->clip.size() || msg->files.size()) {
		len += wcsncpyz(wbuf + len, msg->clip.size() ? msg->files.size() ?
			L"  (files/image)" : L"  (image)" : L"  (files)", max_len - len);

		for (auto &f: msg->files) {
			len += wcsncpyz(wbuf + len, L" ", max_len - len);
			len += wcsncpyz(wbuf + len, f.s(), max_len - len);
		}
		for (auto &c: msg->clip) {
			len += wcsncpyz(wbuf + len, L" ", max_len - len);
			len += wcsncpyz(wbuf + len, c.fname.s(), max_len - len);
		}
		len += wcsncpyz(wbuf + len, L"\r\n", max_len - len);
	}

	len += U8toW(LOGMSG_HEAD_END, wbuf + len, max_len - len);

	return	len;
}

BOOL LogMng::MakeMsgStr(LogMsg *msg, Wstr *wbuf, CopyMsgFlags flags)
{
#define MAX_HEAD_BUF	8192
	int		max_len = (msg->body.Len() + 5) * 2 + MAX_HEAD_BUF;
	int		len = 0;

	wbuf->Init(max_len);
	if (flags & HEADMSG_COPY) {
		len += MakeMsgHead(msg, wbuf->Buf(), max_len, (flags & SELFHEAD_COPY));
	}
	len += UnixNewLineToLocalW(msg->body.s(), wbuf->Buf() + len, max_len - len);
	len += wcsncpyz(wbuf->Buf() + len, L"\r\n\r\n", max_len - len);

	return	TRUE;
}

void LogMng::StrictLogFile(char *logFile)
{
	if (logFile[0] == 0) {
		return;
	}
	if (strncmp(logFile, "\\\\", 2) == 0 || logFile[1] == ':') {
		return;
	}

	char	orgPath[MAX_PATH_U8];
	strcpy(orgPath, logFile);

	if (strchr(logFile, '\\') == NULL) {
		if (SHGetSpecialFolderPathU8(0, logFile, CSIDL_MYDOCUMENTS, FALSE)) {
			AddPathU8(logFile, IP_MSG);
			AddPathU8(logFile, orgPath);
			return;
		}
	}

	char	*tmp=NULL;
	GetFullPathNameU8(orgPath, MAX_PATH_U8, logFile, &tmp);

	return;
}

BOOL LogToDbName(const char *log_name, char *db_name)
{
	char	*fname=NULL;

	if (!GetFullPathNameU8(log_name, MAX_PATH_U8, db_name, &fname) || !fname) {
		return FALSE;
	}

//	char *ext = strchr(fname, '.');
//	if (ext) {
//		if (!strcmpi(ext, ".db")) {
//			return	FALSE;
//		}
//	}
//	else {
//		ext = fname + strlen(fname);
//	}
//
//	strcpy(ext, ".db");

	strcpy(fname, IPMSG_LOGDBNAME);

	return	TRUE;
}

BOOL LogMng::IsLogDbExists()
{
	char path[MAX_PATH_U8];

	if (!LogToDbName(cfg->LogFile, path)) {
		return	FALSE;
	}

	return	GetFileAttributesU8(path) != 0xffffffff ? TRUE : FALSE;
}

BOOL LogMng::InitLogDb(BOOL with_import)
{
	char	path[MAX_PATH_U8];

	if (!LogToDbName(cfg->LogFile, path)) {
		return	FALSE;
	}

	Wstr	wpath(path);
	BOOL	db_exists = ::GetFileAttributesW(wpath.s()) != 0xffffffff;

	logDb = new LogDb(cfg);
	logDb->Init(wpath.s());

	if (!db_exists && with_import) {
		AddTextLogToDb(cfg, logDb, cfg->LogFile);
	}
	return	TRUE;
}

BOOL LogMng::IsInitLogDb()
{
	return	logDb && logDb->IsInit();
}

