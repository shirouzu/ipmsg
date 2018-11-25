/*	@(#)Copyright (C) H.Shirouzu 2018   sendmsg.h	Ver4.90 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Message Core
	Create					: 2018-09-01(Sat)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SENDMSG_H
#define SENDMSG_H

enum SendStatus { ST_INIT=0, ST_GETCRYPT, ST_MAKECRYPTMSG, ST_MAKEMSG, ST_SENDMSG, ST_DONE };
class SendEntry {
	Host		*host = NULL;
	SendStatus	status = ST_INIT;
	UINT		command = 0;
	DynBuf		msg;
	int			msgLen;
	bool		useUlist = false;
	bool		useFile = false;
	bool		useClip = false;

// For Fragment UDP packet checksum problem (some NIC has this problem)
// Last fragment payload is smaller than 64, some NIC generate wrong checksum.
#define UDP_CHECKSUM_FIXBUF 64

public:
	SendEntry(Host *_host=NULL) : status(ST_GETCRYPT) {
		if (_host) {
			SetHost(_host);
		}
	}
	~SendEntry() {
		if (host && host->RefCnt(-1) == 0) {
			host->SafeRelease();
		}
	}
	void SetMsg(char *_msg, int len) {
		msgLen = len;
		msg.Alloc(len + UDP_CHECKSUM_FIXBUF);
		memcpy(msg.Buf(), _msg, len);
		memset(msg.Buf() + len, 0, UDP_CHECKSUM_FIXBUF);
	}
	const char *Msg(void) {
		return msg;
	}
	int MsgLen(bool udp_checksum_fix=false) {
		return msgLen + (udp_checksum_fix ? UDP_CHECKSUM_FIXBUF : 0);
	}
	void SetHost(Host *_host) {
		(host = _host)->RefCnt(1);
	}
	Host *Host(void) {
		return host;
	}
	void SetStatus(SendStatus _status) {
		status = _status;
	}
	SendStatus Status(void) {
		return status;
	}
	void SetCommand(UINT _command) {
		command = _command;
	}
	UINT Command(void) {
		return command;
	}
	void SetUseUlist(bool _useUlist) {
		useUlist = _useUlist;
	}
	bool UseUlist() {
		return useUlist;
	}
	void SetUseFile(bool use) {
		useFile = use;
	}
	bool UseFile() {
		return	useFile;
	}
	void SetUseClip(bool use) {
		useClip = use;
	}
	bool UseClip() {
		return	useClip;
	}

	SendEntry(const SendEntry&) = delete;
	SendEntry& operator =(const SendEntry &) = delete;
};


class SendMsg {
protected:
	MsgMng	*msgMng = NULL;
	Cfg		*cfg = NULL;
	ULONG	packetNo = 0;
	DynBuf	msgBuf;
	UINT	retryCnt = 0;
	HWND	cmdHWnd = NULL;
	bool	onceNotify = false;
	bool	isSendMsg = true;
	time_t	timestamp = 0;
	TMsgBox	retryDlg;

	std::vector<std::shared_ptr<SendEntry>> sendEntry;
	U8str		shareStr;
	IPDictList	shareDictList;

	void	MakeUlistCore(SendEntry *self, std::vector<User> *uvec);
	void	MakeUlistDict(SendEntry *self, IPDict *dict);
	void	MakeDelayFoot(char *foot);
	bool	MakeMsgPacket(SendEntry *ent);
	void	MakeUlistStr(SendEntry *ent, char *ulist);
	void	MakeHistMsg(SendEntry *ent, U8str *str);

public:
	SendMsg(Cfg *_cfg, MsgMng *_msgmng, int unum=0, ULONG packetNo=0, HWND cmdHWnd=NULL,
			bool onceNotify=false, time_t _timestamp=0, bool _isSendMsg=true);
	~SendMsg();

	void	RegisterMsg(const char *msg);
	void	RegisterHost(Host *host);
	int		RegisterShare(ShareInfo *shareInfo);

	bool	Send(void);
	bool	SendMsgEntry(SendEntry *ent);
	bool	SendFinishNotify(HostSub *hostSub, ULONG packet_no);
	bool	SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa);
	bool	Del(HostSub *hostSub, ULONG packet_no);
	bool	IsSendFinish(void);
	int		GetUlistStrLen();
	void	Finished();
	SendEntry *Entry(int idx) { return sendEntry[idx].get(); }
	size_t	Size() { return sendEntry.size(); }
	bool	TimerProc();
	bool	IsSendMsg() { return isSendMsg; }

	ULONG	PacketNo() { return packetNo; }
	bool	NeedActive(Host *host);
	void	Reset() { retryCnt = 0; }

	bool	Serial(IPDict *dict);
	bool	Deserial(const IPDict &dict);
	void	SaveHost();

	void	RegisterHist(SendEntry *ent, bool use_msg, bool delayed);
};

#endif

