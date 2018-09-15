/*	@(#)Copyright (C) H.Shirouzu 2013-2018   senddlg.h	Ver4.90 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SENDDLG_H
#define SENDDLG_H

class SendEntry;
class TRecvDlg;

typedef std::vector<HostSub> HostVec;
typedef std::vector<std::shared_ptr<U8str>> U8SVec;

struct ReplyInfo {
	TRecvDlg		*recvDlg;
	const HostVec	*replyList;
	U8str			*body;
	enum PosMode { NONE, POS_RIGHT, POS_MID, POS_MIDDOWN } posMode;
	DWORD			foreDuration;
	bool			isMultiRecv;

	const U8SVec	*fileList;
	HWND			cmdHWnd;
	int64			cmdFlags;

	ReplyInfo(PosMode _posMode=NONE) {
		recvDlg = NULL;
		replyList = NULL;
		body = NULL;
		posMode = _posMode;
		foreDuration = 0;
		isMultiRecv = FALSE;
		fileList = NULL;
		cmdHWnd = NULL;
		cmdFlags = 0;
	}
};

class TSendDlg : public TListDlg {
protected:
	MsgMng		*msgMng;
	Cfg			*cfg;
	LogMng		*logmng;
	DWORD		recvId;
	HWND		hRecvWnd;
	std::vector<HostSub> replyList;
	ReplyInfo::PosMode posMode;
	DWORD		foreDuration;
	bool		isMultiRecv;

	MsgBuf		msg;
	ShareMng	*shareMng;
	ShareInfo	*shareInfo;
	SendMng		*sendMng;

	THosts		*hosts;
	TFindDlg	*findDlg;
	std::vector<Host *> hostVec;
	int			memberCnt;

	char		selectGroup[MAX_NAMEBUF];
	char		filterStr[MAX_NAMEBUF];

	ULONG		packetNo;
	int			packetLen;
	UINT_PTR	timerID;
	bool		retryEx;
	bool		listConfirm;
	bool		sendRecvList;
	HWND		cmdHWnd;
	int64		cmdFlags;

// display
	static HFONT	hListFont;
	static HFONT	hEditFont;
	static LOGFONT	orgFont;

	enum		send_item {
					host_item=0, member_item, refresh_item, capture_item,
					ok_item, edit_item, secret_item, menu_item, 
					passwd_item, repfil_item,
					file_item, max_senditem
				};
	WINPOS		item[max_senditem];

	int			currentMidYdiff;
	int			dividYPos;
	int			lastYPos;
	int			listOperateCnt;
	bool		captureMode;
	bool		hiddenDisp;
	bool		repFilDisp;

	int			maxItems;
	UINT		ColumnItems;
	int			FullOrder[MAX_SENDWIDTH];
	int			items[MAX_SENDWIDTH];
	bool		lvStateEnable;
	bool		sortRev;
	int			sortItem;

	TEditSub		editSub;
	TListHeader		hostListHeader;
	TListViewEx		hostListView;
	TImageWin		imageWin;	// Image Area Selection for capture
	TSubClassCtl	sendBtn;
	TSubClassCtl	captureBtn;
	TSubClassCtl	refreshBtn;
	TSubClassCtl	menuCheck;
	TSubClassCtl	memCntText;
	TSubClassCtl	repFilCheck;
//	HMENU			hCurMenu;

	void	AttachItemWnd();
	void	SetupItemIcons();
	void	SetFont(bool force_reset=FALSE);
	void	SetSize(void);
	void	SetMainMenu(HMENU hMenu);
	void	PopupContextMenu(POINTS pos);
	void	GetOrder(void);
	void	GetSeparateArea(RECT *sep_rc);
	bool	IsSeparateArea(int x, int y);
	bool	OpenLogView(bool is_dblclk=FALSE);
	void	SetReplyInfoTip();

	void	SetQuoteStr(LPCSTR str, LPCSTR quoteStr);
	bool	SelectHost(HostSub *hostSub, bool force=FALSE, bool byAddr=TRUE);
	void	DisplayMemberCnt(void);
	void	ReregisterEntry(bool keep_select=FALSE);
	UINT	GetInsertIndexPoint(Host *host);
	int		CompareHosts(Host *host1, Host *host2);
	int		GroupCompare(Host *host1, Host *host2);
	int		SubCompare(Host *host1, Host *host2);
	void	AddLruUsers(SendMsg *sendMsg);
	bool	Send(void);
	bool	SendMsgSetClip(void);
	void	SendMsgSetUsers(SendMsg *sendMsg, bool is_multi, ULONG cmd, ULONG opt,
							ULONG *osum, bool *use_sign, bool *is_delay);
	void	InitializeHeader(void);
	void	GetListItemStr(Host *host, int item, char *buf);
	bool	IsFilterHost(Host *host);
	bool	RestrictShare();
	void	CheckDisp();
	bool	IsReplyListConsist();
	int		GetHostIdx(Host *host, bool *is_selected=NULL);
	void	Finished();

public:
	TSendDlg(MsgMng *_msgmng, ShareMng *_shareMng, SendMng *_sendMng, THosts *_hosts, Cfg *cfg,
			 LogMng *logmng, ReplyInfo *rInfo=NULL, TWin *parent=NULL);
	virtual ~TSendDlg();

	DWORD	GetRecvId(void) { return recvId; }
	void	SetLvi(LV_ITEMW *lvi, int idx, Host *host, bool is_sel);
	void	AddHost(Host *host, bool is_sel=FALSE, bool disp_upd=TRUE);
	void	ModifyHost(Host *host, bool disp_upd=TRUE);
	void	DispUpdate();
	void	DelHost(Host *host, bool *is_sel=NULL, bool disp_upd=TRUE);
	void	DelAllHost(void);
	void	ModifyAllHost(void);
//	bool	DetachParent(HWND hTarget=NULL);
	bool	SelectFilterHost(void);
	int		FilterHost(char *filterStr);
	void	InsertBitmapByHandle(HBITMAP hBitmap, int pos=-1) {
				editSub.InsertBitmapByHandle(hBitmap, pos);
			}
	bool	AppendDropFilesAsText(const char *path);

	static HFONT	GetEditFont() { return hEditFont; }

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
	virtual BOOL	EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg);
	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);

	virtual BOOL	EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);
	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void	Show(int mode = SW_SHOWDEFAULT);
};

#endif

