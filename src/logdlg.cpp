static char *logdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   logdlg.cpp	Ver3.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Log Dialog
	Create					: 1996-08-12(Mon)
	Update					: 2011-04-20(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"

TLogDlg::TLogDlg(Cfg *_cfg, TWin *_parent) : TDlg(LOG_DIALOG, _parent)
{
	cfg = _cfg;
}

BOOL TLogDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case SET_BUTTON:
		GetData();
		if (GetDriveType(NULL) == DRIVE_REMOTE
				&& cfg->LogCheck && strchr(cfg->LogFile, '\\') == NULL)
			MessageBox(GetLoadStr(IDS_LOGALERT));
		if (cfg->LogCheck)
			LogMng::StrictLogFile(cfg->LogFile);
		cfg->WriteRegistry(CFG_GENERAL);
		::SendMessage(GetMainWnd(), WM_IPMSG_INITICON, 0, 0);
		SetDlgIcon(hWnd);
		SetHotKey(cfg);
		if (wID == IDOK)
			EndDialog(TRUE);
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;

	case MAINICON_BUTTON: case REVICON_BUTTON:
		OpenFileDlg(this).Exec(wID == MAINICON_BUTTON ? MAINICON_EDIT : REVICON_EDIT, GetLoadStrU8(IDS_OPENFILEICON), GetLoadStrAsFilterU8(IDS_OPENFILEICONFLTR));
		return	TRUE;

	case SOUNDFILE_BUTTON:
		OpenSoundFileDlg();
		return	TRUE;

	case LOGFILE_BUTTON:
		OpenFileDlg(this, OpenFileDlg::SAVE).Exec(LOG_EDIT, GetLoadStrU8(IDS_OPENFILELOG), GetLoadStrAsFilterU8(IDS_OPENFILELOGFLTR));
		return	TRUE;
	}

	return	FALSE;
}

BOOL TLogDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int x = rect.left + 50, y = rect.top + 50;
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	if (cfg->LogCheck)
		LogMng::StrictLogFile(cfg->LogFile);
	MoveWindow(x, y, xsize, ysize, FALSE);
	SetData();

	return	TRUE;
}

void TLogDlg::SetData(void)
{
	SendDlgItemMessage(HOTKEY_CHECK, BM_SETCHECK, cfg->HotKeyCheck, 0);
	SendDlgItemMessage(OPEN_CHECK, BM_SETCHECK, cfg->OpenCheck, 0);
	SendDlgItemMessage(BALLOONNOTIFY_CHECK, BM_SETCHECK, cfg->BalloonNotify, 0);
	SendDlgItemMessage(CONTROLIME_CHECK, BM_SETCHECK, cfg->ControlIME, 0);
	SendDlgItemMessage(NOERASE_CHECK, BM_SETCHECK, cfg->NoErase, 0);
	SendDlgItemMessage(ABNORMALBTN_CHECK, BM_SETCHECK, cfg->AbnormalButton, 0);
	SendDlgItemMessage(ONECLICK_CHECK, BM_SETCHECK, cfg->OneClickPopup, 0);
	SendDlgItemMessage(ABSENCENONPOPUP_CHECK, BM_SETCHECK, cfg->AbsenceNonPopup, 0);
	SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_LIMITED));
	SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_DIRECTED));
	SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_BOTH));
	SendDlgItemMessage(EXTBROADCAST_COMBO, CB_SETCURSEL, cfg->ExtendBroadcast, 0);

	if (cfg->lcid != -1 || GetSystemDefaultLCID() == 0x411) {
		::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
		::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
		CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid == 0x411 ? FALSE : TRUE);
	}

	SendDlgItemMessage(LOG_CHECK, BM_SETCHECK, cfg->LogCheck, 0);
	SendDlgItemMessage(PASSWDLOG_CHECK, BM_SETCHECK, cfg->PasswdLogCheck, 0);
	SetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr);
	SetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile);
	SetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile);
	SetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile);
	SetDlgItemTextU8(LOG_EDIT, cfg->LogFile);

	SendDlgItemMessage(NICKNAME_CHECK, BM_SETCHECK, !cfg->NickNameCheck, 0);
	SendDlgItemMessage(IPADDR_CHECK, BM_SETCHECK, cfg->IPAddrCheck, 0);
	SendDlgItemMessage(LOGUTF8_CHECK, BM_SETCHECK, cfg->LogUTF8, 0);
	SendDlgItemMessage(IMAGESAVE_CHECK, BM_SETCHECK, (cfg->ClipMode & 2) ? 1 : 0, 0);
}

void TLogDlg::GetData(void)
{
	cfg->HotKeyCheck = (int)SendDlgItemMessage(HOTKEY_CHECK, BM_GETCHECK, 0, 0);
	cfg->OpenCheck = (int)SendDlgItemMessage(OPEN_CHECK, BM_GETCHECK, 0, 0);
	cfg->BalloonNotify = (int)SendDlgItemMessage(BALLOONNOTIFY_CHECK, BM_GETCHECK, 0, 0);
	cfg->ControlIME = (int)SendDlgItemMessage(CONTROLIME_CHECK, BM_GETCHECK, 0, 0);
	cfg->NoErase = (int)SendDlgItemMessage(NOERASE_CHECK, BM_GETCHECK, 0, 0);
	cfg->AbnormalButton = (int)SendDlgItemMessage(ABNORMALBTN_CHECK, BM_GETCHECK, 0, 0);
	cfg->OneClickPopup = (int)SendDlgItemMessage(ONECLICK_CHECK, BM_GETCHECK, 0, 0);
	cfg->AbsenceNonPopup = (int)SendDlgItemMessage(ABSENCENONPOPUP_CHECK, BM_GETCHECK, 0, 0);
	cfg->ExtendBroadcast = (int)SendDlgItemMessage(EXTBROADCAST_COMBO, CB_GETCURSEL, 0, 0);

	if (::IsWindowEnabled(GetDlgItem(LCID_CHECK))) {
		cfg->lcid = IsDlgButtonChecked(LCID_CHECK) ? 0x409 : -1;
	}

	cfg->LogCheck = (int)SendDlgItemMessage(LOG_CHECK, BM_GETCHECK, 0, 0);
	cfg->PasswdLogCheck = (int)SendDlgItemMessage(PASSWDLOG_CHECK, BM_GETCHECK, 0, 0);
	GetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr, sizeof(cfg->QuoteStr));
	GetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile, sizeof(cfg->IconFile));
	GetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile, sizeof(cfg->SoundFile));
	GetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile, sizeof(cfg->RevIconFile));
	GetDlgItemTextU8(LOG_EDIT, cfg->LogFile, sizeof(cfg->LogFile));

	cfg->NickNameCheck = !(int)SendDlgItemMessage(NICKNAME_CHECK, BM_GETCHECK, 0, 0);
	cfg->IPAddrCheck = (int)SendDlgItemMessage(IPADDR_CHECK, BM_GETCHECK, 0, 0);
	cfg->LogUTF8 = (int)SendDlgItemMessage(LOGUTF8_CHECK, BM_GETCHECK, 0, 0);
	if (SendDlgItemMessage(IMAGESAVE_CHECK, BM_GETCHECK, 0, 0))	cfg->ClipMode |=  2;
	else														cfg->ClipMode &= ~2;
}

BOOL TLogDlg::OpenSoundFileDlg(void)
{
	char	szDirName[MAX_PATH_U8] = "", dirNameBak[MAX_PATH_U8];

	GetCurrentDirectoryU8(sizeof(dirNameBak), dirNameBak);
	GetWindowsDirectoryU8(szDirName, sizeof(szDirName));
	if (IsNewShell())
		strcat(szDirName, "\\Media");

	BOOL	ret = OpenFileDlg(this).Exec(SOUND_EDIT, GetLoadStrU8(IDS_OPENFILESND), GetLoadStrAsFilterU8(IDS_OPENFILESNDFLTR), szDirName);

	SetCurrentDirectoryU8(dirNameBak);

	return	ret;
}

