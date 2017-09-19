/*	@(#)Copyright (C) H.Shirouzu 2013-2017   cfg.h	Ver4.60 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Configuration Manager
	Create					: 2013-03-03(Sun)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef CONFIG_H
#define CONFIG_H

#include <regex>
#include "logdb.h"

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

//#include "logdb.h"

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
#define CFG_LINKACT		0x00000200
#define CFG_HOSTINFO	0x00001000
#define CFG_DELHOST		0x00002000
#define CFG_DELCHLDHOST	0x00004000
#define CFG_CRYPT		0x00008000
#define CFG_SAVEMSG		0x00010000
#define CFG_LVFINDHIST	0x00100000
#define CFG_UPLOADPKT	0x01000000
#define CFG_SLACKOPT	0x02000000
#define CFG_HOOKOPT		0x04000000
#define CFG_UPDATEOPT	0x08000000
#define CFG_LASTWND		0x10000000

#define FT_STRICTDATE	0x00000001
//#define FT_DISABLEENC	0x00000002

#define RS_REALTIME		0x00000001

enum HistWidth {
	HW_USER, HW_ODATE, HW_SDATE, /*HW_ID, */ HW_MSG, MAX_HISTWIDTH
};

#define	AUTOSAVE_ENABLED	0x1
#define AUTOSAVE_INCDIR		0x2

#define CLIP_ENABLE			0x1
#define CLIP_SAVE			0x2
#define CLIP_CONFIRM_NORMAL	0x4
#define CLIP_CONFIRM_STRICT	0x8
#define CLIP_CONFIRM_ALL	(CLIP_CONFIRM_NORMAL|CLIP_CONFIRM_STRICT)

#define ABSENCESAVE_ONESHOT	0x2

struct TWstrObj : TListObj {
	Wstr	w;
};

struct TUpLogObj : TListObj {
	enum Status { INIT, STORED, UPLOADED } status;
	UINT	packetNo;
	DynBuf	buf;
	TUpLogObj(UINT _packet_no, Status _status) {
		status = _status;
		packetNo = _packet_no;
	}
};

class ShareInfo;

struct Cfg {
protected:
	BOOL	ReadFontRegistry(TRegistry *reg, char *key, LOGFONT *font);
	BOOL	WriteFontRegistry(TRegistry *reg, char *key, LOGFONT *font);
	BOOL	InitLinkRe();
	BOOL	StrToExtVec(const char *_s, std::vector<Wstr> *extVec);

#ifdef IPMSG_PRO
#define CFG_HEAD
#include "miscext.dat"
#undef  CFG_HEAD
#endif

public:
	Condition	cv;

	THosts	priorityHosts;
	THosts	fileHosts;
	int		PriorityMax;
	int		PriorityReject;
//	char	**PrioritySound;
	LogHost	selfHost;

	Addr	nicAddr;
	int		portNo;
	int		lcid;
	int		NoPopupCheck;
	int		OpenCheck;
	int		ReplyFilter;
	BOOL	NoErase;
	BOOL	ReproMsg;
	BOOL	NoBeep;
	BOOL	NoTcp;
	int		NoFileTrans;
	BOOL	OneClickPopup;
	BOOL	BalloonNotify;
	BOOL	TrayIcon;
	int		DelayTime;
	BOOL	LogCheck;
	BOOL	LogUTF8;
	char	LogFile[MAX_PATH_U8];
	char	SoundFile[MAX_PATH_U8];
	int		AbsenceSave;
	BOOL	AbsenceCheck;
	int		AbsenceMax;
	int		AbsenceChoice;
	char	(*AbsenceStr)[MAX_PATH_U8];
	char	(*AbsenceHead)[MAX_NAMEBUF];

	BOOL	useLockName;
	char	LockName[MAX_NAMEBUF];

	int		FindMax;
	char	(*FindStr)[MAX_NAMEBUF];
	BOOL	FindAll;
	int64	lastWnd;

	int					LvFindMax;
	TListEx<TWstrObj>	LvFindList;

	PubKey	officialPub;	// for update from ipmsg.org
	PubKey	pub [MAX_KEY];
	PrivKey	priv[MAX_KEY];
	char	regOwner[MAX_NAMEBUF];

	int		DirMode;
	Addr	masterAddr;
	DWORD	pollTick;
	int		masterStrict;
	int		DirAgentNum;

#ifdef IPMSG_PRO
#define CFG_HEAD2
#include "miscext.dat"
#undef  CFG_HEAD2
#else
	char	masterSvr[MAX_FQDN];
	int		dirSpan;

	enum { UPDATE_ON=0x01, UPDATE_MANUAL=0x02 };
	DWORD	updateFlag;
	DWORD	updateSpan;		// 6000000000
	time_t	updateLast;
#endif

	int		QuoteCheck; // 0:none, 1:normal, 2:full | 0x10: end care
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
	int		HotKeyLogView;
	int		ControlIME; // 0:off, 1:senddlg on (finddlg:off), 2:always on
	int		GridLineCheck;
	UINT	ColumnItems;
	BOOL	AllowSendList;
	int		fileTransOpt;
	int		ResolveOpt;
	BOOL	LetterKey;
	BOOL	ListConfirm;

	int		ClipMode;
	int		ClipMax;
	int		CaptureDelay;
	int		CaptureDelayEx;
	BOOL	CaptureMinimize;
	BOOL	CaptureClip;
	BOOL	CaptureSave;
	int		OpenMsgTime;
	int		RecvMsgTime;
	BOOL	BalloonNoInfo;
	BOOL	TaskbarUI;
	int		MarkerThick;

	int		IPv6Mode;
	int		IPv6ModeNext;
	char	IPv6Multicast[INET6_ADDRSTRLEN];
	int		MulticastMode;

	int		ViewMax;
	int		TransMax;
	int		TcpbufMax;
	int		IoBufMax;
	BOOL	LumpCheck;
	BOOL	EncTransCheck;

	char	NickNameStr[MAX_NAMEBUF];
	char	GroupNameStr[MAX_NAMEBUF];
	BOOL	AbnormalButton;
	BOOL	AbsenceNonPopup;
	BOOL	logViewAtRecv;

	BOOL	ListGet;
	UINT	ListGetMSec;
	UINT	RetryMSec;
	UINT	RetryMax;
	int		RecvMax;
	char	IconFile[MAX_PATH_U8];
	char	RevIconFile[MAX_PATH_U8];
	char	lastSaveDir[MAX_PATH_U8];
	char	lastOpenDir[MAX_PATH_U8];

	char	autoSaveDir[MAX_PATH_U8];
	int		autoSaveFlags; // 0x1: disable 0x2: inc_folder
	int		autoSaveTout;
	int		autoSaveLevel;
	int		autoSaveMax;
	int64	autoSavedSize;	// 現在の自動保存サイズ（非config）
	int		downloadLink;	// 0x1: リンクを作らない

	int		waitUpdate;
	int		autoUpdateMode;
	time_t	viewEpoch;
	time_t	viewEpochSave;

	ULONG	Sort;
	int		UpdateTime;
	int		KeepHostTime;
	BOOL	DefaultUrl;
	BOOL	ShellExec;
	BOOL	ExtendEntry;
	int		ExtendBroadcast;
	char	QuoteStr[MAX_NAMEBUF];
	WCHAR	QuoteStrW[MAX_NAMEBUF];

	int		logViewDispMode; // enum { LVDM... } logview.h
	char	directOpenExt[MAX_BUF];
	std::vector<Wstr> directExtVec;
	int		linkDetectMode;
	int		clickOpenMode;

	std::wregex	*urlex_re;
	std::wregex	*url_re;
	std::wregex	*file_re;
	std::wregex	*fileurl_re;

	int		Debug;
	int		Wine;
	int		FwCheckMode;
	int		Fw3rdParty;
	int		RecvIconMode;

	int		RemoteGraceSec;
	char	RemoteReboot[MAX_NAMEBUF];
	int		RemoteRebootMode;
	char	RemoteExit[MAX_NAMEBUF];
	int		RemoteExitMode;
	char	RemoteIPMsgExit[MAX_NAMEBUF];
	int		RemoteIPMsgExitMode;

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
	LOGFONT	LogViewFont;

	int		HistWidth[MAX_HISTWIDTH];
	int		HistXdiff;
	int		HistYdiff;

	int		PasswordUse;
	int		PasswordUseNext;
	char	PasswordStr[MAX_NAMEBUF];
	BOOL	PasswdLogCheck;

	int		hookMode; // 0:none, 1:transfer(lock) 2:transfer(always:not impl)
	int		hookKind; // 0:general, 1:slack

	U8str	hookUrl;
	U8str	hookBody;

	U8str	slackHost;
	U8str	slackKey;
	U8str	slackChan;
	U8str	slackToken;
	U8str	slackIcon;

	TListEx<UrlObj>		urlList;
	BOOL				DialUpCheck;
	TListEx<AddrObj>	DialUpList;
	TBrList				brList;
	char				*execDir;

	TListEx<UsersObj>	lruUserList;
	int					lruUserMax;

	TListEx<TUpLogObj>	upLogList;

	Cfg(const Addr &_nicAddr, int _portNo);
	~Cfg();
	enum PART { FIND };

	BOOL	ReadRegistry(void);
	BOOL	WriteRegistry(int ctl_flg = CFG_ALL);
	void	GetRegName(char *buf, Addr nic_addr, int port_no);
	void	GetSelfRegName(char *buf);
	BOOL	GetBaseDir(char *dir) { return GetParentDirU8(LogFile, dir); }
	BOOL	GetBaseDirW(WCHAR *dir) {
				Wstr w(LogFile);
				return GetParentDirW(w.s(), dir);
			}
	BOOL	SavePacket(const MsgBuf *msg, const char *head, ULONG img_base);
	BOOL	UpdatePacket(const MsgBuf *msg, const char *auto_saved);
	BOOL	LoadPacket(MsgMng *msgMng, int idx, MsgBuf *msg, char *head,
				ULONG *img_base, char *auto_saved);
	BOOL	DeletePacket(const MsgBuf *msg);
	BOOL	IsSavedPacket(const MsgBuf *msg);
	BOOL	CleanupPackets();

	BOOL	SaveShare(UINT packet_no, const IPDict &dict);
	BOOL	LoadShare(int idx, IPDict *dict);
	BOOL	DeleteShare(UINT packet_no);

	int		GetCapa() {
		return	pub[KEY_1024].Capa() | pub[KEY_2048].Capa();
	}
	BOOL	IPDictEnabled() {
		return	(pub[KEY_2048].Capa() & IPMSG_SIGN_SHA256) ? TRUE : FALSE;
	}
};

#endif

