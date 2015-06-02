/*	@(#)Copyright (C) H.Shirouzu 2013-2014   senddlg.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2014-04-14(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SENDDLG_H
#define SENDDLG_H

enum SendStatus { ST_GETCRYPT=0, ST_MAKECRYPTMSG, ST_MAKEMSG, ST_SENDMSG, ST_DONE };
class SendEntry {
	Host		*host;
	SendStatus	status;
	UINT		command;
	char		*msg;
	int			msgLen;

// For Fragment UDP packet checksum problem (some NIC has this problem)
// Last fragment payload is smaller than 64, some NIC generate wrong checksum.
#define UDP_CHECKSUM_FIXBUF 64

public:
	SendEntry() { msg = NULL; host = NULL; }
	~SendEntry() { delete [] msg; if (host && host->RefCnt(-1) == 0) delete host; }
	void SetMsg(char *_msg, int len) {
		msgLen=len;
		msg = new char[len + UDP_CHECKSUM_FIXBUF];
		memcpy(msg, _msg, len);
		memset(msg + len, 0, UDP_CHECKSUM_FIXBUF);
	}
	const char *Msg(void) { return msg; }
	int MsgLen(bool udp_checksum_fix=false) { return msgLen + (udp_checksum_fix ? 64 : 0); }
	void SetHost(Host *_host) { (host = _host)->RefCnt(1); }
	Host *Host(void) { return host; }
	void SetStatus(SendStatus _status) { status = _status; }
	SendStatus Status(void) { return status; }
	void SetCommand(UINT _command) { command = _command ; }
	UINT Command(void) { return command; }
};

class TRecvDlg;

class TSendDlg : public TListDlg {
protected:
	MsgMng		*msgMng;
	Cfg			*cfg;
	LogMng		*logmng;
	DWORD		recvId;
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
	char		filterStr[MAX_NAMEBUF];

	ULONG		packetNo;
	int			packetLen;
	UINT_PTR	timerID;
	UINT		retryCnt;

// display
	static HFONT	hListFont;
	static HFONT	hEditFont;
	static LOGFONT	orgFont;

	enum		send_item {
					host_item=0, member_item, refresh_item, camera_item,
					ok_item, edit_item, secret_item,
					menu_item, passwd_item, file_item, max_senditem
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
	TImageWin		imageWin;	// Image Area Selection for capture
//	HMENU			hCurMenu;

	void	SetFont(BOOL force_reset=FALSE);
	void	SetSize(void);
	void	SetMainMenu(HMENU hMenu);
	void	PopupContextMenu(POINTS pos);
	void	GetOrder(void);
	void	GetSeparateArea(RECT *sep_rc);
	BOOL	IsSeparateArea(int x, int y);

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
	BOOL	IsSendFinish(void);
	void	InitializeHeader(void);
	void	GetListItemStr(Host *host, int item, char *buf);
	BOOL	IsFilterHost(Host *host);

public:
	TSendDlg(MsgMng *_msgmng, ShareMng *_shareMng, THosts *_hosts, Cfg *cfg,
			 LogMng *logmng, TRecvDlg *recvDlg=NULL, TWin *parent=NULL);
	virtual ~TSendDlg();

	DWORD	GetRecvId(void) { return recvId; }
	void	AddHost(Host *host);
	void	ModifyHost(Host *host);
	void	DelHost(Host *host);
	void	DelAllHost(void);
	BOOL	IsSending(void);
	BOOL	SendFinishNotify(HostSub *hostSub, ULONG packet_no);
	BOOL	SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa);
	BOOL	SelectFilterHost(void);
	int		FilterHost(char *filterStr);
	void	InsertBitmapByHandle(HBITMAP hBitmap, int pos=-1) {
				editSub.InsertBitmapByHandle(hBitmap, pos);
			}
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

