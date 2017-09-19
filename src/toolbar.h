/*	@(#)Copyright (C) H.Shirouzu 2016   toolbar.h	Ver4.00 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Tool bar
	Create					: 2016-01-17(Sun)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TOOLBAR_H
#define TOOLBAR_H

#define TOOLBAR_TIMER_ID 2001
#define TOOLBAR_TIMER_TICK 100

#include "tipwnd.h"

// ToolBar
class ToolBar {
public:
	enum Kind  { BTN, TITLE, SEP, HOLE };
	enum State { NORMAL=0, FOCUS=1, SELECT=2, DISABLE=4 };
	enum { MAX_STATE = 4 };

	struct BaseInfo {
		TSize	base;
		TSize	stat;
		TSize	statMini;
		UINT	baseBmpId;
		UINT	statBmpId[MAX_STATE];
		UINT	statMiniBmpId[MAX_STATE];
		HFONT	hFont;
	};

	struct BtnInfo {
		UINT	id;
		Kind	kind;
		UINT	bmpId;
		TRect	rc;
		UINT	tipId;
		const WCHAR	*title;
	};

public:
	ToolBar(TWin *parent, int cy);
	~ToolBar();

	BOOL	RegisterBaseInfo(BaseInfo *bi);
	BOOL	RegisterBtn(BtnInfo *info, TRect *placed_rc=NULL);
	BOOL	UpdateBtnBmp(UINT id, UINT bmpId);
	BOOL	UpdateBtnState(UINT id, State state);
	BOOL	UpdateTitleStr(UINT id, const WCHAR *title);
	BOOL	UpdateTipId(UINT id, UINT tip_id);
	BOOL	Paint(HDC hDc, PAINTSTRUCT *ps, TRect *rc);
	BOOL	MouseMove(UINT fwKeys, POINTS pos);
	BOOL	Button(UINT uMsg, int nHitTest, POINTS pos);
	BOOL	Timer(WPARAM timerID, TIMERPROC proc);
	BOOL	NcHitTest(POINTS pos, LRESULT *result);
	int		Cy() { return sz.cy; }
	BOOL	GetRect(UINT id, TRect *rc);
	int		BtnNum() {
		return btnList.Num();
	}
	BOOL	UnInit();

private:
	struct Btn : TListObj {
		UINT		id;
		Kind		kind;
		UINT		lastBmpId;
		HBITMAP		hBmp[MAX_STATE];
		DWORD		state;
		BOOL		isPushing;
		TRect		rc;
		Wstr		title;
		UINT		lastTipId;
		Wstr		tip;
		Btn() {
			memset(this, 0, offsetof(Btn, rc));
		}
	};
	HBITMAP			hBaseBmp;
	HBITMAP			hStateBmp[MAX_STATE];
	HBITMAP			hStateMiniBmp[MAX_STATE];
	TListEx<Btn>	btnList;
	TWin			*parent;
	HDC				hMemDc;
	HDC				hTmpDc;

	BaseInfo		bi;
	DWORD			timerCnt;
	TTipWnd			tipWnd;
	UINT			curFocusId;
	UINT			curAvoidId;
	TSize			sz;

	int		StateToIdx(Btn *btn) {
		if (btn->state & DISABLE) {
			return	0;	// 暫定
		}
		else if ((btn->state & SELECT) || btn->isPushing) {
			return	2;
		}
		else if (btn->state & FOCUS) {
			return	1;
		}
		else {
			return	0;
		}
	}
	BOOL	MouseInArea(int x, int y);
	BOOL	IsInvalidArea(POINT pt);
	void	StartTimer();
	void	StopTimer();
	Btn		*SearchBtn(UINT id);
	BOOL	CreateBtnBmp(Btn *btn, UINT bmpId);
	BOOL	CreateTipWnd(Btn *btn);
	BOOL	HideTipWnd();
	BOOL	FreeBtnBmp(Btn *btn);
	HBITMAP	MergeBmpCore(HDC hDeskDc, HBITMAP hBmp, HBITMAP hBaseBmp, int cx, int cy,
		int base_cx, int base_cy);
	HBITMAP	MergeBmp(HBITMAP hBmp, HBITMAP hBaseBmp, int cx, int cy,
		int base_cx, int base_cy);
//	HBITMAP	AlphaBmp(HBITMAP hBmp, HBITMAP hBaseBmp, int cx, int cy, int alpha);
};

#endif

