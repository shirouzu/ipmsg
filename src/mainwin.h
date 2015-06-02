/*	@(#)Copyright (C) H.Shirouzu 2013-2015   mainwin.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Main Window
	Create					: 2013-03-03(Sun)
	Update					: 2015-06-02(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MAINWIN_H
#define MAINWIN_H

class SendFileObj : public TListObj {
public:
	SendFileObj() { Init(); }
	void Init() {
		memset(&conInfo, 0, offsetof(SendFileObj, path) - offsetof(SendFileObj, header));
		header[0] = path[0] = 0;
	}

	VBuf		vbuf;
	AES			aes;

	/* start of memset(0) */
	ConnectInfo	*conInfo;
	FileInfo	*fileInfo;
	FILETIME	attachTime;

	UINT		command;
	BOOL		isDir;
	_int64		fileSize;
	_int64		offset;
	HANDLE		hFile;
	HANDLE		hThread;

	Host		*host;
	int			packetNo;
	FileStatus	status;

	int			dirCnt;
	HANDLE		*hDir;	// FindFirst handle array
	int			headerLen;
	int			headerOffset;
	/* memset(0) */

	char		header[MAX_BUF];	// for dirinfo
	char		path[MAX_PATH_U8];
	WIN32_FIND_DATA_U8	fdata;
};

class AnsQueueObj : public TListObj {
public:
	AnsQueueObj() { command = 0; }
	HostSub	hostSub;
	int		command;
};

class TMainWin : public TWin {
protected:
	static HICON	hMainIcon;
	static HICON	hRevIcon;
	static TMainWin	*mainWin;	// for thread proc

	TListEx<TSendDlg>		sendList;
	TListEx<TRecvDlg>		recvList;
	TListEx<TMsgDlg>		msgList;
	TListEx<SendFileObj>	sendFileList;
	TListEx<ConnectInfo>	connList;
	THosts					hosts;

	TSetupDlg		*setupDlg;
	TAboutDlg		*aboutDlg;
	TShareStatDlg	*shareStatDlg;
	TAbsenceDlg		*absenceDlg;
	THistDlg		*histDlg;
//	TLogView		*logView;
	TRemoteDlg		*remoteDlg;
	MsgMng			*msgMng;
	LogMng			*logmng;
	ShareMng		*shareMng;
	Cfg				*cfg;
	TRecycleListEx<AnsQueueObj> *ansList;
	TBrList			brListEx;

	int			portNo;
	int			memberCnt;
	Time_t		refreshStartTime;
	Time_t		entryStartTime;
	UINT		entryTimerStatus;
	UINT		reverseTimerStatus;
	UINT		reverseCount;
	UINT_PTR	ansTimerID;
	UINT		TaskBarCreateMsg;
	UINT		TaskBarButtonMsg;
	UINT		TaskBarNotifyMsg;
	char		*className;
	BOOL		terminateFlg;
	BOOL		activeToggle;
	DWORD		writeRegFlags;
	enum TrayMode { TRAY_NORMAL, TRAY_RECV, TRAY_OPENMSG };
	TrayMode	trayMode;
	char		trayMsg[MAX_BUF];

#define MAX_PACKETLOG	16
	struct {
		Addr		addr;
		int			port;
		ULONG		no;
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
	static UINT WINAPI SendFileThread(void *_sendFileObj);
	BOOL	SendFile(SendFileObj *obj);
	BOOL	SendMemFile(SendFileObj *obj);
	BOOL	SendDirFile(SendFileObj *obj);
	BOOL	CloseSendFile(SendFileObj *obj);
	BOOL	EndSendFile(SendFileObj *obj);

	void	BroadcastEntry(ULONG mode);
	void	BroadcastEntrySub(Addr addr, int portNo, ULONG mode);
	void	BroadcastEntrySub(HostSub *hostSub, ULONG mode);
	void	Terminate(void);

	BOOL	SendDlgOpen(DWORD recvid=0);
	void	SendDlgHide(DWORD sendid);
	void	SendDlgExit(DWORD sendid);
	BOOL	RecvDlgOpen(MsgBuf *msg);
	void	RecvDlgExit(DWORD recvid);
	void	MsgDlgExit(DWORD msgid);
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
	void	ChangeMulticastMode(int new_mcast_mode);

	BOOL	SetAnswerQueue(HostSub *hostSub);
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
	TMainWin(Addr _nicAddr, int _portNo=IPMSG_DEFAULT_PORT, TWin *_parent = NULL);
	virtual ~TMainWin();

	virtual BOOL	CreateU8(LPCSTR className=NULL, LPCSTR title="",
						DWORD style=(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
						DWORD exStyle=0, HMENU hMenu=NULL);
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
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);

	static	HICON	GetIPMsgIcon(void);
};

#endif

