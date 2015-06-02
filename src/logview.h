/*	@(#)Copyright (C) H.Shirouzu 2015   logview.h	Ver3.50 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer
	Create					: 2015-04-10(Sat)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGVIEWDLG_H
#define LOGVIEWDLG_H

class TLogView;

#include <vector>
#include <list>

#define VIEWCHILD_CALSS	"TLogViewChild"

#define LOGVIEW_OFFSET        3100
#define LOGVIEW_SETTINGS     (LOGVIEW_OFFSET + 0)
#define LOGVIEW_USERBTN      (LOGVIEW_OFFSET + 1)
#define LOGVIEW_TB_MAX        2
#define LOGVIEW_TIMER         4000
#define LOGVIEW_TIMER_TICK    20

#define CVW_GAP				5
#define LOG_HEAD_TOP_FLAG	0x0001
#define LOG_HEAD_MID_FLAG	0x0002
#define LOG_HEAD_END_FLAG	0x0004
#define LOG_CLIP_FLAG		0x0008

struct LineInfo {
	WCHAR	*s;
	int		len;
	int		viewNo;
	int		viewNoEnd;
	int		bmpSize;
	int		msgNo;
	int		flags;

	LineInfo(WCHAR *_s, bool allocated=true) {
		len = (int)wcslen(_s);
		if (allocated) {
			s = _s;
		} else {
			s = new WCHAR [len + 1];
			memcpy(s, _s, sizeof(WCHAR) * (len + 1));
		}
		viewNo		= 0;
		viewNoEnd	= 0;
		bmpSize		= 0;
		msgNo		= 0;
		flags		= 0;
	}
	~LineInfo() {
		delete [] s;
	}
};

struct LogStr {
	const WCHAR	*s;
	int			len;

	LogStr(const WCHAR *_s=L"", int _len=0) { s=_s; len=_len; }
	bool operator ==(const LogStr& ls) {
		return len == ls.len && !memcmp(s, ls.s, sizeof(WCHAR)*len);
	}
	bool operator <(const LogStr& ls) {
		if (len < ls.len) return true;
		if (len == ls.len && memcmp(s, ls.s, sizeof(WCHAR)*len) < 0) return true;
		return	false;
	}
	LogStr& operator=(const LogStr& ls) {
		s=ls.s; len=ls.len; return *this;
	}
	void Get(WCHAR *buf) {
		memcpy(buf, s, sizeof(WCHAR)*len);
		buf[len] = 0;
	}
};

struct LogUser {
	LogStr	uname;
	LogStr	gname;
	LogStr	host;
	LogStr	uid;
	LogUser() {
	}
	LogUser& operator=(const LogUser& lu) {
		uname	= lu.uname;
		gname	= lu.gname;
		host	= lu.host;
		uid		= lu.uid;
		return	*this;
	}
};

struct LogClip {
	LogStr	fname;
	int		x;
	int		y;
	int		pos;
	LogClip() {
		x = y = pos = 0;
	}
};

struct LogMsg {
	int		no;		// top of line no
	int		num;	// line no
	time_t	date;
	bool	isTo;
	std::vector<LogUser> users;
	std::vector<LogClip> clips;
};

class TUserBtn : public TSubClassCtl {
public:
	TUserBtn(TWin *_parent=NULL) : TSubClassCtl(_parent) {}
//	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

struct LinePos {
	int	no;		// line no
	int	xOff;

	LinePos(int _no=-1, int x=0) {
		no=_no; xOff=x;
	}
	bool operator ==(const LinePos &lp) {
		return no == lp.no && (no == -1 || xOff == lp.xOff);
	}
	bool operator <(const LinePos &lp) {
		return no < lp.no || (no == lp.no && xOff < lp.xOff);
	}
	void Init() {
		no=-1; xOff=0;
	}
	bool Valid() {
		return	no != -1;
	}
};

class TChildView : public TWin {
protected:
	Cfg			*cfg;
	HWND		hParentToolBar;
	TLogView	*parentView;
	int			toolBarCy;
	int			lineCy;
	TRect		crc;	// client rect
	TRect		vrc;	// view rect
	HFONT		hFont;
	UINT_PTR	scrTimerID;

	std::vector<LineInfo *>	fullLines;	// total logical lines
	std::vector<LineInfo *>	curLines;	// total logical lines
	std::vector<LineInfo *>	*lines;		// total logical lines
	std::vector<LogMsg *>	msgs;		// total logical lines

	int			viewPos;	// start view line of rect
	int			viewLines;	// view lines (total)
	int			rectLines;	// view rect lines
	LinePos		selStart;	// select start pos
	LinePos		selTopPos;	// select top pos
	LinePos		selEndPos;	// select end pos

	BOOL	LoadLog(const char *fname);
	BOOL	UnloadLog();
	BOOL	ParseLog();
	BOOL	ParseMsg(int idx, LogMsg *lm);
	BOOL	ParseSender(LineInfo *l, LogMsg *lm);
	BOOL	ParseDate(LineInfo *l, LogMsg *lm);
	BOOL	ParseAttach(LineInfo *l, LogMsg *lm);
	int		ToLineNo(int view_no);
	BOOL	PointsToLine(POINTS pos, LinePos *lp);
	BOOL	SetClipBoard();

public:
	TChildView(Cfg *_cfg, TLogView *_parent);
	virtual ~TChildView();

	BOOL	CreateU8(HWND _hParentToolBar);
	BOOL	FitSize();
	BOOL	SetFont();
	HFONT	GetFont() { return hFont; }
	BOOL	UserPopupMenu();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvPaint();
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
	virtual BOOL	EvNcDestroy(LPARAM lParam);
//	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);
//	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);

	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
	virtual BOOL	EventScroll(UINT uMsg, int nScrollCode, int nPos, HWND hScroll);
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
};

class TLogView : public TDlg {
protected:
	Cfg			*cfg;
	HWND		hToolBar;
	TChildView	childView;
	TUserBtn	userBtn;

public:
	TLogView(Cfg *_cfg);
	virtual ~TLogView();

	virtual BOOL	Create();

//	virtual BOOL	PreProcMsg(MSG *msg);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);

	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvPaint();
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
//	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
//	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
//	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
//	virtual BOOL	EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis);
//	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);
//	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);

//	virtual BOOL	EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu);
//	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);
//	virtual BOOL	EvDropFiles(HDROP hDrop);
//	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);
//	virtual BOOL	EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu);
//	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
//	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
//	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
};


#endif

