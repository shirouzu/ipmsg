/*	@(#)Copyright (C) H.Shirouzu 2013-2018   histdlg.h	Ver4.90 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: History Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2018-09-12(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef HISTDLG_H
#define HISTDLG_H

struct HistObj : public THashObj {
	HistObj	*prev = NULL;
	HistObj	*next = NULL;
	HistObj	*lruPrev = NULL;
	HistObj	*lruNext = NULL;
	int		packetNo = 0;
	bool	delayed = false;
	HostSub	hostSub;
	U8str	info;
	U8str	sdate;
	U8str	odate;
	U8str	msg;
	HistObj() : THashObj() {}
};

class HistHash : public THashTbl {
protected:
	HistObj	*top = NULL;
	HistObj	*end = NULL;
	HistObj	*lruTop = NULL;
	HistObj	*lruEnd = NULL;
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

struct HistNotify {
	HostSub		*hostSub = NULL;
	ULONG		packetNo = 0;
	const char	*msg = NULL;
	bool		delayed = false;
};

class THistDlg : public TDlg {
protected:
	Cfg			*cfg = NULL;
	THosts		*hosts = NULL;
	HistHash	histHash;
	int			unOpenedNum = 0;
	bool		openedMode = false;

	TListHeader	histListHeader;
	TListViewEx	histListView;
	HFONT		hListFont = NULL;
	void		DelItem(HistObj *obj);

public:
	THistDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent = NULL);
	virtual ~THistDlg();

	virtual BOOL	Create(HINSTANCE hI = NULL);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvDestroy();
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual void	SendNotify(HostSub *hostSub, ULONG packetNo, const char *msg,
								bool delayed=false);
	virtual void	OpenNotify(HostSub *hostSub, ULONG packetNo, const char *notify=NULL);
	virtual void	SaveColumnInfo();
	virtual void	SetTitle();
	virtual void	SetFont();
	virtual void	SetHeader();
	virtual void	SetData(HistObj *obj);
	virtual void	SetAllData();
	virtual void	ClearData();
	virtual int		MakeHistInfo(HostSub *hostSub, ULONG packet_no, char *buf);
	virtual void	Show(int mode = SW_NORMAL);
	virtual int		UnOpenedNum() { return unOpenedNum; }
	virtual void	SetMode(bool is_opened) {
		if (is_opened == openedMode) return;
		if (hWnd)	PostMessage(WM_COMMAND, OPENED_CHECK, 0);
		else		openedMode = is_opened;
	}
};

#endif

