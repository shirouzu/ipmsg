/*	@(#)Copyright (C) H.Shirouzu 2016   tipwnd.h	Ver4.00 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Tip Window for ToolBar
	Create					: 2016-01-17(Sun)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef TIPWND_H
#define TIPWND_H

#define TIPWIN_FRAME	RGB(150, 150, 150)
#define TIPWIN_FORE		RGB(110, 110, 110)
#define TIPWIN_BACK		RGB(255, 255, 255)

// TTipWnd;
class TTipWnd : public TWin {
	UINT	tipId;	// == toolbar btn_id
	Wstr	tip;
	TRect	tipRc;
	TRect	frameRc;
	HBRUSH	hBrush;
	HPEN	hPen;
	HFONT	hMiniFont;
	BOOL	isAnimate;

	HPEN	GetPen();
	HBRUSH	GetBrush();
	HFONT	GetFont();
	BOOL	PaintCore(HDC hDc);

public:
	TTipWnd(TWin *parent);
	BOOL	Create(const WCHAR *s, int x, int y, UINT tip_id, BOOL is_animate);
	BOOL	Hide(BOOL is_animate);
	UINT	GetTipId() {
		return	tipId;
	}

	virtual BOOL EvCreate(LPARAM lParam);
	virtual BOOL EvPaint();
	virtual BOOL EventPrint(UINT uMsg, HDC hDc, DWORD opt);
};

#endif

