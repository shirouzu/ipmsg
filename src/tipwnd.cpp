static char *toolbar_id = 
	"@(#)Copyright (C) H.Shirouzu 2016   toolbar.cpp	Ver4.00";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Toolbar
	Create					: 2016-01-17(Sun)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

//
//  TTipWnd
//
#define TOOLTIP_CLASS	"ipmsg_tip"

TTipWnd::TTipWnd(TWin *_parent) : TWin(_parent)
{
	static BOOL once = TRegisterClassU8(TOOLTIP_CLASS, CS_HREDRAW | CS_VREDRAW);
}

BOOL TTipWnd::Hide(BOOL is_animate)
{
	tipId = 0;
	if (!AnimateWindow(hWnd, is_animate ? 200 : 0, AW_HIDE)) {
		Debug("hide anime failed=%d\n", GetLastError());
		Show(SW_HIDE);
	}
	return	TRUE;
}

BOOL TTipWnd::Create(const WCHAR *_tip, int x, int y, UINT tip_id, BOOL is_animate)
{
	if (hWnd) {
		Destroy();
	}

	tip = _tip;
	tipId = tip_id;
	isAnimate = is_animate;

	rect.left = x;
	rect.top  = y;
	rect.right  = x + 10;
	rect.bottom = y + 10;
	orgRect = rect;

	return	CreateU8(TOOLTIP_CLASS, TOOLTIP_CLASS, WS_POPUP|WS_DISABLED, WS_EX_TOOLWINDOW);
}

HFONT TTipWnd::GetFont()
{
	static HFONT	hFontSv;

	if (!hFontSv) {
		HFONT	hFontMain = (HFONT)parent->SendMessage(WM_GETFONT, 0, 0);
		LOGFONT	logFont;

		::GetObject(hFontMain, sizeof(LOGFONT), &logFont);
		logFont.lfHeight = lround(logFont.lfHeight * 0.9);

		hFontSv = ::CreateFontIndirect(&logFont);
	}
	return	hFontSv;
}

HPEN TTipWnd::GetPen()
{
	static HPEN	hPenSv;

	if (!hPenSv) {
		hPenSv = ::CreatePen(PS_SOLID, 1, TIPWIN_FRAME);
	}

	return	hPenSv;
}

HBRUSH TTipWnd::GetBrush()
{
	static HBRUSH	hBrushSv;

	if (!hBrushSv) {
		LOGBRUSH lb = { BS_SOLID, TIPWIN_BACK };
		hBrushSv	= ::CreateBrushIndirect(&lb);
	}

	return	hBrushSv;
}

BOOL TTipWnd::EvCreate(LPARAM lParam)
{
	hMiniFont = GetFont();
	hPen      = GetPen();
	hBrush    = GetBrush();

	HDC hDc = ::GetDC(hWnd);

	HGDIOBJ	hFontBk = ::SelectObject(hDc, hMiniFont);
	tipRc.Init();
	::DrawTextW(hDc, tip.s(), -1, &tipRc, DT_LEFT|DT_CALCRECT|DT_SINGLELINE|DT_NOPREFIX);
	::SelectObject(hDc, hFontBk);
	::ReleaseDC(hWnd, hDc);
	tipRc.Slide(3, 3);

	frameRc = tipRc;
	frameRc.Inflate(3, 3);

	GetWindowRect(&rect);
	rect.set_cx(frameRc.cx());
	rect.set_cy(frameRc.cy());

	MoveWindow(rect.x(), rect.y(), rect.cx(), rect.cy(), FALSE);

	if (!AnimateWindow(hWnd, isAnimate ? 200 : 0, AW_BLEND)) {
		Debug("show anime failed=%d\n", GetLastError());
		Show();
	}
	return	TRUE;
}

// move later
extern void PaintRect(HDC hDc, HPEN hPen, HBRUSH hBrush, TRect &rc, int round_w=0, int round_h=0);


BOOL TTipWnd::PaintCore(HDC hDc)
{
	PaintRect(hDc, hPen, hBrush, frameRc);

	int		svBkm = ::SetBkMode(hDc, TRANSPARENT);
	COLORREF colBk = ::SetTextColor(hDc, TIPWIN_FORE);
	HGDIOBJ	hFontBk = ::SelectObject(hDc, hMiniFont);

	::DrawTextW(hDc, tip.s(), -1, &tipRc, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);

	::SelectObject(hDc, hFontBk);
	::SetBkMode(hDc, svBkm);
	::SetTextColor(hDc, colBk);

	return	TRUE;
}


BOOL TTipWnd::EvPaint()
{
	PAINTSTRUCT ps;
	HDC	hDc = BeginPaint(hWnd, &ps);

	PaintCore(hDc);

	EndPaint(hWnd, &ps);

	return	TRUE;
}

BOOL TTipWnd::EventPrint(UINT uMsg, HDC hDc, DWORD opt)
{
	return	PaintCore(hDc);
}



