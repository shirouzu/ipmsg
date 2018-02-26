static char *logeditdlg_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2016  logeditdlg.cpp	Ver4.00";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child item
	Create					: 2015-04-10(Sat)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
using namespace std;
#include "logeditdlg.h"

/* ================================================================
		メッセージヘッダ編集ダイアログ
   ================================================================ */
TMsgHeadEditDlg::TMsgHeadEditDlg(Cfg *_cfg, TWin *_parent) : TDlg(MSGHEAD_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	in_host = out_host = NULL;
}

BOOL TMsgHeadEditDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		{
			WCHAR	wbuf[1024]={};

			GetDlgItemTextW(NICK_EDIT,   wbuf, wsizeof(wbuf));
			out_host->nick  = wbuf;
			GetDlgItemTextW(UID_EDIT,    wbuf, wsizeof(wbuf));
			out_host->uid   = wbuf;
			GetDlgItemTextW(GNAME_EDIT,  wbuf, wsizeof(wbuf));
			out_host->gname = wbuf;
			GetDlgItemTextW(HOST_EDIT,   wbuf, wsizeof(wbuf));
			out_host->host  = wbuf;
			GetDlgItemTextW(IPADDR_EDIT, wbuf, wsizeof(wbuf));
			out_host->addr  = wbuf;
		}
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMsgHeadEditDlg::EvCreate(LPARAM lParam)
{
	SetDlgItem(NICK_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(UID_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(GNAME_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(HOST_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(IPADDR_EDIT, X_FIT|TOP_FIT);
	SetDlgItem(IDOK, MIDX_FIT|TOP_FIT);

	GetWindowRect(&rect);

	if (parent) {
		MoveWindow(rect.left +100, rect.top +100, rect.right - rect.left,
			rect.bottom - rect.top, FALSE);
	}
	else {
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	x = (::GetSystemMetrics(SM_CXFULLSCREEN) - xsize)/2;
		int y = (::GetSystemMetrics(SM_CYFULLSCREEN) - ysize)/2;
		MoveWindow(x, y, xsize, ysize, FALSE);
	}

	if (in_host->nick)  SetDlgItemTextW(NICK_EDIT,  in_host->nick.s());
	if (in_host->uid)   SetDlgItemTextW(UID_EDIT,   in_host->uid.s());
	if (in_host->gname) SetDlgItemTextW(GNAME_EDIT, in_host->gname.s());
	if (in_host->host)  SetDlgItemTextW(HOST_EDIT,  in_host->host.s());
	if (in_host->addr)  SetDlgItemTextW(IPADDR_EDIT,in_host->addr.s());

	FitDlgItems();

	::PostMessage(GetDlgItem(NICK_EDIT), EM_SETSEL, 0, 0);

	return	TRUE;
}

int TMsgHeadEditDlg::Exec(LogHost *_in_host, LogHost *_out_host)
{
	in_host  = _in_host;
	out_host = _out_host;

	return	TDlg::Exec();
}

BOOL TMsgHeadEditDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED) {
		return	FitDlgItems();
	}
	return	FALSE;;
}

/* ================================================================
		メッセージ本文編集ダイアログ
   ================================================================ */
TMsgEditDlg::TMsgEditDlg(Cfg *_cfg, UINT _id, TWin *_parent) : TDlg(_id, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	in_msg = NULL;
	out_msg = NULL;
	title_ids = 0;
}

BOOL TMsgEditDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		{
			Wstr	wbuf(MAX_UDPBUF), wbuf2(MAX_UDPBUF);

			GetDlgItemTextW(MSG_EDIT, wbuf.Buf(), MAX_UDPBUF);
			LocalNewLineToUnixW(wbuf.s(), wbuf2.Buf(), MAX_UDPBUF);
			*out_msg = wbuf2;
		}
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case DEL_BTN:	// 内容削除
		*out_msg = L"";
		EndDialog(IDOK);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMsgEditDlg::EvCreate(LPARAM lParam)
{
	SetDlgItem(MSG_EDIT, XY_FIT);
	SetDlgItem(IDOK, MIDX_FIT|BOTTOM_FIT);

	if (GetDlgItem(IDCANCEL)) {
		SetDlgItem(IDCANCEL, RIGHT_FIT|BOTTOM_FIT);
	}
	if (GetDlgItem(DEL_BTN)) {
		SetDlgItem(DEL_BTN, RIGHT_FIT|BOTTOM_FIT);
	}
	if (GetDlgItem(MSG_STATIC)) {
		SetDlgItem(MSG_STATIC, LEFT_FIT|BOTTOM_FIT);
	}

	if (title_ids) {
		if (const WCHAR *title = LoadStrW(title_ids)) {
			SetWindowTextW(title);
		}
	}

	GetWindowRect(&rect);
	if (parent) {
		MoveWindow(rect.left +100, rect.top +100, rect.right - rect.left,
			rect.bottom - rect.top, FALSE);
	}
	else {
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	x = (::GetSystemMetrics(SM_CXFULLSCREEN) - xsize)/2;
		int y = (::GetSystemMetrics(SM_CYFULLSCREEN) - ysize)/2;
		MoveWindow(x, y, xsize, ysize, FALSE);
	}

//	SendDlgItemMessage(MSG_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);

	Wstr	wbuf(MAX_UDPBUF);
	UnixNewLineToLocalW(in_msg->s(), wbuf.Buf(), MAX_UDPBUF);
	SetDlgItemTextW(MSG_EDIT, wbuf.s());

	::PostMessage(GetDlgItem(MSG_EDIT), EM_SETSEL, -1, -1);

	return	TRUE;
}

int TMsgEditDlg::Exec(Wstr *_in_msg, Wstr *_out_msg, int _title_ids)
{
	in_msg  = _in_msg;
	out_msg = _out_msg;
	title_ids = _title_ids;

	return	TDlg::Exec();
}

BOOL TMsgEditDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED) {
		return	FitDlgItems();
	}
	return	FALSE;;
}

