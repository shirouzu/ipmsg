/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logview.h	Ver4.50 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer
	Create					: 2015-04-10(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGVIEW_H
#define LOGVIEW_H

#include "logchildview.h"
#include "toolbar.h"

// for toolbar items
#define LOGVIEW_OFFSET        3100
#define LOGVIEW_LISTBTN      (LOGVIEW_OFFSET + 0)
#define LOGVIEW_USERBTN      (LOGVIEW_OFFSET + 1)
#define LOGVIEW_ALLBTN       (LOGVIEW_OFFSET + 2)
#define LOGVIEW_IMGCHECK     (LOGVIEW_OFFSET + 3)
#define LOGVIEW_USEREDIT     (LOGVIEW_OFFSET + 4)

// cfg.logVIewDispMode
enum {
	LVDM_REV			= 0x1,
	LVDM_NOFOLDQUOTE	= 0x2,
};

class TOwnDrawCheck : public TSubClassCtl {	// onwer draw用
	Cfg *cfg;
public:
	TOwnDrawCheck(Cfg *_cfg, TWin *_parent=NULL) : TSubClassCtl(_parent), cfg(_cfg) {}
//	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

class TLvCombo : public TSubClassCtl {
	Cfg *cfg;

public:
	TLvCombo(Cfg *_cfg, TWin *_parent=NULL) : TSubClassCtl(_parent), cfg(_cfg) {
	}
//	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos) {
		POINT	pt;
		TRect	rc;

		::GetCursorPos(&pt);
		GetWindowRect(&rc);

		if (::PtInRect(&rc, pt)) {
			return	FALSE;
		}
		parent->EvMouseWheel(fwKeys, zDelta, xPos, xPos);
		return	TRUE;
	}
};

class TUserCombo : public TLvCombo {
public:
	TUserCombo(Cfg *_cfg, TWin *_parent=NULL) : TLvCombo(_cfg, _parent) {}
	virtual BOOL EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
};

class TBodyCombo : public TLvCombo {
public:
	TBodyCombo(Cfg *_cfg, TWin *_parent=NULL) : TLvCombo(_cfg, _parent) {}
};

class TRangeCombo : public TLvCombo {
public:
	TRangeCombo(Cfg *_cfg, TWin *_parent=NULL) : TLvCombo(_cfg, _parent) {}
};

class TComboEdit : public TSubClassCtl {
	Cfg		*cfg;
	TWin	*view;
public:
	TComboEdit(Cfg *_cfg, TWin *_view) : TSubClassCtl(NULL), cfg(_cfg), view(_view) {}
	virtual BOOL EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
};


class TLogStatus : public TSubClassCtl {
	Cfg *cfg;
public:
	TLogStatus(Cfg *_cfg, TWin *_parent=NULL) : TSubClassCtl(_parent), cfg(_cfg) {}
//	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

#define MAX_STATUS_MSGNUM	2
#define MAX_STATUS_MSGLEN	256

#define LVBASE_CX		64
#define LVBASE_CY		30
#define LVSTAT_CX		24
#define LVSTAT_CY		28
#define LVSTATMINI_CX	20
#define LVSTATMINI_CY	20

class TLogView : public TDlg, public TListObj {
protected:
	Cfg				*cfg;
	LogMng			*logMng;
	TChildView		childView;
	BOOL			isMain;

	ToolBar			toolBar;

	TUserCombo		userCombo;
	TUserCombo		userComboEx;
	TBodyCombo		bodyCombo;
	TComboEdit		bodyEdit;
	TComboEdit		userEdit;
	TRangeCombo		rangeCombo;

	TLogStatus		statusCtrl;

	HFONT			hMiniFont;

	WCHAR			statusMsg[MAX_STATUS_MSGNUM][MAX_STATUS_MSGLEN];
	BOOL			SetupToolbar();
	BOOL			SetStatusMsg(int pos, const WCHAR *msg);

	friend			TChildView;

public:
	TLogView(Cfg *_cfg, LogMng *_logMng, BOOL is_main, TWin *_parent=NULL);
	virtual ~TLogView();

	virtual BOOL	Create();

//	virtual BOOL	PreProcMsg(MSG *msg);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvSysCommand(WPARAM uCmdType, POINTS pos);

	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvPaint();
	virtual BOOL	EventFocus(UINT uMsg, HWND hFocusWnd);
	virtual BOOL	EventKey(UINT uMsg, int nVirtKey, LONG lKeyData);
//	virtual BOOL	EvGetMinMaxInfo(MINMAXINFO *info);
	virtual BOOL	EvMouseMove(UINT fwKeys, POINTS pos);
//	virtual BOOL	EvContextMenu(HWND childWnd, POINTS pos);
//	virtual BOOL	EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis);
//	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);
	virtual BOOL	EvNcHitTest(POINTS pos, LRESULT *result);

//	virtual BOOL	EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu);
	virtual BOOL	EvNotify(UINT ctlID, NMHDR *pNmHdr);
//	virtual BOOL	EvDropFiles(HDROP hDrop);
	virtual BOOL	EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos);
	virtual BOOL	EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis);
	virtual BOOL	EventButton(UINT uMsg, int nHitTest, POINTS pos);

//	virtual BOOL	EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu);
	virtual BOOL	EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu);
//	virtual BOOL	EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result);
//	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);

	BOOL UpdateLog(int64 msg_id=0) {
		return childView.UpdateLog(msg_id);
	}
	BOOL UpdateLogView() {
		return hWnd ? childView.InvalidateRect(0, 0) : FALSE;
	}
	time_t GetStatusCheckEpoch() {
		return childView.GetStatusCheckEpoch();
	}
	BOOL SetStatusCheckEpoch(time_t t) {
		return childView.SetStatusCheckEpoch(t);
	}
	BOOL LastView() {
		return hWnd ? childView.LastView() : FALSE;
	}
	BOOL SetUser(const Wstr &uid);
	BOOL ResetCache();
	BOOL ResetFindBodyCombo();

	BOOL IsUserComboEx() {
		return	userComboEx.IsWindowVisible();
	}
	BOOL SwitchUserCombo();
};

#endif

