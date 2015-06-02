/*	@(#)Copyright (C) H.Shirouzu 2013-2015   miscdlg.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
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

	virtual	int		InsertColumn(int idx, char *title, int width, int fmt=LVCFMT_LEFT);
	virtual	BOOL	DeleteColumn(int idx);
	virtual int		GetColumnWidth(int idx);
	virtual	void	DeleteAllItems();
	virtual	int		InsertItem(int idx, char *str, LPARAM lParam=0);
	virtual	int		SetSubItem(int idx, int subIdx, char *str);
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
	virtual bool IsSame(const void *id) { return (DWORD)id == twinId; }
};


class OpenFileDlg {
public:
	enum			Mode { OPEN, MULTI_OPEN, SAVE, NODEREF_SAVE };

protected:
	TWin			*parent;
	LPOFNHOOKPROC	hook;
	Mode			mode;
	DWORD			flags;

public:
	OpenFileDlg(TWin *_parent, Mode _mode=OPEN, LPOFNHOOKPROC _hook=NULL, DWORD _flags=0) {
		parent = _parent; hook = _hook; mode = _mode; flags = _flags;
	}
	BOOL Exec(char *target, int size, char *title=NULL, char *filter=NULL, char *defaultDir=NULL,
				char *defaultExt=NULL);
	BOOL Exec(UINT editCtl, char *title=NULL, char *filter=NULL, char *defaultDir=NULL,
				char *defaultExt=NULL);
};

class TAboutDlg : public TDlg {
public:
	TAboutDlg(TWin *_parent = NULL);
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

	virtual BOOL	Create(char *text, char *title, int _showMode = SW_SHOW);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	static int		GetCreateCnt(void) { return createCnt; }
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

#endif

