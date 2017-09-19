/*	@(#)Copyright (C) H.Shirouzu 2013-2016   miscdlg.h	Ver4.10 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2016-11-01(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef MISCDLG_H
#define MISCDLG_H

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
	int		focusIndex;
	BOOL	letterKey;   // Select item by first letter key

public:
	TListViewEx(TWin *_parent);

	virtual	int		GetFocusIndex(void) { return focusIndex; }
	virtual	void	SetFocusIndex(int index) { focusIndex = index; }
	virtual void	LetterKey(BOOL on) { letterKey = on; }

	virtual	int		InsertColumn(int idx, const char *title, int width, int fmt=LVCFMT_LEFT);
	virtual	BOOL	DeleteColumn(int idx);
	virtual int		GetColumnWidth(int idx);
	virtual	void	DeleteAllItems();
	virtual	int		InsertItem(int idx, const char *str, LPARAM lParam=0);
	virtual	int		SetSubItem(int idx, int subIdx, const char *str);
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
	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
	virtual BOOL	EvChar(WCHAR code, LPARAM keyData);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class TListDlg : public TDlg, public TListObj {
public:
	TListDlg(UINT	resid, TWin *_parent = NULL) : TDlg(resid, _parent) {}
	virtual bool IsSame(const void *id) { return DWORD_RDC(id) == twinId; }
	virtual void ActiveDlg(BOOL active=TRUE) {
		if (!hWnd) return;
		if (active) {
			Show();
			SetForegroundWindow();
		} else {
			Show(SW_HIDE);
		}
	}
};

class TAboutDlg : public TDlg {
	Cfg	*cfg;
public:
	TAboutDlg(Cfg *_cfg, TWin *_parent = NULL);
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

	virtual BOOL	Create(const char *text, const char *title, int _showMode = SW_SHOW);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	static int		GetCreateCnt(void) { return createCnt; }
};

class TMsgBox : public TDlg {
public:
	enum { NOCANCEL=0x1, CENTER=0x2, DBLX=0x4 };

protected:
	const char *text;
	const char *title;

	DWORD	flags;

	int		x;
	int		y;
	void	Setup();
	void	InitPos();

public:
	TMsgBox(TWin *_parent = NULL);

	virtual int		Exec(const char *text, const char *title, int x, int y);
	virtual int		Exec(const char *text, const char *title=IP_MSG);
	virtual BOOL	Create(const char *_text, const char *_title=IP_MSG);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);

	void SetFlags(DWORD _flags) { flags = _flags; }
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
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
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
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual int		FilterHost();
};

class TInputDlg : public TDlg {
protected:
	char	*buf;
	int		max_buf;
	POINT	pt;

public:
	TInputDlg(TWin *_parent);

	virtual int		Exec(POINT _pt, char *_buf, int _max_buf);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
};

class TFwDlg : public TDlg {
public:
	TFwDlg(TWin *_parent=NULL);
//	virtual int		Exec(void);
//	virtual void	EndDialog(int);

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
//	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);

	static BOOL SetFirewallExcept(HWND hTarget=0);
};

#endif

