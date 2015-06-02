/*	@(#)Copyright (C) H.Shirouzu 2011   ipmsgcmn.h	Ver3.20 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Common Header
	Create					: 2011-05-03(Tue)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSGCMN_H
#define IPMSGCMN_H

#include "tlib/tlib.h"
#include <time.h>
#include <richedit.h>
#include "version.h"
#include <stddef.h>
#include "../external/libpng/png.h"
#include <richole.h>

/*  IP Messenger for Windows  internal define  */
#define IPMSG_REVERSEICON			0x0100
#define IPMSG_TIMERINTERVAL			500
#define IPMSG_ENTRYMINSEC			5
#define IPMSG_GETLIST_FINISH		0
#define IPMSG_OPENMSG_TIME			3000
#define IPMSG_DELAYMSG_TIME			5000
#define IPMSG_RECVMSG_TIME			10000

#define IPMSG_BROADCAST_TIMER		0x0101
#define IPMSG_SEND_TIMER			0x0102
#define IPMSG_LISTGET_TIMER			0x0104
#define IPMSG_LISTGETRETRY_TIMER	0x0105
#define IPMSG_ENTRY_TIMER			0x0106
#define IPMSG_DUMMY_TIMER			0x0107
#define IPMSG_RECV_TIMER			0x0108
#define IPMSG_ANS_TIMER				0x0109
#define IPMSG_CLEANUP_TIMER			0x010a
#define IPMSG_BALLOON_RECV_TIMER	0x010b
#define IPMSG_BALLOON_OPEN_TIMER	0x010c
#define IPMSG_BALLOON_DELAY_TIMER	0x010d


#define IPMSG_NICKNAME			1
#define IPMSG_FULLNAME			2

#define IPMSG_NAMESORT			0x00000000
#define IPMSG_IPADDRSORT		0x00000001
#define IPMSG_HOSTSORT			0x00000002
#define IPMSG_NOGROUPSORTOPT	0x00000100
#define IPMSG_ICMPSORTOPT		0x00000200
#define IPMSG_NOKANJISORTOPT	0x00000400
#define IPMSG_ALLREVSORTOPT		0x00000800
#define IPMSG_GROUPREVSORTOPT	0x00001000
#define IPMSG_SUBREVSORTOPT		0x00002000


#define WM_NOTIFY_TRAY			(WM_USER + 101)
#define WM_NOTIFY_RECV			(WM_USER + 102)
#define WM_NOTIFY_OPENCHECK		(WM_USER + 103)
#define WM_IPMSG_INITICON		(WM_USER + 104)
#define WM_IPMSG_BRRESET		(WM_USER + 105)
#define WM_RECVDLG_OPEN			(WM_USER + 110)
#define WM_RECVDLG_EXIT			(WM_USER + 111)
#define WM_RECVDLG_FILEBUTTON	(WM_USER + 112)
#define WM_SENDDLG_OPEN			(WM_USER + 121)
#define WM_SENDDLG_CREATE		(WM_USER + 122)
#define WM_SENDDLG_EXIT			(WM_USER + 123)
#define WM_SENDDLG_HIDE			(WM_USER + 124)
#define WM_SENDDLG_RESIZE		(WM_USER + 125)
#define WM_SENDDLG_FONTCHANGED	(WM_USER + 126)
#define WM_UDPEVENT				(WM_USER + 130)
#define WM_TCPEVENT				(WM_USER + 131)
#define WM_REFRESH_HOST			(WM_USER + 140)
#define WM_MSGDLG_EXIT			(WM_USER + 150)
#define WM_DELMISCDLG			(WM_USER + 151)
#define WM_HIDE_CHILDWIN		(WM_USER + 160)
#define WM_EDIT_DBLCLK			(WM_USER + 170)
#define WM_DELAYSETTEXT			(WM_USER + 180)
#define WM_DELAY_BITMAP			(WM_USER + 182)
#define WM_DELAY_PASTE			(WM_USER + 183)
#define WM_PASTE_REV			(WM_USER + 184)
#define WM_PASTE_IMAGE			(WM_USER + 185)
#define WM_HISTDLG_OPEN			(WM_USER + 190)
#define WM_HISTDLG_HIDE			(WM_USER + 191)
#define WM_HISTDLG_NOTIFY		(WM_USER + 192)
#define WM_FORCE_TERMINATE		(WM_USER + 193)
#define WM_FINDDLG_DELAY		(WM_USER + 194)
#define WM_IPMSG_IMECTRL		(WM_USER + 200)
#define WM_IPMSG_BRNOTIFY		(WM_USER + 201)

typedef long	Time_t;		// size of time_t is 64bit in VS2005 or later

#define SKEY_HEADER_SIZE	12

#define PRIV_BLOB_USER			0x0001
#define PRIV_BLOB_DPAPI			0x0002
#define PRIV_BLOB_RAW			0x0003
#define PRIV_SEED_HEADER		"ipmsg:"
#define PRIV_SEED_HEADER_LEN	6
#define PRIV_SEED_LEN			(PRIV_SEED_HEADER_LEN + (128/8))	// 128bit seed

// General define
#define MAX_SOCKBUF		65536
#define MAX_UDPBUF		32768
#define MAX_BUF			1024
#define MAX_BUF_EX		(MAX_BUF * 3)
#define MAX_NAMEBUF		80
#define MAX_LISTBUF		(MAX_NAMEBUF * 4)
#define MAX_ANSLIST		100
#define MAX_FILENAME	256

#define HS_TOOLS		"HSTools"
#define IP_MSG			"IPMsg"
#define NO_NAME			"no_name"
#define URL_STR			"://"
#define MAILTO_STR		"mailto:"
#define MSG_STR			"msg"

#define DEFAULT_PRIORITY	10
#define PRIORITY_OFFSET		10
#define DEFAULT_PRIORITYMAX	5

#define CLIP_ENABLE			0x1
#define CLIP_SAVE			0x2
#define CLIP_CONFIRM		0x4

class PubKey {
protected:
	int		keyLen;
	int		e;
	int		capa;
	BYTE	*key;

public:
	PubKey(void) { key = NULL; keyLen = 0; e = 0; capa = 0; }
	~PubKey() { UnSet(); }

	// 代入対応
	PubKey(const PubKey& o) { PubKey(); *this = o; }
	PubKey &operator=(const PubKey &o) {
		Set(o.key, o.keyLen, o.e, o.capa);
		return	*this;
	}

	void Set(BYTE *_key, int len, int _e, int _capa) {
		UnSet(); e = _e; capa = _capa; key = new BYTE [keyLen=len]; memcpy(key, _key, len);
	}
	void UnSet(void) {
		delete [] key; key = NULL; keyLen = 0; e = 0; capa = 0;
	}
	const BYTE *Key(void) { return key; }
	int KeyLen(void) { return keyLen; }
	int Exponent(void) { return e; }
	BOOL KeyBlob(BYTE *blob, int maxLen, int *len) {
		if ((*len = KeyBlobLen()) > maxLen) return FALSE;
		/* PUBLICSTRUC */	blob[0] = PUBLICKEYBLOB; blob[1] = CUR_BLOB_VERSION;
					*(WORD *)(blob+2) = 0; *(ALG_ID *)(blob+4) = CALG_RSA_KEYX;
		/* RSAPUBKEY */		memcpy(blob+8, "RSA1", 4);
					*(DWORD *)(blob+12) = keyLen * 8; *(int *)(blob+16) = e;
		/* PUBKEY_DATA */	memcpy(blob+20, key, keyLen);
		return	TRUE;
	}
	int KeyBlobLen(void) { return keyLen + 8 + 12; /* PUBLICKEYSTRUC + RSAPUBKEY */ }
	void SetByBlob(BYTE *blob, int _capa) {
		UnSet();
		keyLen = *(int *)(blob+12) / 8;
		key = new BYTE [keyLen];
		memcpy(key, blob+20, keyLen);
		e = *(int *)(blob+16);
		capa = _capa;
	}
	int Capa(void) { return capa; }

	int Serialize(BYTE *buf, int size) {
		int	offset = offsetof(PubKey, key);
		if (offset + keyLen > size) return -1;
		memcpy(buf, this, offset);
		if (keyLen > 0) memcpy(buf + offset, key, keyLen);
		return	offset + keyLen;
	}
	BOOL UnSerialize(BYTE *buf, int size) {
		int	offset = offsetof(PubKey, key);
		if (offset >= size) return FALSE;
		PubKey	*k = (PubKey *)buf;
		if (k->keyLen + offset != size) return FALSE;
		UnSet();
		Set((BYTE *)&k->key, k->keyLen, k->e, k->capa);
		return	TRUE;
	}
};

struct HostSub {
	char	userName[MAX_NAMEBUF];
	char	hostName[MAX_NAMEBUF];
	ULONG	addr;
	int		portNo;
};

struct Host {
	HostSub	hostSub;
	char	nickName[MAX_NAMEBUF];
	char	groupName[MAX_NAMEBUF];
	char	alterName[MAX_NAMEBUF];
	ULONG	hostStatus;
	Time_t	updateTime;
	int		priority;
	int		refCnt;
	PubKey	pubKey;
	BOOL	pubKeyUpdated;

	Host(void) { refCnt = 0; pubKeyUpdated = FALSE; *alterName = 0; }
	~Host() { refCnt = 0; }
	int	RefCnt(int cnt=0) { return refCnt += cnt; }
};

class THosts {
public:
	enum Kind { NAME, ADDR, NAME_ADDR, MAX_ARRAY };
	BOOL enable[MAX_ARRAY];

protected:
	int		hostCnt;
	Host	**array[MAX_ARRAY];
	Host	*Search(Kind kind, HostSub *hostSub, int *insertIndex=NULL);
	int		Cmp(HostSub *hostSub1, HostSub *hostSub2, Kind kind);

public:
	THosts(void);
	~THosts();

	void	Enable(Kind kind, BOOL _enable) { enable[kind] = _enable; }
	BOOL	AddHost(Host *host);
	BOOL	DelHost(Host *host);
	int		HostCnt(void) { return hostCnt; }
	Host	*GetHost(int index, Kind kind=NAME) { return array[kind][index]; }
	Host	*GetHostByName(HostSub *hostSub) { return enable[NAME] ? Search(NAME, hostSub) : NULL; }
	Host	*GetHostByAddr(HostSub *hostSub) { return enable[ADDR] ? Search(ADDR, hostSub) : NULL; }
	Host	*GetHostByNameAddr(HostSub *hostSub) {
		return enable[NAME_ADDR] ? Search(NAME_ADDR, hostSub) : NULL;
	}
	int		PriorityHostCnt(int priority, int range=1);
};

struct AddrObj : public TListObj {
	ULONG	addr;
	int		portNo;
};

struct AnsQueueObj : public TListObj {
	HostSub	hostSub;
	int		command;
};

struct UrlObj : public TListObj {
	char	protocol[MAX_NAMEBUF];
	char	program[MAX_PATH_U8];
};

struct UserObj : public TListObj {
	HostSub	hostSub;
};

struct UsersObj : public TListObj {
	TList	users;
	~UsersObj() {
		UserObj *obj;
		while ((obj = (UserObj *)users.TopObj())) { users.DelObj(obj); delete obj; }
	}
};

ULONG ResolveAddr(const char *_host);

struct TBrObj : public TListObj {
	char	*host;
	ULONG	addr;
public:
	TBrObj(const char *_host=NULL, ULONG _addr=0) {
		host = NULL;
		if (_host) host = strdup(_host);
		addr=_addr;
	}
	~TBrObj() { if (host) free(host); };
	const char *Host() { return	host; }
	ULONG	Addr(BOOL realTime=FALSE) { return realTime ? (addr = ResolveAddr(host)) : addr; }
};

class TBrList : public TList {
public:
	TBrList() {}
	~TBrList() { Reset(); }
	void Reset(void);
//	BOOL SetHost(const char *host);
	void SetHostRaw(const char *host, ULONG addr=0) {
		TBrObj *obj = new TBrObj(host, addr); AddObj(obj);
	}
//	BOOL IsExistHost(const char *host);
	TBrObj *Top() { return (TBrObj *)TopObj(); }
	TBrObj *Next(TBrObj *obj) { return (TBrObj *)NextObj(obj); }
};

enum SendWidth {
	SW_NICKNAME, SW_ABSENCE, SW_GROUP, SW_HOST, SW_IPADDR, SW_USER, SW_PRIORITY, MAX_SENDWIDTH
};
inline BOOL GetItem(UINT columnItems, int sw) {
	return (columnItems & (1 << sw)) ? TRUE : FALSE;
}
inline void SetItem(UINT *columnItems, int sw, BOOL on) {
	if (on) *columnItems |= (1 << sw);
	else    *columnItems &= ~(1 << sw);
}

#define CFG_ALL			0xffffffff
#define CFG_GENERAL		0x00000001
#define CFG_ABSENCE		0x00000002
#define CFG_WINSIZE		0x00000004
#define CFG_FONT		0x00000008
#define CFG_BROADCAST	0x00000010
#define CFG_CLICKURL	0x00000020
#define CFG_PRIORITY	0x00000040
#define CFG_FINDHIST	0x00000080
#define CFG_LRUUSER		0x00000100
#define CFG_HOSTINFO	0x00001000
#define CFG_DELHOST		0x00002000
#define CFG_DELCHLDHOST	0x00004000
#define CFG_CRYPT		0x00008000

#define FT_STRICTDATE	0x00000001
#define RS_REALTIME		0x00000001

struct PrivKey {
	BYTE		*blob;
	int			blobLen;
	int			encryptType;
	BYTE		*encryptSeed;
	int			encryptSeedLen;
	HCRYPTKEY	hKey;
	HCRYPTPROV	hCsp;
};

enum KeyType { KEY_512, KEY_1024, KEY_2048, MAX_KEY };

enum HistWidth {
	HW_USER, HW_ODATE, HW_SDATE, HW_ID, MAX_HISTWIDTH
};

struct Cfg {
protected:
	BOOL	ReadFontRegistry(TRegistry *reg, char *key, LOGFONT *font);
	BOOL	WriteFontRegistry(TRegistry *reg, char *key, LOGFONT *font);

public:
	THosts	priorityHosts;
	THosts	fileHosts;
	int		PriorityMax;
	int		PriorityReject;
	char	**PrioritySound;

	ULONG	nicAddr;
	int		portNo;
	int		lcid;
	BOOL	NoPopupCheck;
	int		OpenCheck;
	BOOL	NoErase;
	BOOL	NoBeep;
	BOOL	OneClickPopup;
	BOOL	BalloonNotify;
	int		DelayTime;
	BOOL	LogCheck;
	BOOL	LogUTF8;
	char	LogFile[MAX_PATH_U8];
	char	SoundFile[MAX_PATH_U8];
	BOOL	AbsenceSave;
	BOOL	AbsenceCheck;
	int		AbsenceMax;
	int		AbsenceChoice;
	char	(*AbsenceStr)[MAX_PATH_U8];
	char	(*AbsenceHead)[MAX_NAMEBUF];

	int		FindMax;
	char	(*FindStr)[MAX_NAMEBUF];
	BOOL	FindAll;

	PubKey	pub [MAX_KEY];
	PrivKey	priv[MAX_KEY];

	BOOL	QuoteCheck;
	BOOL	SecretCheck;
	BOOL	IPAddrCheck;
	BOOL	RecvIPAddr;
	BOOL	LogonLog;
	BOOL	RecvLogonDisp;
	BOOL	HotKeyCheck;
	int		HotKeyModify;
	int		HotKeySend;
	int		HotKeyRecv;
	int		HotKeyMisc;
	int		ControlIME; // 0:off, 1:senddlg on (finddlg:off), 2:always on
	int		GridLineCheck;
	UINT	ColumnItems;
	BOOL	AllowSendList;
	int		fileTransOpt;
	int		ResolveOpt;
	int		EncryptNum;
	int		ClipMode;
	int		ClipMax;

	int		ViewMax;
	int		TransMax;
	int		TcpbufMax;
	int		IoBufMax;
	BOOL	LumpCheck;

	char	NickNameStr[MAX_NAMEBUF];
	char	GroupNameStr[MAX_NAMEBUF];
	BOOL	AbnormalButton;
	BOOL	AbsenceNonPopup;
	BOOL	ListGet;
	UINT	ListGetMSec;
	UINT	RetryMSec;
	UINT	RetryMax;
	int		RecvMax;
	char	IconFile[MAX_PATH_U8];
	char	RevIconFile[MAX_PATH_U8];
	char	lastSaveDir[MAX_PATH_U8];
	char	lastOpenDir[MAX_PATH_U8];
	ULONG	Sort;
	int		UpdateTime;
	int		KeepHostTime;
	BOOL	DefaultUrl;
	BOOL	ShellExec;
	BOOL	ExtendEntry;
	int		ExtendBroadcast;
	char	QuoteStr[MAX_NAMEBUF];
	BOOL	Debug;

	int		SendWidth[MAX_SENDWIDTH];
	int		SendOrder[MAX_SENDWIDTH];

	int		SendXdiff;
	int		SendYdiff;
	int		SendMidYdiff;
	BOOL	SendSavePos;
	int		SendXpos;
	int		SendYpos;

	int		RecvXdiff;
	int		RecvYdiff;
	BOOL	RecvSavePos;
	int		RecvXpos;
	int		RecvYpos;

	LOGFONT	SendEditFont;
	LOGFONT	SendListFont;
	LOGFONT	RecvHeadFont;
	LOGFONT	RecvEditFont;

	int		HistWidth[MAX_HISTWIDTH];
	int		HistXdiff;
	int		HistYdiff;

	char	PasswordStr[MAX_NAMEBUF];
	BOOL	PasswdLogCheck;
	TList	urlList;
	BOOL	DialUpCheck;
	TList	DialUpList;
	TBrList	brList;
	char	*execDir;

	TList	lruUserList;
	int		lruUserMax;

	Cfg(ULONG _nicAddr, int _portNo);
	~Cfg();
	enum PART { FIND };

	BOOL	ReadRegistry(void);
	BOOL	WriteRegistry(int ctl_flg = CFG_ALL);
	void	GetRegName(char *buf, ULONG nic_addr, int port_no);
};

struct MsgBuf {
	HostSub	hostSub;
	int		version;
	int		portNo;
	ULONG	packetNo;
	ULONG	command;
	char	msgBuf[MAX_UDPBUF];
	char	exBuf[MAX_UDPBUF];
	char	packetNoStr[256/8]; // for IV (256bit)

	MsgBuf() {
		*msgBuf = 0;
		*exBuf = 0;
		*packetNoStr = 0;
	}

	// メモリコピー節約のため。
	void	Init(MsgBuf *org) {
		if (org == NULL) {
			memset(this, 0, (char *)&this->msgBuf - (char *)this);
			*msgBuf = 0;
			*exBuf = 0;
			*packetNoStr = 0;
			return;
		}
		memcpy(this, org, (char *)&this->msgBuf - (char *)this);
		strcpy(this->msgBuf, org->msgBuf);
		strcpy(this->exBuf, org->exBuf);
		strcpy(this->packetNoStr, org->packetNoStr);
	}
};

struct DynBuf {
	char	*buf;
	int		size;
	DynBuf(int _size=0)	{
		buf = NULL;
		if ((size = _size) > 0) Alloc(size);
	}
	~DynBuf() {
		free(buf);
	}
	char *Alloc(int _size) {
		if (buf) free(buf);
		buf = NULL;
		if ((size = _size) <= 0) return NULL;
		return	(buf = (char *)malloc(size));
	}
	char 	*Buf()		{ return buf; }
	WCHAR	*Wbuf()		{ return (WCHAR *)buf; }
	int		Size()		{ return size; }
};

struct RecvBuf {
	struct sockaddr_in	addr;
	int					addrSize;
	int					size;
	char				msgBuf[MAX_UDPBUF];
};

struct ConnectInfo : public TListObj {
	SOCKET	sd;
	ULONG	addr;
	int		port;
	BOOL	server;
	BOOL	complete;
	DWORD	startTick;
	DWORD	lastTick;
};

struct AddrInfo {
	ULONG	addr;
	ULONG	mask;
	ULONG	br_addr;
};

class MsgMng {
protected:
	SOCKET		udp_sd;
	SOCKET		tcp_sd;

	BOOL		status;
	ULONG		packetNo;
	Cfg			*cfg;

	HWND		hAsyncWnd;
	UINT		uAsyncMsg;
	UINT		lAsyncMode;
	HostSub		local;
	HostSub		localA;
	HostSub		orgLocal;

	BOOL		WSockInit(BOOL recv_flg);
	void		WSockTerm(void);
	BOOL		WSockReset(void);

public:
	MsgMng(ULONG nicAddr, int portNo, Cfg *cfg=NULL);
	~MsgMng();

	BOOL	GetStatus(void)	{ return	status; }
	HostSub	*GetLocalHost(void) { return	&local; }
	HostSub	*GetLocalHostA(void) { return	&localA; }
	HostSub	*GetOrgLocalHost(void) { return	&orgLocal; }
	void	CloseSocket(void);
	BOOL	IsAvailableTCP() { return tcp_sd != INVALID_SOCKET ? TRUE : FALSE; }

	BOOL	Send(HostSub *hostSub, ULONG command, int val);
	BOOL	Send(HostSub *hostSub, ULONG command, const char *message=NULL, const char *exMsg=NULL);
	BOOL	Send(ULONG host_addr, int port_no, ULONG command, const char *message=NULL,
			const char *exMsg=NULL);
	BOOL	AsyncSelectRegister(HWND hWnd);
	BOOL	Recv(MsgBuf *msg);
	BOOL	ResolveMsg(RecvBuf *buf, MsgBuf *msg);
	ULONG	MakePacketNo(void);
	ULONG	MakeMsg(char *udp_msg, int packetNo, ULONG command, const char *msg,
			const char *exMsg=NULL, int *packet_len=NULL);
	ULONG	MakeMsg(char *udp_msg, ULONG command, const char *msg, const char *exMsg=NULL,
					int *packet_len=NULL) {
				return	MakeMsg(udp_msg, MakePacketNo(), command, msg, exMsg, packet_len);
			}
	BOOL	UdpSend(ULONG host_addr, int port_no, const char *udp_msg);
	BOOL	UdpSend(ULONG host_addr, int port_no, const char *udp_msg, int len);
	BOOL	UdpRecv(RecvBuf *buf);

	BOOL	Accept(HWND hWnd, ConnectInfo *info);
	BOOL	Connect(HWND hWnd, ConnectInfo *info);
	BOOL	AsyncSelectConnect(HWND hWnd, ConnectInfo *info);
	BOOL	ConnectDone(HWND hWnd, ConnectInfo *info);

	static int LocalNewLineToUnix(const char *src, char *dest, int maxlen);
	static int UnixNewLineToLocal(const char *src, char *dest, int maxlen);
};

class TAbsenceDlg : public TDlg {
protected:
	Cfg		*cfg;
	int		currentChoice;
	char	(*tmpAbsenceStr)[MAX_PATH_U8];
	char	(*tmpAbsenceHead)[MAX_NAMEBUF];
	void	SetData(void);
	void	GetData(void);

public:
	TAbsenceDlg(Cfg *_cfg, TWin *_parent = NULL);
	virtual ~TAbsenceDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TSortDlg : public TDlg {
protected:
	static	TSortDlg *exclusiveWnd;
	Cfg		*cfg;
	void	SetData(void);
	void	GetData(void);

public:
	TSortDlg(Cfg *_cfg, TWin *_parent = NULL);

	virtual int		Exec(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TSendDlg;
class TFindDlg : public TDlg {
protected:
	Cfg			*cfg;
	TSendDlg	*sendDlg;
	BOOL		imeStatus;

public:
	TFindDlg(Cfg *_cfg, TSendDlg *_parent);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDestroy();
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual int		FilterHost();
};

enum SendStatus { ST_GETCRYPT=0, ST_MAKECRYPTMSG, ST_MAKEMSG, ST_SENDMSG, ST_DONE };
class SendEntry {
	Host		*host;
	SendStatus	status;
	UINT		command;
	char		*msg;
	int			msgLen;

public:
	SendEntry() { msg = NULL; host = NULL; }
	~SendEntry() { delete [] msg; if (host && host->RefCnt(-1) == 0) delete host; }
	void SetMsg(char *_msg, int len) { msg = new char[msgLen=len]; memcpy(msg, _msg, len); }
	const char *Msg(void) { return msg; }
	int MsgLen() { return msgLen; }
	void SetHost(Host *_host) { (host = _host)->RefCnt(1); }
	Host *Host(void) { return host; }
	void SetStatus(SendStatus _status) { status = _status; }
	SendStatus Status(void) { return status; }
	void SetCommand(UINT _command) { command = _command ; }
	UINT Command(void) { return command; }
};

class TRichEditOleCallback;

class TEditSub : public TSubClassCtl {
protected:
	Cfg						*cfg;
	BOOL					dblClicked;
	TRichEditOleCallback	*cb;
	IRichEditOle			*richOle;

public:
	TEditSub(Cfg *_cfg, TWin *_parent);
	virtual ~TEditSub();

	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual	HMENU	CreatePopupMenu();
	virtual BOOL	SetFont(LOGFONT *lf, BOOL dualFont=FALSE);

	virtual void	InsertBitmapByHandle(HBITMAP hBmp, int pos);
	virtual void	InsertBitmap(BITMAPINFO	*bmi, int size, int pos);

	virtual BOOL	InsertPng(VBuf *vbuf, int pos);
	virtual VBuf	*GetPngByte(int idx, int *pos);
	virtual int		GetImagePos(int idx);
	virtual int		GetImageNum();

	virtual int		GetTextUTF8(char *buf, int max_len, BOOL force_select=FALSE);
	virtual int		ExGetText(void *buf, int max_len, DWORD flags=GT_USECRLF,
						UINT codepage=CP_UTF8);
	virtual BOOL	ExSetText(const void *buf, int max_len=-1, DWORD flags=ST_DEFAULT,
						UINT codepage=CP_UTF8);
	virtual BOOL	GetStreamText(void *buf, int max_len, DWORD flags=SF_TEXTIZED|SF_TEXT);

	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
};

class TSeparateSub : public TSubClassCtl {
protected:
	HCURSOR	hCursor;

public:
	TSeparateSub(TWin *_parent);

	virtual BOOL	EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg);
	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
};

class TListHeader : public TSubClassCtl {
protected:
	LOGFONT	logFont;

public:
	TListHeader(TWin *_parent);

	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	ChangeFontNotify(void);
};

class TListViewEx : public TSubClassCtl {
protected:
	int		focus_index;

public:
	TListViewEx(TWin *_parent);

	virtual	int		GetFocusIndex(void) { return focus_index; }
	virtual	void	SetFocusIndex(int index) { focus_index = index; }

	virtual	int		InsertColumn(int idx, char *title, int width, int fmt=LVCFMT_LEFT);
	virtual	BOOL	DeleteColumn(int idx);
	virtual int		GetColumnWidth(int idx);
	virtual	void	DeleteAllItems();
	virtual	int		InsertItem(int idx, char *str, LPARAM lParam=0);
	virtual	int		SetSubItem(int idx, int subIdx, char *str);
	virtual	BOOL	DeleteItem(int idx);
	virtual	int		FindItem(LPARAM lParam);
	virtual	int		GetItemCount();
	virtual	int		GetSelectedItemCount();
	virtual	LPARAM	GetItemParam(int idx);
	virtual BOOL	GetColumnOrder(int *order, int num);
	virtual BOOL	SetColumnOrder(int *order, int num);
	virtual BOOL	IsSelected(int idx);

	virtual BOOL	AttachWnd(HWND _hWnd, DWORD addStyle=LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT);
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TListDlg : public TDlg, public TListObj {
public:
	TListDlg(UINT	resid, TWin *_parent = NULL) : TDlg(resid, _parent) {}
};

class FileInfo : public TListObj {
	int			id;
	char		*fname;
	BYTE		*memData;
	UINT		attr;
	int			pos;
	_int64		size;
	Time_t		mtime;
	Time_t		atime;
	Time_t		crtime;
	BOOL		isSelected;		// for recvdlg

public:
	FileInfo(int _id=0) {
		id = _id;
		fname = NULL;
		memData = NULL;
		attr = 0;
		pos = -1;
		size = 0;
		mtime = 
		atime = 0;
		crtime = 0;
		isSelected = FALSE;
	}
	FileInfo(const FileInfo& org) {
		fname	= NULL;
		memData	= NULL;
		*this = org;
	}
	~FileInfo() {
		free(fname);
		free(memData);
	}

	int Id() { return id; }
	void SetId(int _id) { id = _id; }
	const char *Fname() { return fname; }
	void SetFname(const char *_fname) {
		free(fname);
		fname = (char *)strdup(_fname);
	}
	const BYTE *MemData() { return memData; }
	void SetMemData(const BYTE *_memData, _int64 _size) {
		free(memData);
		if ((memData = (BYTE *)malloc((int)_size))) {
			memcpy(memData, _memData, (int)_size);
			size = _size;
		}
	}
	_int64 Size() { return size; }
	void SetSize(_int64 _size) { size = _size; }
	Time_t Mtime() { return mtime; }
	void SetMtime(Time_t _mtime) { mtime = _mtime; }
	Time_t Atime() { return atime; }
	void SetAtime(Time_t _atime) { atime = _atime; }
	Time_t Crtime() { return crtime; }
	void SetCrtime(Time_t _crtime) { crtime = _crtime; }
	UINT Attr() { return attr; }
	void SetAttr(UINT _attr) { attr = _attr; }
	int  Pos() { return pos; }
	void SetPos(UINT _pos) { pos = _pos; }
	BOOL IsSelected() { return isSelected; }
	void SetSelected(BOOL _isSelected) { isSelected = _isSelected; }
	FileInfo& operator =(const FileInfo& org) {
		id = org.id;
		if (org.fname) SetFname(org.fname);
		if (org.memData) SetMemData(org.memData, org.size);
		attr = org.attr;
		pos = org.pos;
		size = org.size;
		mtime = org.mtime;
		atime = org.atime;
		crtime = org.crtime;
		isSelected = org.isSelected;
		return *this;
	}
};

struct ShareInfo : public TListObj {
	int			packetNo;		// not use recvdlg
	Host		**host;			// allow host list, not use recvdlg
	int			hostCnt;		// not use recvdlg
	char		*transStat;		// not use recvdlg
	FileInfo	**fileInfo;		// allow file list
	int			fileCnt;
	FILETIME	attachTime;

public:
	ShareInfo(int packetNo=0);
	void LinkList(ShareInfo *top);
};

BOOL EncodeShareMsg(ShareInfo *info, char *buf, int bufsize, BOOL incMem=FALSE);
ShareInfo *DecodeShareMsg(char *buf, BOOL enable_clip);
BOOL FreeDecodeShareMsg(ShareInfo *info);
BOOL FreeDecodeShareMsgFile(ShareInfo *info, int index);
BOOL SetFileButton(TDlg *dlg, int buttonID, ShareInfo *info);


struct AcceptFileInfo {
	FileInfo	*fileInfo;
	Host		*host;
	_int64		offset;
	int			packetNo;
	UINT		command;
	FILETIME	attachTime;
};

struct ShareCntInfo {
	int		hostCnt;
	int		fileCnt;
	int		dirCnt;
	int		transferCnt;
	int		doneCnt;
	int		packetCnt;
	_int64	totalSize;
};

class TShareStatDlg;
class ShareMng {
public:
	enum			{ TRANS_INIT, TRANS_BUSY, TRANS_DONE };
protected:
	TListObj		_top;	// 番兵用
	ShareInfo		*top;
	TShareStatDlg	*statDlg;
	Cfg				*cfg;
	FileInfo		*SetFileInfo(char *fname);
	BOOL			AddShareCore(ShareInfo *info, FileInfo	*fileInfo);

public:
	ShareMng(Cfg *_cfg);
	~ShareMng();
	ShareInfo *CreateShare(int packetNo);
	void	DestroyShare(ShareInfo *info);
	BOOL	AddFileShare(ShareInfo *info, char *fname);
	BOOL	AddMemShare(ShareInfo *info, char *dummy_name, BYTE *data, int size, int pos);
	BOOL	DelFileShare(ShareInfo *info, int fileNo);

	BOOL	EndHostShare(int packetNo, HostSub *hostSub, FileInfo *fileInfo=NULL, BOOL done=TRUE);
	BOOL	AddHostShare(ShareInfo *info, SendEntry *entry, int entryNum);
	ShareInfo	*Top(void) { return top->next != top ? (ShareInfo *)top->next : NULL; }
	ShareInfo	*Next(ShareInfo *info) {
		return info->next != top ? (ShareInfo *)info->next : NULL;
	}
	ShareInfo	*Search(int packetNo);
	BOOL	GetShareCntInfo(ShareCntInfo *info, ShareInfo *shareInfo=NULL);
	BOOL	GetAcceptableFileInfo(ConnectInfo *info, char *buf, AcceptFileInfo *fileInfo);
	void	RegisterShareStatDlg(TShareStatDlg *_dlg) { statDlg = _dlg; }
	void	Cleanup();
	static int GetFileInfoNo(ShareInfo *info, FileInfo *fileInfo);
};

class TShareDlg : public TDlg {
	ShareMng	*shareMng;
	ShareInfo	*shareInfo;
	Cfg			*cfg;
	BOOL		AddList(int idx);
	BOOL		DelList(int idx);
	TListViewEx	shareListView;

public:
	TShareDlg(ShareMng *_shareMng, ShareInfo *_shareInfo, Cfg *_cfg, TWin *_parent = NULL);
	~TShareDlg();
//	virtual int		Exec(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDropFiles(HDROP hDrop);
static BOOL FileAddDlg(TDlg *dlg, ShareMng *sharMng, ShareInfo *shareInfo, Cfg *cfg);
};

class TSaveCommonDlg : public TDlg {
protected:
	TWin		*parentWin;
	ShareInfo	*shareInfo;
	Cfg			*cfg;
	int			offset;
	BOOL		isLinkFile;
	BOOL		SetInfo(void);
	BOOL		LumpCheck();

public:
	TSaveCommonDlg(ShareInfo *_shareInfo, Cfg *_cfg, TWin *_parentWin);
	virtual int		Exec(void);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void	EndDialog(int) {}	// 無視させる
};

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

class TSendDlg : public TListDlg {
protected:
	MsgMng		*msgMng;
	Cfg			*cfg;
	LogMng		*logmng;
	HWND		hRecvWnd;
	MsgBuf		msg;
	ShareMng	*shareMng;
	ShareInfo	*shareInfo;

	THosts		*hosts;
	TFindDlg	*findDlg;
	Host		**hostArray;
	int			memberCnt;

	SendEntry	*sendEntry;
	int			sendEntryNum;
	char		*shareStr;
	char		*shareExStr;
	char		selectGroup[MAX_NAMEBUF];

	ULONG		packetNo;
	int			packetLen;
	UINT		timerID;
	UINT		retryCnt;

// display
	HFONT		hListFont;
	HFONT		hEditFont;
	LOGFONT		orgFont;

	enum		send_item {
					host_item=0, member_item, refresh_item, ok_item, edit_item, secret_item,
					menu_item, passwd_item, separate_item, file_item, max_senditem
				};
	WINPOS		item[max_senditem];

	int			currentMidYdiff;
	int			dividYPos;
	int			lastYPos;
	BOOL		captureMode;
	BOOL		listOperateCnt;
	BOOL		hiddenDisp;

	int			maxItems;
	UINT		ColumnItems;
	int			FullOrder[MAX_SENDWIDTH];
	int			items[MAX_SENDWIDTH];
	BOOL		lvStateEnable;
	int			sortItem;
	BOOL		sortRev;

	TEditSub		editSub;
	TListHeader		hostListHeader;
	TListViewEx		hostListView;
	TSeparateSub	separateSub;

	void	SetFont(void);
	void	SetSize(void);
	void	SetMainMenu(HMENU hMenu);
	void	PopupContextMenu(POINTS pos);
	void	GetOrder(void);

	void	SetQuoteStr(LPSTR str, LPCSTR quoteStr);
	void	SelectHost(HostSub *hostSub, BOOL force=FALSE, BOOL byAddr=TRUE);
	void	DisplayMemberCnt(void);
	void	ReregisterEntry(void);
	UINT	GetInsertIndexPoint(Host *host);
	int		CompareHosts(Host *host1, Host *host2);
	int		GroupCompare(Host *host1, Host *host2);
	int		SubCompare(Host *host1, Host *host2);
	void	AddLruUsers(void);
	BOOL	SendMsg(void);
	BOOL	SendMsgCore(void);
	BOOL	SendMsgCoreEntry(SendEntry *entry);
	BOOL	MakeMsgPacket(SendEntry *entry);
	BOOL	MakeEncryptMsg(SendEntry *entry, char *msgstr, char *buf);
	BOOL	IsSendFinish(void);
	void	InitializeHeader(void);
	void	GetListItemStr(Host *host, int item, char *buf);

public:
	TSendDlg(MsgMng *_msgmng, ShareMng *_shareMng, THosts *_hosts, Cfg *cfg, LogMng *logmng,
			HWND _hRecvWnd = NULL, MsgBuf *msg = NULL);
	virtual ~TSendDlg();

	HWND	GetRecvWnd(void) { return	hRecvWnd; }
	void	AddHost(Host *host);
	void	ModifyHost(Host *host);
	void	DelHost(Host *host);
	void	DelAllHost(void);
	BOOL	IsSending(void);
	BOOL	SendFinishNotify(HostSub *hostSub, ULONG packet_no);
	BOOL	SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa);
	BOOL	FindHost(char *findStr);
	int		FilterHost(char *findStr);

	virtual BOOL	PreProcMsg(MSG *msg);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
	virtual BOOL	EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis);
	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);

	virtual BOOL	EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);
	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	
	virtual void	Show(int mode = SW_SHOWDEFAULT);
};

enum FileStatus {
	FS_DIRFILESTART, FS_OPENINFO, FS_FIRSTINFO, FS_NEXTINFO, FS_MAKEINFO, FS_TRANSINFO,
	FS_TRANSFILE, FS_ENDFILE, FS_MEMFILESTART, FS_COMPLETE, FS_ERROR
};

struct RecvFileObj {
	ConnectInfo	*conInfo;
	FileInfo	*fileInfo;

	BOOL		isDir;
	BOOL		isClip;
	FileInfo	curFileInfo;

	_int64		offset;
	_int64		woffset;
	char		*recvBuf;
	HANDLE		hFile;
	HANDLE		hThread;

	int			infoLen;
	int			dirCnt;

	_int64		totalTrans;
	DWORD		startTick;
	DWORD		lastTick;
	int			totalFiles;
	FileStatus	status;
	char		saveDir[MAX_PATH_U8];
	char		path[MAX_PATH_U8];
	char		info[MAX_BUF];	// for dirinfo buffer
	char		u8fname[MAX_PATH_U8];	// for dirinfo buffer
};

struct ClipBuf : public TListObj {
	VBuf	vbuf;
	int		pos;
	BOOL	finished;
	ClipBuf(int size, int _pos) : vbuf(size), pos(_pos) { finished = FALSE; }
};

class TRecvDlg : public TListDlg {
public:
	enum SelfStatus { INIT, SHOW, ERR };

protected:
	static int		createCnt;
	static HBITMAP	hDummyBmp;
	MsgMng		*msgMng;
	MsgBuf		msg;
	THosts		*hosts;
	char		head[MAX_LISTBUF];
	RECT		headRect;
	Cfg			*cfg;
	LogMng		*logmng;
	BOOL		openFlg;
	SYSTEMTIME	recvTime;
	HFONT		hHeadFont;
	HFONT		hEditFont;
	LOGFONT		orgFont;
	TEditSub	editSub;
	SelfStatus	status;

	RecvFileObj	*fileObj;
	ShareInfo	*shareInfo;
	TList		clipList;
	int			useClipBuf;
	int			useDummyBmp;

	UINT		timerID;
	UINT		retryCnt;
	char		readMsgBuf[MAX_LISTBUF];
	ULONG		packetNo;
	int			cryptCapa;
	int			logOpt;

	enum	recv_item {
				title_item=0, head_item, head2_item, open_item, edit_item, image_item,
				ok_item, cancel_item, quote_item, file_item, max_recvitem
			};
	WINPOS	item[max_recvitem];

	void	SetFont(void);
	void	SetSize(void);
	void	SetMainMenu(HMENU hMenu);
	void	PopupContextMenu(POINTS pos);
	BOOL	TcpEvent(SOCKET sd, LPARAM lParam);
	BOOL	StartRecvFile(void);
	BOOL	ConnectRecvFile(void);
static DWORD WINAPI RecvFileThread(void *_recvDlg);
	BOOL	SaveFile(void);
	BOOL	OpenRecvFile(void);
	BOOL	RecvFile(void);
	BOOL	RecvDirFile(void);
	BOOL	CloseRecvFile(BOOL setAttr=FALSE);
	BOOL	EndRecvFile(BOOL manual_suspend=FALSE);
	void	SetTransferButtonText(void);
	BOOL	DecryptMsg(void);
	BOOL	DecodeDirEntry(char *buf, FileInfo *info, char *u8fname);

public:
	TRecvDlg(MsgMng *_msgmng, MsgBuf *_msg, THosts *hosts, Cfg *cfg, LogMng *logmng);
	virtual ~TRecvDlg();

	virtual BOOL	IsClosable(void) {
						return openFlg && (fileObj == NULL || fileObj->conInfo == NULL) &&
								!clipList.TopObj();
					}
	virtual BOOL	IsSamePacket(MsgBuf *test_msg);
	virtual BOOL	SendFinishNotify(HostSub *hostSub, ULONG packet_no);

	virtual BOOL	PreProcMsg(MSG *msg);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);

	virtual void	Show(int mode = SW_NORMAL);
	virtual BOOL	InsertImages(void);
	SelfStatus		Status() { return status; }
	void			SetStatus(SelfStatus _status) { status = _status; }
	static int		GetCreateCnt(void) { return createCnt; }
	SYSTEMTIME		GetRecvTime() { return recvTime; }

	int				UseClipboard() { return useClipBuf; }
};

class TSetupSheet : public TDlg {
protected:
	Cfg		*cfg;
	THosts	*hosts;
	TList	tmpUrlList;	// for sheet6
	int		curIdx;		// for sheet6 

public:
	TSetupSheet() { cfg = NULL; hosts = NULL; }
	BOOL Create(int resid, Cfg *_cfg, THosts *hosts, TWin *_parent);
	BOOL SetData();
	BOOL GetData();
	BOOL CheckData();
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
};

#define MAX_SETUP_SHEET	6
class TSetupDlg : public TDlg {
	Cfg			*cfg;
	THosts		*hosts;
	int			curIdx;
	TSubClass	setupList;
	TSetupSheet	sheet[MAX_SETUP_SHEET];

public:
	TSetupDlg(Cfg *_cfg, THosts *hosts, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	void	SetSheet(int idx=-1);
};

class TAboutDlg : public TDlg {
public:
	TAboutDlg(TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TMsgDlg : public TListDlg {
protected:
	static int	createCnt;
	int			showMode;

public:
	TMsgDlg(TWin *_parent = NULL);
	virtual ~TMsgDlg();

	virtual BOOL	Create(char *text, char *title, int _showMode = SW_SHOW);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	static int		GetCreateCnt(void) { return createCnt; }
};

struct HistObj : public THashObj {
	HistObj	*prior;
	HistObj	*next;
	HistObj	*lruPrior;
	HistObj	*lruNext;
	char	info[MAX_LISTBUF];
	char	user[MAX_LISTBUF];
	char	sdate[MAX_NAMEBUF];
	char	odate[MAX_NAMEBUF];
	char	pktno[MAX_NAMEBUF];
	HistObj() : THashObj() {
		*info = *user = *sdate = *odate = *pktno = 0;
		prior = next = lruPrior = lruNext = NULL;
	}
};

class HistHash : public THashTbl {
protected:
	HistObj	*top;
	HistObj	*end;
	HistObj	*lruTop;
	HistObj	*lruEnd;
	virtual BOOL	IsSameVal(THashObj *, const void *val);

public:
	HistHash();
	virtual ~HistHash();
	virtual void Clear();
	virtual void Register(THashObj *obj, u_int hash_id);
	virtual void RegisterLru(HistObj *obj);
	virtual void UnRegister(THashObj *obj);
	virtual void UnRegisterLru(HistObj *obj);
	virtual HistObj *Top() { return top; }
	virtual HistObj *End() { return end; }
	virtual HistObj *LruTop() { return lruTop; }
	virtual HistObj *LruEnd() { return lruEnd; }
};

class THistDlg : public TDlg {
protected:
	Cfg			*cfg;
	THosts		*hosts;
	HistHash	histHash;
	int			unOpenedNum;
	BOOL		openedMode;

	TListHeader	histListHeader;
	TListViewEx	histListView;
	HFONT		hListFont;

public:
	THistDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent = NULL);
	virtual ~THistDlg();

	virtual BOOL	Create(HINSTANCE hI = NULL);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDestroy();
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual void	SendNotify(HostSub *hostSub, ULONG packetNo);
	virtual void	OpenNotify(HostSub *hostSub, ULONG packetNo, char *notify=NULL);
	virtual void	SaveColumnInfo();
	virtual void	SetTitle();
	virtual void	SetFont();
	virtual void	SetHeader();
	virtual void	SetData(HistObj *obj);
	virtual void	SetAllData();
	virtual int		MakeHistInfo(HostSub *hostSub, ULONG packet_no, char *buf);
	virtual void	Show(int mode = SW_NORMAL);
	virtual int		UnOpenedNum() { return unOpenedNum; }
	virtual void	SetMode(BOOL is_opened) {
		if (is_opened == openedMode) return;
		if (hWnd)	PostMessage(WM_COMMAND, OPENED_CHECK, 0);
		else		openedMode = is_opened;
	}
};

class TPasswordDlg : public TDlg {
protected:
	Cfg		*cfg;
	char	*outbuf;
public:
	TPasswordDlg(Cfg *_cfg, TWin *_parent = NULL);
	TPasswordDlg(char *_outbuf, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TShareStatDlg : public TDlg {
	ShareMng	*shareMng;
	Cfg			*cfg;
	BOOL		SetAllList(void);
	BOOL		DelList(int cnt);
	TListViewEx	shareListView;

public:
	TShareStatDlg(ShareMng *_shareMng, Cfg *_cfg, TWin *_parent = NULL);
	~TShareStatDlg();
//	virtual int		Exec(void);
	virtual BOOL	Refresh(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

struct SendFileObj : public TListObj {
	ConnectInfo	*conInfo;
	FileInfo	*fileInfo;
	FILETIME	attachTime;

	UINT		command;
	BOOL		isDir;
	_int64		fileSize;
	_int64		offset;
	char		*mapAddr;
	HANDLE		hFile;
	HANDLE		hMap;
	HANDLE		hThread;

	Host		*host;
	int			packetNo;
	FileStatus	status;

	int			dirCnt;
	HANDLE		*hDir;	// FindFirst handle
	char		path[MAX_PATH_U8];

	int			headerLen;
	int			headerOffset;
	char		header[MAX_BUF];	// for dirinfo
	WIN32_FIND_DATA_U8	fdata;
};

class TMainWin : public TWin {
protected:
	static HICON	hMainIcon;
	static HICON	hRevIcon;
	static TMainWin	*mainWin;	// for thread proc

	TList			sendList;
	TList			recvList;
	TList			msgList;
	TList			sendFileList;
	TList			connectList;
	THosts			hosts;

	TSetupDlg		*setupDlg;
	TAboutDlg		*aboutDlg;
	TShareStatDlg	*shareStatDlg;
	TAbsenceDlg		*absenceDlg;
	THistDlg		*histDlg;
	MsgMng			*msgMng;
	LogMng			*logmng;
	ShareMng		*shareMng;
	Cfg				*cfg;
	TRecycleList	*ansList;
	TBrList			brListEx;

	int			portNo;
	int			memberCnt;
	Time_t		refreshStartTime;
	Time_t		entryStartTime;
	UINT		entryTimerStatus;
	UINT		reverseTimerStatus;
	UINT		reverseCount;
	UINT		ansTimerID;
	UINT		TaskBarCreateMsg;
	BOOL		terminateFlg;
	BOOL		activeToggle;
	DWORD		writeRegFlags;
	enum TrayMode { TRAY_NORMAL, TRAY_RECV, TRAY_OPENMSG };
	TrayMode	trayMode;
	char		trayMsg[MAX_BUF];

#define MAX_PACKETLOG	16
	struct {
		ULONG		no;
		ULONG		addr;
		int			port;
	} packetLog[MAX_PACKETLOG];
	int		packetLogCnt;

	BOOL	IsLastPacket(MsgBuf *msg);
	void	SetIcon(HICON hSetIcon);
	void	ReverseIcon(BOOL startFlg);

	void	EntryHost(void);
	void	ExitHost(void);
	void	Popup(UINT resId);
	BOOL	PopupCheck(void);
	BOOL	AddAbsenceMenu(HMENU hMenu, int insertIndex);
	void	ActiveChildWindow(BOOL hide=FALSE);
	BOOL	TaskTray(int nimMode, HICON hSetIcon = NULL, LPCSTR tip = NULL);
	BOOL	BalloonWindow(TrayMode _tray_mode, LPCSTR msg=NULL, LPCSTR title=NULL, DWORD timer=0);
	BOOL	UdpEvent(LPARAM lParam);
	BOOL	TcpEvent(SOCKET sd, LPARAM lParam);

	BOOL	CheckConnectInfo(ConnectInfo *conInfo);
	inline	SendFileObj *FindSendFileObj(SOCKET sd);
	BOOL	StartSendFile(SOCKET sd);
	BOOL	OpenSendFile(const char *fname, SendFileObj *obj);
static DWORD WINAPI SendFileThread(void *_sendFileObj);
	BOOL	SendFile(SendFileObj *obj);
	BOOL	SendMemFile(SendFileObj *obj);
	BOOL	SendDirFile(SendFileObj *obj);
	BOOL	CloseSendFile(SendFileObj *obj);
	BOOL	EndSendFile(SendFileObj *obj);

	void	BroadcastEntry(ULONG mode);
	void	BroadcastEntrySub(ULONG addr, int portNo, ULONG mode);
	void	BroadcastEntrySub(HostSub *hostSub, ULONG mode);
	void	Terminate(void);

	BOOL	SendDlgOpen(HWND hRecvWnd = NULL, MsgBuf *msg = NULL);
	void	SendDlgHide(TSendDlg *sendDlg);
	void	SendDlgExit(TSendDlg *sendDlg);
	BOOL	RecvDlgOpen(MsgBuf *msg);
	void	RecvDlgExit(TRecvDlg *recvDlg);
	void	MsgDlgExit(TMsgDlg *msgDlg);
	void	MiscDlgOpen(TDlg *dlg);
	void	LogOpen(void);

	void	AddHost(HostSub *hostSub, ULONG command, char *nickName="", char *groupName="");
	inline	void SetHostData(Host *destHost, HostSub *hostSub, ULONG command, Time_t now_time,
				char *nickName="", char *groupName="", int priority=DEFAULT_PRIORITY);
	void	DelAllHost(void);
	void	DelHost(HostSub *hostSub);
	void	DelHostSub(Host *host);
	void	RefreshHost(BOOL unRemove);
	void	SetCaption(void);
	void	SendHostList(MsgBuf *msg);
	void	AddHostList(MsgBuf *msg);
	ULONG	HostStatus(void);
	void	ActiveListDlg(TList *dlgList, BOOL active = TRUE);
	void	DeleteListDlg(TList *dlgList);
	void	ActiveDlg(TDlg *dlg, BOOL active = TRUE);
	char	*GetNickNameEx(void);
	void	InitIcon(void);
	void	ControlIME(TWin *win, BOOL on);
	void	MakeBrListEx(void);

	BOOL	SetAnswerQueue(AnsQueueObj *obj);
	void	ExecuteAnsQueue(void);
	void	MsgBrEntry(MsgBuf *msg);
	void	MsgBrExit(MsgBuf *msg);
	void	MsgAnsEntry(MsgBuf *msg);
	void	MsgBrAbsence(MsgBuf *msg);
	void	MsgSendMsg(MsgBuf *msg);
	void	MsgRecvMsg(MsgBuf *msg);
	void	MsgReadMsg(MsgBuf *msg);
	void	MsgBrIsGetList(MsgBuf *msg);
	void	MsgOkGetList(MsgBuf *msg);
	void	MsgGetList(MsgBuf *msg);
	void	MsgAnsList(MsgBuf *msg);
	void	MsgGetInfo(MsgBuf *msg);
	void	MsgSendInfo(MsgBuf *msg);
	void	MsgGetPubKey(MsgBuf *msg);
	void	MsgAnsPubKey(MsgBuf *msg);
	void	MsgGetAbsenceInfo(MsgBuf *msg);
	void	MsgSendAbsenceInfo(MsgBuf *msg);
	void	MsgReleaseFiles(MsgBuf *msg);
	void	MsgInfoSub(MsgBuf *msg);

public:
	TMainWin(ULONG _nicAddr=INADDR_ANY, int _portNo=IPMSG_DEFAULT_PORT, TWin *_parent = NULL);
	virtual ~TMainWin();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvClose(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvEndSession(BOOL nSession, BOOL nLogOut);
	virtual BOOL	EvQueryOpen(void);
	virtual BOOL	EvHotKey(int hotKey);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);

	static	HICON	GetIPMsgIcon(void);
};

class TMsgApp : public TApp {
public:
	TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TMsgApp();

	virtual void	InitWindow(void);
};

class OpenFileDlg {
public:
	enum			Mode { OPEN, MULTI_OPEN, SAVE, NODEREF_SAVE };

protected:
	TWin			*parent;
	LPOFNHOOKPROC	hook;
	Mode			mode;

public:
	OpenFileDlg(TWin *_parent, Mode _mode=OPEN, LPOFNHOOKPROC _hook=NULL) {
		parent = _parent; hook = _hook; mode = _mode;
	}
	BOOL Exec(char *target, char *title=NULL, char *filter=NULL, char *defaultDir=NULL);
	BOOL Exec(UINT editCtl, char *title=NULL, char *filter=NULL, char *defaultDir=NULL);
};

#define		KEY_REBUILD		0x0001
#define		KEY_DIAG		0x0002
#define		KEY_INTERNAL	0x0004
BOOL	SetupCryptAPI(Cfg *cfg, MsgMng *msgMng);
BOOL	SetupCryptAPICore(Cfg *cfg, MsgMng *msgMng, int ctl_flg = 0);
BOOL	SetupRSAKey(Cfg *cfg, MsgMng *msgMng, KeyType key_type, int ctl_flg = 0);
BOOL	LoadPrivBlob(PrivKey *priv, BYTE *rawBlob, int *rawBlobLen);
BOOL	StorePrivBlob(PrivKey *priv, BYTE *rawBlob, int rawBlobLen);

BOOL CheckPassword(const char *cfgPasswd, const char *inputPasswd);
void MakePassword(const char *inputPasswd, char *outputPasswd);

#define MSS_SPACE	0x00000001
#define MSS_NOPOINT	0x00000002
int MakeSizeString(char *buf, _int64 size, int flg=0);

UrlObj *SearchUrlObj(TList *list, char *protocol);
void SetDlgIcon(HWND hWnd);
void MakeListString(Cfg *cfg, HostSub *hostSub, THosts *hosts, char *buf, BOOL is_log=FALSE);
void MakeListString(Cfg *cfg, Host *host, char *buf, BOOL is_log=FALSE);
HWND GetMainWnd(void);
char *GetVersionStr(void);
BOOL GenUserNameDigestVal(const BYTE *key, BYTE *digest);
BOOL GenUserNameDigest(char *org_name, const BYTE *key, char *new_name);
int VerifyUserNameDigest(const char *user, const BYTE *key);
inline int GetLocalCapa(Cfg *cfg) {
	return	cfg->pub[KEY_512].Capa() | cfg->pub[KEY_1024].Capa() | cfg->pub[KEY_2048].Capa();
}

BOOL SetImeOpenStatus(HWND hWnd, BOOL flg);
BOOL GetImeOpenStatus(HWND hWnd);
BOOL SetHotKey(Cfg *cfg);
BOOL IsSameHost(HostSub *host, HostSub *host2);
inline BOOL IsSameHostEx(HostSub *host, HostSub *host2) {
	return IsSameHost(host, host2) && host->addr == host2->addr &&
			host->portNo == host2->portNo ? TRUE : FALSE;
}
void RectToWinPos(const RECT *rect, WINPOS *wpos);
Time_t Time(void);
char *Ctime(SYSTEMTIME *st=NULL);
BOOL BrowseDirDlg(TWin *parentWin, const char *title, const char *defaultDir, char *buf);
BOOL PathToDir(const char *org_path, char *target_dir);
BOOL PathToFname(const char *org_path, char *target_fname);
void ForcePathToFname(const char *org_path, char *target_fname);
void ConvertShareMsgEscape(char *str);
BOOL IsSafePath(const char *fullpath, const char *fname);
BOOL GetLastErrorMsg(char *msg=NULL, TWin *win=NULL);
BOOL GetSockErrorMsg(char *msg=NULL, TWin *win=NULL);
int MakePath(char *dest, const char *dir, const char *file);
BOOL IsValidFileName(char *fname);
const char *GetUserNameDigestField(const char *user);
BOOL VerifyUserNameExtension(Cfg *cfg, MsgBuf *msg);
BOOL IsUserNameExt(Cfg *cfg);

void MakeClipFileName(int id, BOOL is_send, char *buf);
BOOL MakeImageFolder(Cfg *, char *dir);
BOOL SaveImageFile(Cfg *cfg, const char *target_fname, VBuf *buf);

char *separate_token(char *buf, char separetor, char **handle=NULL);
void MakeHash(const BYTE *data, int len, char *hashStr);
BOOL VerifyHash(const BYTE *data, int len, const char *orgHash);

BITMAPINFO *BmpHandleToInfo(HBITMAP hBmp, int *size);
HBITMAP BmpInfoToHandle(BITMAPINFO *bmi, int size);
HANDLE PngByteToBmpHandle(BYTE *data, int size);
BYTE *BmpHandleToPngByte(HBITMAP hBmp, int *size);
BYTE *PngStructToByte(png_struct *png, int *size);
png_struct *PngByteToStruct(BYTE *data, int size);
BITMAPINFO *PngToBmpInfo(png_struct *png, int *size);
png_struct *BmpInfoToPng(BITMAPINFO *bmi);
int GetColorDepth();

BOOL PrepareBmp(int cx, int cy, int *_aligned_line_size, VBuf *vbuf);
HBITMAP FinishBmp(VBuf *vbuf);
BOOL IsImageInClipboard(HWND hWnd);

AddrInfo *GetIPAddrs(BOOL strict, int *num);

#define rRGB(r,g,b)  ((DWORD)(((BYTE)(b)|((WORD)(g) << 8))|(((DWORD)(BYTE)(r)) << 16)))

// 1601年1月1日から1970年1月1日までの通算100ナノ秒
#define UNIXTIME_BASE	((_int64)0x019db1ded53e8000)

inline Time_t FileTime2UnixTime(FILETIME *ft) {
	return	(Time_t)((*(_int64 *)ft - UNIXTIME_BASE) / 10000000);
}
inline void UnixTime2FileTime(Time_t ut, FILETIME *ft) {
	*(_int64 *)ft = (_int64)ut * 10000000 + UNIXTIME_BASE;
}
char *GetLoadStrAsFilterU8(UINT id);
BOOL GetCurrentScreenSize(RECT *rect, HWND hRefWnd = NULL);

#endif
