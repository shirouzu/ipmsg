static char *logimportdlg_id = 
	"@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2017   logimportdlg.cpp	Ver4.50";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child item
	Create					: 2015-04-10(Sat)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
using namespace std;
#include "logimportdlg.h"

#define PROCESS_MSG "processing... (wait a minute)"

/* ================================================================
	ログ取り込み/削除
   ================================================================ */
TLogImportDlg::TLogImportDlg(Cfg *_cfg, LogDb *_logDb, TChildView *_view) :
	logListView(this), logDb(_logDb),
	TDlg(LOGIMPORT_DIALOG, _view)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	view = _view;
	logVec.Init(0, 1000, 10);
}

BOOL TLogImportDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);

	SetDlgItem(ADDLOG_BTN, BOTTOM_FIT|LEFT_FIT);
	SetDlgItem(DELLOG_BTN, BOTTOM_FIT|LEFT_FIT);
	SetDlgItem(LOG_LISTVIEW, XY_FIT);
	SetDlgItem(STATUS_STATIC, BOTTOM_FIT|X_FIT);
	SetDlgItemText(STATUS_STATIC, "");

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

	Init();
	LoadData();
	FitDlgItems();

	return	TRUE;
}

BOOL TLogImportDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case ADDLOG_BTN:
		AddLog();
		return	TRUE;

	case DELLOG_BTN:
		DelLog();
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TLogImportDlg::Init()
{
	logListView.AttachWnd(GetDlgItem(LOG_LISTVIEW));

	char	*title[] = { "No", "ImportDate", "FileName", "Size", "Num" };
	int		width[]  = { 25,   120,          200,        70,     60    };

	for (int i=0; i < LLV_MAX; i++) {
		logListView.InsertColumn(i, title[i], width[i]);
	}
	return	TRUE;
}

BOOL TLogImportDlg::LoadData()
{
	logVec.SetUsedNum(0);
	if (!logDb->SelectImportId(&logVec)) {
		return FALSE;
	}

	// 非インポートデータ
	int		idx = 0;
	char	buf[MAX_PATH_U8];
	logListView.InsertItem(idx, "0", 0);	// importid == 0
	logListView.SetSubItem(idx, 2, "(Non imported data)");
	comma_int64(buf, logDb->GetMaxNum(0));
	logListView.SetSubItem(idx, 4, buf);
	idx++;

	for (int i=0; i < logVec.UsedNumInt(); i++, idx++) {
		LogVec	&log = logVec[i];
		WCHAR	wbuf[MAX_PATH];

		logListView.InsertItem(idx, Fmt("%d", idx), (LPARAM)log.importid);

		MakeDateStr(log.date, wbuf);
		WtoU8(wbuf, buf, sizeof(buf));
		logListView.SetSubItem(idx, 1, buf);

		WtoU8(log.logname.s(), buf, sizeof(buf));
		logListView.SetSubItem(idx, 2, buf);

		comma_int64(buf, log.logsize);
		logListView.SetSubItem(idx, 3, buf);

		comma_int64(buf, logDb->GetMaxNum(log.importid));
		logListView.SetSubItem(idx, 4, buf);
	}
	return	TRUE;
}

BOOL TLogImportDlg::EvNcDestroy()
{
	logListView.DeleteAllItems();
	return	TRUE;
}

BOOL TLogImportDlg::AddLog()
{
	int		bufsize = MAX_MULTI_PATH;
	U8str	buf(bufsize);
	U8str	path(bufsize);
	char	dir[MAX_PATH_U8] = "";

	OpenFileDlg		ofdlg(this, OpenFileDlg::MULTI_OPEN);

	cfg->GetBaseDir(dir);
	if (!ofdlg.Exec(buf.Buf(), bufsize, "Select Import log", "*.log", dir)) {
		return	FALSE;
	}
	SetDlgItemText(STATUS_STATIC, PROCESS_MSG);

	int		dirlen = (int)strlen(dir);
	BOOL	ret = TRUE;

	if (buf[dirlen]) {
		strcpy(path.Buf(), buf.s());
		ret = AddTextLogToDb(cfg, logDb, path.s());
	}
	else {
		for (const char *fname = buf.s() + dirlen + 1; *fname; fname += strlen(fname) + 1)
		{
			if (MakePathU8(path.Buf(), buf.s(), fname) >= MAX_PATH_U8)
				continue;
			if (!(ret = AddTextLogToDb(cfg, logDb, path.s()))) {
				break;
			}
		}
	}
	SetDlgItemText(STATUS_STATIC, ret ? "Done" : "Import error (or No Data)");

	logListView.DeleteAllItems();
	LoadData();
	view->RequestResetCache();

	return	ret;
}

BOOL TLogImportDlg::DelLog()
{
	int	sel_cnt = logListView.GetSelectedItemCount();

	if (sel_cnt == 0) {
		return FALSE;
	}

	if (TMsgBox(this).Exec(Fmt("Delete %d log(s) OK?", sel_cnt)) != IDOK) {
		return FALSE;
	}

	SetDlgItemText(STATUS_STATIC, PROCESS_MSG);

	BOOL	ret = TRUE;
	int		max_num = logListView.GetItemCount();
	for (int i=0; i <= max_num; i++) {
		if (logListView.IsSelected(i)) {
			int	importid = (int)logListView.GetItemParam(i);
			if (!(ret = logDb->DeleteImportData(importid))) {
				break;
			}
		}
	}
	SetDlgItemText(STATUS_STATIC, ret ? "delete done" : "delete error");

	logListView.DeleteAllItems();
	LoadData();
	view->RequestResetCache();

	return	ret;
}


BOOL TLogImportDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	FitDlgItems();
	return	TRUE;
}


