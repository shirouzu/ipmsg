/*	@(#)Copyright (C) H.Shirouzu 2013-2017   recvdlg.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Receive Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef RECVDLG_H
#define RECVDLG_H

enum FileStatus {
	FS_DIRFILESTART, FS_OPENINFO, FS_FIRSTINFO, FS_NEXTINFO, FS_MAKEINFO, FS_TRANSINFO,
	FS_TRANSFILE, FS_ENDFILE, FS_MEMFILESTART, FS_COMPLETE, FS_ERROR
};

struct RecvFileObj {
	RecvFileObj() {
		Init();
	}
	void Init() {
		memset(this, 0, sizeof(*this));
	}

	ConnectInfo	*conInfo;
	FileInfo	*fileInfo;

	bool		isDir;
	bool		isClip;
	FileInfo	curFileInfo;

	int64		offset;		// read offset
	int64		woffset;	// written offset
	char		*recvBuf;
	HANDLE		hFile;
	HANDLE		hThread;

	int			infoLen;
	int			dirCnt;

	int64		totalTrans;
	int			totalFiles;
	DWORD		startTick;
	DWORD		lastTick;

	int64		curTrans;
	int			curFiles;

	FileStatus	status;
	AES			aes;

	char		saveDir[MAX_PATH_U8];
	char		path[MAX_PATH_U8];
	char		info[MAX_BUF];	// for dirinfo buffer
	char		u8fname[MAX_PATH_U8];	// for dirinfo buffer
};

struct ClipBuf : public TListObj {
	VBuf	vbuf;
	int		pos;
	bool	finished;
	ClipBuf(int size, int _pos) : vbuf(size), pos(_pos) {
		finished = FALSE;
	}
};

class RecvTransEndDlg : public TDlg {
	RecvFileObj	*fileObj;

public:
	RecvTransEndDlg(RecvFileObj *_fileObj, TWin *_win) :
		TDlg(_fileObj->status == FS_COMPLETE ? TRANSEND_DIALOG : SUSPEND_DIALOG, _win) {
		fileObj = _fileObj;
	}
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

class TRecvDlg : public TListDlg {
public:
	enum SelfStatus { INIT, SHOW, ERR, REMOTE };

protected:
	static int		createCnt;
	static HBITMAP	hDummyBmp;
	MsgMng			*msgMng;
	MsgBuf			msg;
	THosts			*hosts;
	char			head[MAX_LISTBUF];
	RECT			headRect;
	Cfg				*cfg;
	LogMng			*logmng;
	bool			openFlg;
	bool			isRep;	// reproduction
	static HFONT	hHeadFont;
	static HFONT	hEditFont;
	static LOGFONT	orgFont;
	TEditSub		editSub;
	TSubClassCtl	replyBtn;
	TSubClassCtl	logViewBtn;
	TSubClassCtl	recvHead;

	SelfStatus		status;

	int64			autoSavedSize;
	bool			isAutoSave;
	char			autoSaves[MAX_BUF];
	std::vector<HostSub> replyList;
	std::vector<HostSub> recvList;

	RecvFileObj		*fileObj;
	ShareInfo		*shareInfo;
	RecvTransEndDlg *recvEndDlg;
	TListEx<ClipBuf> clipList;
	int				useClipBuf;
	int				useDummyBmp;
//	DynBuf			ulData;

	UINT_PTR	timerID;
	UINT		retryCnt;
	char		readMsgBuf[MAX_LISTBUF];
	ULONG		packetNo;
	UINT		cryptCapa;
	UINT		logOpt;

	enum	recv_item {
				title_item=0, head_item, head2_item, open_item, edit_item, image_item,
				logview_item, ok_item, cancel_item, quote_item, file_item, max_recvitem
			};
	WINPOS	item[max_recvitem];

	bool	InitCliped(ULONG *clipBase);
	bool	InitAutoSaved(const char *auto_saved);

	void 	SetReplyInfo();
	void	SetFont(bool force_reset=FALSE);
	void	SetSize(void);
	void	SetMainMenu(HMENU hMenu);
	void	PopupContextMenu(POINTS pos);
	bool	TcpEvent(SOCKET sd, LPARAM lParam);
	bool	StartRecvFile(void);
	bool	ConnectRecvFile(void);
	static UINT WINAPI RecvFileThread(void *_recvDlg);
	bool	SaveFile(bool auto_save=FALSE, bool is_first=TRUE);
	bool	OpenRecvFile(void);
	bool	RecvFile(void);
	bool	RecvDirFile(void);
	bool	CloseRecvFile(bool setAttr=FALSE);
	bool	EndRecvFile(bool manual_suspend=FALSE);
	void	SetTransferButtonText(void);
	bool	CheckSpecialCommand();
	bool	DecodeDirEntry(char *buf, FileInfo *info, char *u8fname);
	bool	LoadClipFromFile(void);
	bool	AutoSaveCheck(void);

public:
	TRecvDlg(MsgMng *_msgmng, THosts *hosts, Cfg *cfg, LogMng *logmng, TWin *_parent=NULL);
	virtual ~TRecvDlg();

	SelfStatus Init(MsgBuf *_msg, const char *rep_head=NULL, ULONG clipBase=0,
					const char *auto_saved=NULL);
	bool	IsClosable(void) {
				return openFlg &&
					(!fileObj || !fileObj->conInfo) && !clipList.TopObj() && !recvEndDlg;
			}
	bool	IsOpened(void) {
				return	openFlg;
			}
	bool	IsSamePacket(MsgBuf *test_msg);
	bool	SendFinishNotify(HostSub *hostSub, ULONG packet_no);

	virtual BOOL	PreProcMsg(MSG *msg);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);

	virtual void	Show(int mode = SW_NORMAL);
	bool	InsertImages(void);

	SelfStatus	Status() { return status; }
	void		SetStatus(SelfStatus _status);
	static int	GetCreateCnt(void) { return createCnt; }
	time_t		GetRecvTime() { return msg.timestamp; }
	MsgBuf		*GetMsgBuf() { return &msg; }
	bool		GetLogStr(U8str *u);
	bool		IsRep() { return isRep; }

	int			UseClipboard() { return useClipBuf; }
	bool		FileAttached() { return !!shareInfo; }
	int			FileAttacheRemain() { return shareInfo ? shareInfo->fileCnt : 0; }
	bool		IsOpenMsgSending() { return timerID ? TRUE : FALSE; }
	void		CleanupAutoSaveSize();
	bool		IsAutoSaved() { return *autoSaves ? TRUE : FALSE; }
	const std::vector<HostSub> &GetReplyList() const { return replyList; }

	// test
	bool		hookCheck;
};

#endif

