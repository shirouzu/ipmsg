/*	@(#)Copyright (C) H.Shirouzu 2013-2017   mainwin.h	Ver4.61 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Main Window
	Create					: 2013-03-03(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MAINWIN_H
#define MAINWIN_H

struct SendFileObj : public TListObj {
	SendFileObj() {
		Init();
	}
	void Init() {
		memset(&conInfo, 0, offsetof(SendFileObj, path) - offsetof(SendFileObj, header));
		header[0] = 0;
		path[0] = 0;
		fileCapa = 0;
		needStop = FALSE;
	}

	VBuf		vbuf;
	AES			aes;
	UINT		fileCapa;

	/* start of memset(0) */
	ConnectInfo	*conInfo;
	FileInfo	*fileInfo;
	time_t		attachTime;

	UINT		command;
	BOOL		isDir;
	int64		fileSize;
	int64		offset;
	HANDLE		hFile;
	HANDLE		hThread;

	Host		*host;
	int			packetNo;
	FileStatus	status;
	BOOL		needStop;

	int			dirCnt;
	HANDLE		*hDir;	// FindFirst handle array
	int			headerLen;
	int			headerOffset;
	/* memset(0) */

	char		header[MAX_BUF];	// for dirinfo
	uint64		auth;
	char		path[MAX_PATH_U8];
	WIN32_FIND_DATA_U8	fdata;
};

struct AnsQueueObj : public TListObj {
	AnsQueueObj() { command = 0; }
	HostSub	hostSub;
	int		command;
};

template<class T>
class TDlgListEx : public TListEx<T> {
public:
	T *Search(DWORD id) {
		for (T *d=TopObj(); d; d=NextObj(d)) {
			if (d->twinId == id) return d;
		}
		return NULL;
	}
	void ActiveListDlg(BOOL active=TRUE) {
		for (T *dlg = TopObj(); dlg; dlg = NextObj(dlg)) {
			dlg->ActiveDlg(active);
		}
	}
	void DeleteListDlg() {
		while (auto dlg = TopObj()) {
			DelObj(dlg);
			delete dlg;
		}
	}
};

struct PktData {
	PktData() {
		offset = 0;
		cmd = 0;
	}
	ConnectInfo	info;
	DynBuf		buf;
	size_t		offset;
	HostSub		hostSub;
	int64		cmd;
};

enum UpdFlags {
	UPD_NONE		= 0x0000,
	UPD_STEP		= 0x0001,
	UPD_CONFIRM		= 0x0002,
	UPD_BUSYCONFIRM	= 0x0004,
	UPD_SKIPNOTIFY	= 0x0008,
	UPD_SHOWERR		= 0x0010,
};

struct UpdateData {
	DWORD	flags;
	HWND	hWnd;
	U8str	ver;
	U8str	path;
	int64	size;
	DynBuf	hash;
	DynBuf	dlData;

	UpdateData() {
		Init();
	}
	void Init() {
		flags = UPD_NONE;
		hWnd = NULL;
		DataInit();
	}
	void DataInit() {
		ver.Init();
		path.Init();
		size = 0;
		hash.Free();
		dlData.Free();
	}
};

struct UpdateReply {
	U8str	ver;
	U8str	errMsg;
	BOOL	needUpdate;
};

class TUpdConfim : public TDlg {
	U8str	body;
public:
	TUpdConfim(TWin *_parent) : TDlg(UPDATE_DLG, _parent) {}
	BOOL	Create(const char *_body) {
		body = _body;
		return TDlg::Create();
	}
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
};

#define MAX_CYCLE_ICON 8

enum UpdIdleState { UPDI_IDLE=0, UPDI_WIN=1, UPDI_TRANS=2 };

class TMainWin : public TWin {
public:
	struct Param {
		Addr	nicAddr;
		int		portNo;
		BOOL	isFirstRun;
		BOOL	isInstalled;
		BOOL	isUpdated;
		BOOL	isUpdateErr;
		BOOL	isHistory;

		Param() {
			portNo		= 0;
			isFirstRun	= FALSE;
			isInstalled	= FALSE;
			isUpdated	= FALSE;
			isUpdateErr	= FALSE;
			isHistory	= FALSE;
		}
	};

protected:
	Param			param;

//	static HICON	hMainIcon; // define in maiwin.cpp
//	static HICON	hMainBigIcon;

	static HICON	hRevIcon;
	static HICON	hRevBigIcon;
	HICON			hCycleIcon[MAX_CYCLE_ICON];
	HACCEL			hHelpAccel;
	HWND			hHelp;

	TDlgListEx<TSendDlg>	sendList;
	TDlgListEx<TRecvDlg>	recvList;
	TDlgListEx<TMsgDlg>		msgList;
	TDlgListEx<SendFileObj>	sendFileList;
	TDlgListEx<ConnectInfo>	connList;
	TListEx<TLogView>		logViewList;
	THosts					hosts;

	enum { FW_OK, FW_NEED, FW_PENDING, FW_CANCEL } fwMode;

	TSetupDlg		*setupDlg;
	TAboutDlg		*aboutDlg;
	TShareStatDlg	*shareStatDlg;
	TAbsenceDlg		*absenceDlg;
	THistDlg		*histDlg;
	TRemoteDlg		*remoteDlg;
	TFwDlg			*fwDlg;
	TMsgBox			*msgBox;
	MsgMng			*msgMng;
	LogMng			*logmng;
	ShareMng		*shareMng;
	Cfg				*cfg;
	TRecycleListEx<AnsQueueObj> *ansList;
	TBrList			brListEx;	// master & dialup & 手動brlist込み
	TBrList			brListOrg;	// nic情報からのbrのみ
	Addr			selfAddr;	// default gatewayの自アドレス
	std::vector<Addr> allSelfAddrs;

	int			portNo;
	int			memberCnt;
	time_t		refreshStartTime;
	time_t		entryStartTime;
	UINT		entryTimerStatus;
	UINT		reverseTimerStatus;
	UINT		reverseCount;
	UINT_PTR	ansTimerID;
	UINT_PTR	pollTimerID;
	UINT		pollTick;

	UINT		TaskBarCreateMsg;
	UINT		TaskBarButtonMsg;
	UINT		TaskBarNotifyMsg;
	char		*className;
	BOOL		terminateFlg;
	BOOL		activeToggle;
	DWORD		writeRegFlags;

	BOOL		lastExitTick;
	BOOL		isFirstBroad;
	DWORD		desktopLockCnt;
	DWORD		monitorState;

	struct RecvCmd {
		HWND	hWnd;
		int64	flags;
	};
	std::vector<RecvCmd>	recvCmdVec;

	// Agent動作用
	UINT		brDirAgentTick;
	UINT		brDirAgentLast;
	UINT		brDirAgentLimit;
	Addr		brDirAddr;

	std::map<SOCKET, std::shared_ptr<PktData>> tcpMap;

#ifdef IPMSG_PRO
#define MAIN_HEAD
#include "mainext.dat"
#undef MAIN_HEAD
#else
// DirMaster用
#define DIR_BASE_TICK	2000
#define DIR_BASE_SEC	(DIR_BASE_TICK / 1000)

	enum DirPhase { DIRPH_NONE, DIRPH_START };
	int			dirPhase;
	DWORD		dirTick;
	enum DirEvFlag { DIRF_NOOP=1, DIRF_BR=2 };
	DWORD		dirEvFlags;
	THosts		allHosts;
	std::map<Addr, std::shared_ptr<THosts>>	segMap;

	BOOL	CheckMaster(MsgBuf *msg) { return FALSE; }
	BOOL	MasterPubKey(Host *host);
	BOOL	CheckDirPacket(MsgBuf *msg, BOOL addr_check=FALSE);
	BOOL	SetBrDirAddr(MsgBuf *msg);
	BOOL	GetWrappedMsg(MsgBuf *msg, MsgBuf *wmsg, BOOL force_master=FALSE);

	BOOL	SendAgentReject(HostSub *hostSub);
	BOOL	MsgDirBroadcast(MsgBuf *msg);
	BOOL	MsgDirPollAgent(MsgBuf *msg);
	BOOL	MsgDirAgentReject(MsgBuf *msg);
	BOOL	MsgDirPacket(MsgBuf *msg, BOOL is_agent=FALSE);
	BOOL	MsgDirRequest(MsgBuf *msg);
	BOOL	MsgDirAgentPacket(MsgBuf *msg);

	BOOL	UpdateSegs(MsgBuf *msg, const Addr &net_addr);

	BOOL	DirPollCore(MsgBuf *msg);
	void	DirBroadcastCore(MsgBuf *msg);
	void	ReplyDirBroadcast();
	void	SendHostListDict(HostSub *hostSub);

	BOOL	AgentDirHost(Host *host, BOOL is_direct=FALSE);
	BOOL	AddHostListDict(IPDict *dict);

	UpdateData	updData;
	DWORD		updRetry;
	TUpdConfim	*updConfirm;

#endif
	BOOL	PollSend();
	BOOL	CleanupProc();
	void	LoadStoredPacket();
	void	RemoveMessageFilters();

	enum TrayMode { TRAY_NORMAL, TRAY_INST, TRAY_UPDATE, TRAY_RECV, TRAY_OPENMSG };
	TrayMode	trayMode;
	char		trayMsg[MAX_BUF];
	int			msgDuration;

#define MAX_PACKETLOG	32
	struct PktLog {
		ULONG	no;
		User	u;
	} packetLog[MAX_PACKETLOG];
	int		packetLogCnt;

	BOOL	IsLastPacket(MsgBuf *msg);
	void	SetIcon(BOOL is_recv=FALSE, int cnt=0);
	void	ReverseIcon(BOOL startFlg);

	void	EntryHost(void);
	void	ExitHost(void);

	void	Popup(UINT resId);
	BOOL	PopupCheck(void);
	BOOL	AddAbsenceMenu(HMENU hMenu, int insertIndex);
	void	ActiveChildWindow(BOOL hide=FALSE);
	BOOL	TaskTray(int nimMode, HICON hSetIcon = NULL, LPCSTR tip = NULL);
	BOOL	BalloonWindow(TrayMode _tray_mode, LPCSTR msg=NULL, LPCSTR title=NULL,
		DWORD timer=0, BOOL force_icon=FALSE);
	BOOL	UdpEvent(LPARAM lParam);
	BOOL	UdpEventCore(MsgBuf *msg);

	BOOL	RecvTcpMsg(SOCKET sd);
	BOOL	TcpEvent(SOCKET sd, LPARAM lParam);
	BOOL	TcpDirEvent(SOCKET sd, LPARAM lParam);
	void	StopSendFile(int packet_no);

	BOOL	SendDirTcp(Host *host, IPDict *dict);
	BOOL	PollReqAccept(HostSub *hostSub, BOOL result);
	BOOL	CleanupDirTcp();

	BOOL	CheckConnectInfo(ConnectInfo *conInfo);
	SendFileObj *FindSendFileObj(SOCKET sd);
	BOOL	StartSendFile(SOCKET sd, ConnectInfo *conInfo, AcceptFileInfo *fileInfo);
	BOOL	OpenSendFile(const char *fname, SendFileObj *obj);
	static UINT WINAPI SendFileThread(void *_sendFileObj);
	BOOL	SendFile(SendFileObj *obj);
	BOOL	SendMemFile(SendFileObj *obj);
	BOOL	SendDirFile(SendFileObj *obj);
	BOOL	CloseSendFile(SendFileObj *obj);
	BOOL	EndSendFile(SendFileObj *obj);

	BOOL	TryBroadcastForResume();
	void	InitToastDll();
	void	UnInitToastDll();
	void	ToastHide(void);
	void	Terminate(void);
	BOOL	GetInfoByDomain();

	BOOL	SendDlgOpen(DWORD recvid=0, ReplyInfo *ri=NULL);
	void	SendDlgHide(DWORD sendid);
	void	SendDlgExit(DWORD sendid);
	BOOL	RecvDlgOpenCore(TRecvDlg *dlg, MsgBuf *msg, const char *rep_head, BOOL is_rproc);
	BOOL	RecvDlgOpen(MsgBuf *msg, const char *rep_head=NULL, ULONG clipBase=0,
				const char *auto_saved=NULL);
	void	RecvDlgExit(DWORD recvid);
	void	RecvDlgOpenByViewer(MsgIdent *mi, BOOL is_file_save=FALSE);

	void	MsgDlgExit(DWORD msgid);
	void	MiscDlgOpen(TDlg *dlg);
	void	LogViewOpen(BOOL last_view=FALSE);
	void	LogOpen(void);

	Host	*AddHost(HostSub *hostSub, ULONG command, const char *nickName="",
				const char *groupName="", const char *verInfo="", BOOL byHostList=FALSE);
	void	PostAddHost(Host *host, const char *verInfo, time_t now_time, BOOL byHostList);
	inline	void SetHostData(Host *destHost, HostSub *hostSub, ULONG command, time_t now_time,
				const char *nickName="", const char *groupName="", int priority=DEFAULT_PRIORITY);
	void	DelAllHost(void);
	void	DelHost(HostSub *hostSub);
	void	DelHostSub(Host *host);
	void	RefreshHost(BOOL removeFlg=TRUE);
	void	SetCaption(void);
	void	SendHostList(MsgBuf *msg);
	BOOL	AddHostListCore(char *buf, BOOL is_master, int *_cont_cnt=NULL, int *_host_cnt=NULL);
	void	AddHostList(MsgBuf *msg);

	void	GetSelfHost(LogHost *host);

	void	ActiveDlg(TDlg *dlg, BOOL active = TRUE);

	void	InitIcon(void);
	void	ControlIME(TWin *win, BOOL on);
	void	MakeBrListEx(void);
	void	ChangeMulticastMode(int new_mcast_mode);

	BOOL	FwInitCheck();
	BOOL	FwCheckProc();

	BOOL	FirstSetup();
	void	SetMainWnd(HWND);

	BOOL	Cmd(HWND targ_wnd);
	BOOL	CmdGetAbs(const IPDict &in, IPDict *out);
	BOOL	CmdSetAbs(const IPDict &in, IPDict *out);
	BOOL	CmdGetHostList(const IPDict &in, IPDict *out);
	BOOL	CmdRefresh(const IPDict &in, IPDict *out);
	BOOL	CmdSendMsg(const IPDict &in, IPDict *out, HWND targ_wnd);
	BOOL	CmdRecvMsg(const IPDict &in, IPDict *out, HWND targ_wnd);
	BOOL	CmdGetState(const IPDict &in, IPDict *out);
	BOOL	CmdTerminate(const IPDict &in, IPDict *out);
	Host	*FindHostForCmd(std::shared_ptr<U8str> u);

#define DESKTOP_LOCKMAX 5
	BOOL	IsLockDetected() {
		return	desktopLockCnt >= DESKTOP_LOCKMAX;
	}
	void	FlushToHook();
	void	SendToHook(MsgBuf *msg, BOOL is_attached=FALSE);

#ifdef IPMSG_PRO
#else
// Server系
	void	DirTimerMain();
	void	ClearDirAll();

	BOOL	DirRefreshAgent();
	void	DirSendAgent(IPDict *in_dict);
	void	DirSendDirect(IPDict *dict);
	void	DirSendAll(IPDict *in_dict);
	void	DirSendAnsList(const std::list<Host *> &host_list);

	void	DirSendNoOpe();
	void	DirSendBroadcast();
	void	DirSendAgentBroad(BOOL force_agent=FALSE);
	void	DirClean();
	void	MsgDirPoll(MsgBuf *msg);
	BOOL	UpdateSegs(Host *host);
	BOOL	DirDelHost(Host *host, BOOL is_notify=TRUE);
	BOOL	DirUpdateHost(const Host &s, Host *d);
	void	DelSegHost(Host *host, BOOL allow_segdel=TRUE);
	Host	*DirAddHost(Host *host, BOOL is_poll, BOOL is_notify=TRUE, BOOL *need_notify=NULL);
	void	DirSendHostList(MsgBuf *msg, Host *host);
	int		GetAgent(MsgBuf *msg, std::shared_ptr<THosts> *seg_h);
	void	MsgDirAnsBroad(MsgBuf *msg, BOOL is_ansbroad);
	void	DirSendAnsHosts(const std::list<Host *> &hlist);
	void	DirSendAnsOne(Host *host);
	void	MsgAnsListDict(MsgBuf *msg);

	void	UpdateCheckTimer();

	BOOL	UpdateCheckResCore(InetReqReply *irr, BOOL *need_update);
	void	UpdateCheckRes(InetReqReply *irr);
	void	UpdateDlRes(InetReqReply *irr);
	BOOL	UpdateProc(U8str *errMsg=NULL);
#endif
	BOOL	UpdateFileCleanup();

	BOOL	SetAnswerQueue(HostSub *hostSub);
	void	ExecuteAnsQueue(void);
	BOOL	CheckSelfAlias(MsgBuf *msg);
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
	TMainWin(Param *_param, TWin *_parent = NULL);
	virtual ~TMainWin();

	virtual BOOL	CreateU8(LPCSTR className=NULL, LPCSTR title="",
						DWORD style=(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN),
						DWORD exStyle=0, HMENU hMenu=NULL);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvEndSession(BOOL nSession, BOOL nLogOut);
	virtual BOOL	EvPowerBroadcast(WPARAM pbtEvent, LPARAM pbtData);
	virtual BOOL	EvQueryOpen(void);
	virtual BOOL	EvHotKey(int hotKey);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);

	char	*GetNickNameEx(void);
	ULONG	HostStatus(void);
	Addr	GetSelfAddr() { return selfAddr; }
	std::vector<Addr> GetAllSelfAddrs() { return allSelfAddrs; }

	BOOL	PreProcMsgFilter(MSG *msg);

	void	BroadcastEntry(ULONG mode);
	BOOL	BroadcastEntrySub(Addr addr, int portNo, ULONG mode, IPDict *ipdict=NULL);
	BOOL	BroadcastEntrySub(HostSub *hostSub, ULONG mode, IPDict *ipdict=NULL);

#ifdef IPMSG_PRO
#else
	BOOL	UpdateCheck(DWORD flags, HWND targ_hwnd=NULL);
	BOOL	UpdateExec();
	BOOL	UpdateWritable(BOOL force=FALSE);
#endif
	UpdIdleState CheckUpdateIdle();
	void	ShowUpdateDlg();

	BOOL	RequireHookTrans();
};

void	GenUpdateFileName(char *path, BOOL is_tmp=FALSE);
HICON	GetIPMsgIcon(HICON *hBigIcon=NULL);

#endif


