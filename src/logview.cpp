static char *logview_id = 
	"@(#)Copyright (C) H.Shirouzu 2015   mainwin.cpp	Ver3.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer
	Create					: 2015-04-10(Sat)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#if 0
#include "ipmsg.h"
#include <gdiplus.h>
using namespace Gdiplus;

static HBITMAP hDummyBmp;

TLogView::TLogView(Cfg *_cfg) : cfg(_cfg), childView(cfg, this), TDlg(LOGVIEW_DIALOG, this)
{
	hToolBar = NULL;
	hAccel	= ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)LOGVIEW_ACCEL);
	hDummyBmp = ::LoadBitmap(TApp::GetInstance(), (LPCSTR)DUMMYPIC_BITMAP);
}

TLogView::~TLogView()
{
}

BOOL TLogView::Create()
{
	return	TDlg::Create();
}

BOOL TLogView::EvCreate(LPARAM lParam)
{
	if (rect.left == CW_USEDEFAULT) {
		MoveWindow(200, 10, 500, 400, FALSE);
	}
	TBBUTTON tb_setting = { 0, LOGVIEW_SETTINGS, TBSTATE_ENABLED, TBSTYLE_BUTTON };
	hToolBar = ::CreateToolbarEx(hWnd, WS_CHILD|WS_VISIBLE|TBSTYLE_TOOLTIPS, LOGVIEW_TOOLBAR,
								1, TApp::GetInstance(), LOGVIEWTB_BITMAP, &tb_setting,
								1, 0, 0, 16, 16, sizeof(TBBUTTON));

//	// 赤・緑・青・黄色のビットマップを追加 (iBitmap 1-16)
//	for (int i=0; i < 4; i++) {
//		TBADDBITMAP tab = { TApp::GetInstance(), MARKERRED_BITMAP + i };
//		::SendMessage(hToolBar, TB_ADDBITMAP, 4, (LPARAM)&tab);
//	}
//	TBBUTTON tb_sep = {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0, 0};
//	::SendMessage(hToolBar, TB_INSERTBUTTON, 3, (LPARAM)&tb_sep);

	childView.CreateU8(hToolBar);

	userBtn.AttachWnd(::CreateWindow("BUTTON", "Not implement", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
								30,0,100,25,
								hToolBar, (HMENU)LOGVIEW_USERBTN, TApp::GetInstance(), 0));
	userBtn.SendMessage(WM_SETFONT, (WPARAM)childView.GetFont(), MAKELPARAM(TRUE, 0));

	SetDlgIcon(hWnd);
	Show();
	return	TRUE;
}

BOOL TLogView::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case LOGVIEW_SETTINGS:
		MessageBox("Not Implemented", "ipmsg", MB_OK);
		return	TRUE;

	case LOGVIEW_USERBTN:
		childView.EvCommand(wNotifyCode, wID, hwndCtl);
		return	TRUE;

	case LOGVIEW_COPY:
		childView.EvCommand(wNotifyCode, wID, hwndCtl);
		return	TRUE;
	}
	return FALSE;
}

BOOL TLogView::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
//	if (uCmdType == SC_CLOSE) ::PostMessage(GetMainWnd(), WM_SYSCOMMAND, MENU_EXIT, 0);

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
	return	FALSE;
}

BOOL TLogView::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
//	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED)
	::SendMessage(hToolBar, TB_AUTOSIZE, sizeof(TBBUTTON), 0);
	childView.FitSize();
	InvalidateRect(NULL, TRUE);

	return	FALSE;;
}

BOOL TLogView::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	return	TRUE;
}

/*
   ==========================  ChildView  =================================
*/
TChildView::TChildView(Cfg *_cfg, TLogView *_parent) :
	TWin(_parent), cfg(_cfg), parentView(_parent)
{
	viewPos		= 0;
	viewLines	= 0;
	rectLines	= 0;
	hFont		= NULL;
	scrTimerID	= 0;
	lines		= &fullLines;
}

TChildView::~TChildView()
{
}

BOOL TChildView::LoadLog(const char *logname)
{
//	FILE	*fp = fopen(U8toA(logname), "r");
//	char	buf[1024 * 64];
//	while (fgets(buf, sizeof(buf), fp)) {
//		fullLines.push_back(new LineInfo(U8toWs((char *)buf)));
//		if (fullLines.size() > 1000) break;
//	}
//	fclose(fp);

	DWORD	tick = GetTickCount();
	BOOL	ret = FALSE;
	HANDLE	hFile = CreateFileU8(cfg->LogFile, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
									NULL, OPEN_EXISTING, 0, 0);
	DWORD	size = ::GetFileSize(hFile, 0); // non support 4GB over
	HANDLE	hMap = ::CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, NULL);
	char	*top = (char *)::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	char	*end = top + size;

	if (top) {
		char	*s=top, *e;

		while (s < end && (e = (char *)memchr(s, '\n', end-s))) {
			int	len = int(e - s);
			if (len > 0 && e[-1] == '\r') len--;
			LineInfo	*li = new LineInfo(U8toW(s, len));
			fullLines.push_back(li);
			s = e + 1;
//			if (fullLines.size() > 1000) break;
		}
	}
	fullLines.push_back(new LineInfo(U8toW("", 0)));

	if (top) ::UnmapViewOfFile(top);
	if (hMap) ::CloseHandle(hMap);
	if (hFile != INVALID_HANDLE_VALUE) ::CloseHandle(hFile);

	Debug("tick=%d\n", GetTickCount() - tick);

	ParseLog();

	for (int i=0; i < msgs.size(); i++) {
		LogMsg	*m = msgs[i];
		//if (i < 1000) {
			for (int j=0; j < m->num; j++) {
				fullLines[m->no+j]->msgNo = i;
				curLines.push_back(fullLines[m->no+j]);
			}
		//}
	}
	curLines.push_back(new LineInfo(U8toW("", 0)));
	lines = &curLines;

	return	ret;
}

BOOL TChildView::UnloadLog()
{
	return TRUE;
}

BOOL TChildView::ParseSender(LineInfo *l, LogMsg *lm)
{
	LogUser	user;

	if (l->s[1] == 'T') {		// To:
		lm->isTo = true;
	}
	else if (l->s[1] == 'F') {	// From:
		lm->isTo = false;
	}
	else return	FALSE;

	if (l->s[l->len-1] != ')') return false;

	int		pl_num = 1;
	WCHAR	*p     = l->s + l->len -2;
	WCHAR	*end   = l->s;

	for ( ; p != end && pl_num > 0; p--) {
		if      (*p == ')') pl_num++;
		else if (*p == '(') pl_num--;
	}
	if (p == end) return false;

	user.uname.s   = l->s + (lm->isTo ? 5 : 7);
	user.uname.len = int(p - user.uname.s - 1);

	lm->users.push_back(user);
	return	true;
}

BOOL TChildView::ParseDate(LineInfo *l, LogMsg *lm)
{
	static WCHAR *mon_dict[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec", 0 };

	if (memcmp(l->s, L"  at ", sizeof(WCHAR)*5)) return FALSE;

	struct tm tm= {};
	tm.tm_mon = -1;

	for (int i=0; mon_dict[i]; i++) {
		if (!memcmp(l->s + 9, mon_dict[i], 3 * sizeof(WCHAR))) {
			tm.tm_mon = i;
			break;
		}
	}
	if (tm.tm_mon < 0) return FALSE;

//at Mon Jul 02 12:50:56 2012
	tm.tm_mday	= _wtoi(l->s+13);
	tm.tm_hour	= _wtoi(l->s+16);
	tm.tm_min	= _wtoi(l->s+19);
	tm.tm_sec	= _wtoi(l->s+22);
	tm.tm_year	= _wtoi(l->s+25) - 1900;
	tm.tm_isdst	= -1;

	if ((lm->date = mktime(&tm)) == -1) return	FALSE;

	return	TRUE;
}

BOOL TChildView::ParseAttach(LineInfo *l, LogMsg *lm)
{
	static WCHAR image[]   = L"(画像)";
	static WCHAR image_e[] = L"(image)";
	static WCHAR both[]    = L"(添付/画像)";
	static WCHAR both_e[]  = L"(files/images)";

	LogClip	clip;

	if (memcmp(l->s, L"  (", sizeof(WCHAR)*3)) return FALSE;

	if (memcmp(l->s+2, image,   sizeof(image)  -sizeof(WCHAR)) &&
		memcmp(l->s+2, image_e, sizeof(image_e)-sizeof(WCHAR)) &&
		memcmp(l->s+2, both,    sizeof(both)   -sizeof(WCHAR)) &&
		memcmp(l->s+2, both_e,  sizeof(both_e) -sizeof(WCHAR))) return TRUE;

	for (WCHAR *p=wcsstr(l->s+7, L"ipmsgclip_"); p; p=wcsstr(p+1, L"ipmsgclip_")) {
		WCHAR *e=wcsstr(p+17, L".png");
		if (e) {
			clip.fname.s   = p;
			clip.fname.len = int(e-p+4);
			for (int i=0; i < 6; i++) {
				if (e[-i] >= '0' && e[-i] <= '9') continue;
				if (e[-i] == '_') {
					clip.pos = _wtoi(e - i);
				}
				else break;
			}
			lm->clips.push_back(clip);
			l->flags |= LOG_CLIP_FLAG;
//			DebugW(L"image=%s\n", l->s);
		}
	}
	return	TRUE;
}

#define LOG_HEAD1 L"====================================="
#define LOG_HEAD2 L"-------------------------------------"

BOOL TChildView::ParseMsg(int idx, LogMsg *lm)
{
	bool	is_attach=false;
	int		i=0;

	if ((*lines)[idx]->len != 37 || memcmp((*lines)[idx]->s, LOG_HEAD1, sizeof(WCHAR)*37)) return FALSE;

	for (i=idx+1; i < lines->size(); i++) {
		if ((*lines)[i]->s[0] != ' ' || ((*lines)[i]->s[1] != 'T' && (*lines)[i]->s[1] != 'F')) break;
		if (!ParseSender((*lines)[i], lm)) return FALSE;
	}
	if (i == idx+1) return FALSE; // no user
	if (!ParseDate((*lines)[i++], lm)) return FALSE;
	if (ParseAttach((*lines)[i], lm)) i++;
	if ((*lines)[i]->len != 37 || memcmp((*lines)[i]->s, LOG_HEAD2, 37*sizeof(WCHAR))) return FALSE;

	lm->no  = idx;
	lm->num = i - idx + 1;

	return	TRUE;
}

BOOL TChildView::ParseLog()
{
	int		i   = 0;
	LogMsg	*lm = NULL;
	LogMsg	*nm = new LogMsg();
	int		max = (int)lines->size();

	while (i < max) {
		LineInfo	*l = (*lines)[i];

		if (ParseMsg(i, nm)) {
			if (lm) {
				if (nm->date >= lm->date) {
					lm->num = i - lm->no;
					msgs.push_back(lm);
					lm = nm;
					nm = new LogMsg;
				}
			} else {
				lm = nm;
				nm = new LogMsg;
			}
			i += lm->num;
		}
		else i++;
	}
	if (lm && lm->date) {
		lm->num = int(lines->size() - lm->no - 1);
		msgs.push_back(lm);
	} else {
		delete lm;
	}
	delete nm;

	return	TRUE;
}

BOOL TChildView::CreateU8(HWND _hParentToolBar)
{
	hParentToolBar = _hParentToolBar;

	static bool once = false;
	if (!once) {
		TRegisterClassU8(VIEWCHILD_CALSS, CS_DBLCLKS, NULL, ::LoadCursor(NULL, IDC_IBEAM),
						 (HBRUSH)::GetStockObject(WHITE_BRUSH));
		once = true;
	}

	LoadLog(cfg->LogFile);

	return	TWin::CreateU8(VIEWCHILD_CALSS, VIEWCHILD_CALSS,
							WS_CHILD|WS_VISIBLE|WS_TABSTOP/*|WS_HSCROLL*/|WS_VSCROLL);
}

BOOL TChildView::EvCreate(LPARAM lParam)
{
	TRect	trc;
	::GetWindowRect(hParentToolBar, &trc);
	toolBarCy = trc.cy();

	SetFont();
	FitSize();

	return	TRUE;
}

BOOL TChildView::EvNcDestroy(LPARAM lParam)
{
	UnloadLog();
	return	TRUE;
}

BOOL TChildView::UserPopupMenu()
{
	return TRUE;
}


BOOL TChildView::SetClipBoard()
{
	if (!selTopPos.Valid())     return FALSE;
	if (!::OpenClipboard(hWnd)) return	FALSE;
	::EmptyClipboard();

	int	size = 0;
	for (int i=selTopPos.no; i < selEndPos.no && i < lines->size(); i++) {
		size += (*lines)[i]->len + 1;
	}
	GBuf	gbuf(size * sizeof(WCHAR));
	WCHAR	*p = (WCHAR *)gbuf.Buf();

	if (p) {
		for (int i=selTopPos.no; i < selEndPos.no && i < lines->size(); i++) {
			LineInfo	*l = (*lines)[i];
			memcpy(p, l->s, l->len * sizeof(WCHAR));
			p[l->len] = '\n';
			p += l->len + 1;
		}
	}

	gbuf.Unlock();

	::SetClipboardData(CF_UNICODETEXT, gbuf.Handle());
	::CloseClipboard();

	selTopPos.Init();
	InvalidateRect(0, TRUE);

	return	TRUE;
}

BOOL TChildView::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
	case IDCANCEL:
//		PostQuitMessage(0);
		return	TRUE;

	case LOGVIEW_USERBTN:
		UserPopupMenu();
		return	TRUE;

	case LOGVIEW_COPY:
		return	SetClipBoard();
	}
	return FALSE;
}

BOOL TChildView::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	if (uMsg != WM_KEYDOWN) return	FALSE;

	switch (nVirtKey) {
	case VK_UP:
		EventScroll(WM_VSCROLL, SB_LINEUP, 0, 0);
		break;
	case VK_DOWN:
		EventScroll(WM_VSCROLL, SB_LINEDOWN, 0, 0);
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
	}
	return	FALSE;
}

BOOL TChildView::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	switch (uMsg) {
	case WM_SETFOCUS:
		break;

	case WM_KILLFOCUS:
		break;
	}

	return	FALSE;
}

int TChildView::ToLineNo(int view_no)
{
	int	min = 0;
	int	max = (int)lines->size() - 1;

	while (min <= max) {
		int			i   = (min + max) / 2;
		LineInfo	*l = (*lines)[i];

		if (l->viewNoEnd < view_no) {
			min = i + 1;
		}
		else if (l->viewNo > view_no) {
			max = i - 1;
		}
		else {
			return i;
		}
	}
	return	min == lines->size() ? max : 0;
}

BOOL TChildView::EvPaint()
{
	PAINTSTRUCT	ps;
	HDC			hDc = ::BeginPaint(hWnd, &ps);
 	HFONT		hOldFont = (HFONT)::SelectObject(hDc, hFont);
	SIZE		sz = {};
	TRect		rc;
	HDC			hBmpDc    = NULL;
	HBITMAP		hOldBmp   = NULL;
	HBRUSH		hOldBrush = NULL;

	int		i		= ToLineNo(viewPos);
	int		y_off	= (viewPos <= (*lines)[i]->viewNoEnd) ? viewPos - (*lines)[i]->viewNo : 0;
	int		y_total	= 0;

	for ( ; i < lines->size() && y_total < rectLines; i++) {
		LineInfo	*l	= (*lines)[i];
		int			len	= 0;
		int			rlen = 0;
		int			cx = vrc.cx();

		if ((l->flags & LOG_CLIP_FLAG) && y_off == 0) {
			if (!hBmpDc) {
				hBmpDc = ::CreateCompatibleDC(NULL);
				hOldBmp = (HBITMAP)::SelectObject(hBmpDc, hDummyBmp);
			}
			for (int j=0; j < msgs[l->msgNo]->clips.size(); j++) {
				::BitBlt(hDc, vrc.x() + 16*j + 5, y_total * lineCy + CVW_GAP, 16, 16,
						 hBmpDc, 0, 0, SRCCOPY);
			}
			cx -= int(msgs[l->msgNo]->clips.size()*16 + 5);
		}

		while (::GetTextExtentExPointW(hDc, l->s+len, l->len-len, cx, &rlen, 0, &sz)) {
			if (y_off > 0) {
				y_off--;
			}
			else {
				if (selTopPos.Valid() && i >= selTopPos.no && i < selEndPos.no) {
					SetTextColor(hDc, 0xffffff);
					SetBkColor(hDc,   0x000000);
				}
				TextOutW(hDc, CVW_GAP + (vrc.cx()-cx), y_total * lineCy + CVW_GAP, l->s+len, rlen);
				if (selTopPos.Valid() && i >= selTopPos.no && i < selEndPos.no) {
					SetTextColor(hDc, 0x000000);
					SetBkColor(hDc,   0xffffff);
				}
				cx = vrc.cx();
				if (++y_total >= rectLines) break;
			}
			if ((len += rlen) >= l->len) break;
		}
	}

	POINT	pt;
	::GetCursorPos(&pt);
	::ScreenToClient(hWnd, &pt);
	POINTS	pos = { (short)pt.x, (short)pt.y };

	LinePos		lp;
	PointsToLine(pos, &lp);
	LineInfo *l = (*lines)[lp.no];

	if (l->flags & LOG_CLIP_FLAG) {
		int			view_no	= (pos.y / lineCy) + viewPos;
		int			y_off	= view_no - l->viewNo;
		LogMsg		*m		= msgs[l->msgNo];
		int			img_i	= (pos.x - vrc.x() - 5) / 16;

		if (pos.x >= vrc.x() && img_i < m->clips.size()) {
			char	image_dir[MAX_PATH_U8];

			if (MakeImageFolderName(cfg, image_dir)) {
				Wstr	image_dirw(image_dir);
				WCHAR	fname[MAX_PATH], *p=fname;

				p += swprintf(fname, L"%s\\", image_dirw);
				m->clips[img_i].fname.Get(p);

				Bitmap	*bmp = Bitmap::FromFile(fname);
				Graphics g(hDc);
				int y = ((*lines)[m->no]->viewNo - viewPos) * lineCy - bmp->GetHeight();
				if (y < CVW_GAP) y = CVW_GAP;
				g.DrawImage(bmp, 30, y);
				delete bmp;
			}
		}
	}

	::SelectObject(hDc, hOldFont);

	if (hOldBmp) ::SelectObject(hBmpDc, hOldBmp);
	if (hBmpDc)  ::DeleteDC(hBmpDc);

	::EndPaint(hWnd, &ps);

	return	TRUE;
}

BOOL TChildView::EventScroll(UINT uMsg, int nScrollCode, int nPos, HWND hScroll)
{
	if (uMsg == WM_VSCROLL) {
		int	svPos = viewPos;

		switch (nScrollCode) {
		case SB_BOTTOM:
			viewPos = viewLines - rectLines;
			break;
		case SB_TOP:
			viewPos = 0;
			break;
		case SB_LINEDOWN:
			viewPos++;
			break;
		case SB_LINEUP:
			viewPos--;
			break;
		case SB_PAGEDOWN:
			viewPos += rectLines;
			break;
		case SB_PAGEUP:
			viewPos -= rectLines;
			break;
		case SB_THUMBPOSITION:
			viewPos = nPos;
			break;
		case SB_THUMBTRACK:
			{
				static DWORD last;
				DWORD	cur = GetTickCount();

				if (cur - last > 50 || nPos == viewLines - rectLines) {
					viewPos = nPos;
					last = cur;
				}
			}
			break;
//		case SB_ENDSCROLL:
		}
		if (viewPos + rectLines > viewLines) viewPos = viewLines - rectLines;
		if (viewPos <= 0) viewPos = 0;

		int	dy = (svPos - viewPos) * lineCy;
		if (dy != 0) {
			::ScrollWindow(hWnd, 0, dy, &vrc, &vrc);
			::SetScrollPos(hWnd, SB_VERT, viewPos, TRUE);

			TRect	rc = crc;
			if (dy > 0) {
				rc.bottom = rc.top + dy + CVW_GAP;
			}
			else {
				rc.top = rc.bottom + dy - CVW_GAP - lineCy;
			}
			InvalidateRect(&rc, FALSE);

 Debug(" nPos=%d viewPos=%d rectLines=%d viewLines=%d\n", nPos, viewPos, rectLines, viewLines);

		}
	}
	return	TRUE;
}

BOOL TChildView::FitSize()
{
	if (lineCy == 0) return FALSE; // not initialized, yet

	TRect	prc;
	parentView->GetClientRect(&prc);
	MoveWindow(0, toolBarCy, prc.cx(), prc.cy() - toolBarCy, FALSE);
 	GetClientRect(&crc);
 	vrc = crc;
 	vrc.Infrate(-CVW_GAP, -CVW_GAP);

	rectLines = vrc.cy() / lineCy;
	if (lines->size() == 0) return TRUE;

	int	line_no	= ToLineNo(viewPos);
	int	sv_pos	= viewPos;

	int	cx	= vrc.cx();
	int	cx2	= cx - 5;

	viewLines = 0;
	for (int i=0; i < lines->size(); i++) {
		LineInfo	*l = (*lines)[i];
		int			len = 1;

		int	remain = l->bmpSize - cx;
		if (remain > 0) {
			len += (remain + cx2 - 1) / cx2;
		}
		l->viewNo	 = viewLines;
		l->viewNoEnd = viewLines + len - 1;
		viewLines	+= len;
	}
	::SetScrollRange(hWnd, SB_VERT, 0, viewLines, FALSE);

	viewPos = (*lines)[line_no]->viewNo;
	if (viewPos > viewLines - rectLines) {
		viewPos = viewLines - rectLines;
		if (viewPos <= 0) viewPos = 0;
	}

 Debug("max/page/pos=%d/%d/%d  view=%d\n", max(viewLines - 1, 0), rectLines, viewPos, viewLines);

	SCROLLINFO	si = { sizeof(si) };
	si.fMask		= SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
	si.nMin			= 0;
	si.nMax			= max(viewLines - 1, 0);
	si.nPage		= rectLines;
	si.nTrackPos	= viewPos;
	::SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
	::SetScrollPos(hWnd, SB_VERT, viewPos, TRUE);
	InvalidateRect(NULL, TRUE);

	return	TRUE;
}

BOOL TChildView::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (scrTimerID == timerID) {
		if (selStart.Valid() && GetForegroundWindow() == parent->hWnd) {
			POINT	pt;
			LinePos	lp;

			::GetCursorPos(&pt);
			::ScreenToClient(hWnd, &pt);
			POINTS	pos = { (short)pt.x, (short)pt.y };
			PointsToLine(pos, &lp);
			Debug("xy=%d/%d lp=%d\n", pos.x, pos.y, lp.no);

			if (pos.y > vrc.bottom) {
				if (lp.no < lines->size() - 1) {
					PostMessage(WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
				}
				return	TRUE;
			}
			else if (pos.y < vrc.top) {
				if (lp.no > 0) {
					PostMessage(WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
				}
				return	TRUE;
			}
		}
		::KillTimer(hWnd, scrTimerID);
		scrTimerID = 0;
		Debug("timer end\n");

		return	TRUE;
	}

	return	FALSE;
}


BOOL TChildView::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg) {
	case WM_LBUTTONDOWN:
		if (!selStart.Valid()) {
			PointsToLine(pos, &selStart);
			selTopPos.Init();
			::SetCapture(hWnd);
		}
		InvalidateRect(0, TRUE);
		break;

	case WM_LBUTTONUP:
		if (selStart.Valid()) {
			::ReleaseCapture();
			LinePos		lp;
			PointsToLine(pos, &lp);

			if (selStart.no != lp.no) {
				if (selStart < lp) {
					selTopPos = selStart;
					selEndPos = lp;
				} else {
					selTopPos = lp;
					selEndPos = selStart;
				}
				Debug("sel set(%d-%d)\n", selTopPos.no, selEndPos.no);
			}
			InvalidateRect(0, TRUE);
			selStart.Init();
		}
		break;
	}
	return	FALSE;
}

BOOL TChildView::SetFont()
{
	// フォントセット
	HDC hDc = ::GetDC(hWnd);

	if (*cfg->LogViewFont.lfFaceName == 0) {
		char	*ex_font = GetLoadStrA(IDS_LOGVIEWFONT);
		if (ex_font) {
			char	*p;
			char	*height	= separate_token(ex_font, ':', &p);
			char	*face	= separate_token(NULL, 0, &p);
			if (height && face) {
				strcpy(cfg->LogViewFont.lfFaceName, face);
				cfg->LogViewFont.lfCharSet = DEFAULT_CHARSET;
				POINT pt={}, pt2={};
				pt.y = ::MulDiv(::GetDeviceCaps(hDc, LOGPIXELSY), atoi(height), 72);
				::DPtoLP(hDc, &pt,  1);
				::DPtoLP(hDc, &pt2, 1);
				cfg->LogViewFont.lfHeight = -abs(pt.y - pt2.y);
			}
		}
	}

	if (*cfg->LogViewFont.lfFaceName) {
		if (hFont) ::DeleteObject(hFont);
		hFont = ::CreateFontIndirect(&cfg->LogViewFont);
		::GetObject(hFont, sizeof(LOGFONT), &cfg->LogViewFont);
	}

	// 設定フォントに基づく、表示行の計算
	HFONT	hOldFont = (HFONT)::SelectObject(hDc, hFont);
	SIZE	size	 = {};
	TEXTMETRIC	tm	 = {};

 	GetTextMetrics(hDc, &tm);
 	lineCy = tm.tmHeight;

	for (int i=0; i < lines->size(); i++) {
		LineInfo	*l = (*lines)[i];
		::GetTextExtentPoint32W(hDc, l->s, l->len, &size);
		l->bmpSize = size.cx;
		l->viewNo  = i; // not need ...
		if (l->flags & LOG_CLIP_FLAG) {
			LogMsg *m = msgs[l->msgNo];
			l->bmpSize += int(m->clips.size() * 16 + 5); // dummpy bmp is 16px + 2px margin
		}
	}
	::SelectObject(hDc, hOldFont);
	::ReleaseDC(hWnd, hDc);

	return	TRUE;
}


BOOL TChildView::EvMouseMove(UINT fwKeys, POINTS pos)
{
	if (selStart.Valid()) {
		static int	y_top_sv, y_end_sv;
		LinePos		lp;
		PointsToLine(pos, &lp);

		if (selStart < lp) {
			selTopPos = selStart;
			selEndPos = lp;
		} else {
			selTopPos = lp;
			selEndPos = selStart;
		}

		int	y_top = (*lines)[selTopPos.no]->viewNo    - viewPos;
		int	y_end = (*lines)[selEndPos.no]->viewNoEnd - viewPos;
		if (y_top < 0)         y_top = 0;
		if (y_end > rectLines) y_end = rectLines;
		TRect rc(vrc.x(),  vrc.y() + min(y_top, y_top_sv) * lineCy,
				 vrc.cx(), vrc.y() + max(y_end, y_end_sv) * lineCy);
		InvalidateRect(&rc, 0);
		y_top_sv = y_top;
		y_end_sv = y_end;

		if (pos.y > vrc.bottom || pos.y < vrc.top) {
			if (scrTimerID == 0) {
				scrTimerID = ::SetTimer(hWnd, LOGVIEW_TIMER, LOGVIEW_TIMER_TICK, 0);
				Debug("timer start\n");
			}
		}
	}
	else {
		LinePos		lp;
		PointsToLine(pos, &lp);
		LineInfo	*l = (*lines)[lp.no];
		int			view_no	= (pos.y / lineCy) + viewPos;
		int			y_off	= view_no - l->viewNo;
		LogMsg		*m		= msgs[l->msgNo];
		int			img_i	= (pos.x - vrc.x() - 5) / 16;
		static int	last_idx = -1;

		if ((l->flags & LOG_CLIP_FLAG) && img_i>= 0 && img_i < m->clips.size()) {
			if (last_idx != img_i) {
				last_idx = img_i;
				InvalidateRect(0, TRUE);
			}
		}
		else if (last_idx != -1) {
			InvalidateRect(0, TRUE);
			last_idx = -1;
		}
	}
	return	FALSE;
}

BOOL TChildView::PointsToLine(POINTS pos, LinePos *lp)
{
	pos.x -= (short)vrc.x();
	pos.y -= (short)vrc.y();
	if (pos.x < 0)			pos.x = 0;
	if (pos.x > vrc.cx())	pos.x = (short)vrc.cx();
	if (pos.y < 0)			pos.y = 0;
	if (pos.y > vrc.cy())	pos.y = (short)vrc.cy();
	lp->Init();

	HDC		hDc = ::GetDC(hWnd);
 	HFONT	hOldFont = (HFONT)::SelectObject(hDc, hFont);

	int			view_no	= (pos.y / lineCy) + viewPos;
	int			line_no	= ToLineNo(view_no);
	LineInfo	*l		= (*lines)[line_no];
	int			y_off	= view_no - l->viewNo;
	int			y_off_sv = y_off;
	int			len	 = 0;
	int			rlen = 0;
	SIZE		sz = {};

	while (y_off > 0 && ::GetTextExtentExPointW(hDc, l->s+len, l->len-len, vrc.cx(), &rlen, 0, &sz)) {
//		Debug("y=%d rlen=%d cx=%d\n", y_off, rlen, sz.cx);
		y_off--;
		if ((len += rlen) >= l->len) break;
	}
	if (len < l->len) {
		if (::GetTextExtentExPointW(hDc, l->s+len, l->len-len, pos.x, &rlen, NULL, &sz)) {
//			Debug("x=%d len=%d rlen=%d cx=%d\n", pos.x, len, rlen, sz.cx);
			len += rlen;
		}
	}
	lp->no		= line_no;
	lp->xOff 	= len;

//	Debug("lp=%d/%d view=%d len=%d/%d yoff=%d\n", lp->no, lp->xOff, view_no, l->len, len, y_off_sv);

	::SelectObject(hDc, hOldFont);
	::ReleaseDC(hWnd, hDc);

	return	TRUE;
}

#endif

