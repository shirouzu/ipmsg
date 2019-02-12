static char *logchildviewev_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logchildviewev.cpp Ver4.60";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child Pane for Event
	Create					: 2015-04-10(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "logchilddef.h"
#include "ipmsg.h"
#include <gdiplus.h>

using namespace Gdiplus;
using namespace std;

#include "logdbconv.h"
#include "logimportdlg.h"

#ifdef USE_MEM
unordered_map<int64, ViewMsg *> fullMsgMap;
static void test_msgMap_set();
static void test_msgMap_set_fromdb();
#endif

/* ===============================================================
                          ChildView for event
   =============================================================== */
BOOL TChildView::EvCreate(LPARAM lParam)
{
	InitGDI();
	logDb = NULL;

	findTick = 0;
	findUpdateFlags = UF_NONE;

	findGenNo++;
	findBody.Init();
	findBodyOrg.Init();
	findBodyVec.clear();
	findUser.Init();
	dispFlags =
		((cfg->logViewDispMode & LVDM_REV) ? DISP_REV : 0) |
		((cfg->logViewDispMode & LVDM_NOFOLDQUOTE) ? 0 : DISP_FOLD_QUOTE);
	foldGenNo++;

	scrTrackNpos = -1;
	scrHresReduce = 0;
	mouseScrMode = MOUSE_SCR_INIT;

	dblClickStart = 0;

//	test_msgMap_set_fromdb();
	if (!logMng->IsInitLogDb()) {
		BOOL	with_import = !logMng->IsLogDbExists();
		logMng->InitLogDb(with_import);
	}
	logDb = logMng->GetLogDb();

#define BASEMSG_NUM		(1000000)
	int cur_num = max(logDb->GetMaxNum(), 0);
	int	max_num = (cur_num * 2) + BASEMSG_NUM;

#define MSGCHUNK_NUM	(16384)
	msgIds.Init(0, max_num, MSGCHUNK_NUM);
	msgFindIds.Init(0, max_num, MSGCHUNK_NUM);

#define MAXHOST_NUM		(100000)
#define HOSTCHUNK_NUM	(1024)
	hostList.Init(0, MAXHOST_NUM, HOSTCHUNK_NUM);

	UpdateMsgList(UF_NO_FITSIZE);
	UserComboSetup();

	SetFont();
	FitSize();
	SetFocus();
	isCapture = FALSE;
	lastMousePos.Init();

	InitBtnBmp();
	UpdateBtnBmp();
	UpdateFindComboList();

	if (savedCurMsgId == 0) {
		SetFindedIdx((dispFlags & DISP_REV) ? TOP_IDX : LAST_IDX);
	}

	SetTimer(UPDATE_TIMER, UPDATE_TICK);

	return	TRUE;
}

BOOL TChildView::UnInitGDI()
{
	if (hMemDc) {
		::SelectObject(hMemDc, hMemBmpSave);
		::DeleteObject(hMemBmp);
		::DeleteDC(hMemDc);
		hMemBmp = NULL;
		hMemBmpSave = NULL;
		hMemDc = NULL;
	}
	if (hTmpDc) {
		::DeleteDC(hTmpDc);
		hTmpDc = NULL;
	}
	return	TRUE;
}

BOOL TChildView::InitGDI()
{
	TRect	trc;
//	parentView->toolBar.GetWindowRect(&trc);
	parentView->userCombo.GetWindowRect(&trc);
	trc.set_cy(LVBASE_CY);
	toolBarCy = trc.cy();
	parentView->statusCtrl.GetWindowRect(&trc);
	statusCtrlCy = trc.cy();

	HWND	hDesktop   = ::GetDesktopWindow();
	HDC		hDesktopDc = ::GetDC(hDesktop);

	hMemDc	= ::CreateCompatibleDC(hDesktopDc);
	hTmpDc	= ::CreateCompatibleDC(hDesktopDc);
//	SelectObject(hTmpDc, ::CreateCompatibleBitmap(hDesktopDc, 12, 12));
	::ReleaseDC(hDesktop, hDesktopDc);

	hToMsgBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)TOMSG_BITMAP);
	hToMsg2Bmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)TOMSG2_BITMAP);
	hFromMsgBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FROMMSG_BITMAP);
	hReplyMsgBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)REPLYMSG_BITMAP);
	hCropUserBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)CROPUSER_BITMAP);
	hReplyBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)REPLY_BITMAP);
	hReplyHideBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)REPLYHIDE_BITMAP);
	hFileBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FILE_BITMAP);
	hClipBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)CLIP_BITMAP);
	hFavBmp  = ::LoadBitmap(TApp::hInst(), (LPCSTR)FAV_BITMAP);
	hFavMidBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FAVMID_BITMAP);
	hFavRevBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FAVREV_BITMAP);
	hMarkBmp  = ::LoadBitmap(TApp::hInst(), (LPCSTR)MARK_BITMAP);
	hMenuBmp  = ::LoadBitmap(TApp::hInst(), (LPCSTR)MISC_BITMAP);
	hImenuBmp    = ::LoadBitmap(TApp::hInst(), (LPCSTR)IMENU_BITMAP);
	hImenuMidBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)IMENUMID_BITMAP);
	hBaseHeadBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)BASEHEAD_BITMAP);
	hAscBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)ASC_BITMAP);
	hDscBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)DESC_BITMAP);
	hCmntBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)CMNT_BITMAP);
	hBottomBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)BOTTOM_BITMAP);
	hBottomHotBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)BOTTOMHOT_BITMAP);
	hTopBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)TOP_BITMAP);
	hTopHotBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)TOPHOT_BITMAP);
	hNoClipBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)NOCLIP_BITMAP);

	hUnOpenSBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)UNOPEN_BITMAP);
	hUnOpenRBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)UNOPENR_BITMAP);
	hUnOpenBigBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)UNOPENBIG_BITMAP);

	hFoldFBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FOLDF_BITMAP);
	hFoldTBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)FOLDT_BITMAP);

	hShinePen	= ::CreatePen(PS_SOLID, 1, SHINE_RGB);
	hMidShinePen = ::CreatePen(PS_SOLID, 1, MIDSHINE_RGB);
	hShadowPen	= ::CreatePen(PS_SOLID, 1, SHADOW_RGB);
	hGrayPen	= ::CreatePen(PS_SOLID, 1, GRAY_RGB);
	hSelPen		= ::CreatePen(PS_SOLID, 1, SEL_RGB);
	hMarkPen	= ::CreatePen(PS_SOLID, 1, MARK_RGB);
	hFindPen	= ::CreatePen(PS_SOLID, 1, FIND_RGB);
	hWhitePen	= (HPEN)::GetStockObject(WHITE_PEN);
	hNullPen	= (HPEN)::GetStockObject(NULL_PEN);

	// font set is SetFont()
	LOGBRUSH lb = { BS_SOLID, GRAY_RGB, 0 };
	hGrayBrush	= ::CreateBrushIndirect(&lb);
	lb.lbColor = MIDGRAY_RGB;
	hMidGrayBrush = ::CreateBrushIndirect(&lb);
	lb.lbColor = SHINE_RGB;
	hShineBrush	= ::CreateBrushIndirect(&lb);
	lb.lbColor = MIDSHINE_RGB;
	hMidShineBrush = ::CreateBrushIndirect(&lb);
	lb.lbColor = SHADOW_RGB;
	hShadowBrush = ::CreateBrushIndirect(&lb);
	lb.lbColor = SEL_RGB;
	hSelBrush	= ::CreateBrushIndirect(&lb);
	lb.lbColor = MARK_RGB;
	hMarkBrush	= ::CreateBrushIndirect(&lb);
	lb.lbColor = CMNT_RGB;
	hCmntBrush = ::CreateBrushIndirect(&lb);
	lb.lbColor = FIND_RGB;
	hFindBrush	= ::CreateBrushIndirect(&lb);
	hWhiteBrush	= (HBRUSH)::GetStockObject(WHITE_BRUSH);

	hArrowCur	= ::LoadCursor(NULL, IDC_ARROW);
	hIBeamCur	= ::LoadCursor(NULL, IDC_IBEAM);
    hHandCur	= ::LoadCursor(NULL, IDC_HAND);
	hLoupeCur	= ::LoadCursor(TApp::hInst(), (LPCSTR)LOUPE_CUR);

	cropUserItem = NULL;
	fileItem	= NULL;
	itemMenu	= NULL;

	SetBkMode(hMemDc, TRANSPARENT);
	SetBkMode(hTmpDc, TRANSPARENT);

//	auto	&rangeCombo = parentView->rangeCombo;
//	rangeCombo.SendMessage(CB_ADDSTRING, 0, (LPARAM)"300");
//	rangeCombo.SendMessage(CB_ADDSTRING, 0, (LPARAM)"1000");
//	rangeCombo.SendMessage(CB_ADDSTRING, 0, (LPARAM)"10000");
//	rangeCombo.SendMessage(CB_ADDSTRING, 0, (LPARAM)"全件");
//	rangeCombo.SendMessage(CB_SETCURSEL, 3, 0);

	return	TRUE;
}

BOOL TChildView::EvNcDestroy(void)
{
	if (dispFlags & DISP_UNOPENR_ONLY) {
		if (logDb) {
			logDb->ClearFlags(DB_FLAG_UNOPENRTMP);
		}
	}

	::DeleteObject(hToMsgBmp);
	::DeleteObject(hToMsg2Bmp);
	::DeleteObject(hFromMsgBmp);
	::DeleteObject(hReplyMsgBmp);
	::DeleteObject(hCropUserBmp);
	::DeleteObject(hReplyBmp);
	::DeleteObject(hReplyHideBmp);
	::DeleteObject(hFileBmp);
	::DeleteObject(hClipBmp);
	::DeleteObject(hFavBmp);
	::DeleteObject(hFavMidBmp);
	::DeleteObject(hFavRevBmp);
	::DeleteObject(hMarkBmp);
	::DeleteObject(hMenuBmp);
	::DeleteObject(hImenuBmp);
	::DeleteObject(hImenuMidBmp);
	::DeleteObject(hBaseHeadBmp);
	::DeleteObject(hAscBmp);
	::DeleteObject(hDscBmp);
	::DeleteObject(hCmntBmp);
	::DeleteObject(hBottomBmp);
	::DeleteObject(hBottomHotBmp);
	::DeleteObject(hTopBmp);
	::DeleteObject(hTopHotBmp);
	::DeleteObject(hNoClipBmp);

	::DeleteObject(hUnOpenSBmp);
	::DeleteObject(hUnOpenRBmp);
	::DeleteObject(hUnOpenBigBmp);

	::DeleteObject(hFoldFBmp);
	::DeleteObject(hFoldTBmp);

	::DeleteObject(hShinePen);
	::DeleteObject(hMidShinePen);
	::DeleteObject(hShadowPen);
	::DeleteObject(hGrayPen);
	::DeleteObject(hSelPen);
	::DeleteObject(hMarkPen);
	::DeleteObject(hFindPen);
	::DeleteObject(hWhitePen);
	::DeleteObject(hNullPen);

	::DeleteObject(hGrayBrush);
	::DeleteObject(hMidGrayBrush);
	::DeleteObject(hShineBrush);
	::DeleteObject(hMidShineBrush);
	::DeleteObject(hShadowBrush);
	::DeleteObject(hSelBrush);
	::DeleteObject(hMarkBrush);
	::DeleteObject(hCmntBrush);
	::DeleteObject(hFindBrush);
	::DeleteObject(hWhiteBrush);

	::DeleteObject(hArrowCur);
	::DeleteObject(hIBeamCur);
	::DeleteObject(hHandCur);
	::DeleteObject(hLoupeCur);

	::DeleteObject(hFont);
	::DeleteObject(hBoldFont);
	::DeleteObject(hMiniFont);
	::DeleteObject(hLinkFont);
//	::DeleteObject(hTinyFont);

	UnInitGDI();

	memset(&hToMsgBmp, 0, offsetof(TChildView, hMemDc) - offsetof(TChildView, hToMsgBmp));

	if (curIdx >= 0 && curIdx < msgIds.UsedNumInt()) {
		savedCurMsgId = msgIds[curIdx].msg_id;
	}
	//ClearMsgCacheAll();

	return	TRUE;
}

BOOL TChildView::UserPopupMenu()
{
	return TRUE;
}

int host_to_str(LogHost *host, WCHAR *buf) {
	WCHAR	*s = buf;

	if (host->gname) {
		return	swprintf(s, L"%s (%s/%s/%s/%s)", host->nick.s(), host->gname.s(),
			host->host.s(), host->addr.s(), host->uid.s());
	}
	return	swprintf(s, L"%s (%s/%s/%s)", host->nick.s(), host->host.s(), host->addr.s(),
		host->uid.s());
}

#define FIND_TICK 100


BOOL TChildView::MainMenuEvent()
{
	HMENU	hMenu = ::LoadMenu(TApp::hInst(), (LPCSTR)LOGVIEW_MENU);
	HMENU	hSubMenu = ::GetSubMenu(hMenu, 0);	//かならず、最初の項目に定義

	TRect	rc;
	parentView->toolBar.GetRect(MENU_BTN, &rc);
	POINT	pt = { rc.x(), rc.bottom - toolBarCy };
	::ClientToScreen(hWnd, &pt);

	::SetForegroundWindow(hWnd);

	BOOL ret = ::TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON,
		pt.x, pt.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	if (!ret) itemMenu = NULL;
	return	ret;
}

void TChildView::InitBtnBmp()
{
}

void TChildView::UpdateBtnBmp()
{
	BOOL	is_rev = (dispFlags & DISP_REV) ? TRUE : FALSE;

//	parentView->toolBar.UpdateBtnBmp(REV_BTN, is_rev ? DESC_BITMAP : ASC_BITMAP);

	parentView->toolBar.UpdateBtnBmp(BOTTOM_BTN,
		is_rev ?  ((scrollMode == AUTO_SCROLL) ? TOPHOT_BITMAP    : TOP_BITMAP)
		: ((scrollMode == AUTO_SCROLL) ? BOTTOMHOT_BITMAP : BOTTOM_BITMAP));
	parentView->toolBar.UpdateTipId(BOTTOM_BTN,
		//findBodyOrg ? IDS_FINDBOTTOMTIP :
		(scrollMode == AUTO_SCROLL) ? IDS_BOTTOMHOTTIP : IDS_BOTTOMTIP);

	parentView->toolBar.UpdateBtnBmp(LIST_BTN,
		(dispFlags & DISP_TITLE_ONLY) ? FULLDISP_BITMAP : TITLEDISP_BITMAP);

	parentView->toolBar.UpdateBtnBmp(NARROW_CHK, findBodyOrg ? NARROW_BITMAP : NARROWD_BITMAP);
	parentView->toolBar.UpdateBtnState(NARROW_CHK, findBodyOrg ? (dispFlags & DISP_NARROW) ?
		ToolBar::SELECT : ToolBar::NORMAL : ToolBar::DISABLE);

	parentView->toolBar.UpdateBtnState(UNOPENR_CHK,
		(dispFlags & DISP_UNOPENR_ONLY) ? ToolBar::SELECT : ToolBar::NORMAL);

	parentView->toolBar.UpdateBtnState(FAV_CHK,
		(dispFlags & DISP_FAV_ONLY) ? ToolBar::SELECT : ToolBar::NORMAL);

	parentView->toolBar.UpdateBtnState(CLIP_CHK,
		(dispFlags & DISP_CLIP_ONLY) ? ToolBar::SELECT : ToolBar::NORMAL);

	parentView->toolBar.UpdateBtnState(FILE_CHK,
		(dispFlags & DISP_FILE_ONLY) ? ToolBar::SELECT : ToolBar::NORMAL);

	parentView->toolBar.UpdateBtnState(MARK_CHK,
		(dispFlags & DISP_MARK_ONLY) ? ToolBar::SELECT : ToolBar::NORMAL);

	parentView->toolBar.UpdateBtnState(USER_CHK,
		parentView->IsUserComboEx() ? ToolBar::SELECT : ToolBar::NORMAL);
}

//BOOL MiniBitBlt(HDC hDstDc, HDC hMemDc, HBITMAP hBmp, TRect rc)
//{
//	HGDIOBJ	svObj = ::SelectObject(hMemDc, hBmp);
//	BOOL ret = ::BitBlt(hDstDc, rc.x(), rc.y(), rc.cx(), rc.cy(), hMemDc, 0, 0, SRCCOPY);
//	::SelectObject(hMemDc, svObj);
//
//	return	ret;
//}

//BOOL TChildView::EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis)
//{
//	if (ctlID == LIST_BTN) {
//		MiniBitBlt(lpDis->hDC, hMemDc,
//			(lpDis->itemState & ODS_SELECTED) ? hTitleDispsBmp :
//			(lpDis->itemState & ODS_FOCUS)    ? hTitleDispfBmp :
//			(lpDis->itemState & ODS_HOTLIGHT) ? hTitleDispfBmp : hTitleDispnBmp,
//			lpDis->rcItem);
//		return	TRUE;
//	}
//	return	FALSE;
//}

int TChildView::DispToDbFlags(DispFlags flag)
{
	return	((flag & DISP_CLIP_ONLY)    ? DB_FLAG_CLIP : 0) |
			((flag & DISP_FILE_ONLY)    ? DB_FLAG_FILE : 0) |
			((flag & DISP_MARK_ONLY)    ? (DB_FLAG_MARK|DB_FLAG_CMNT) : 0) |
			((flag & DISP_FAV_ONLY)     ? DB_FLAG_FAV     : 0) |
			((flag & DISP_UNOPENR_ONLY) ? DB_FLAG_UNOPENR : 0);
}

BOOL TChildView::FilterBtnAction(DispFlags flag)
{
	if (dispFlags & flag) {
		dispFlags &= ~(DWORD)flag;
	} else {
		dispFlags |= flag;
	}
	int		pre_num = msgIds.UsedNumInt();
	BOOL	ret = UpdateMsgList(UF_NO_FINDMOVE);
	int		num = msgIds.UsedNumInt();
	BOOL	reset = !ret;

	if ((dispFlags & flag) && !reset) {
		if (num == 0 || pre_num == num && logDb->GetFlagsNum(DispToDbFlags(flag)) == 0) {
			reset = TRUE;
		}
	}
	if (reset) {
		dispFlags &= ~(DWORD)flag;
	}
	UpdateBtnBmp();
	return	ret;
}

BOOL TChildView::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
		return	TRUE;

	case IDCANCEL:
		parentView->EndDialog(IDCANCEL);
		return	TRUE;

	case LIST_ACCEL:
	case LIST_BTN:
		{
			BOOL is_list = (dispFlags & DISP_TITLE_ONLY) ? TRUE : FALSE;
			if (is_list) {
				dispFlags &= ~DISP_TITLE_ONLY;
			} else {
				dispFlags |= DISP_TITLE_ONLY;
			}
			UpdateBtnBmp();
			FitSize();
		}
		return	TRUE;

	case USER_COMBO:
		if (wNotifyCode == CBN_DROPDOWN) {
			UserComboSetup();
		}
		else if (wNotifyCode == CBN_CLOSEUP /* CBN_SELCHANGE */) {
			UserComboClose();
		}
		return	TRUE;

	case USEREX_COMBO:
		{
			Wstr	buf(FIND_CHAR_MAX);
			parentView->userComboEx.GetWindowTextW(buf.Buf(), FIND_CHAR_MAX);
			findUser = buf;
			UpdateMsgList(UF_NO_FINDMOVE);
		}
		return	TRUE;

	case USER_ACCEL:
	case USER_CHK:
		parentView->SwitchUserCombo();
		UpdateBtnBmp();
		findUser = L"";
		UpdateMsgList(UF_NO_FINDMOVE);
		if (parentView->IsUserComboEx()) {
			parentView->userComboEx.SetFocus();
		}
		return	TRUE;

	case BODY_COMBO:
		{
			Wstr	buf(FIND_CHAR_MAX);
			BOOL	is_selchange = (wNotifyCode == CBN_SELCHANGE) ? TRUE : FALSE;

			if (is_selchange) {
				LRESULT	idx = parentView->bodyCombo.SendMessage(CB_GETCURSEL, 0, 0);
				parentView->bodyCombo.SendMessageW(CB_GETLBTEXT, idx, (LPARAM)buf.Buf());
				parentView->bodyCombo.SetWindowTextW(buf.s());
			}
			parentView->bodyCombo.GetWindowTextW(buf.Buf(), FIND_CHAR_MAX);

			if (findBodyOrg != buf) {
//				DebugW(L"combo=%s old=%s\n", buf.s(), findBodyOrg.s());
				if (is_selchange) {
					StartFindTimer(UF_UPDATE_BODYLRU, 1);
				}
				else {
					StartFindTimer();
				}
			}
		}
		return	TRUE;

//	case RANGE_COMBO:
//		if (wNotifyCode == CBN_SELCHANGE) {
//			auto idx = parentView->rangeCombo.SendMessage(CB_GETCURSEL, 0, 0);
//			switch (idx) {
//			case 0:	range =   300; break;
//			case 1:	range =  1000; break;
//			case 2:	range = 10000; break;
//			default:range =    -1; break;
//			}
//			SetFocus();
//			UpdateMsgList(UF_NO_FINDMOVE);
//		}
//		return	TRUE;

	case DOWN_BTN:
		SetFindedIdx(NEXT_IDX);
		return	TRUE;

	case UP_BTN:
		SetFindedIdx(PREV_IDX);
		return	TRUE;

	case NARROW_ACCEL:
	case NARROW_CHK:
		if (findBodyOrg) {
			if (dispFlags & DISP_NARROW) {
				dispFlags &= ~DISP_NARROW;
			} else {
				dispFlags |= DISP_NARROW;
			}
			UpdateBtnBmp();
			UpdateMsgList((dispFlags & DISP_NARROW) ? UF_NONE : UF_NO_FINDMOVE);
		}
		return	TRUE;

	case TOP_ACCEL:
	case BOTTOM_ACCEL:
	case BOTTOM_BTN:
		{
			FindMode mode = (wID == TOP_ACCEL) ? TOP_IDX : LAST_IDX;
			BOOL check = (dispFlags & DISP_REV) ? TRUE : FALSE;
			BOOL shift = (::GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;

			if (check ^ shift) {
				mode = (mode == TOP_IDX) ? LAST_IDX : TOP_IDX;
			}
			SetFindedIdx(mode);
		}
		return	TRUE;

	case FILE_CHK:
		FilterBtnAction(DISP_FILE_ONLY);
		return	TRUE;

	case CLIP_CHK:
		FilterBtnAction(DISP_CLIP_ONLY);
		return	TRUE;

	case UNOPENR_CHK:
		if (dispFlags & DISP_UNOPENR_ONLY) {
			logDb->ClearFlags(DB_FLAG_UNOPENRTMP);
		}
		FilterBtnAction(DISP_UNOPENR_ONLY);
		return	TRUE;

	case FAV_CHK:
		FilterBtnAction(DISP_FAV_ONLY);
		return	TRUE;

	case MARK_CHK:
		FilterBtnAction(DISP_MARK_ONLY);
		return	TRUE;

	case MENU_DBVACUUM:
		SetStatusMsg(0, LoadStrW(IDS_DBVACUUM));
		logDb->Vacuum();
		SetStatusMsg(0, LoadStrW(IDS_DBVACUUMDONE));
		return	TRUE;

	case MENU_FETCHDB:
		logDb->PrefetchCache();
		SetStatusMsg(0, LoadStrW(IDS_DBFETCH));
		return	TRUE;

	case MENU_REVERSE:
	case REV_BTN:
		{
			SetDispedCurIdx();
			UpdateMsgList((dispFlags & DISP_REV) ? UF_TO_ASC : UF_TO_DSC); // 反転
		//	UpdateBtnBmp();

			if (dispFlags & DISP_REV) {
				cfg->logViewDispMode |= LVDM_REV;
			}
			else {
				cfg->logViewDispMode &= ~LVDM_REV;
			}
			cfg->WriteRegistry(CFG_GENERAL);
		}
		return	TRUE;

	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case FIND_ACCEL:
		if (::GetParent(::GetFocus()) == parentView->bodyCombo.hWnd) {
			SetFocus();
		}
		else {
			parentView->bodyCombo.SetFocus();
			SetUserSelected();
		}
		break;

	case MENU_BTN:
		MainMenuEvent();
		return	TRUE;

	case RANGE_SCRL:
		return	TRUE;

	case MENU_MSGHEAD:
		if (itemMenu) {
			EditMsgHead(itemMenu->msg_id);
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_EDITMSG:
		if (itemMenu) {
			EditBody(itemMenu->msg_id);
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_DELMSG:
		if (itemMenu) {
			DelMsg(itemMenu->msg_id);
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_DELALLMSG:
		if (IsSelected(TRUE)) {
			DelSelMsgs(GetMsgIdx(selTop.msg_id), GetMsgIdx(selEnd.msg_id));
		}
		return	TRUE;

	case MENU_COPYCOMMENT:
		if (itemMenu && itemMenu->msg_id) {
			CmntToClip(itemMenu->msg_id);
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_COMMENT:
		if (itemMenu && itemMenu->msg_id) {
			EditComment(itemMenu->msg_id);
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_DELCOMMENT:
		if (itemMenu && itemMenu->msg_id) {
			int64	msg_id = itemMenu->msg_id;
			if (TMsgBox(this).Exec(LoadStrU8(IDS_DELCOMMENT)) == IDOK) {
				Wstr	empty_comment;
				if (itemMenu && itemMenu->msg_id == msg_id) {
					SetComment(msg_id, empty_comment);
				}
			}
			itemMenu = NULL;
		}
		return	TRUE;

	case MENU_MEMO:
		AddMemo();
		return	TRUE;

	case MENU_LOGIMPORT:
		{
			TLogImportDlg	dlg(cfg, logMng->GetLogDb(), this);
			dlg.Exec();
			ClearMsgCacheAll();
			UpdateMsgList(UF_FORCE|UF_NO_FINDMOVE);
		}
		return	TRUE;

	case MENU_LOGTOFILE:
		{
			char	path[MAX_PATH_U8] = {};
			OpenFileDlg	dlg(this->parent, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);
			if (dlg.Exec(path, sizeof(path), NULL, "Log file(*.log)\0*.log\0\0", NULL, "log")) {
				WriteLogToFile(path);
			}
		}
		return	TRUE;

	case MENU_BODYCOPY:
		if (itemMenu) {
			if (ViewMsg *msg = GetMsg(itemMenu->msg_id)) {
				MsgToClip(msg, BODYMSG_COPY);
			}
		}
		return	TRUE;

	case MENU_ALLCOPY:
		if (itemMenu) {
			if (ViewMsg *msg = GetMsg(itemMenu->msg_id)) {
				MsgToClip(msg, ALLMSG_COPY);
			}
		}
		return	TRUE;

	case COPY_ACCEL:
	case MENU_SELCOPY:
		if (dispFlags & DISP_OVERLAPPED_EX) {
			if (Item *item = itemMgr.GetLastProcItem()) {
				if (item->kind & (Item::CLIP|Item::CLIPBODY)) {
					ItemToClip(item);
					return	TRUE;
				}
			}
		}
		SelToClip();
		return	TRUE;

	case MENU_COPYCLIP:
		if (itemMenu) {
			ItemToClip(itemMenu);
		}
		return	TRUE;

	case MENU_SAVECLIP:
		if (itemMenu) {
			SaveClip(itemMenu);
		}
		return	TRUE;

	case MENU_COPYCLIPPATH:
		if (itemMenu && (itemMenu->kind & Item::CLIPBODY)) {
			CopyClipPath(itemMenu);
		}
		return	TRUE;

	case MENU_OPENCLIP:
		if (itemMenu) {
			ButtonDblClickItem(itemMenu, TPoints());
		}
		return	TRUE;

	case MENU_DELCLIP:
		if (itemMenu && (itemMenu->kind & Item::CLIPBODY)) {
			DelClip(itemMenu->msg_id, itemMenu->clip_idx);
		}
		return TRUE;

	case MENU_COPYLINK:
		if (itemMenu && (itemMenu->kind & Item::LINK)) {
			LinkToClip(itemMenu);
		}
		return TRUE;

	case MENU_MARK:
		SelToMark();
		return	TRUE;

	case MENU_MARKOFF:
		SelToMarkOff();
		return	TRUE;

	case MENU_FIND:
		SelToFind();
		return	TRUE;

	case MENU_SEARCHENG:
		SelToSearchEng();
		return	TRUE;

	case MENU_REPLY:
		if (itemMenu) {
			if (ViewMsg *msg = GetMsg(itemMenu->msg_id)) {
				ReplyMsg(msg, (itemMenu->kind == Item::USER) ? itemMenu->user_idx : -1);
			}
		}
		return	TRUE;

	case LOGVIEW_REPLY:
		if (GetMsgIdx(userMsgId) == curIdx) {
			if (ViewMsg *msg = GetMsg(userMsgId)) {
				ReplyMsg(msg);
			}
		}
		return	TRUE;

	case SEND_ACCEL:
	case MENU_SENDDLG:
		{
			// cursor位置に表示
			ReplyInfo	rInfo(wID == SEND_ACCEL ? ReplyInfo::POS_MID : ReplyInfo::POS_MIDDOWN);
			rInfo.foreDuration	= SENDMSG_FOREDURATION;

			::SendMessage(GetMainWnd(), WM_SENDDLG_OPEN, 0, (LPARAM)&rInfo);
		}
		return	TRUE;

	case MENU_FONT:
		SelectMainFont();
		return	TRUE;

	case MENU_FOLDQUOTE:
		dispFlags ^= DISP_FOLD_QUOTE;
		if (dispFlags & DISP_FOLD_QUOTE) {
			cfg->logViewDispMode &= ~LVDM_NOFOLDQUOTE;
		}
		else {
			cfg->logViewDispMode |= LVDM_NOFOLDQUOTE;
		}
		cfg->WriteRegistry(CFG_GENERAL);
		ClearMsgCacheAll();
		InvalidateRect(0, 0);
		return	TRUE;

	case MENU_NEWLOGVIEW:
		if (itemMenu && (itemMenu->kind == Item::USER)) {
			if (ViewMsg *msg = GetMsg(itemMenu->msg_id)) {
				U8str	uid(msg->msg.host[itemMenu->user_idx].uid.s());
				::SendMessage(GetMainWnd(), WM_LOGVIEW_OPEN, 1, (LPARAM)uid.s());
			}
		}
		return TRUE;

	case PASTE_ACCEL:
		PasteComment(0);
		break;

	case MENU_PASTECMNT:
		PasteComment(1);
		return TRUE;

	case MENU_PASTEIMAGE:
		PasteComment(2);
		return TRUE;

	case MENU_INSERTIMAGE:
		if (itemMenu) {
			InsertImage(itemMenu->msg_id);
		}
		return TRUE;

	default:
		if (cropUserItem) {
			if (wID >= WM_CROPMENU && wID <= WM_CROPMENU_MAX) {
				CropUserItem(wID - WM_CROPMENU);
			}
			cropUserItem = NULL;
			return	TRUE;
		}
		else if (fileItem) {
			if (wID >= WM_FILEMENU && wID <= WM_FILEMENU_MAX) {
				FileMenuItem(wID - WM_FILEMENU);
			}
			fileItem = NULL;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TChildView::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	if (uMsg != WM_KEYDOWN) {
		return	FALSE;
	}

	switch (nVirtKey) {
	case VK_UP:
		EventScroll(WM_VSCROLL, SB_LINEUP, 0, 0);
		break;

	case VK_DOWN:
		EventScroll(WM_VSCROLL, SB_LINEDOWN, 0, 0);
		break;

	case VK_HOME:
		SetFindedIdx(TOP_IDX);
		break;

	case VK_END:
		SetFindedIdx(LAST_IDX);
		break;

	case VK_PRIOR:
		EventScroll(WM_VSCROLL, SB_PAGEUP, 0, 0);
		break;

	case VK_NEXT:
		EventScroll(WM_VSCROLL, SB_PAGEDOWN, 0, 0);
		break;

	case VK_LEFT:
		break;

	case VK_RIGHT:
		break;

	case VK_RETURN: case VK_SPACE:
		{
			FindMode mode = (::GetKeyState(VK_SHIFT) & 0x8000) ? PREV_IDX : NEXT_IDX;
			if (SetFindedIdx(mode, SF_REDRAW|SF_FASTSCROLL|SF_NOMOVE)) {
				SetUserSelected();
			}
		}
		break;

	case VK_PROCESSKEY:
		{
			auto key = ::MapVirtualKey(HIWORD(lKeyData) & 0xff, 1);
			if (key && key != VK_PROCESSKEY) {
				return	EventKey(uMsg, key, lKeyData);
			}
		}
		break;
	}
	return	FALSE;
}

BOOL TChildView::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	switch (uMsg) {
	case WM_SETFOCUS:
		//::CreateCaret(hWnd, 0, 2, lineCy);
		//::SetCaretPos(100, 100);
		//::ShowCaret(hWnd);
		break;

	case WM_KILLFOCUS:
		//::DestroyCaret();
		break;
	}

	return	FALSE;
}

BOOL TChildView::EvMouseWheel(WORD fwKeys, short zDelta, short xPos, short yPos)
{
	float	clickTicks = float(zDelta) / WHEEL_DELTA;

	if (fabs(clickTicks) < 0.1) {
		return	FALSE;
	}

	scrOffPix = int(headCy * clickTicks * 2);	// 上下ボタンの2倍の移動量に
	SetScrollPos();

	return	TRUE;
}


//static int scr_cnt=0;


BOOL TChildView::EventScroll(UINT uMsg, int nScrollCode, int nPos, HWND hScroll)
{
	int		idx_sv = curIdx;
	int		new_idx = curIdx;
	int		diff = (dispFlags & DISP_TITLE_ONLY) ? 15 : 1;

	scrOffPix = 0;

	if (uMsg == WM_VSCROLL) {
		switch (nScrollCode) {
		case SB_BOTTOM:
			new_idx = msgIds.UsedNumInt() -1;
			break;

		case SB_TOP:
			new_idx = 0;
			break;

		case SB_LINEDOWN:
//			scrOffPix = -headCy / 3;
//			scr_cnt++;
//			if (scr_cnt == 1) {
//				SetTimer(SCROLL_TIMER, 10);
//			}
//			if (scr_cnt == 3) {
//				scr_cnt = 0;
//				KillTimer(SCROLL_TIMER);
//			}
//			Debug("draw %d\n", scr_cnt);
			scrOffPix = -headCy;
			break;

		case SB_LINEUP:
			scrOffPix = headCy;
			break;

		case SB_PAGEDOWN:
			scrOffPix = headCy - vrc.cy();
			break;

		case SB_PAGEUP:
			scrOffPix = vrc.cy() - headCy;
			break;

		case SB_THUMBPOSITION:
			return TRUE;
//			new_idx = ScrollPosToIdx(nPos, &scrOffPix);
//			break;

		case SB_THUMBTRACK:
			if (msgIds.UsedNumInt() > SCR_HIRES_MAX) {
				scrTrackNpos = nPos;
			}
			new_idx = ScrollPosToIdx(nPos, &scrOffPix);
//			Debug("trac npos=%d  idx=%d\n", nPos, new_idx);
			offsetPix = 0;
			break;

		case SB_ENDSCROLL:
			scrTrackNpos = -1;
			SetScrollPos();
//			Debug("end track\n");
			return TRUE;
		//	break;
		}
		if (new_idx >= msgIds.UsedNumInt()) {
			new_idx = msgIds.UsedNumInt() - 1;
			scrOffPix = 0;
		}
		if (new_idx < 0) {
			new_idx = 0;
			scrOffPix = 0;
		}
		int findDiff = new_idx - curIdx;

//Debug("Scroll(msg=%d code=%d pos=%d) curIdx=%d offPix=%d scrOff=%d\n",
//	uMsg, nScrollCode, nPos, curIdx, offsetPix, scrOffPix);

		if (curIdx != new_idx) {
			//offsetPix = scrOffPix;
			SetNewCurIdx(new_idx);
		}
		SetScrollPos();

//		if (nScrollCode == SB_LINEDOWN || nScrollCode == SB_LINEUP) {
//			TRect	rc;
//			ScrollWindowEx(hWnd, 0, offsetPix - off_sv, &vrc, 0, 0, &rc, 0);
//			InvalidateRect(&rc, 0);
//		}
//		else {
//			InvalidateRect(0, 0);
//		}
	}
	return	TRUE;
}

BOOL TChildView::StartFindTimer(DWORD flags, DWORD tick)
{
	findTick = GetTick();
	findUpdateFlags = flags;
	SetTimer(FIND_TIMER, tick);
	return	TRUE;
}

BOOL TChildView::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	switch (timerID) {
	case FIND_TIMER:
		if (findTick && (GetTick() - findTick) > FIND_TICK) {
			KillTimer(FIND_TIMER);
			UpdateMsgList(findUpdateFlags);
		}
		break;

	case FINDNEXT_TIMER:
		if (scrHresReduce && (GetTick() - scrHresReduce) > FINDNEXT_TICK) {
			scrHresReduce = 0;
			KillTimer(FIND_TIMER);
			SetScrollPos();
		}
		break;

	case CAPTURE_TIMER:
		{
			TPoint	last_pt(lastMousePos);
			ClientToMemPt(&last_pt);

			if (!PtInRect(&vrc, last_pt)) {
				TPoint	cur_pt;
				::GetCursorPos(&cur_pt);
				::ScreenToClient(hWnd, &cur_pt);
				TPoints	cur_pos(cur_pt);

				if (cur_pos == lastMousePos) {
					EvMouseMoveCore(0, lastMousePos);
				}
			}
		}
		break;

	case UPDATE_TIMER:
		{
			SYSTEMTIME	t;
			::GetLocalTime(&t);
			if (t.wYear  != curTime.wYear  ||
				t.wMonth != curTime.wMonth ||
				t.wDay   != curTime.wDay) {
				curTime = t;	// PaintMemBmpに突入しない場合に再実行させない
				InvalidateRect(0, 0);
			}
		}
		break;

//	case SCROLL_TIMER:
//		EventScroll(WM_VSCROLL, SB_LINEDOWN, 0, 0);
//		break;
	}

	return	FALSE;
}

BOOL TChildView::ButtonDownItem(Item *item, POINTS pos)
{
	if (dispFlags & DISP_OVERLAPPED_ALL) {
		if (!item || (dispFlags & DISP_OVERLAPPED_EX) ||
			(item->kind & (Item::CLIP|Item::CLIPBODY|Item::FILE|Item::CMNT)) == 0) {
			dispFlags &= ~DISP_OVERLAPPED_ALL;
			InvalidateRect(0, 0);
			return	TRUE;
		}
	}
	BOOL	is_shift = (::GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;

#define SEL_NONINIT (Item::FROMTO|Item::CLIP|Item::FAV|Item::IMENU)
	if (IsSelected() && !is_shift && (!item || (item->kind & SEL_NONINIT) == 0)) {
		BOOL	sel_init = TRUE;

		if (item && selTop.msg_id == item->msg_id && selTop.msg_id == selEnd.msg_id &&
				(item->kind & Item::LINK)) {	// link が選択状態の場合は初期化しない
			BodyAttr	link;
			if (GetLinkAttr(item, &link)) {
				if (link.pos == selTop.pos && link.len == (selEnd.pos-selTop.pos)) {
					sel_init = FALSE;
				}
			}
		}
		if (sel_init) {
			AllSelInit();
			InvalidateRect(0, 0);
		}
	}
	SelItem		*target_sel = &selTop;

	if (is_shift) {
		if (selTop.IsValid()) {
			target_sel = &selEnd;
		}
	}

	if (!item) {
		return	FALSE;
	}
	SetUserSelected(item->msg_id);

	ViewMsg *msg = GetMsg(item->msg_id);

	if ((item->kind & (Item::UNOPEN|Item::UNOPENBODY)) ||
		(msg && msg->unOpenR &&
			(item->kind & (Item::HEADBOX|Item::FROMTO|Item::UNOPENFILE)))) {
		if (msg && msg->msg.host.size()) {
			MsgIdent	*mi = new MsgIdent;
			mi->uid		= msg->msg.host[0].uid;
			mi->host	= msg->msg.host[0].host;
			mi->packetNo = msg->msg.packet_no;
			mi->msgId	= msg->msg.msg_id;

			if (logMng->ReadCheckStatus(mi, TRUE,
				(dispFlags & DISP_UNOPENR_ONLY) ? TRUE : FALSE)) {
				::PostMessage(GetMainWnd(), WM_RECVDLG_BYVIEWER,
					/*((item->kind & Item::UNOPENFILE) ? RDLG_FILESAVE : 0) |*/
					RDLG_FREE_MI, (LPARAM)mi);

				if ((dispFlags & DISP_UNOPENR_ONLY) && logDb->GetFlagsNum(DB_FLAG_UNOPENR) == 0) {
					EvCommand(0, UNOPENR_CHK, 0);
				}
			}
			else {
				ClearMsgCache(msg->msg.msg_id);
				msg = NULL;
				delete mi;
			}
		}
	}
	else if (item->kind & Item::HEADBOX) {
		SetSelectedPos(item, pos, target_sel);
	}
	else if (item->kind & Item::DATE) {
		SetSelectedPos(item, pos, target_sel);
	}
	else if (item->kind & Item::FROMTO) {
	}
	else if (item->kind & Item::USER) {
		if (msg) {
			if (msg->dispGenNo == msg->dispGenNo) {
				offsetPix = msg->dispYOff;
			}
			SetNewCurIdxByMsgId(msg->msg.msg_id);
			ToggleFindUser(msg->msg.host[item->user_idx].uid);
		}
	}
	else if (item->kind & Item::CROPUSER) {
		CropUserMenu(item);
	}
	else if (item->kind & Item::FILE) {
		dispFlags |= DISP_OVERLAPPED_ALL;
		if (msg) {
			PaintFullFiles(msg, item->drawRc);
			LinkFileMenu(msg, item);
		}
	}
	else if (item->kind & Item::CLIP) {
		if (msg && PaintFullClip(item->msg_id, &msg->msg.clip[item->clip_idx], item->rc.y()) > 0) {
			dispFlags |= DISP_OVERLAPPED_ALL;
		}
		else {
			SetStatusMsg(0, LoadStrW(IDS_NOIMAGE));
		}
	}
	else if (item->kind & Item::CLIPBODY) {
		if (msg && PaintFullClip(item->msg_id, &msg->msg.clip[item->clip_idx], item->rc.y()) > 0) {
			dispFlags |= DISP_OVERLAPPED_ALL;
		}
		else {
			SetStatusMsg(0, LoadStrW(IDS_NOIMAGE));
		}
	}
	else if (item->kind & Item::CMNT) {
		EditComment(item->msg_id);
	}
	else if (item->kind & Item::CMNTBODY) {
		EditComment(item->msg_id);
	}
	else if (item->kind & Item::FOLDBTN) {
		if (msg) {
			msg->foldGenNo = IsFold(msg) ? foldGenNo : 0;
			if (IsFold(msg)) {
				int idx = GetMsgIdx(item->msg_id);
				if (idx == curIdx && offsetPix < 0 || idx < curIdx) {
					offsetPix = 0;
					curIdx = idx;
				}
			}
			InvalidateRect(0, 0);
		}
	}
	else if (item->kind & Item::FAV) {
		if (msg) {
			if (msg->msg.flags & DB_FLAG_FAV) {
				msg->msg.flags &= ~DB_FLAG_FAV;
			} else {
				msg->msg.flags |= DB_FLAG_FAV;
			}
			itemMgr.SaveProcItem(item);
			UpdateMsgFlags(msg);
			InvalidateRect(0, 0);	// 取り消し可能性のためUpdateListはしない
		}
	}
	else if (item->kind & Item::IMENU) {
		ItemMenu(item);
	}
	else if (item->kind & Item::BODY) {
		if (item->kind & Item::LINK) {
		}
		else {
			if (item->kind & Item::MARK) {
			}
			if (item->kind & Item::SEL) {
			}
			if (item->kind & Item::FIND) {
			}
			SetSelectedPos(item, pos, target_sel);
		}
	}
	else {
	}

	if (target_sel == &selEnd && IsSelected()) {
		InvalidateRect(0, 0);
	}

	return	TRUE;
}

BOOL TChildView::RButtonDownItem(Item *item, POINTS pos)
{
	if (!item) {
		return	FALSE;
	}

	SetUserSelected(item->msg_id);

	if (item->kind & Item::HEADBOX) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::DATE) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::FROMTO) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::USER) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::CROPUSER) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::FILE) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::CLIP) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::CLIPBODY) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::CMNT) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::FAV) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::IMENU) {
		ItemRMenu(item);
	}
	else if (item->kind & Item::BODY) {
		ItemRMenu(item);
	}
	else {
		ItemRMenu(item);
	}

	return	TRUE;
}

BOOL TChildView::ButtonUpItem(Item *item, POINTS pos)
{
	if (!item) {
		if (selTop.IsValid()) {
			SetSelectedPos(NULL, pos, &selEnd);
			InvalidateRect(0, 0);
		}
		return	TRUE;
	}
	ViewMsg *msg = GetMsg(item->msg_id);

	if (item->kind & (Item::HEADBOX | Item::BODYBLANK)) {
		if (dblClickStart && (GetTick() - dblClickStart) < 500) {
			if (ViewMsg *msg = GetMsg(item->msg_id)) {
				ReplyMsg(msg, -1);
			}
		}
		dblClickStart = 0;
	}
	else if (item->kind & Item::DATE) {
	}
	else if (item->kind & Item::FROMTO) {
		ReplyMsg(msg);
	}
	else if (item->kind & Item::USER) {
	}
	else if (item->kind & Item::CROPUSER) {
	}
	else if (item->kind & Item::CMNT) {
	}
	else if (item->kind & Item::FILE) {
	}
	else if (item->kind & Item::CLIP) {
	}
	else if (item->kind & Item::CLIPBODY) {
	}
	else if (item->kind & Item::FAV) {
	}
	else if (item->kind & Item::IMENU) {
	}
	else if (item->kind & Item::BODY) {
		if (!selTop.IsValid() && (item->kind & Item::LINK)) {
			LinkSelArea(item);
			InvalidateRect(0, 0);

			JumpLink(item, FALSE);
		}
	}
	else if (item->kind & Item::CMNTBODY) {
	}
	else if (item->kind & Item::FOLDBTN) {
	}
	else {
	}

	if (selTop.IsValid() && !selEnd.IsValid()) {
		SetSelectedPos(item, pos, &selEnd);
	}

	return	TRUE;
}

BOOL TChildView::ButtonDblClickItem(Item *item, POINTS pos)
{
	if (!item) {
		return	FALSE;
	}
	ViewMsg *msg = GetMsg(item->msg_id);
	if (!msg) {
		return	FALSE;
	}

	if (item->kind &
		(Item::HEADBOX | Item::BODYBLANK | Item::DATE | Item::USERMULTI | Item::USERMULOLD)) {
		if (msg->dispGenNo == msg->dispGenNo) {
			offsetPix = msg->dispYOff;
		}
		SetNewCurIdxByMsgId(item->msg_id);

		if ((dblClickStart = ::GetTick()) == 0) {
			// btnUp時にviewerがactiveになるため、その後にreply
			dblClickStart = 1;
		}
	}
	else if (item->kind & Item::FROMTO) {
		ReplyMsg(msg);
	}
	else if (item->kind & Item::USER) {
	}
	else if (item->kind & Item::CROPUSER) {
	}
	else if (item->kind & Item::FILE) {
		dispFlags &= ~DISP_OVERLAPPED_ALL;
		InvalidateRect(0, 0);
	}
	else if (item->kind & (Item::CLIP | Item::CLIPBODY)) {
		if (msg) {
			WCHAR	clip_path[MAX_PATH];
			MakeClipPathW(clip_path, cfg, msg->msg.clip[item->clip_idx].fname.s());
			ShellExecuteW(NULL, NULL, clip_path, 0, 0, SW_SHOW);
			InvalidateRect(0, 0);
		}
	}
	else if (item->kind & Item::CMNT) {
	//	EditComment(item->msg_id);
	}
	else if (item->kind & Item::FAV) {
	}
	else if (item->kind & Item::IMENU) {
	}
	else if (item->kind & Item::BODY) {
		if (item->kind & Item::LINK) {
			JumpLink(item, TRUE);
		}
		else {
			if (SetSelectedPos(item, pos, &selTop)) {
				if (ExpandSelArea()) {
					InvalidateRect(0, 0);
				}
			}
		}
	}
	else if (item->kind & Item::CMNTBODY) {
	}
	else if (item->kind & Item::FOLDBTN) {
	}

	return	TRUE;
}

BOOL TChildView::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	ClientToMemPos(&pos);

	Item *item = itemMgr.FindItem(pos.x, pos.y);

	MouseOver(item, pos);

	if (item && (dispFlags & DISP_OVERLAPPED) == 0) {
		if (ViewMsg *msg = GetMsg(item->msg_id)) {
			int	new_idx = GetMsgIdx(item->msg_id);
			if (new_idx != curIdx) {
				SetNewCurIdx(new_idx);
				offsetPix = msg->dispYOff;
				PaintFocus(TRUE);
			}
		}
	}

	switch (uMsg) {
	case WM_LBUTTONDOWN:
		SetFocus();
		ButtonDownItem(item, pos);
		break;

	case WM_RBUTTONDOWN:
		SetFocus();
		RButtonDownItem(item, pos);
		break;

	case WM_LBUTTONUP:
		ButtonUpItem(item, pos);
		break;

	case WM_LBUTTONDBLCLK:
		SetFocus();
		ButtonDblClickItem(item, pos);
		break;
	}

	return	FALSE;
}

BOOL TChildView::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_LOGVIEW_MOUSE_EMU:
		{
			TPoint	pt;
			::GetCursorPos(&pt);
			::ScreenToClient(hWnd, &pt);

			if (::PtInRect(&vrc, pt) && msgIds.UsedNumInt() > 0) {
				TPoints	pos(pt);
				EvMouseMoveCore(0, pos);
			}
		}
		return	TRUE;
	}

	return	FALSE;
}


BOOL TChildView::EvMouseMove(UINT fwKeys, POINTS pos)
{
	lastMousePos = pos;

	return	EvMouseMoveCore(fwKeys, pos);
}


BOOL TChildView::LinkLabel(Item *item, WCHAR *label)
{
	Wstr	w;

	if (!GetLinkStr(item, &w)) {
		return	FALSE;
	}

	LinkKind	lk = GetLinkKind(cfg, w.s());
	LinkActKind	ak = GetLinkActKind(cfg, lk, TRUE);
	UINT		id = IDS_LINK;

	if (ak == LAK_NONE) {
		return	FALSE;
	}

	if (lk != LK_URL) {
		BOOL	is_dir;
		if (CheckExtW(cfg, w.s(), &is_dir)) {
			id = is_dir ? IDS_FOLDERLINK : IDS_FILELINK;
		}
		else {
			id = IDS_FILELINK_CONFIRM;
		}
	}
	swprintf(label, L"%s%s", LoadStrW(id), ak == LAK_CLICK ? L"": LoadStrW(IDS_DBLCLICK));

	return	TRUE;
}

BOOL TChildView::EvMouseMoveCore(UINT fwKeys, POINTS pos)
{
	WCHAR			mbuf1[MAX_BUF];
	static WCHAR	_mbuf2[MAX_BUF];
	WCHAR			*mbuf2 = NULL;

	*mbuf1 = 0;

	ClientToMemPos(&pos);
	TPoint	pt(pos.x, pos.y);

	if (!::PtInRect(&vrc, pt) && msgIds.UsedNumInt() > 0) {
		if (pt.y <= 0) {
			if (mouseScrMode == MOUSE_SCR_DONE) {
				mouseScrMode = MOUSE_SCR_INIT;
			}
			else {
				mouseScrMode = MOUSE_SCR_REQ;
				scrOffPix = int(headCy * ((-pt.y / headCy) + 1));
				SetScrollPos();
			}
			pos.x = CVW_GAPX + BODY_GAP + 1;
			pos.y = 1;
		}
		else if (pt.y >= vrc.bottom) {
			if (mouseScrMode == MOUSE_SCR_DONE) {
				mouseScrMode = MOUSE_SCR_INIT;
			}
			else {
				mouseScrMode = MOUSE_SCR_REQ;
				scrOffPix = -int(headCy * (((pt.y - vrc.bottom) / headCy) + 1));
				SetScrollPos();
			}
			pos.x = SHORT(vrc.right - CVW_GAPX - 1);
			pos.y = SHORT(vrc.bottom - 1);
		}
		else if (pt.x >= vrc.right) {
			pos.x = SHORT(vrc.right - CVW_GAPX - 1);
		}
		else {
			pos.x = CVW_GAPX + BODY_GAP + 1;
		}
	}

	Item	*item = itemMgr.FindItem(pos.x, pos.y);
//	Debug("%d %d item=%x\n", pos.x, pos.y, item ? item->kind : 0);

	if (item) {
		if (ViewMsg	*msg = GetMsg(item->msg_id)) {

			if (item->kind & Item::DATE) {
				int len = MakeDateStr(msg->msg.date, mbuf1, MDS_WITH_SEC|MDS_WITH_DAYWEEK);
				if (msg->msg.alter_date) {
					len += wcscpyz(mbuf1 + len, L"  modify (");
					len += MakeDateStr(msg->msg.alter_date, mbuf1 + len, MDS_WITH_SEC);
					len += wcscpyz(mbuf1 + len, L") ");
				}
				mbuf2 = _mbuf2;
				if (msg->msg.flags & DB_FLAG_SIGNED2) {
					wcscpyz(mbuf2, LoadStrW(IDS_ENCRYPT2_SIGNED2));
				}
				else if (msg->msg.flags & DB_FLAG_SIGNED) {
					wcscpyz(mbuf2, LoadStrW(IDS_ENCRYPT2_SIGNED));
				}
				else if (msg->msg.flags & DB_FLAG_SIGNERR) {
					wcscpyz(mbuf2, LoadStrW(IDS_ENCRYPT2_ERROR));
				}
				else if (msg->msg.flags & DB_FLAG_UNAUTH) {
					wcscpyz(mbuf2, LoadStrW(IDS_UNAUTHORIZED_TITLE));
				}
				else if (msg->msg.flags & DB_FLAG_RSA2) {
					wcscpyz(mbuf2, LoadStrW(IDS_ENCRYPT2));
				}
				else if (msg->msg.flags & DB_FLAG_RSA) {
					wcscpyz(mbuf2, LoadStrW(IDS_ENCRYPT));
				}
				else {
					wcscpyz(mbuf2, LoadStrW(IDS_UNCRYPTED));
				}
			}
			else if (item->kind & Item::FROMTO) {
				UINT	id = msg->msg.IsRecv() ? IDS_REPLYR : IDS_REPLYS;
				if (msg->unOpenR) {
					id = IDS_OPENREPLY;
				}
				wcscpy(mbuf1, LoadStrW(id));
			}
			else if ((item->kind & Item::UNOPEN) || (item->kind & Item::UNOPENBODY)
				|| (item->kind & Item::UNOPENFILE)) {
				wcscpy(mbuf1, msg->msg.IsRecv() ?
					LoadStrW(IDS_OPENRECVMSG) : LoadStrW(IDS_UNOPENMSG));
			}
			else if (item->kind & Item::USER) {
				WCHAR	*s = mbuf1;
				LogHost	&host = msg->msg.host[item->user_idx];

				if (auto t = msgid_to_time(host.flags)) {
					WCHAR	wdate[128];
					MakeDateStrEx(t, wdate, msg->msg.date);
					s += snwprintfz(s, wsizeof(mbuf1), LoadStrW(IDS_OPENED_FMT), wdate);
					s += wcscpyz(s, L"  ");
				}
				host_to_str(&host, s);

	//		Debug("msg_id=%d kind=%d/%d/%d rc=%d/%d/%d/%d\n",
	//			item->msg_id, item->kind, item->kind_no, item->body_len,
	//			item->rc.x(), item->rc.y(), item->rc.cx(), item->rc.cy());
			}
			else if (item->kind & Item::USERMULTI) {
				wcscpy(mbuf1, LoadStrW(IDS_MULTIUSER));
			}
			else if (item->kind & Item::USERMULOLD) {
				wcscpy(mbuf1, LoadStrW(IDS_MULTIOLD));
			}
			else if (item->kind & Item::FAV) {
				wcscpy(mbuf1, LoadStrW(IDS_FAV));
			}
			else if (item->kind & Item::FILE) {
				DownloadLinkStat	dls;
				CheckDownloadLink(&msg->msg, 0, FALSE, &dls);
				if (*dls.wlink) {
					wcsncpyz(mbuf1, FmtW(L"%s: %s -> %s",
						LoadStrW(IDS_FILE), dls.wlink, dls.wtarg), wsizeof(mbuf1));
				} else {
					wcsncpyz(mbuf1, FmtW(L"%s: %s",
						LoadStrW(IDS_FILE), dls.wtarg), wsizeof(mbuf1));
				}
			}
			else if (item->kind & Item::IMENU) {
				wcscpy(mbuf1, LoadStrW(IDS_IMENU));
			}
			else if (item->kind & Item::LINK) {
				LinkLabel(item, mbuf1);
			}
			else if (item->kind & (Item::CLIP | Item::CLIPBODY)) {
				wcscpy(mbuf1, LoadStrW(IDS_THUMBNAIL));
			}
			else if (item->kind & (Item::CMNT | Item::CMNTBODY)) {
				wcscpy(mbuf1, LoadStrW(IDS_EDITCOMMENT));
			}
			else if (item->kind & Item::MARK) {
				wcscpy(mbuf1, LoadStrW(IDS_MARKER));
			}
			else if (item->kind & Item::SEL) {
				wcscpy(mbuf1, LoadStrW(IDS_SEL));
			}
			else if (item->kind & (Item::HEADBOX | Item::BODYBLANK)) {
				UINT id = msg->unOpenR ? IDS_OPENRECVMSG :
					(userMsgId == item->msg_id) ? IDS_HEADREPLY : IDS_HEADREPLYSEL;

				wcscpy(mbuf1, LoadStrW(id));
			}
			else if (item->kind & Item::BODY) {
			//	wcscpy(mbuf1, LoadStrW(IDS_BODYSEL));
			}
			else if (item->kind & Item::FOLDBTN) {
				UINT id = IsFold(msg) ? IDS_FOLD_TO : IDS_FOLD_FROM;
				wcscpy(mbuf1, LoadStrW(id));
			}
		}
	}
	SetStatusMsg(0, mbuf1);
	if (mbuf2) {
		SetStatusMsg(1, mbuf2);
	}
	else if (*_mbuf2) { // 前回設定がある場合は消去
		SetStatusMsg(1);
		*_mbuf2 = 0;
	}
	MouseOver(item, pos);

	return	FALSE;
}

BOOL TChildView::EvNcHitTest(POINTS _ps, LRESULT *result)
{
	POINT	pos = { (LONG)_ps.x, (LONG)_ps.y };
	::ScreenToClient(hWnd, &pos);

	POINTS	ps = { (SHORT)pos.x, (SHORT)pos.y };
	::ClientToMemPos(&ps);

	Item *item = itemMgr.FindItem(ps.x, ps.y);

	MouseOver(item, ps);
	if (item) {
		*result = HTCLIENT;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TChildView::CropUserMenu(Item *item)
{
	HMENU	hMenu = ::CreatePopupMenu();
	ViewMsg	*msg = GetMsg(item->msg_id);

	if (!msg) {
		return FALSE;
	}

	POINT	pt = { item->rc.x(), item->rc.y() };
	::ClientToScreen(hWnd, &pt);

	for (int i=item->user_idx; i < msg->msg.host.size(); i++) {
		::AppendMenuW(hMenu, MF_STRING|(msg->msg.host[i].uid == findUser ? MF_CHECKED : 0),
			WM_CROPMENU + i, msg->msg.host[i].nick.s());
	}
	cropUserItem = item;
	::SetForegroundWindow(hWnd);

	BOOL ret = ::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x+10, pt.y+10, 0, hWnd, NULL);
	::DestroyMenu(hMenu);
	if (!ret) cropUserItem = NULL;
	return	ret;
}

void TChildView::CropUserItem(int idx)
{
	ViewMsg	*msg = GetMsg(cropUserItem->msg_id);
	cropUserItem = NULL;

	if (msg && idx >= 0 && idx < msg->msg.host.size()) {
		ToggleFindUser(msg->msg.host[idx].uid);
	}
}

BOOL TChildView::CheckDownloadLink(LogMsg *msg, int idx, BOOL is_update, DownloadLinkStat *dls)
{
	WCHAR	wpath[MAX_PATH];
	time_t	t = msgid_to_time(msg->msg_id);

	dls->Init();

	MakeDownloadLinkW(cfg, msg->files[idx].s(), t, msg->IsRecv(), wpath);

	dls->link_ok = (::GetFileAttributesW(wpath) != 0xffffffff);
	if (!dls->link_ok) {
		return	FALSE;
	}

	dls->targ_ok = ConfirmDownloadLinkW(cfg, wpath, is_update, dls->wtarg, dls->wlink);
	if (dls->targ_ok) {
		DWORD	attr = ::GetFileAttributesW(dls->wtarg);

		dls->is_dir = (attr != 0xffffffff && (attr & FILE_ATTRIBUTE_DIRECTORY));
		dls->is_direct = CheckExtW(cfg, dls->wtarg);
	}
	return	TRUE;
}

BOOL TChildView::LinkFileMenu(ViewMsg *msg, Item *item)
{
	HMENU	hMenu = ::CreatePopupMenu();

	POINT	pt = { item->rc.x(), item->rc.y() };
	::ClientToScreen(hWnd, &pt);

	BOOL	direct_open = FALSE;
	int		fnum = (int)min(msg->msg.files.size(), WM_FILEMENU_MAX - WM_FILEMENU);

	for (int i=0; i < fnum; i++) {
		DownloadLinkStat	dls;
		CheckDownloadLink(&msg->msg, i, TRUE, &dls);

		WCHAR	wbuf[MAX_PATH + 100];
		WCHAR	*s = dls.targ_ok ? L"" : LoadStrW(dls.link_ok ? IDS_INVALIDLINK : IDS_INVALID);

		snwprintfz(wbuf, wsizeof(wbuf), L"%s%s", s, msg->msg.files[i].s());
		::AppendMenuW(hMenu, MF_STRING|(dls.link_ok ? 0 : MF_GRAYED), WM_FILEMENU + i, wbuf);

		if (dls.targ_ok && (dls.is_dir || dls.is_direct)) {
			direct_open = TRUE;
		}
	}

	fileItem = item;
	::SetForegroundWindow(hWnd);

	SetStatusMsg(0, LoadStrW(direct_open ? IDS_CTRLFOLDER : IDS_OPENPARENT), 500);

	BOOL ret = ::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x+10, pt.y+10, 0,
		hWnd, NULL);
	::DestroyMenu(hMenu);
	if (!ret) fileItem = NULL;
	return	ret;
}

void TChildView::FileMenuItem(int idx)
{
	ViewMsg	*msg = GetMsg(fileItem->msg_id);
	fileItem = NULL;

	if (!msg || idx >= msg->msg.files.size()) {
		return;
	}

	DownloadLinkStat	dls;
	CheckDownloadLink(&msg->msg, idx, TRUE, &dls);

	SetStatusMsg(0, FmtW(L"%s%s", dls.targ_ok ? L"" : LoadStrW(IDS_INVALIDLINK), dls.wtarg), 2000);

	if (!dls.link_ok) {
		return;
	}

	BOOL	is_ctrl = (::GetKeyState(VK_CONTROL) & 0x8000);

	if (!is_ctrl && (dls.is_dir || dls.is_direct)) {	// 直接実行
		ShellExecuteW(NULL, NULL, dls.wtarg, 0, 0, SW_SHOW);
	}
	else {
		// 直接オープンを認めていない拡張子の場合、親フォルダを開く
		WCHAR	wpath[MAX_PATH];
		WCHAR	*fname = NULL;

		if (GetFullPathNameW(*dls.wlink ? dls.wlink : dls.wtarg, MAX_PATH, wpath, &fname)) {
			if (fname) {
				fname[-1] = 0;
				TOpenExplorerSelW(wpath, &fname, 1);
			}
			else if (dls.is_dir) {	// root は root自体を開く
				ShellExecuteW(NULL, NULL, dls.wtarg, 0, 0, SW_SHOW);
			}
		}
	}
}

BOOL AppendItemSubMenu(HWND hWnd, HMENU hMenu, BOOL is_fimg=FALSE)
{
	BOOL is_img  = IsImageInClipboard(hWnd);
	BOOL is_text = IsTextInClipboard(hWnd);


	if (is_img || is_text || is_fimg) {
		::AppendMenuW(hMenu, MF_SEPARATOR, 0, 0);
	}
	if (is_fimg) {
		::AppendMenuW(hMenu, MF_STRING, MENU_INSERTIMAGE, LoadStrW(IDS_INSERT_IMAGE));
	}

	if (is_img) {
		::AppendMenuW(hMenu, MF_STRING, MENU_PASTEIMAGE, LoadStrW(IDS_PASTEIMAGE));
	}
	if (is_text) {
		::AppendMenuW(hMenu, MF_STRING, MENU_PASTECMNT, LoadStrW(IDS_PASTECMNT));
	}
	return	TRUE;
}

BOOL TChildView::ItemMenu(Item *item)
{
	HMENU		hMenu = ::LoadMenu(TApp::hInst(), (LPCSTR)ITEM_MENU);
	HMENU		hSubMenu = ::GetSubMenu(hMenu, 0);	//かならず、最初の項目に定義
	ViewMsg		*msg = GetMsg(item->msg_id);

	if (!msg) {
		return FALSE;
	}

	POINT	pt = { item->rc.x(), item->rc.y() };
	::ClientToScreen(hWnd, &pt);

	if (msg->msg.comment) {
		::ModifyMenuW(hMenu, MENU_COMMENT, MF_BYCOMMAND|MF_STRING, MENU_COMMENT,
			LoadStrW(IDS_COMMENT));
	}

	for (int i=item->user_idx; i < msg->msg.host.size(); i++) {
		::AppendMenuW(hMenu, MF_STRING|(msg->msg.host[i].uid == findUser ? MF_CHECKED : 0),
			WM_CROPMENU + i, msg->msg.host[i].nick.s());
	}
	itemMenu = item;
	::SetForegroundWindow(hWnd);

	if (item->kind & Item::IMENU) {
		AppendItemSubMenu(hWnd, hSubMenu, TRUE);
	}

	BOOL ret = ::TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON,
		pt.x+10, pt.y+10, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	if (!ret) {
		itemMenu = NULL;
	}
	return	ret;
}

BOOL TChildView::ItemRMenu(Item *item)
{
	ViewMsg	*msg = GetMsg(item->msg_id);
	if (!msg) {
		return FALSE;
	}

	BOOL	is_cmnt = item->kind & (Item::CMNT | Item::CMNTBODY);
	BOOL	is_user = item->kind & (Item::USER);
	BOOL	is_clip = item->kind & (Item::CLIP|Item::CLIPBODY);
	BOOL	is_link = item->kind & (Item::LINK);
	BOOL	is_sel  = IsSelected(TRUE) && !is_cmnt;
	BOOL	is_mark = (item->kind & Item::BODY) && PosInMark(item->msg_id, item->body_pos);
	HMENU	hMenu = ::LoadMenu(TApp::hInst(), (LPCSTR)(DWORD_PTR)
				(is_user ? ITEMRUSER_MENU :
				 is_cmnt ? ITEMRCMNT_MENU :
				 is_clip ? ITEMRCLIP_MENU :
				 is_sel  ? ITEMRSEL_MENU  : ITEMR_MENU));
	HMENU	hSubMenu = ::GetSubMenu(hMenu, 0);	//かならず、最初の項目に定義

	if (is_cmnt) {
		// 専用メニュー
	}
	else if (is_user) {
		// 専用メニュー
	}
	else if (is_clip) {
		// 専用メニュー
	}
	else if (is_mark) {
		// そのまま
	}
	else if (is_sel) {
		// 専用メニュー
		Wstr	wbuf;
		if (!GetOneSelStr(&wbuf, TRUE)) {
			::EnableMenuItem(hSubMenu, MENU_FIND, MF_BYCOMMAND|MF_GRAYED);
			::EnableMenuItem(hSubMenu, MENU_SEARCHENG, MF_BYCOMMAND|MF_GRAYED);

			if ((::GetKeyState(VK_SHIFT) & 0x8000)) {
				::AppendMenuW(hSubMenu, MF_STRING, MENU_DELALLMSG, LoadStrW(IDS_DELALLMSG));
			}
		}
	}
	else {
		::DeleteMenu(hSubMenu, 1, MF_BYPOSITION); // separator
		::DeleteMenu(hSubMenu, MENU_MARKOFF, MF_BYCOMMAND);
	}

	if (is_link) {
		::InsertMenuW(hSubMenu, 0, MF_STRING|MF_BYPOSITION, MENU_COPYLINK, LoadStrW(IDS_COPYLINK));
		::InsertMenuW(hSubMenu, 1, MF_SEPARATOR|MF_BYPOSITION, 0, 0);
	}

	if (!is_sel && !is_cmnt && !is_user && !is_clip) {
		AppendItemSubMenu(hWnd, hSubMenu);
	}

	POINT	pt;
	::GetCursorPos(&pt);

	itemMenu = item;
	::SetForegroundWindow(hWnd);

	BOOL ret = ::TrackPopupMenu(hSubMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON,
		pt.x+10, pt.y+10, 0, hWnd, NULL);
	::DestroyMenu(hMenu);

	if (!ret) {
		itemMenu = NULL;
	}
	return	ret;
}

BOOL TChildView::MouseOver(Item *item, POINTS pos)
{
	Item	*last_item = itemMgr.GetLastProcItem();

//	if (lastReplyBtnId && (!item || (item->kind & Item::BODY) || lastReplyBtnId != item->msg_id)) {
//		if (Item *reply = itemMgr.FindItemByKind(lastReplyBtnId, Item::FROMTO)) {
//			PaintBmpIcon(reply->is_from ? hFromMsgBmp : hToMsgBmp, reply->drawRc, TRUE);
//		}
//		lastReplyBtnId = 0;
//	}

	if (last_item != item && last_item) {
		if (ViewMsg *msg = GetMsg(last_item->msg_id)) {
			if (last_item->kind & Item::FROMTO) {
				PaintBmpIcon(msg->msg.IsRecv() ? hFromMsgBmp : hToMsgBmp, last_item->drawRc, TRUE);
			}
			else if (last_item->kind & Item::UNOPEN) {
				if (msg->msg.flags & DB_FLAG_FROM) {
					TRect	irc = last_item->drawRc;
					irc.Inflate(1, 1);
					PaintPushBox(irc, CLEAR_BOX, 0, TRUE);
				}
			}
			else if (last_item->kind & Item::UNOPENBODY) {
				InvalidateRect(0, 0);
			}
			else if (last_item->kind & Item::UNOPENFILE) {
				InvalidateRect(0, 0);
			}
			else if (last_item->kind & (Item::CLIP|Item::CLIPBODY)) {
				if (dispFlags & DISP_OVERLAPPED_EX) {
					return TRUE;
				}
				else if (dispFlags & DISP_OVERLAPPED) {
					dispFlags &= ~DISP_OVERLAPPED_ALL;
					InvalidateRect(0, 0);
				}
			}
			else if (last_item->kind & Item::FILE) {
				if (dispFlags & DISP_OVERLAPPED_ALL) {
					dispFlags &= ~DISP_OVERLAPPED_ALL;
					InvalidateRect(0, 0);
				}
			}
			else if (last_item->kind & Item::HEADBOX) {
				InvalidateRect(0, 0);
			}
			else if (last_item->kind & Item::CMNT) {
				if (dispFlags & DISP_OVERLAPPED) {
					dispFlags &= ~DISP_OVERLAPPED;
					InvalidateRect(0, 0);
				}
			}
			else if (last_item->kind & Item::FAV) {
				PaintBmpIcon((msg->msg.flags & DB_FLAG_FAV) ? hFavBmp : hFavRevBmp,
					last_item->drawRc, TRUE);
			}
			else if (last_item->kind & Item::IMENU) {
				PaintBmpIcon(hImenuMidBmp, last_item->drawRc, TRUE);
			}
		}
	}


	if (item) {
		ViewMsg *msg = GetMsg(item->msg_id);

//		if (msg && (item->kind & Item::BODY) == 0 && lastReplyBtnId != item->msg_id) {
//			if (Item *reply = itemMgr.FindItemByKind(item->msg_id, Item::FROMTO)) {
//				PaintBmpIcon(hReplyMsgBmp, reply->drawRc, TRUE);
//				lastReplyBtnId = item->msg_id;
//			}
//		}

		if (item->kind & Item::FROMTO) {
			if (item != last_item) {
				if (msg) {
					PaintBmpIcon(hReplyMsgBmp, item->drawRc, TRUE);
				}
			}
			::SetCursor(hHandCur);
		}
		else if (item->kind & (Item::USER | Item::CROPUSER)) {
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::UNOPEN) {
			if (msg) {
				if (msg->msg.IsRecv()) {
					TRect	irc = item->drawRc;
					irc.Inflate(1, 1);
					PaintPushBox(irc, NORMAL_BOX, 0, TRUE);
					::SetCursor(hHandCur);
				}
			}
		}
		else if (item->kind & Item::UNOPENBODY) {
			if (msg) {
				PaintUnOpenPict(item->drawRc, TRUE);
				::SetCursor(hHandCur);
			}
		}
		else if (item->kind & Item::UNOPENFILE) {
			if (msg) {
				if (Item *targ = itemMgr.FindItemByKind(item->msg_id, Item::UNOPENBODY, item)) {
					PaintUnOpenPict(targ->drawRc, TRUE);
				}
				::SetCursor(hHandCur);
			}
		}
		else if (item->kind & Item::FILE) {
//			if (item != last_item) {
//				if (msg) {
//					PaintFullFiles(msg, item->drawRc);
//				}
//			}
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::CLIP) {
			if (item != last_item) {
				if (msg) {
					dispFlags |= DISP_OVERLAPPED;
					int		width = vrc.cx() - BODY_GAP;
					PaintThumb(msg, pos.y, &width, TRUE);
				}
			}
			::SetCursor(hLoupeCur);
		}
		else if (item->kind & Item::CLIPBODY) {
			::SetCursor(hLoupeCur);
		}
		else if (item->kind & Item::CMNT) {
			if (dispFlags & DISP_TITLE_ONLY) {
				if (item != last_item) {
					dispFlags |= DISP_OVERLAPPED;
					PaintComment(item->msg_id, item->rc);
				}
			}
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::FAV) {
			if (item != last_item) {
				if (msg) {
					PaintBmpIcon(hFavMidBmp, item->drawRc, TRUE);
				}
			}
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::IMENU) {
			if (item != last_item) {
				if (msg) {
					PaintBmpIcon(hImenuBmp, item->drawRc, TRUE);
				}
			}
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::HEADBOX) {
			if (Item *targ = itemMgr.FindItemByKind(item->msg_id, Item::UNOPENBODY, item)) {
				PaintUnOpenPict(targ->drawRc, TRUE);
				::SetCursor(hHandCur);
			}
			else if (msg && msg->unOpenR) {
				::SetCursor(hHandCur);
			}
			else {
				::SetCursor(hArrowCur);
			}
		}
		else if (item->kind & Item::BODY) {
			if (item->kind & Item::LINK) {
				::SetCursor(hHandCur);
			}
			else if (item->kind & Item::BODYBLANK) {
				::SetCursor(hArrowCur);
			}
			else {
				::SetCursor(hIBeamCur);
			}
		}
		else if (item->kind & Item::CMNTBODY) {
			::SetCursor(hHandCur);
		}
		else if (item->kind & Item::FOLDBTN) {
			::SetCursor(hHandCur);
		}
		else {
			::SetCursor(hArrowCur);
		}
	}
	else {
		::SetCursor(hArrowCur);
	}

	if (selTop.IsValid() && !selEnd.IsValid() && item) {
		SetSelectedPos(item, pos, &selMid);
		InvalidateRect(0, 0);
	}

	itemMgr.SetLastProcItem(item);

	return	TRUE;
}

