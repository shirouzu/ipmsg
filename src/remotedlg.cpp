static char *remotedlg_id = 
	"@(#)Copyright (C) H.Shirouzu 2013-2017   remotedlg.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Remote Dialog
	Create					: 2013-09-29(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

TRemoteDlg::TRemoteDlg(Cfg *_cfg, TWin *_parent) : TDlg(REMOTE_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg      = _cfg;
	mode     = INIT;
	title    = NULL;
	graceSec = 0;
	count    = 0;
}

BOOL TRemoteDlg::Start(Mode _mode)
{
	if (_mode < REBOOT ||  _mode > HIBERNATE) return FALSE;

	mode     = _mode;
	count    = 0;
	graceSec = cfg->RemoteGraceSec;

	if (hWnd)	StartCore();
	else		Create();

	return	TRUE;
}

BOOL TRemoteDlg::StartCore()
{
	char	buf[MAX_BUF];

	title = LoadStrU8(mode == REBOOT    ? IDS_REMOTE_REBOOT    :
					  mode == STANDBY   ? IDS_REMOTE_STANDBY   :
					  mode == IPMSGEXIT ? IDS_REMOTE_IPMSGEXIT :
					  mode == HIBERNATE ? IDS_REMOTE_HIBERNATE : IDS_REMOTE_EXIT);
	notifyFmt = LoadStrU8(IDS_REMOTE_NOTIFY_FMT);

	SetTimer(IPMSG_REMOTE_TIMER, 1000);
	SetWindowTextU8(title);
	snprintfz(buf, sizeof(buf), notifyFmt, title, graceSec, title);
	SetDlgItemTextU8(NOTIFY_STATIC, buf);

	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	int x = (cx - xsize) / 2, y = (cy - ysize) / 2;
	MoveWindow(x, y, xsize, ysize, FALSE);

	Show();
	return	TRUE;
}

BOOL TRemoteDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);
	StartCore();
	return	TRUE;
}

BOOL TRemoteDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	if (wID == IDOK || wID == IDCANCEL) {
		KillTimer(IPMSG_REMOTE_TIMER);
		EndDialog(wID);
		return TRUE;
	}
	return FALSE;
}

BOOL TRemoteDlg::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (++count <= graceSec) {
		char	buf[MAX_BUF];
		snprintfz(buf, sizeof(buf), notifyFmt, title, graceSec - count, title);
		SetDlgItemTextU8(NOTIFY_STATIC, buf);
	}
	else {
		KillTimer(IPMSG_REMOTE_TIMER);
		Show(SW_HIDE);
		PostMessage(WM_COMMAND, IDOK, 0);

		TSetPrivilege(SE_SHUTDOWN_NAME, TRUE);

		switch (mode) {
		case REBOOT:
			::InitiateSystemShutdownEx(NULL, NULL, 0, TRUE, TRUE,
				SHTDN_REASON_MAJOR_APPLICATION |
				SHTDN_REASON_MINOR_OTHER |
				SHTDN_REASON_FLAG_PLANNED
			);
			break;

		case EXIT:
			::InitiateSystemShutdownEx(NULL, NULL, 0, TRUE, FALSE,
				SHTDN_REASON_MAJOR_APPLICATION |
				SHTDN_REASON_MINOR_OTHER |
				SHTDN_REASON_FLAG_PLANNED
			);
			break;

		case IPMSGEXIT:
			::PostMessage(GetMainWnd(), WM_COMMAND, IDCANCEL, 0);
			break;

		case STANDBY:
			::SetSystemPowerState(TRUE, TRUE);
			break;

		case HIBERNATE:
			::SetSystemPowerState(FALSE, TRUE);
			break;
		}
	}

	return	TRUE;
}

