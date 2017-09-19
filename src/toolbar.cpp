static char *toolbar_id = 
	"@(#)Copyright (C) H.Shirouzu 2016   toolbar.cpp	Ver4.60";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Toolbar
	Create					: 2016-01-17(Sun)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "logchilddef.h"

ToolBar::ToolBar(TWin *_parent, int _cy) : parent(_parent), tipWnd(_parent)
{
	sz.cy = _cy;
	timerCnt = 0;
	curFocusId = 0;
	curAvoidId = 0;
	hMemDc = NULL;
	hBaseBmp = NULL;
	memset(hStateBmp, 0, sizeof(hStateBmp));
}

ToolBar::~ToolBar()
{
	UnInit();
}

BOOL ToolBar::UnInit()
{
	if (hBaseBmp) {
		::DeleteObject(hBaseBmp);
		hBaseBmp = NULL;
	}
	for (int i=0; i < MAX_STATE; i++) {
		if (hStateBmp[i]) {
			::DeleteObject(hStateBmp[i]);
			hStateBmp[i] = NULL;
		}
		if (hStateMiniBmp[i]) {
			::DeleteObject(hStateMiniBmp[i]);
			hStateMiniBmp[i] = NULL;
		}
	}

	while (auto btn=btnList.TopObj()) {
		btnList.DelObj(btn);
		FreeBtnBmp(btn);
		delete btn;
	}

	if (hMemDc) {
		::DeleteDC(hMemDc);
		hMemDc = NULL;
	}
	if (hTmpDc) {
		::DeleteDC(hTmpDc);
		hTmpDc = NULL;
	}
	return	TRUE;
}

BOOL ToolBar::CreateBtnBmp(Btn *btn, UINT bmpId)
{
	if (btn->kind != BTN) {
		return	FALSE;
	}

	HBITMAP	hBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)(INT_PTR)bmpId);

	for (int i=0; i < MAX_STATE; i++) {
		if (btn->rc.cy() >= bi.stat.cy) {
			btn->hBmp[i] = MergeBmp(hBmp, hStateBmp[i] ? hStateBmp[i] : hStateBmp[0],
				btn->rc.cx(), btn->rc.cy(), bi.stat.cx, bi.stat.cy);
		}
		else {
			btn->hBmp[i] = MergeBmp(hBmp, hStateMiniBmp[i] ? hStateMiniBmp[i] : hStateMiniBmp[0],
				btn->rc.cx(), btn->rc.cy(), bi.statMini.cx, bi.statMini.cy);
		}
	//	btn->hBmp[i] = AlphaBmp(hBmp, hStateBmp[0], btn->rc.cx(), btn->rc.cy(), 128);
	}
	::DeleteObject(hBmp);

	btn->lastBmpId = bmpId;

	return	TRUE;
}

BOOL ToolBar::FreeBtnBmp(Btn *btn)
{
	for (int i=0; i < MAX_STATE; i++) {
		if (btn->hBmp[i]) {
			::DeleteObject(btn->hBmp[i]);
			btn->hBmp[i] = NULL;
		}
	}
	btn->lastBmpId = 0;

	return	TRUE;
}

BOOL ToolBar::RegisterBaseInfo(BaseInfo *_bi)
{
	HWND	hDesktop = ::GetDesktopWindow();
	HDC		hDc = ::GetDC(hDesktop);
	hMemDc = ::CreateCompatibleDC(hDc);
	hTmpDc = ::CreateCompatibleDC(hDc);
	::ReleaseDC(hDesktop, hDc);

	bi = *_bi;

	hBaseBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)(INT_PTR)bi.baseBmpId);

	for (int i=0; i < MAX_STATE; i++) {
		if (bi.statBmpId[i]) {
			hStateBmp[i] = ::LoadBitmap(TApp::hInst(), (LPCSTR)(INT_PTR)bi.statBmpId[i]);
		}
		if (bi.statMiniBmpId[i]) {
			hStateMiniBmp[i] = ::LoadBitmap(TApp::hInst(),
				(LPCSTR)(INT_PTR)bi.statMiniBmpId[i]);
		}
	}
	return	TRUE;
}

BOOL ToolBar::RegisterBtn(BtnInfo *info, TRect *placed_rc)
{
	Btn	*btn = new Btn;

	btn->id    = info->id;
	btn->kind  = info->kind;
	btn->state = 0;
	btn->rc    = info->rc;
	if (Btn *last = btnList.EndObj()) {
		btn->rc.Slide(last->rc.right, 0);
	}
	if (placed_rc) {
		*placed_rc = btn->rc;
	}
	if (info->tipId) {
		btn->tip = LoadStrW(info->tipId);
		btn->lastTipId = info->tipId;
	}
	else {
		btn->lastTipId = 0;
	}
	if (info->title) {
		btn->title = info->title;
	}

	CreateBtnBmp(btn, info->bmpId);
	btnList.AddObj(btn);

	return	TRUE;
}

ToolBar::Btn *ToolBar::SearchBtn(UINT id)
{
	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->id == id) {
			return	btn;
		}
	}
	return	NULL;
}

BOOL ToolBar::UpdateBtnBmp(UINT id, UINT bmpId)
{
	if (Btn *btn = SearchBtn(id)) {
		if (btn->lastBmpId != bmpId) {
			FreeBtnBmp(btn);
			CreateBtnBmp(btn, bmpId);
			parent->InvalidateRect(&btn->rc, FALSE);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::UpdateBtnState(UINT id, State state)
{
	if (Btn *btn = SearchBtn(id)) {
		if (btn->state != state) {
			btn->state = state;
			parent->InvalidateRect(&btn->rc, FALSE);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::UpdateTitleStr(UINT id, const WCHAR *title)
{
	if (Btn *btn = SearchBtn(id)) {
		if (btn->title != title) {
			btn->title = title;
			parent->InvalidateRect(&btn->rc, FALSE);
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::UpdateTipId(UINT id, UINT tip_id)
{
	if (Btn *btn = SearchBtn(id)) {
		if (btn->lastTipId != tip_id) {
			btn->tip = LoadStrW(tip_id);
			btn->lastTipId = tip_id;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::CreateTipWnd(Btn *btn)
{
	POINT	pt;
	::GetCursorPos(&pt);

	BOOL	is_animate = tipWnd.GetTipId() ? FALSE : TRUE;
	POINT	ptc = { 0, btn->rc.bottom };
	::ClientToScreen(parent->hWnd, &ptc);

	//Debug("CreateTipWnd\n");

	return	tipWnd.Create(btn->tip.s(), pt.x, ptc.y + 5, btn->id, is_animate);
}

BOOL ToolBar::HideTipWnd()
{
	//Debug("hide TipWnd\n");

	if (tipWnd.GetTipId()) {
		tipWnd.Hide(TRUE);
		return TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::GetRect(UINT id, TRect *rc)
{
	if (Btn *btn = SearchBtn(id)) {
		*rc = btn->rc;
		return	TRUE;
	}
	return	FALSE;
}

BOOL ToolBar::Paint(HDC hDc, PAINTSTRUCT *ps, TRect *rc)
{
	sz.cx = rc->cx();

	TRect	brc(0, 0, sz.cx, sz.cy);
	TRect	prc;

	if (!::IntersectRect(&prc, &brc, &ps->rcPaint)) {
		return	FALSE;
	}
	HGDIOBJ	hBmpSv = ::SelectObject(hMemDc, hBaseBmp);

	for (LONG x=prc.left; x < prc.right; x+=bi.base.cx) {
		::BitBlt(hDc, x, 0, bi.base.cx, bi.base.cy, hMemDc, 0, 0, SRCCOPY);
	}

	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->kind == BTN) {
			::SelectObject(hMemDc, btn->hBmp[StateToIdx(btn)]);
			::BitBlt(hDc, btn->rc.x(), btn->rc.y(), btn->rc.cx(), btn->rc.cy(), hMemDc,
				0, 0, SRCCOPY);
		}
		else if (btn->kind == TITLE) {
			HGDIOBJ	hFontBk = ::SelectObject(hDc, bi.hFont);
			int		svBk = ::SetBkMode(hDc, TRANSPARENT);
			::DrawTextW(hDc, btn->title.s(), -1, &btn->rc,
				DT_LEFT|DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER);
			::SetBkMode(hDc, svBk);
			::SelectObject(hDc, hFontBk);
		}
	}

	::SelectObject(hDc, hBmpSv);

	return	TRUE;
}

BOOL ToolBar::MouseMove(UINT fwKeys, POINTS pos) // pos:クライアント領域
{
	if (!timerCnt) {
		if (!MouseInArea(pos.x, pos.y)) {
			return	FALSE;
		}
	}

	BOOL	ret = FALSE;
	POINT	pt = { pos.x, pos.y };

	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->kind == HOLE || btn->kind == SEP || (btn->state & DISABLE)) {
			continue;
		}
		if (::PtInRect(&btn->rc, pt)) {
			if ((btn->state & FOCUS) == 0) {
				if ((btn->state & DISABLE) == 0) {
					btn->state |= FOCUS;
					if (btn->kind == BTN) {
						parent->InvalidateRect(&btn->rc, FALSE);
					}
					if (timerCnt == 0) {
						StartTimer();
					}
				}
			}
			if (tipWnd.GetTipId() && tipWnd.GetTipId() != btn->id) {
				CreateTipWnd(btn);	// 既にtipwndが表示している場合は直ちに表示
			}
			else {
				if (curFocusId != btn->id && curAvoidId != btn->id) {
					curFocusId = btn->id;
					curAvoidId = 0;
				}
				if (tipWnd.GetTipId() == 0) {
					timerCnt = 1;	// タイマーリセット
				}
			}
			ret = TRUE;
		}
		else {
			if (btn->state & FOCUS) {
				btn->state &= ~FOCUS;
				if (btn->kind == BTN) {
					parent->InvalidateRect(&btn->rc, FALSE);
				}
			}
		}
	}
	if (!ret) {
		if (timerCnt) {
			HideTipWnd();
			curFocusId = 0;
			if ((::GetKeyState(VK_LBUTTON) & 0x8000) == 0) {
				StopTimer();
			}
		}
	}

	return	ret;
}

BOOL ToolBar::Button(UINT uMsg, int nHitTest, POINTS pos)
{
	if (uMsg != WM_LBUTTONDOWN && uMsg != WM_LBUTTONUP) {
		return	FALSE;
	}
	if (timerCnt == 0) {
		if (!MouseInArea(pos.x, pos.y)) {
			return	FALSE;
		}
	}

	BOOL	is_up = (uMsg == WM_LBUTTONUP) ? TRUE : FALSE;
	BOOL	ret = FALSE;
	POINT	pt = { pos.x, pos.y };

	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->kind == HOLE || btn->kind == SEP || (btn->state & DISABLE)) {
			continue;
		}
		BOOL	next_pushing = FALSE;

		if (::PtInRect(&btn->rc, pt)) {
			if (is_up) {
				if (btn->isPushing) {
					if (btn->kind == BTN) {
						parent->PostMessage(WM_COMMAND, btn->id, 0);
//						parent->EvCommand(0, btn->id, 0);
						//btn->state &= ~FOCUS;
						parent->InvalidateRect(&btn->rc, FALSE);
					}
					btn->isPushing = FALSE;
				}
			}
			else {
				next_pushing = TRUE;
			}
			curAvoidId = btn->id;
			HideTipWnd();
			ret = TRUE;
		}
		else {
			if (btn->isPushing) {
				btn->isPushing = FALSE;
				btn->state &= ~FOCUS;
				if (btn->kind == BTN) {
					parent->InvalidateRect(&btn->rc, FALSE);
				}
			}
		}
		if (btn->isPushing != next_pushing) {
			btn->isPushing = next_pushing;
			if (btn->kind == BTN) {
				parent->InvalidateRect(&btn->rc, FALSE);
			}
		}
	}
	if (!ret && timerCnt) {
		StopTimer();
	}

	return	FALSE;
}

BOOL ToolBar::MouseInArea(int x, int y)
{
	POINT	pt = { x, y };
	TRect	brc(0, 0, sz.cx, sz.cy);

	return	::PtInRect(&brc, pt);
}

BOOL ToolBar::IsInvalidArea(POINT pt)
{
	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->kind == HOLE || btn->kind == SEP || (btn->state & DISABLE)) {
			continue;
		}
		if (::PtInRect(&btn->rc, pt)) {
			return	FALSE;
		}
	}
	return	TRUE;
}

void ToolBar::StartTimer()
{
	//Debug("start timer\n");
	parent->SetTimer(TOOLBAR_TIMER_ID, TOOLBAR_TIMER_TICK);
	timerCnt = 1;
	curFocusId = 0;
	curAvoidId = 0;
}

void ToolBar::StopTimer()
{
	::KillTimer(parent->hWnd, TOOLBAR_TIMER_ID);
	timerCnt = 0;
	curFocusId = 0;
	curAvoidId = 0;
	//Debug("stop timer\n");
	HideTipWnd();

	for (auto btn=btnList.TopObj(); btn; btn=btnList.NextObj(btn)) {
		if (btn->kind == HOLE || btn->kind == SEP || (btn->state & DISABLE)) {
			continue;
		}
		if (btn->isPushing || (btn->state & FOCUS)) {
			btn->isPushing = FALSE;
			btn->state &= ~FOCUS;
			if (btn->kind == BTN) {
				parent->InvalidateRect(&btn->rc, FALSE);
			}
		}
	}
}

BOOL ToolBar::Timer(WPARAM _timerID, TIMERPROC proc)
{
	if (_timerID == TOOLBAR_TIMER_ID) {
		timerCnt++;

		POINT	pt;
		::GetCursorPos(&pt);
		::ScreenToClient(parent->hWnd, &pt);

		if (!MouseInArea(pt.x, pt.y) || IsInvalidArea(pt)) {
			StopTimer();
			return FALSE;
		}
		if (timerCnt >= 6 && curFocusId && curFocusId != curAvoidId &&
			tipWnd.GetTipId() != curFocusId) {
			if (Btn *btn = SearchBtn(curFocusId)) {
				if (::PtInRect(&btn->rc, pt)) {
					CreateTipWnd(btn);
				}
			}
		}
		return	TRUE;
	}
	return	FALSE;
}


BOOL ToolBar::NcHitTest(POINTS pos, LRESULT *result)
{
	return	FALSE;

/*	::ScreenToClient(parent->hWnd, &pos);
	if (!MouseInArea(pos.x, pos.y)) {
		return	FALSE;
	}
	*result = HTCLIENT;
	return	TRUE;
*/
}

HBITMAP ToolBar::MergeBmpCore(HDC hDesktopDc, HBITMAP hBmp, HBITMAP hBaseBmp,
	int cx, int cy, int base_cx, int base_cy)
{
	HBITMAP	hTargBmp = ::CreateCompatibleBitmap(hDesktopDc, cx, base_cy);

	HGDIOBJ	hTargBk = ::SelectObject(hMemDc, hTargBmp);
	HGDIOBJ	hBaseBk = ::SelectObject(hTmpDc, hBaseBmp);

	if (!::BitBlt(hMemDc, 0, 0, cx, base_cy, hTmpDc, 0, 0, SRCCOPY)) {
		Debug(" BitBlt err=%d (%d/%d)\n", GetLastError(), cx, cy);
	}
	if (cx < base_cx) {
		if (!::BitBlt(hMemDc, cx-3, 0, 3, base_cy, hTmpDc, base_cx-3, 0, SRCCOPY)) {
			Debug(" BitBlt2 err=%d (%d/%d)\n", GetLastError(), cx, cy);
		}
	}

	::SelectObject(hTmpDc, hBmp);

	if (!::TransparentBlt(hMemDc, 0, (base_cy - cy)/2, cx, cy,
		hTmpDc, 0, 0, cx, cy, TRANS_RGB)) {
		Debug("err=%d (%d/%d)\n", GetLastError(), cx, cy);
	}

	::SelectObject(hTmpDc, hBaseBk);
	::SelectObject(hMemDc, hTargBk);

	return	hTargBmp;
}

HBITMAP ToolBar::MergeBmp(HBITMAP hBmp, HBITMAP hBaseBmp, int cx, int cy,
	int base_cx, int base_cy)
{
	HWND	hDesktop   = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);

	HBITMAP	hTargBmp = MergeBmpCore(hDesktopDc, hBmp, hBaseBmp, cx, cy, base_cx, base_cy);

	::ReleaseDC(hDesktop, hDesktopDc);

	return	hTargBmp;
}

//HBITMAP ToolBar::AlphaBmp(HBITMAP hBmp, HBITMAP hBaseBmp, int cx, int cy, int alpha)
//{
//	HWND	hDesktop   = ::GetDesktopWindow();
//	HDC		hDesktopDc = ::GetDC(hDesktop);
//
//	HBITMAP	hTargBmp = /*MergeBmpCore(hDesktopDc, hBmp, hBaseBmp, cx, cy)*/ NULL;
//
//	HGDIOBJ	hTargBk = ::SelectObject(hMemDc, hTargBmp);
//	HGDIOBJ	hBaseBk = ::SelectObject(hTmpDc, hBaseBmp);
//
//	::BitBlt(hMemDc, 0, 0, cx, cy, hTmpDc, 0, 0, SRCCOPY);
//
//	::SelectObject(hTmpDc, hBmp);
//
//	BLENDFUNCTION bf;
//	bf.BlendOp             = AC_SRC_OVER;
//	bf.BlendFlags          = 0;
//	bf.SourceConstantAlpha = 128;
//	bf.AlphaFormat         = 0;
//	AlphaBlend(hMemDc, 0, 0, cx, cy, hTmpDc, 0, 0, cx, sz.cy, bf);
//
//	::SelectObject(hTmpDc, hBaseBk);
//	::SelectObject(hMemDc, hTargBk);
//
//	::ReleaseDC(hDesktop, hDesktopDc);
//
//	return	hTargBmp;
//}
//

