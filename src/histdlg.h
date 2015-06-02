/*	@(#)Copyright (C) H.Shirouzu 2013-2015   histdlg.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: History Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef HISTDLG_H
#define HISTDLG_H

struct HistObj : public THashObj {
	HistObj	*prev;
	HistObj	*next;
	HistObj	*lruPrev;
	HistObj	*lruNext;
	char	info[MAX_LISTBUF];
	char	user[MAX_LISTBUF];
	char	sdate[MAX_NAMEBUF];
	char	odate[MAX_NAMEBUF];
	char	pktno[MAX_NAMEBUF];
	HistObj() : THashObj() {
		*info = *user = *sdate = *odate = *pktno = 0;
		prev = next = lruPrev = lruNext = NULL;
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

#endif

