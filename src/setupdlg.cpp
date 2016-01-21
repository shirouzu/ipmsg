static char *setupdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2015   setupdlg.cpp	Ver3.60";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Setup Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2015-11-01(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "plugin.h"

/*
	Setup用Sheet
*/
BOOL TSetupSheet::Create(int _resId, Cfg *_cfg, THosts *_hosts, TWin *_parent)
{
	cfg		= _cfg;
	hosts	= _hosts;
	resId	= _resId;
	parent	= _parent;
	curUrl	= NULL;
	return	TDlg::Create();
}

BOOL TSetupSheet::EvCreate(LPARAM lParam)
{
	SendDlgItemMessage(REBOOT_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);
	SendDlgItemMessage(EXIT_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);

	SetData();

	RECT	rc;
	POINT	pt;
	::GetWindowRect(parent->GetDlgItem(SETUP_LIST), &rc);
	pt.x = rc.right;
	pt.y = rc.top;
	::ScreenToClient(parent->hWnd, &pt);
	SetWindowPos(0, pt.x + 10, pt.y - 10, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE)|WS_EX_CONTROLPARENT);
	SetDlgIcon(hWnd);

	if (IsWinVista() && TIsEnableUAC()) {
		::SendMessage(GetDlgItem(SAVE_BUTTON), BCM_SETSHIELD, 0, 1);
	}

	return	TRUE;
}

BOOL TSetupSheet::EvNcDestroy(void)
{
	if (resId == URL_SHEET) {
		UrlObj *urlObj;

		while ((urlObj = tmpUrlList.TopObj())) {
			tmpUrlList.DelObj(urlObj);
			delete urlObj;
		}
		curUrl	= NULL;
	}
	return	TRUE;
}

#define MAX_LRUUSER 30

BOOL TSetupSheet::CheckData()
{
	char	buf[MAX_PATH_U8];
	int		val;

	if (resId == MAIN_SHEET) {
	}
	else if (resId == DETAIL_SHEET) {
		if (!IsWinVista() &&
			(GetDlgItemInt(OPENTIME_EDIT) > 30 || GetDlgItemInt(RECVTIME_EDIT) > 30)) {
			MessageBox(GetLoadStr(IDS_TOOLONG_BALLOON));
			return	FALSE;
		}
	}
	else if (resId == SENDRECV_SHEET) {
		if ((val = GetDlgItemInt(LRUUSER_EDIT)) > MAX_LRUUSER || val < 0) {
			MessageBox(Fmt(GetLoadStr(IDS_TOOMANYLRU), MAX_LRUUSER));
			return	FALSE;
		}
	}
	else if (resId == IMAGE_SHEET) {
	}
	else if (resId == LOG_SHEET) {
		GetDlgItemTextU8(LOG_EDIT, buf, sizeof(buf));
		if (GetDriveType(NULL) == DRIVE_REMOTE
				&& !IsDlgButtonChecked(LOG_CHECK) && strchr(buf, '\\')) {
			MessageBox(GetLoadStr(IDS_LOGALERT));
			return	FALSE;
		}
	}
	else if (resId == AUTOSAVE_SHEET) {
	}
	else if (resId == PASSWORD_SHEET) {
		char	buf1[MAX_NAMEBUF], buf2[MAX_NAMEBUF], buf3[MAX_NAMEBUF];
		GetDlgItemTextU8(OLDPASSWORD_EDIT,  buf1, sizeof(buf1));
		GetDlgItemTextU8(NEWPASSWORD_EDIT,  buf2, sizeof(buf2));
		GetDlgItemTextU8(NEWPASSWORD_EDIT2, buf3, sizeof(buf3));

		if (strcmp(buf2, buf3) != 0) {
			MessageBoxU8(GetLoadStrU8(IDS_NOTSAMEPASS));
			return	FALSE;
		}

		if (!CheckPassword(cfg->PasswordStr, buf1)) {
			if (*buf1 || *buf2 || *buf3) {
				SetDlgItemTextU8(PASSWORD_EDIT, "");
				MessageBoxU8(GetLoadStrU8(IDS_CANTAUTH));
				return	FALSE;
			}
		}
	}
	else if (resId == URL_SHEET) {
	}
	else if (resId == REMOTE_SHEET) {
	}
	else if (resId == BACKUP_SHEET) {
	}

	return	TRUE;
}

void TSetupSheet::ReflectDisp()
{
	static const char	*HotKeyOrg = NULL;
	char				buf[MAX_PATH_U8] = "";

	if (!HotKeyOrg) {
		GetDlgItemTextU8(HOTKEY_CHECK, buf, sizeof(buf));
		HotKeyOrg = strdup(buf);
	}
	if (HotKeyOrg) {
		sprintf(buf, "%s (", HotKeyOrg);
		if (cfg->HotKeyModify & MOD_CONTROL) strcat(buf, "Ctrl+");
		if (cfg->HotKeyModify & MOD_ALT)     strcat(buf, "Alt+");
		if (cfg->HotKeyModify & MOD_SHIFT)   strcat(buf, "Shift+");
		sprintf(buf + strlen(buf), "%c/%c)", cfg->HotKeySend, cfg->HotKeyRecv);
		SetDlgItemTextU8(HOTKEY_CHECK, buf);
	}
}

BOOL TSetupSheet::SetData()
{
	if (resId == MAIN_SHEET) {
		SetDlgItemTextU8(GROUP_COMBO, cfg->GroupNameStr);
		SetDlgItemTextU8(NICKNAME_EDIT, cfg->NickNameStr);

		for (TBrObj *obj=cfg->brList.TopObj(); obj; obj=cfg->brList.NextObj(obj))
			SendDlgItemMessage(BROADCAST_LIST, LB_ADDSTRING, 0, (LPARAM)obj->Host());

		for (int i=0; i < hosts->HostCnt(); i++) {
			Host	*host = hosts->GetHost(i);

			if (*host->groupName) {
				Wstr	group_w(host->groupName, BY_UTF8);
				if (SendDlgItemMessageW(GROUP_COMBO, CB_FINDSTRING,
						(WPARAM)-1, (LPARAM)group_w.Buf()) == CB_ERR) {
					SendDlgItemMessageW(GROUP_COMBO, CB_ADDSTRING,
							(WPARAM)0, (LPARAM)group_w.Buf());
				}
			}
		}
		CheckDlgButton(DIALUP_CHECK, cfg->DialUpCheck);

		::ShowWindow(GetDlgItem(BROADCAST_STATIC), cfg->IPv6ModeNext != 1 ? SW_SHOW : SW_HIDE);
		::ShowWindow(GetDlgItem(EXTBROADCAST_COMBO), cfg->IPv6ModeNext != 1 ? SW_SHOW : SW_HIDE);
		::ShowWindow(GetDlgItem(MULTICAST_STATIC), cfg->IPv6ModeNext ? SW_SHOW : SW_HIDE);
		::ShowWindow(GetDlgItem(MULTICAST_COMBO), cfg->IPv6ModeNext ? SW_SHOW : SW_HIDE);

		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_LIMITED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_DIRECTED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_BOTH));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_SETCURSEL, cfg->ExtendBroadcast, 0);

		SendDlgItemMessage(IPV6_COMBO, CB_ADDSTRING, 0, (LPARAM)"IPv4 mode");
		SendDlgItemMessage(IPV6_COMBO, CB_ADDSTRING, 0, (LPARAM)"IPv6 mode");
		SendDlgItemMessage(IPV6_COMBO, CB_ADDSTRING, 0, (LPARAM)"IPv4/IPv6");
		SendDlgItemMessage(IPV6_COMBO, CB_SETCURSEL, cfg->IPv6ModeNext, 0);

		SendDlgItemMessage(MULTICAST_COMBO, CB_ADDSTRING, 0, (LPARAM)"Site/LinkLocal dual");
		SendDlgItemMessage(MULTICAST_COMBO, CB_ADDSTRING, 0, (LPARAM)"LinkLocal only");
		SendDlgItemMessage(MULTICAST_COMBO, CB_ADDSTRING, 0, (LPARAM)"SiteLocal only");
		SendDlgItemMessage(MULTICAST_COMBO, CB_SETCURSEL, cfg->MulticastMode, 0);
	}
	else if (resId == DETAIL_SHEET) {
		CheckDlgButton(BALLOONNOTIFY_CHECK, cfg->BalloonNotify);

		SendDlgItemMessage(OPEN_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_OPENCHECK_NONE));
		SendDlgItemMessage(OPEN_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_OPENCHECK_BALLOON));
		SendDlgItemMessage(OPEN_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_OPENCHECK_DLG));
		SendDlgItemMessage(OPEN_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_OPENCHECK_ICON));
		SendDlgItemMessage(OPEN_COMBO, CB_SETCURSEL, cfg->OpenCheck, 0);
//		CheckDlgButton(OPEN_CHECK, cfg->OpenCheck);
		SetDlgItemInt(OPENTIME_EDIT, cfg->OpenMsgTime / 1000);
		SetDlgItemInt(RECVTIME_EDIT, cfg->RecvMsgTime / 1000);
		CheckDlgButton(BALLOONNOINFO_CHECK, cfg->BalloonNoInfo);

		SetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr);

		CheckDlgButton(HOTKEY_CHECK, cfg->HotKeyCheck);

		CheckDlgButton(CTRL_CHECK,  cfg->HotKeyModify & MOD_CONTROL);
		CheckDlgButton(ALT_CHECK,   cfg->HotKeyModify & MOD_ALT);
		CheckDlgButton(SHIFT_CHECK, cfg->HotKeyModify & MOD_SHIFT);

		for (char c[2]="A"; *c <= 'Z'; (*c)++) {
			SendDlgItemMessage(SEND_COMBO, CB_ADDSTRING, 0, (LPARAM)c);
			SendDlgItemMessage(RECV_COMBO, CB_ADDSTRING, 0, (LPARAM)c);
		}
		SendDlgItemMessage(SEND_COMBO, CB_SETCURSEL, cfg->HotKeySend - 'A', 0);
		SendDlgItemMessage(RECV_COMBO, CB_SETCURSEL, cfg->HotKeyRecv - 'A', 0);

		::EnableWindow(GetDlgItem(CTRL_CHECK),  cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(ALT_CHECK),   cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(SHIFT_CHECK), cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(CTRL_CHECK),  cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(SEND_COMBO),  cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(RECV_COMBO),  cfg->HotKeyCheck);

		ReflectDisp();

		CheckDlgButton(ABNORMALBTN_CHECK, cfg->AbnormalButton);

		if (cfg->lcid != -1 || GetSystemDefaultLCID() == 0x411) {
			::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
			::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
			CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid == 0x411 ? FALSE : TRUE);
		}

		SetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile);
		SetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile);
		CheckDlgButton(TRAYICON_CHECK, cfg->TrayIcon);
	}
	else if (resId == SENDRECV_SHEET) {
		CheckDlgButton(QUOTE_CHECK, cfg->QuoteCheck);
		CheckDlgButton(SECRET_CHECK, cfg->SecretCheck);
		CheckDlgButton(ONECLICK_CHECK, cfg->OneClickPopup);
		SetDlgItemTextU8(LRUUSER_EDIT, Fmt("%d", cfg->lruUserMax));
		// ControlIME ... 0:off, 1:senddlg on (finddlg:off), 2:always on
		CheckDlgButton(CONTROLIME_CHECK, cfg->ControlIME ? 1 : 0);
		CheckDlgButton(FINDDLGIME_CHECK, cfg->ControlIME >= 2 ? 0 : 1);

		CheckDlgButton(NOPOPUP_CHECK, cfg->NoPopupCheck);
		CheckDlgButton(ABSENCENONPOPUP_CHECK, cfg->AbsenceNonPopup);
		CheckDlgButton(NOBEEP_CHECK, cfg->NoBeep);
		CheckDlgButton(RECVLOGON_CHECK, cfg->RecvLogonDisp);
		CheckDlgButton(RECVIPADDR_CHECK, cfg->RecvIPAddr);
		CheckDlgButton(NOERASE_CHECK, cfg->NoErase);
		CheckDlgButton(REPROMSG_CHECK, cfg->ReproMsg);

		SetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile);
	}
	else if (resId == IMAGE_SHEET) {
//		CheckDlgButton(CLIPMODE_CHECK, (cfg->ClipMode & CLIP_ENABLE));

		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_CLIPALWAYS));
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_CLIPNORMAL));
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_CLIPSTRICT));

		int idx =   (cfg->ClipMode & CLIP_CONFIRM_STRICT) ? 2 :
					(cfg->ClipMode & CLIP_CONFIRM_NORMAL) ? 1 : 0;
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_SETCURSEL, idx, 0);

		CheckDlgButton(MINIMIZE_CHECK, cfg->CaptureMinimize);
		CheckDlgButton(CLIPBORAD_CHECK, cfg->CaptureClip);
		CheckDlgButton(SAVE_CHECK, cfg->CaptureSave);
		SetDlgItemInt(DELAY_EDIT, cfg->CaptureDelay);
		SetDlgItemInt(DELAYEX_EDIT, cfg->CaptureDelayEx);
	}
	else if (resId == LOG_SHEET) {
		CheckDlgButton(LOG_CHECK, cfg->LogCheck);
		CheckDlgButton(LOGONLOG_CHECK, cfg->LogonLog);
		CheckDlgButton(IPADDR_CHECK, cfg->IPAddrCheck);
		CheckDlgButton(IMAGESAVE_CHECK, (cfg->ClipMode & 2) ? 1 : 0);
		CheckDlgButton(LOGUTF8_CHECK, cfg->LogUTF8);
		CheckDlgButton(PASSWDLOG_CHECK, cfg->PasswdLogCheck);
		SetDlgItemTextU8(LOG_EDIT, cfg->LogFile);
	}
	else if (resId == AUTOSAVE_SHEET) {
		CheckDlgButton(AUTOSAVE_CHECK, !!(cfg->autoSaveFlags & AUTOSAVE_ENABLED));
		SetDlgItemTextU8(DIR_EDIT, cfg->autoSaveDir);
		SetDlgItemInt(TOUT_EDIT, cfg->autoSaveTout);

		SendDlgItemMessage(PRIORITY_COMBO, CB_ADDSTRING, 0,
			(LPARAM)GetLoadStr(IDS_DISPPRI_NOLIMIT));
		for (int i=1; i < cfg->PriorityMax; i++) {
			char	buf[MAX_BUF], *fmt = GetLoadStr(IDS_DISPPRI_LIMIT);
			sprintf(buf, fmt, i);
			SendDlgItemMessage(PRIORITY_COMBO, CB_ADDSTRING, 0, (LPARAM)buf);
		}
		SendDlgItemMessage(PRIORITY_COMBO, CB_SETCURSEL, cfg->autoSaveLevel, 0);

		SetDlgItemInt(PRIORITY_COMBO, cfg->autoSaveLevel);
		SetDlgItemInt(SIZE_EDIT, cfg->autoSaveMax);
		CheckDlgButton(DIR_CHECK, !!(cfg->autoSaveFlags & AUTOSAVE_INCDIR));
	}
	else if (resId == PASSWORD_SHEET) {
		if (*cfg->PasswordStr == 0)
			::EnableWindow(GetDlgItem(OLDPASSWORD_EDIT), FALSE);
	}
	else if (resId == URL_SHEET) {
		CheckDlgButton(DEFAULTURL_CHECK, cfg->DefaultUrl);

		for (UrlObj *obj = cfg->urlList.TopObj(); obj; obj = cfg->urlList.NextObj(obj)) {
			UrlObj *tmp_obj = new UrlObj;
			strcpy(tmp_obj->protocol, obj->protocol);
			strcpy(tmp_obj->program, obj->program);
			tmpUrlList.AddObj(tmp_obj);
			SendDlgItemMessage(URL_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)obj->protocol);
		}
		if ((curUrl = tmpUrlList.TopObj())) {
			SetDlgItemTextU8(URL_EDIT, curUrl->program);
		}
		CheckDlgButton(SHELLEXEC_CHECK, cfg->ShellExec);
		SendDlgItemMessage(URL_LIST, LB_SETCURSEL, 0, 0);
		PostMessage(WM_COMMAND, URL_LIST, 0);
	}
	else if (resId == REMOTE_SHEET) {
		char	buf[MAX_BUF];

		if (!*cfg->RemoteReboot)   GenRemoteKey(cfg->RemoteReboot);
		if (!*cfg->RemoteExit) GenRemoteKey(cfg->RemoteExit);

		::EnableWindow(GetDlgItem(REBOOT_EDIT), cfg->RemoteRebootMode ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(REBOOT_BUTTON), cfg->RemoteRebootMode ? TRUE : FALSE);
		CheckDlgButton(REBOOT_CHECK, cfg->RemoteRebootMode ? TRUE : FALSE);
		sprintf(buf, REMOTE_FMT, cfg->RemoteReboot);
		SetDlgItemTextU8(REBOOT_EDIT, buf);

		::EnableWindow(GetDlgItem(EXIT_EDIT), cfg->RemoteExitMode ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(EXIT_BUTTON), cfg->RemoteExitMode ? TRUE : FALSE);
		CheckDlgButton(EXIT_CHECK, cfg->RemoteExitMode ? TRUE : FALSE);
		sprintf(buf, REMOTE_FMT, cfg->RemoteExit);
		SetDlgItemTextU8(EXIT_EDIT, buf);
	}
	return	TRUE;
}

BOOL TSetupSheet::GetData()
{
	char	buf[MAX_PATH_U8];
	int		i;

	if (resId == MAIN_SHEET) {
		BOOL	need_broadcast = FALSE;

		GetDlgItemTextU8(NICKNAME_EDIT, buf, MAX_NAMEBUF);
		if (strcmp(cfg->NickNameStr, buf)) {
			need_broadcast = TRUE;
			strcpy(cfg->NickNameStr, buf);
		}
		GetDlgItemTextU8(GROUP_COMBO, buf, MAX_NAMEBUF);
		if (strcmp(cfg->GroupNameStr, buf)) {
			need_broadcast = TRUE;
			strcpy(cfg->GroupNameStr, buf);
		}
		if (need_broadcast) {
			::PostMessage(GetMainWnd(), WM_IPMSG_BRNOTIFY, 0, IPMSG_DEFAULT_PORT);
		}

		cfg->brList.Reset();

		for (i=0; SendDlgItemMessage(BROADCAST_LIST, LB_GETTEXT, i, (LPARAM)buf) != LB_ERR; i++)
			cfg->brList.SetHostRaw(buf, ResolveAddr(buf));

		cfg->DialUpCheck = IsDlgButtonChecked(DIALUP_CHECK);

		cfg->ExtendBroadcast = (int)SendDlgItemMessage(EXTBROADCAST_COMBO, CB_GETCURSEL, 0, 0);
		cfg->IPv6ModeNext = (int)SendDlgItemMessage(IPV6_COMBO, CB_GETCURSEL, 0, 0);

		if (cfg->IPv6ModeNext) {
			LRESULT	mode = SendDlgItemMessage(MULTICAST_COMBO, CB_GETCURSEL, 0, 0);
			if (cfg->IPv6Mode) {
				if (cfg->MulticastMode != mode) {
					::SendMessage(GetMainWnd(), WM_IPMSG_CHANGE_MCAST, mode, 0);
				}
			} else {
				cfg->MulticastMode = (int)mode;
			}
		}
	}
	else if (resId == DETAIL_SHEET) {
		cfg->BalloonNotify = IsDlgButtonChecked(BALLOONNOTIFY_CHECK);
		cfg->OpenCheck = (int)SendDlgItemMessage(OPEN_COMBO, CB_GETCURSEL, 0, 0);
//		cfg->OpenCheck = IsDlgButtonChecked(OPEN_CHECK);
		cfg->OpenMsgTime = GetDlgItemInt(OPENTIME_EDIT) * 1000;
		cfg->RecvMsgTime = GetDlgItemInt(RECVTIME_EDIT) * 1000;
		cfg->BalloonNoInfo = IsDlgButtonChecked(BALLOONNOINFO_CHECK);

		GetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr, sizeof(cfg->QuoteStr));

		cfg->HotKeyCheck = IsDlgButtonChecked(HOTKEY_CHECK) ?
								(cfg->HotKeyCheck ? cfg->HotKeyCheck : 1) : 0;
		if (cfg->HotKeyCheck) {
			cfg->HotKeyModify = 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(CTRL_CHECK)  ? MOD_CONTROL : 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(ALT_CHECK)   ? MOD_ALT     : 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(SHIFT_CHECK) ? MOD_SHIFT   : 0;
			cfg->HotKeySend = (int)SendDlgItemMessage(SEND_COMBO, CB_GETCURSEL, 0, 0) + 'A';
			cfg->HotKeyRecv = (int)SendDlgItemMessage(RECV_COMBO, CB_GETCURSEL, 0, 0) + 'A';
		}

		cfg->AbnormalButton = IsDlgButtonChecked(ABNORMALBTN_CHECK);
		if (::IsWindowEnabled(GetDlgItem(LCID_CHECK))) {
			cfg->lcid = IsDlgButtonChecked(LCID_CHECK) ? 0x409 : -1;
		}

		GetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile, sizeof(cfg->IconFile));
		GetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile, sizeof(cfg->RevIconFile));
		cfg->TrayIcon = IsDlgButtonChecked(TRAYICON_CHECK);

		::SendMessage(GetMainWnd(), WM_IPMSG_INITICON, 0, 0);
		SetDlgIcon(hWnd);
		ReflectDisp();
		SetHotKey(cfg);
	}
	else if (resId == SENDRECV_SHEET) {
		cfg->QuoteCheck = IsDlgButtonChecked(QUOTE_CHECK);
		cfg->SecretCheck = IsDlgButtonChecked(SECRET_CHECK);
		cfg->OneClickPopup = IsDlgButtonChecked(ONECLICK_CHECK);
		cfg->lruUserMax = GetDlgItemInt(LRUUSER_EDIT);
	// ControlIME ... 0:off, 1:senddlg on (finddlg:off), 2:always on
		cfg->ControlIME = IsDlgButtonChecked(CONTROLIME_CHECK);
		if (cfg->ControlIME && !IsDlgButtonChecked(FINDDLGIME_CHECK)) {
			cfg->ControlIME = 2;
		}
		if (cfg->NoPopupCheck != 2) cfg->NoPopupCheck = IsDlgButtonChecked(NOPOPUP_CHECK);
		cfg->NoBeep = IsDlgButtonChecked(NOBEEP_CHECK);
		cfg->AbsenceNonPopup = IsDlgButtonChecked(ABSENCENONPOPUP_CHECK);
		cfg->RecvLogonDisp = IsDlgButtonChecked(RECVLOGON_CHECK);
		cfg->RecvIPAddr = IsDlgButtonChecked(RECVIPADDR_CHECK);
		cfg->NoErase = IsDlgButtonChecked(NOERASE_CHECK);
		cfg->ReproMsg = IsDlgButtonChecked(REPROMSG_CHECK);

		GetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile, sizeof(cfg->SoundFile));
	}
	else if (resId == IMAGE_SHEET) {
//		if (IsDlgButtonChecked(CLIPMODE_CHECK)) {
//			cfg->ClipMode |=  CLIP_ENABLE;
//		} else {
//			cfg->ClipMode &= ~CLIP_ENABLE;
//		}
		cfg->ClipMode &= ~CLIP_CONFIRM_ALL;
		switch ((int)SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_GETCURSEL, 0, 0)) {
		case 1: cfg->ClipMode |= CLIP_CONFIRM_NORMAL; break;
		case 2: cfg->ClipMode |= CLIP_CONFIRM_STRICT; break;
		}
		cfg->CaptureMinimize = IsDlgButtonChecked(MINIMIZE_CHECK);
		cfg->CaptureClip = IsDlgButtonChecked(CLIPBORAD_CHECK);
		cfg->CaptureSave = IsDlgButtonChecked(SAVE_CHECK);
		cfg->CaptureDelay = GetDlgItemInt(DELAY_EDIT);
		cfg->CaptureDelayEx = GetDlgItemInt(DELAYEX_EDIT);
	}
	else if (resId == LOG_SHEET) {
		cfg->LogCheck = IsDlgButtonChecked(LOG_CHECK);
		cfg->LogonLog = IsDlgButtonChecked(LOGONLOG_CHECK);
		cfg->IPAddrCheck = IsDlgButtonChecked(IPADDR_CHECK);

		if (IsDlgButtonChecked(IMAGESAVE_CHECK)) {
			cfg->ClipMode |=  2;
		} else {
			cfg->ClipMode &= ~2;
		}
		cfg->LogUTF8 = IsDlgButtonChecked(LOGUTF8_CHECK);
		cfg->PasswdLogCheck = IsDlgButtonChecked(PASSWDLOG_CHECK);

		GetDlgItemTextU8(LOG_EDIT, cfg->LogFile, sizeof(cfg->LogFile));
		if (cfg->LogCheck) LogMng::StrictLogFile(cfg->LogFile);
	}
	else if (resId == AUTOSAVE_SHEET) {
		if (IsDlgButtonChecked(AUTOSAVE_CHECK))	cfg->autoSaveFlags |= AUTOSAVE_ENABLED;
		else									cfg->autoSaveFlags &= ~AUTOSAVE_ENABLED;
		GetDlgItemTextU8(DIR_EDIT, cfg->autoSaveDir, sizeof(cfg->autoSaveDir));
		cfg->autoSaveTout = GetDlgItemInt(TOUT_EDIT);
		cfg->autoSaveLevel = (int)SendDlgItemMessage(PRIORITY_COMBO, CB_GETCURSEL, 0, 0);
		cfg->autoSaveMax = GetDlgItemInt(SIZE_EDIT);
		if (IsDlgButtonChecked(DIR_CHECK))	cfg->autoSaveFlags |= AUTOSAVE_INCDIR;
		else								cfg->autoSaveFlags &= ~AUTOSAVE_INCDIR;
	}
	else if (resId == PASSWORD_SHEET) {
		char	buf[MAX_NAMEBUF];
		GetDlgItemTextU8(OLDPASSWORD_EDIT, buf, sizeof(buf));
		if (CheckPassword(cfg->PasswordStr, buf)) {
			GetDlgItemTextU8(NEWPASSWORD_EDIT, buf, sizeof(buf));
			MakePassword(buf, cfg->PasswordStr);
		}
	}
	else if (resId == URL_SHEET) {
		cfg->DefaultUrl = IsDlgButtonChecked(DEFAULTURL_CHECK);
		if (curUrl) GetDlgItemTextU8(URL_EDIT, curUrl->program, sizeof(curUrl->program));

		for (UrlObj *tmp_obj = tmpUrlList.TopObj(); tmp_obj; tmp_obj = tmpUrlList.NextObj(tmp_obj)) {
			UrlObj *obj = SearchUrlObj(&cfg->urlList, tmp_obj->protocol);

			if (!obj) {
				obj = new UrlObj;
				cfg->urlList.AddObj(obj);
				strcpy(obj->protocol, tmp_obj->protocol);
			}
			strcpy(obj->program, tmp_obj->program);
		}
		cfg->ShellExec = IsDlgButtonChecked(SHELLEXEC_CHECK);
	}
	else if (resId == REMOTE_SHEET) {
		char	buf[MAX_BUF];

		if (GetDlgItemTextU8(REBOOT_EDIT, buf, sizeof(buf)) > REMOTE_HEADERLEN) {
			strcpy(cfg->RemoteReboot, buf + REMOTE_HEADERLEN);
		}
		cfg->RemoteRebootMode = IsDlgButtonChecked(REBOOT_CHECK);

		if (GetDlgItemTextU8(EXIT_EDIT, buf, sizeof(buf)) > REMOTE_HEADERLEN) {
			strcpy(cfg->RemoteExit, buf + REMOTE_HEADERLEN);
		}
		cfg->RemoteExitMode = IsDlgButtonChecked(EXIT_CHECK);
	}

	return	TRUE;
}

BOOL TSetupSheet::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	char	buf[MAX_PATH_U8] = "", buf2[MAX_PATH_U8] = "", protocol[MAX_LISTBUF] = "";
	int		i = 0, ret = 0;
	UrlObj	*obj;

	if (resId == MAIN_SHEET) {
		switch (wID) {
		case ADD_BUTTON:
			if (GetDlgItemText(BROADCAST_EDIT, buf, sizeof(buf)) <= 0) return TRUE;
			if (!ResolveAddr(buf).IsEnabled()) {
				return MessageBox(GetLoadStr(IDS_CANTRESOLVE)), TRUE;
			}
			for (i=0;
				SendDlgItemMessage(BROADCAST_LIST, LB_GETTEXT, i, (LPARAM)buf2) != LB_ERR; i++) {
				if (stricmp(buf, buf2) == 0) return	TRUE;
			}
			SendDlgItemMessage(BROADCAST_LIST, LB_ADDSTRING, 0, (LPARAM)buf);
			SetDlgItemText(BROADCAST_EDIT, "");
			return	TRUE;

		case DEL_BUTTON:
			while ((int)SendDlgItemMessage(BROADCAST_LIST, LB_GETSELCOUNT, 0, 0) > 0)
			{
				if (SendDlgItemMessage(BROADCAST_LIST, LB_GETSELITEMS, 1, (LPARAM)&i) != 1)
					break;
				SendDlgItemMessage(BROADCAST_LIST, LB_GETTEXT, (WPARAM)i, (LPARAM)buf);
				SetDlgItemText(BROADCAST_EDIT, buf);
				if (SendDlgItemMessage(BROADCAST_LIST, LB_DELETESTRING,
										(WPARAM)i, (LPARAM)buf) == LB_ERR) break;
			}
			return	TRUE;

		case IPV6_COMBO:
			{
				LRESULT ipv6_mode = SendDlgItemMessage(IPV6_COMBO, CB_GETCURSEL, 0, 0);
				::ShowWindow(GetDlgItem(BROADCAST_STATIC),     ipv6_mode != 1 ? SW_SHOW : SW_HIDE);
				::ShowWindow(GetDlgItem(EXTBROADCAST_COMBO),   ipv6_mode != 1 ? SW_SHOW : SW_HIDE);
				::ShowWindow(GetDlgItem(MULTICAST_STATIC),     ipv6_mode ? SW_SHOW : SW_HIDE);
				::ShowWindow(GetDlgItem(MULTICAST_COMBO),      ipv6_mode ? SW_SHOW : SW_HIDE);
			}
			return	TRUE;
		}
	}
	else if (resId == DETAIL_SHEET) {
		switch (wID) {
		case MAINICON_BUTTON: case REVICON_BUTTON:
			OpenFileDlg(this).Exec(wID == MAINICON_BUTTON ? MAINICON_EDIT : REVICON_EDIT,
					GetLoadStrU8(IDS_OPENFILEICON), GetLoadStrAsFilterU8(IDS_OPENFILEICONFLTR));
			return	TRUE;

		case HOTKEY_CHECK:
			ret = IsDlgButtonChecked(HOTKEY_CHECK);
			::EnableWindow(GetDlgItem(CTRL_CHECK),  ret);
			::EnableWindow(GetDlgItem(ALT_CHECK),   ret);
			::EnableWindow(GetDlgItem(SHIFT_CHECK), ret);
			::EnableWindow(GetDlgItem(CTRL_CHECK),  ret);
			::EnableWindow(GetDlgItem(SEND_COMBO),  ret);
			::EnableWindow(GetDlgItem(RECV_COMBO),  ret);
			return	TRUE;
		}
	}
	else if (resId == SENDRECV_SHEET) {
		switch (wID) {
		case SENDDETAIL_BUTTON:
			{
				TSortDlg(cfg, this).Exec();
				return	TRUE;
			}

		case SOUNDFILE_BUTTON:
			GetWindowsDirectoryU8(buf, sizeof(buf));
			strcat(buf, "\\Media");
			GetCurrentDirectoryU8(sizeof(buf2), buf2);
			OpenFileDlg(this).Exec(SOUND_EDIT, GetLoadStrU8(IDS_OPENFILESND),
									GetLoadStrAsFilterU8(IDS_OPENFILESNDFLTR), buf);
			SetCurrentDirectoryU8(buf2);
			return	TRUE;

		case SOUNDPLAY_BUTTON:
			if (GetDlgItemTextU8(SOUND_EDIT, buf, sizeof(buf)) > 0) {
				PlaySoundU8(buf, 0, SND_FILENAME|SND_ASYNC);
			}
			return	TRUE;
		}
	}
	else if (resId == IMAGE_SHEET) {
	}
	else if (resId == LOG_SHEET) {
		switch (wID) {
		case LOGFILE_BUTTON:
			OpenFileDlg(this, OpenFileDlg::SAVE).Exec(LOG_EDIT,
				GetLoadStrU8(IDS_OPENFILELOG), GetLoadStrAsFilterU8(IDS_OPENFILELOGFLTR));
			return	TRUE;
		}
	}
	else if (resId == AUTOSAVE_SHEET) {
		char buf[MAX_PATH];
		switch (wID) {
		case DIR_BUTTON:
			MakeAutoSaveDir(cfg, buf);
			CreateDirectoryU8(buf, 0);
			if (BrowseDirDlg(this, GetLoadStrU8(IDS_FOLDERATTACH), buf, buf)) {
				SetDlgItemTextU8(DIR_EDIT, buf);
			}
			return	TRUE;
		case OPEN_BUTTON:
			MakeAutoSaveDir(cfg, buf);
			CreateDirectoryU8(buf, 0);
			ShellExecuteU8(NULL, NULL, buf, 0, 0, SW_SHOW);
			return	TRUE;
		}
	}
	else if (resId == PASSWORD_SHEET) {
	}
	else if (resId == URL_SHEET) {
		switch (wID) {
		case ADD_BUTTON:
			if ((i = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR &&
				SendDlgItemMessage(URL_LIST, LB_GETTEXT, (WPARAM)i, (LPARAM)protocol) != LB_ERR &&
				(obj = SearchUrlObj(&tmpUrlList, protocol))) {
				wsprintf(buf, GetLoadStrU8(IDS_EXECPROGRAM), protocol);
				OpenFileDlg(this).Exec(URL_EDIT, buf, GetLoadStrAsFilterU8(IDS_OPENFILEPRGFLTR));
			}
			return	TRUE;

		case URL_LIST:
			if ((i = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR) {
				if (curUrl) {
					GetDlgItemTextU8(URL_EDIT, curUrl->program, sizeof(curUrl->program));
					curUrl = NULL;
				}
				if (SendDlgItemMessage(URL_LIST, LB_GETTEXT, i, (LPARAM)protocol) != LB_ERR &&
					(obj = SearchUrlObj(&tmpUrlList, protocol))) {
					curUrl = obj;
					SetDlgItemTextU8(URL_EDIT, obj->program);
				}
				
			}
			return	TRUE;
		}
	}
	else if (resId == REMOTE_SHEET) {
		char	buf[MAX_BUF], buf2[MAX_BUF];
		int		ret;

		switch (wID) {
		case REBOOT_CHECK:
			ret = IsDlgButtonChecked(REBOOT_CHECK);
			::EnableWindow(GetDlgItem(REBOOT_EDIT), ret);
			::EnableWindow(GetDlgItem(REBOOT_BUTTON), ret);
			break;

		case EXIT_CHECK:
			ret = IsDlgButtonChecked(EXIT_CHECK);
			::EnableWindow(GetDlgItem(EXIT_EDIT), ret);
			::EnableWindow(GetDlgItem(EXIT_BUTTON), ret);
			break;

		case REBOOT_BUTTON: case EXIT_BUTTON:
			GenRemoteKey(buf);
			sprintf(buf2, REMOTE_FMT, buf);
			SetDlgItemTextU8(wID == REBOOT_BUTTON ? REBOOT_EDIT : EXIT_EDIT, buf2);
			break;

		case REBOOT_EDIT: case EXIT_EDIT:
			SendDlgItemMessage(wID, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
			break;
		}
	}
	else if (resId == BACKUP_SHEET) {
		switch (wID) {
		case SAVE_BUTTON:
			{
				char	path[MAX_PATH_U8] = "", regName[MAX_PATH_U8] = "";

				OpenFileDlg	dlg(this, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);
				if (dlg.Exec(path, sizeof(path), NULL,"Reg file(*.reg)\0*.reg\0\0", NULL, "reg")) {
					cfg->GetSelfRegName(regName);
					sprintf(buf, "/E \"%s\" HKEY_CURRENT_USER\\Software\\HSTools\\%s", path, regName);
					ShellExecuteU8(hWnd, "runas", "regedit.exe", buf, 0, SW_SHOW);
				}
			}
			return	TRUE;
		}
	}

	return	FALSE;
}

/*
	Setup Dialog初期化処理
*/
TSetupDlg::TSetupDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent) : TDlg(SETUP_DIALOG, _parent)
{
	cfg		= _cfg;
	hosts	= _hosts;
	curIdx	= -1;
}

/*
	Window 生成時の CallBack
*/
BOOL TSetupDlg::EvCreate(LPARAM lParam)
{
	setupList.AttachWnd(GetDlgItem(SETUP_LIST));

	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Create(SETUP_SHEET1 + i, cfg, hosts, this);
		setupList.SendMessage(LB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_SETUP_SHEET1 + i));
	}
	SetSheet();

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN);
		int	cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x, (y < 0) ? 0 : y, xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	return	TRUE;
}

BOOL TSetupDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case APPLY_BUTTON:
		{
			if (!sheet[curIdx].CheckData()) {
				return TRUE;
			}
			for (int i=0; i < MAX_SETUP_SHEET; i++) {
				sheet[i].GetData();
			}
			cfg->WriteRegistry(CFG_GENERAL|CFG_BROADCAST|CFG_CLICKURL);
			if (wID == IDOK) {
				EndDialog(wID);
			}
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case SETUP_LIST:
		if (wNotifyCode == LBN_SELCHANGE) SetSheet();
		return	TRUE;
	}
	return	FALSE;
}

void TSetupDlg::SetSheet(int idx)
{
	if (curIdx >= 0 && curIdx == idx) return;

	if (idx < 0) {
		if ((idx = (int)setupList.SendMessage(LB_GETCURSEL, 0, 0)) < 0) {
			idx = 0;
			setupList.SendMessage(LB_SETCURSEL, idx, 0);
		}
	}
	if (curIdx >= 0 && curIdx != idx) {
		if (!sheet[curIdx].CheckData()) {
			setupList.SendMessage(LB_SETCURSEL, curIdx, 0);
			return;
		}
	}
	for (int i=0; i < MAX_SETUP_SHEET; i++) {
		sheet[i].Show(i == idx ? SW_SHOW : SW_HIDE);
	}
	curIdx = idx;
}


