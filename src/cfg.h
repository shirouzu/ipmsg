/*	@(#)Copyright (C) H.Shirouzu 2013-2015   cfg.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Configuration Manager
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef CONFIG_H
#define CONFIG_H

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
//#define FT_DISABLEENC	0x00000002

#define RS_REALTIME		0x00000001

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
//	char	**PrioritySound;

	Addr	nicAddr;
	int		portNo;
	int		lcid;
	BOOL	NoPopupCheck;
	int		OpenCheck;
	BOOL	NoErase;
	BOOL	NoBeep;
	BOOL	NoTcp;
	BOOL	OneClickPopup;
	BOOL	BalloonNotify;
	BOOL	TrayIcon;
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
	BOOL	LetterKey;

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
	int		Debug;

	int		RemoteGraceSec;
	char	RemoteReboot[MAX_NAMEBUF];
	int		RemoteRebootMode;
	char	RemoteExit[MAX_NAMEBUF];
	int		RemoteExitMode;

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

	char	PasswordStr[MAX_NAMEBUF];
	BOOL	PasswdLogCheck;

	TListEx<UrlObj>		urlList;
	BOOL				DialUpCheck;
	TListEx<AddrObj>	DialUpList;
	TBrList				brList;
	char				*execDir;

	TListEx<UsersObj>	lruUserList;
	int					lruUserMax;

	Cfg(Addr _nicAddr, int _portNo);
	~Cfg();
	enum PART { FIND };

	BOOL	ReadRegistry(void);
	BOOL	WriteRegistry(int ctl_flg = CFG_ALL);
	void	GetRegName(char *buf, Addr nic_addr, int port_no);
	void	GetSelfRegName(char *buf);
};

inline int GetLocalCapa(Cfg *cfg) {
	return	cfg->pub[KEY_512].Capa() | cfg->pub[KEY_1024].Capa() | cfg->pub[KEY_2048].Capa();
}

#endif
