static char *logview_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logview.cpp	Ver4.60";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer
	Create					: 2015-04-10(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include <vector>
using namespace std;

/* =====================================================================
             TLogView
 ======================================================================*/
TLogView::TLogView(Cfg *_cfg, LogMng *_logMng, BOOL is_main, TWin *parent) :
	cfg(_cfg),
	logMng(_logMng),
	toolBar(this, LVBASE_CY),
	childView(cfg, _logMng, is_main, this),
	userCombo(cfg, this),
	userComboEx(cfg, this),
	bodyCombo(cfg, this),
	userEdit(cfg, this),
	bodyEdit(cfg, this),
	rangeCombo(cfg),
	statusCtrl(cfg, this),
	isMain(is_main),
	TDlg(LOGVIEW_DIALOG, NULL)
{
	hAccel	= ::LoadAccelerators(TApp::hInst(), (LPCSTR)LOGVIEW_ACCEL);

	hMiniFont = NULL;
}

TLogView::~TLogView()
{
}

BOOL TLogView::Create()
{
	return	TDlg::Create();
}

// [+] [▽] [USERCOMBO] [↓] [Search][✓] (static) [☆][I] [M] […]

BOOL GetStrWidthW(HWND hWnd, const WCHAR *wstr, SIZE *sz, HFONT hFont=NULL, int len=-1)
{
	BOOL	ret = FALSE;
	HDC		hDc = ::GetDC(hWnd);
	HGDIOBJ	hOldFont = NULL;

	if (hFont) {
		hOldFont = ::SelectObject(hDc, hFont);
	}

	ret = ::GetTextExtentPoint32W(hDc, wstr, len == -1 ? (int)wcslen(wstr) : len, sz);

	if (hOldFont) {
		::SelectObject(hDc, hOldFont);
	}
	::ReleaseDC(hWnd, hDc);

	return	ret;
}

BOOL TLogView::SetupToolbar()
{
	HFONT	hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0);

	if (!hMiniFont) {
		LOGFONT	logFont;

		::GetObject(hFont, sizeof(LOGFONT), &logFont);
		logFont.lfHeight = lround(logFont.lfHeight * 0.9);
		hMiniFont = ::CreateFontIndirect(&logFont);
	}

	ToolBar::BaseInfo bi = {
		TSize(LVBASE_CX, LVBASE_CY),
		TSize(LVSTAT_CX, LVSTAT_CY),
		TSize(LVSTATMINI_CX, LVSTATMINI_CY),
		LVBASE_BITMAP,
		{
			BTNN_BITMAP,
			BTNF_BITMAP,
			BTNS_BITMAP,
			//BTND_BITMAP,
		},
		{
			BTNMN_BITMAP,
			BTNMF_BITMAP,
			BTNMS_BITMAP,
		},
		hMiniFont,
	};

	TSize		sz;
	const WCHAR *find_title   = LoadStrW(IDS_FINDTITLE);
	GetStrWidthW(hWnd, find_title, &sz, hMiniFont);
	int			find_width    = sz.cx + (*find_title   > 0xff ? 3 : 6);
	const WCHAR *filter_title = LoadStrW(IDS_FILTERTITLE);
	GetStrWidthW(hWnd, filter_title, &sz, hMiniFont);
	int			filter_width  = sz.cx + (*filter_title > 0xff ? 3 : 4);

	ToolBar::BtnInfo bis[] = {
		{ BOTTOM_BTN, ToolBar::BTN,  TOPHOT_BITMAP,    TRect(  4, 1,  24, 28), IDS_BOTTOMTIP },
//		{ REV_BTN,    ToolBar::BTN,  ASC_BITMAP,       TRect(  0, 1,  24, 28), IDS_REVTIP },
		{ LIST_BTN,   ToolBar::BTN,  TITLEDISP_BITMAP, TRect(  0, 1,  24, 28), IDS_LISTTIP },
		{ MENU_NEWLOGVIEW, ToolBar::BTN, DUPWIN_BITMAP,TRect(  0, 1,  24, 28), IDS_NEWLOGVIEWTIP },

		{ USER_COMBO, ToolBar::HOLE, NULL,             TRect( 10, 4, 110, 28) },
		{ USER_CHK,   ToolBar::BTN,  USERCHK_BITMAP,   TRect(  2, 5,  16, 20), IDS_USERSELTIP },
//		{ SEND_BTN,   ToolBar::BTN,  SEND_BITMAP,      TRect(  0, 2,  24, 28), IDS_SENDTIP },

//		{ IDS_FILTERTITLE, ToolBar::TITLE, NULL,       TRect( 5, 1,  filter_width, 28),
//			IDS_FILTERTIP, filter_title },
		{ FAV_CHK,    ToolBar::BTN,  FAVTB_BITMAP,     TRect( 20, 1,  24, 28), IDS_FAVTIP },
		{ MARK_CHK,   ToolBar::BTN,  MARKTB_BITMAP,    TRect(  0, 1,  24, 28), IDS_MARKTIP },
		{ CLIP_CHK,   ToolBar::BTN,  CLIPTB_BITMAP,    TRect(  0, 1,  24, 28), IDS_CLIPTIP },
		{ FILE_CHK,   ToolBar::BTN,  FILETB_BITMAP,    TRect(  0, 1,  24, 28), IDS_FILETIP },
		{ UNOPENR_CHK, ToolBar::BTN, UNOPENTB_BITMAP,  TRect(  0, 1,  24, 28), IDS_UNOPENTIP },

//		{ RANGE_COMBO, ToolBar::HOLE, NULL,            TRect( 10, 4,  60, 20) },

//		{ MENU_BTN,   ToolBar::BTN,  MISC_BITMAP,      TRect( 17, 2,  24, 28), IDS_MENUTIP },
//		{ MENU_MEMO,  ToolBar::BTN,  MEMO_BITMAP,      TRect(  8, 1,  24, 28), IDS_MEMOTIP },

//		{ MENU_SENDDLG, ToolBar::BTN, MEMO_BITMAP,     TRect( 10, 1,  24, 28), IDS_SENDTIP },

//		{ IDS_FINDTITLE, ToolBar::TITLE, NULL,         TRect( 10, 1,  find_width, 28),
//			IDS_FINDTIP, find_title },
		{ BODY_COMBO, ToolBar::HOLE, NULL,             TRect( 20, 4,  90, 28) },
		{ NARROW_CHK, ToolBar::BTN,  NARROW_BITMAP,    TRect(  1, 5,  16, 20), IDS_NARROWTIP },

		{ STATUS_STATIC, ToolBar::TITLE, NULL,         TRect(  4, 2, 200, 28) },
	};

	if (toolBar.BtnNum() <= 0) {
		toolBar.RegisterBaseInfo(&bi);
		for (int i=0; i < sizeof(bis)/sizeof(bis[0]); i++) {
			toolBar.RegisterBtn(&bis[i]);
		}
	}

	struct {
		int				id;
		TSubClassCtl	*wnd;
		int				tip;
		UINT			flags;
	} combos[] = {
		{ USER_COMBO,	&userCombo,	 IDS_USERTIP,	CBS_DROPDOWNLIST|WS_VISIBLE },
		{ USEREX_COMBO,	&userComboEx,IDS_USERMANTIP,CBS_DROPDOWN },
		{ BODY_COMBO,	&bodyCombo,	 IDS_FINDTIP,	CBS_DROPDOWN|WS_VISIBLE },
//		{ RANGE_COMBO,	&rangeCombo, 0,				CBS_DROPDOWNLIST|WS_VISIBLE},
	};

	TRect	prevRc;
	for (auto &d: combos) {
		TRect	rc;
		if (toolBar.GetRect(d.id, &rc)) {	// USEREX_COMBOはUSER_COMBOを参照
			prevRc = rc;
		} else {
			rc = prevRc;
		}
		HWND hCombo = CreateWindow("COMBOBOX", "",
			WS_CHILD | d.flags | CBS_AUTOHSCROLL| WS_CLIPSIBLINGS | WS_VSCROLL,
			rc.x(), rc.y(), rc.cx(), 400, hWnd, (HMENU)(DWORD_PTR)d.id, TApp::hInst(), NULL);
		d.wnd->AttachWnd(hCombo);
		d.wnd->SendMessage(WM_SETFONT, (WPARAM)hFont, 0);
		if (d.tip) {
			d.wnd->CreateTipWnd(LoadStrW(d.tip));
		}
	}
	bodyEdit.AttachWnd(::GetWindow(bodyCombo.hWnd, GW_CHILD));
	bodyEdit.CreateTipWnd(LoadStrW(IDS_FINDTIP));

	userEdit.AttachWnd(::GetWindow(userComboEx.hWnd, GW_CHILD));
	userEdit.CreateTipWnd(LoadStrW(IDS_USERMANTIP));

	return	TRUE;
}

BOOL TLogView::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT) {
		TRect	scRect;
		GetCurrentScreenSize(&scRect);

		int	sx  = scRect.x();
		int	sy  = scRect.y();
		int	scx = scRect.cx();
		int	scy = scRect.cy() - 50;  // 後日、workspace取得＆動的補正に変更

		int	cx = 650;
		int	cy = 750;
		int	x  = sx + 150;
		int	y  = sy + 10;

		if (cfg->LvX || cfg->LvY || cfg->LvCx || cfg->LvCy) {
			x = cfg->LvX;
			y = cfg->LvY;
			cx = cfg->LvCx;
			cy = cfg->LvCy;
		}
		if (!isMain) {
			x += 60;
			y += 30;
		}

		if (x + cx > sx + scx) {
			x -= (x + cx) - (sx + scx);
			if (x < sx) {
				cx -= (sx - x);
				x = sx;
			}
		}
		if (y + cy > sy + scy) {
			y -= (y + cy) - (sy + scy);
			if (y < sy) {
				cy -= (sy - y);
				y = sy;
			}
		}
		MoveWindow(x, y, cx, cy, FALSE);
	}

	memset(statusMsg, 0, sizeof(statusMsg));

	SetupToolbar();

	statusCtrl.AttachWnd(::CreateStatusWindowW(WS_CHILD|WS_VISIBLE|CCS_BOTTOM|SBARS_SIZEGRIP,
		0, hWnd, LOGVIEW_STATUS));

	TRect cvrc;
	childView.CreateU8();
	childView.GetWindowRect(&cvrc);

//	SetDlgItem(BODY_COMBO, RIGHT_FIT|TOP_FIT);
//	SetDlgItem(NARROW_CHK, RIGHT_FIT|TOP_FIT);
//	SetDlgItem(STATUS_STATIC, X_FIT|TOP_FIT);

//	SetDlgIcon(hWnd);

	static HICON hBigIcon;
	static HICON hSmallIcon;

	if (!hBigIcon) {
		hBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)LOGVIEW_ICON,
			IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
		hSmallIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)LOGVIEW_ICON,
			IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	}
	SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
	SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

	if (!isMain) {
		WCHAR	title[MAX_BUF];
		GetWindowTextW(title, wsizeof(title));
		wcscat(title, L" (Sub Window)");
		SetWindowTextW(title);
	}

	if (IsWin7()) {
		SetWinAppId(hWnd, L"ipmsg_logview");
	}
	EvSize(SIZE_RESTORED, 0, 0);
	Show();
	return	TRUE;
}

BOOL TLogView::EvNcDestroy(void)
{
	::DeleteObject(hMiniFont);
	hMiniFont = NULL;
	toolBar.UnInit();

	::PostMessage(GetMainWnd(), WM_LOGVIEW_CLOSE, 0, (LPARAM)this);

	if (isMain) {
		cfg->WriteRegistry(CFG_WINSIZE);
	}

	return	TRUE;
}

BOOL TLogView::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
		if (auto hFocus = GetFocus()) {
			if (hFocus == bodyEdit.hWnd || hFocus == userEdit.hWnd) {
				TChildView::FindMode mode = (::GetKeyState(VK_SHIFT) & 0x8000) ? 
												TChildView::PREV_IDX : TChildView::NEXT_IDX;
				if (childView.SetFindedIdx(mode,
					TChildView::SF_REDRAW     |
					TChildView::SF_SAVEHIST   |
					TChildView::SF_FASTSCROLL)) {
					childView.SetUserSelected();
				}
			}
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case LIST_BTN:
	case UP_BTN:
	case DOWN_BTN:
	case NARROW_CHK:
	case BOTTOM_BTN:
	case FILE_CHK:
	case CLIP_CHK:
	case UNOPENR_CHK:
	case FAV_CHK:
	case MARK_CHK:
	case MENU_BTN:
	case MENU_MEMO:
	case RANGE_SCRL:
	case MENU_REVERSE:
	case MENU_FETCHDB:
	case MENU_DBVACUUM:
	case REV_BTN:
	case BOTTOM_ACCEL:
	case NARROW_ACCEL:
	case LIST_ACCEL:
	case TOP_ACCEL:
	case USER_ACCEL:
		childView.SetFocus();
		// fall through...

	case FIND_ACCEL:
	case USER_COMBO:
	case USEREX_COMBO:
	case USER_CHK:
	case RANGE_COMBO:
	case LOGVIEW_REPLY:
	case SEND_ACCEL:
	case MENU_SENDDLG:
		return	childView.EvCommand(wNotifyCode, wID, hwndCtl);

	case BODY_COMBO:
		return	childView.EvCommand(wNotifyCode, wID, hwndCtl);

	case COPY_ACCEL:
	case PASTE_ACCEL:
		{
			HWND	hFocus  = ::GetFocus();
			HWND	hParent = ::GetParent(hFocus);

			if (bodyCombo.hWnd == hParent || userComboEx.hWnd == hParent) {
				UINT	cmd = (wID == COPY_ACCEL) ? WM_COPY : WM_PASTE;
				::PostMessage(hFocus, cmd, 0, 0);
				return	TRUE;
			}
			childView.SetFocus();
			return	childView.EvCommand(wNotifyCode, wID, hwndCtl);
		}

	case MENU_LOGIMPORT:
	case MENU_LOGTOFILE:
	case MENU_FONT:
	case MENU_FOLDQUOTE:
		return	childView.EvCommand(wNotifyCode, wID, hwndCtl);

	case MENU_FINDLRUCLEAR:
		if (TMsgBox(this).Exec(LoadStrU8(IDS_CLEARFINDLRU)) == IDOK) {
			childView.ClearFindURL();
			::PostMessage(GetMainWnd(), WM_LOGVIEW_RESETFIND, 0, (LPARAM)this);
		}
		return TRUE;

	case MENU_NEWLOGVIEW:
		::PostMessage(GetMainWnd(), WM_LOGVIEW_OPEN, 1, 0);
		return TRUE;

	case MENU_HELP:
		::PostMessage(GetMainWnd(), WM_COMMAND, MENU_HELP_LOGVIEW, 0);
		return TRUE;

	case MENU_HELP_TIPS:
		::PostMessage(GetMainWnd(), WM_COMMAND, MENU_HELP_TIPS, 0);
		return TRUE;

	case MENU_CLOSE:
		PostMessage(WM_COMMAND, IDCANCEL, 0);
		return TRUE;

	case MENU_SETUP:
		::PostMessage(GetMainWnd(), WM_IPMSG_SETUPDLG, LINK_SHEET, 0);
		return TRUE;

	case HIDE_ACCEL:
	case MENU_URL:
	case MENU_SUPPORTBBS:
	case MENU_LOGOPEN:
	case MENU_LOGIMGOPEN:
	case MENU_DLLINKS:
	case MENU_AUTOSAVE:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return TRUE;
	}
	return FALSE;
}

BOOL TLogView::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
//	if (uCmdType == SC_CLOSE) ::PostMessage(GetMainWnd(), WM_SYSCOMMAND, MENU_EXIT, 0);

//	switch (uCmdType) {
//	case MENU_LOGIMPORT:
//	case MENU_DBVACUUM:
//	case MENU_LOGTOFILE:
//	case MENU_FONT:
//	case MENU_FINDLRUCLEAR:
//	case MENU_NEWLOGVIEW:
//	case MENU_SENDDLG:
//	case MENU_HELP:
//	case MENU_HELP_TIPS:
//	case MENU_SUPPORTBBS:
//	case MENU_MEMO:
//	case MENU_REVERSE:
//	case MENU_CLOSE:
//	case MENU_LOGOPEN:
//	case MENU_LOGIMGOPEN:
//		return	EvCommand(0, (WORD)uCmdType, 0);
//	}

	return	FALSE;
}

BOOL TLogView::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	if (ctlID == LOGVIEW_TOOLBAR) {
		if (pNmHdr->code == TBN_GETINFOTIPW) {
			NMTBGETINFOTIPW	*itip = (NMTBGETINFOTIPW *)pNmHdr;
			itip->pszText = NULL;

			switch (itip->iItem) {
			case LOGTB_TITLE:	itip->pszText = L"ヘッダ行のみ";	break;
			case LOGTB_REV:		itip->pszText = L"逆順";	break;
			case LOGTB_TAIL:	itip->pszText = L"最新位置にスクロール";	break;
			case LOGTB_NARROW:	itip->pszText = L"検索結果のみに絞り込み";	break;
			case LOGTB_FAV:		itip->pszText = L"お気に入り表示";	break;
			case LOGTB_CLIP:	itip->pszText = L"画像メッセージを表示";	break;
			case LOGTB_MEMO:	itip->pszText = L"メモを追加";	break;
			case LOGTB_MENU:	itip->pszText = L"その他メニュー";	break;
			}
			if (itip->pszText) itip->cchTextMax = (int)wcslen(itip->pszText);
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TLogView::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	return	childView.EventFocus(uMsg, hFocusWnd);
}

BOOL TLogView::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	return	childView.EventKey(uMsg, nVirtKey, lKeyData);
}

BOOL TLogView::EvPaint()
{
	PAINTSTRUCT ps;
	HDC	hDc = BeginPaint(hWnd, &ps);

	TRect	brc;
	GetClientRect(&brc);

	toolBar.Paint(hDc, &ps, &brc);

	EndPaint(hWnd, &ps);

	return	TRUE;
}

BOOL TLogView::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_MINIMIZED) {
		return	FALSE;
	}

	TRect	rc;
	GetClientRect(&rc);
	rc.bottom = toolBar.Cy();
//	InvalidateRect(&rc, FALSE);

	FitDlgItems();

	statusCtrl.SendMessage(WM_SIZE, fwSizeType, MAKELPARAM(nWidth, nHeight));
	TRect crc;
	GetClientRect(&crc);
	int size[] = { crc.cx() - 250, crc.cx() };
	statusCtrl.SendMessage(SB_SETPARTS, 2, (LPARAM)size);

	childView.FitSize();

	if (fwSizeType != SIZE_MAXIMIZED && isMain) {
		GetWindowRect(&rect);
		cfg->LvX = rect.left;
		cfg->LvY = rect.top;
		cfg->LvCx = rect.cx();
		cfg->LvCy = rect.cy();
	}

	return	FALSE;;
}

BOOL TLogView::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	return	toolBar.Timer(timerID, proc);
}

BOOL TLogView::EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos)
{
	return	childView.EvMouseWheel(fwKeys, zDelta, xPos, yPos);
}

BOOL TLogView::EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis)
{
	return	childView.EvDrawItem(ctlID, lpDis);
}

BOOL TLogView::EvMouseMove(UINT fwKeys, POINTS pos)
{
	return	toolBar.MouseMove(fwKeys, pos);
}

BOOL TLogView::EvNcHitTest(POINTS _ps, LRESULT *result)
{
	return	toolBar.NcHitTest(_ps, result);
}

BOOL TLogView::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	return	toolBar.Button(uMsg, nHitTest, pos);
}

BOOL TLogView::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	switch (uMsg) {
	case WM_INITMENU:
		::CheckMenuItem(GetSubMenu(hMenu, 1), MENU_REVERSE,
			MF_BYCOMMAND|((childView.GetDispFlasgs() & TChildView::DISP_REV) ?
			MF_CHECKED : MF_UNCHECKED));

		::CheckMenuItem(GetSubMenu(hMenu, 1), MENU_FOLDQUOTE,
			MF_BYCOMMAND|((childView.GetDispFlasgs() & TChildView::DISP_FOLD_QUOTE) ?
			MF_CHECKED : MF_UNCHECKED));

		return	TRUE;
	}
	return	FALSE;
}

BOOL TLogView::SetStatusMsg(int pos, const WCHAR *msg)
{
	if (pos >= MAX_STATUS_MSGNUM) return FALSE;

	if (wcscmp(statusMsg[pos], msg)) {
		statusCtrl.SendMessageW(SB_SETTEXTW, pos | 0, (LPARAM)msg);
		wcsncpyz(statusMsg[pos], msg, wsizeof(statusMsg[pos]));
	}
	return	TRUE;
}

BOOL TLogView::SetUser(const Wstr &uid)
{
	return	hWnd ? childView.SetUser(uid) : FALSE;
}

BOOL TLogView::ResetCache()
{
	return	hWnd ? childView.ResetCache() : FALSE;
}

BOOL TLogView::ResetFindBodyCombo()
{
	if (!hWnd) {
		return	FALSE;
	}
	bodyCombo.SendMessage(CB_RESETCONTENT, 0, 0);
	return	TRUE;
}

BOOL TLogView::SwitchUserCombo()
{
	BOOL to_combo = IsUserComboEx();

	userCombo.Show(to_combo ? SW_SHOW : SW_HIDE);
	userComboEx.Show(to_combo ? SW_HIDE : SW_SHOW);

	userCombo.SendMessage(CB_SETCURSEL, 0, 0);
	userComboEx.SendMessage(CB_SETCURSEL, 0, 0);
	userComboEx.SetWindowTextW(L"");

	return	TRUE;
}

BOOL TUserCombo::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	if (uMsg != WM_KEYDOWN) {
		return	FALSE;
	}

	switch (nVirtKey) {
	case 'U':
		if (::GetKeyState(VK_CONTROL) & 0x8000) {
			parent->PostMessage(WM_COMMAND, USER_CHK, 0);
			return	TRUE;
		}
		break;
	}
	return	FALSE;
}


#if 0

#ifdef USE_TB
	TBBUTTON tb[] = {
		{ LOGTBICO_TITLE_OFF, LOGTB_TITLE,  TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ LOGTBICO_REV_OFF,   LOGTB_REV,    TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ 100, 0, TBSTATE_ENABLED, TBSTYLE_SEP|BTNS_SEP, 0, 0 }, // UserCombo
		{ LOGTBICO_TAIL_OFF,  LOGTB_TAIL,   TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ 100, 0, TBSTATE_ENABLED, TBSTYLE_SEP|BTNS_SEP, 0, 0 }, // SearchCombo
		{ LOGTBICO_NARROW,    LOGTB_NARROW, TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ 100, 0, TBSTATE_ENABLED, TBSTYLE_SEP|BTNS_SEP, 0, 0 }, // StatusStatic
		{ LOGTBICO_FAV,       LOGTB_FAV,    TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ LOGTBICO_CLIP,      LOGTB_CLIP,   TBSTATE_ENABLED, TBSTYLE_CHECK|BTNS_CHECK },
		{ 3, 0, TBSTATE_ENABLED, TBSTYLE_SEP|BTNS_SEP, 0, 0 },   // Separator
		{ LOGTBICO_MEMO,      LOGTB_MEMO,   TBSTATE_ENABLED, TBSTYLE_BUTTON|BTNS_BUTTON },
		{ LOGTBICO_MENU,      LOGTB_MENU,   TBSTATE_ENABLED, TBSTYLE_BUTTON|BTNS_BUTTON },
	};

	HWND hToolBar = ::CreateToolbarEx(hWnd,
		WS_CHILD|WS_VISIBLE|TBSTYLE_TOOLTIPS,
		LOGVIEW_TOOLBAR, LOGTBICO_MAX, TApp::hInst(), LOGVIEWTB_BITMAP,
		tb, sizeof(tb) / sizeof(TBBUTTON), 0, 0, 16, 18, sizeof(TBBUTTON));
	toolBar.AttachWnd(hToolBar);

	HFONT	hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0);

	::DestroyWindow(GetDlgItem(USER_COMBO));
	HWND hComboWnd = CreateWindow(
		"COMBOBOX", "",
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL| WS_CLIPSIBLINGS | WS_VSCROLL,
		45, 2, 100, 400, hToolBar, (HMENU)(USER_COMBO), TApp::hInst(), NULL);
	userCombo.AttachWnd(hComboWnd);
	userCombo.SendMessage(WM_SETFONT, (WPARAM)hFont, 0);

	menuButton.AttachWnd(GetDlgItem(MENU_BTN));

	::DestroyWindow(GetDlgItem(BODY_COMBO));
	HWND bodyWnd = CreateWindow(
		"COMBOBOX", "",
		WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL| WS_CLIPSIBLINGS | WS_VSCROLL,
		169, 2, 100, 400, hToolBar, (HMENU)(BODY_COMBO), TApp::hInst(), NULL);
	bodyCombo.AttachWnd(bodyWnd);
	bodyCombo.SendMessage(WM_SETFONT, (WPARAM)hFont, 0);

	::DestroyWindow(GetDlgItem(STATUS_STATIC));
	HWND statusWnd = CreateWindow(
		"STATIC", "",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		295, 4, 90, 18, hToolBar, (HMENU)(STATUS_STATIC), TApp::hInst(), NULL);
	statusStatic.AttachWnd(statusWnd);
	statusStatic.SendMessage(WM_SETFONT, (WPARAM)hFont, 0);
	toolBar.SendMessage(TB_SETPADDING, 0, MAKELPARAM(10, 10));

#else
	userCombo.AttachWnd(GetDlgItem(USER_COMBO));
	menuButton.AttachWnd(GetDlgItem(MENU_BTN));
	bodyCombo.AttachWnd(GetDlgItem(BODY_COMBO));
	statusStatic.AttachWnd(GetDlgItem(STATUS_STATIC));
#endif
#endif


BOOL TComboEdit::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	if ((::GetKeyState(VK_CONTROL) & 0x8000) &&
			(nVirtKey != 'V' &&
			 nVirtKey != 'N' &&
			 nVirtKey != 'C' &&
			 nVirtKey != 'Z' &&
			 nVirtKey != 'X')
		|| nVirtKey == VK_RETURN) {
		view->PostMessage(uMsg, nVirtKey, lKeyData);
	}
	return	FALSE;
}


