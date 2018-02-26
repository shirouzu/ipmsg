static char *setupdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   setupdlg.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Setup Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "plugin.h"

#ifdef IPMSG_PRO
#include "master.dat"
#endif

/*
	Setup用Sheet
*/
BOOL TSetupSheet::Create(int _resId, Cfg *_cfg, THosts *_hosts, TSetupDlg *_parent)
{
	cfg			= _cfg;
	hosts		= _hosts;
	resId		= _resId;
	parent		= _parent;
	parentDlg	= _parent;
//	curUrl		= NULL;

#ifndef IPMSG_PRO
	updateReady	= FALSE;
#endif

	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);

	return	TDlg::Create();
}

BOOL TSetupSheet::EvCreate(LPARAM lParam)
{
	if (resId == REMOTE_SHEET) {
		SendDlgItemMessage(REBOOT_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);
		SendDlgItemMessage(EXIT_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);
		SendDlgItemMessage(IPMSGEXIT_EDIT, EM_SETWORDBREAKPROC, 0, (LPARAM)EditNoWordBreakProc);
	}
	else if (resId == LABTEST_SHEET) {
		int hook_ids[]  = { HOOKURL_STATIC,   HOOKURL_EDIT,   HOOKBODY_STATIC, HOOKBODY_EDIT };
		int slack_ids[] = { SLACKCHAN_STATIC, SLACKCHAN_EDIT, SLACKKEY_STATIC, SLACKKEY_EDIT };

		for (auto &&id: hook_ids) {
			HWND	hw = GetDlgItem(id);
			size_t	diff = &id - &hook_ids[0];
			Debug("diff=%zd id=%d\n", diff, slack_ids[diff]);
			HWND	sw = GetDlgItem(slack_ids[&id - &hook_ids[0]]);
			TRect	hr;
			TRect	sr;
			::GetWindowRect(hw, &hr);
			::GetWindowRect(sw, &sr);
			hr.left = sr.left;
			sr.right = hr.right;
			hr.ScreenToClient(hWnd);
			sr.ScreenToClient(hWnd);
			::MoveWindow(hw, hr.left, hr.top, hr.cx(), hr.cy(), TRUE);
			::MoveWindow(sw, sr.left, sr.top, sr.cx(), sr.cy(), TRUE);
		}
	}

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

	if (resId == BACKUP_SHEET) {
		if (IsWinVista() && TIsEnableUAC()) {
			::SendMessage(GetDlgItem(SAVE_BUTTON), BCM_SETSHIELD, 0, 1);
		}
	}

	return	TRUE;
}

BOOL TSetupSheet::EvNcDestroy(void)
{
//	if (resId == URL_SHEET) {
//		UrlObj *urlObj;
//
//		while ((urlObj = tmpUrlList.TopObj())) {
//			tmpUrlList.DelObj(urlObj);
//			delete urlObj;
//		}
//		curUrl	= NULL;
//	}
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
			MessageBox(LoadStr(IDS_TOOLONG_BALLOON));
			return	FALSE;
		}
	}
	else if (resId == SENDRECV_SHEET) {
		if ((val = GetDlgItemInt(LRUUSER_EDIT)) > MAX_LRUUSER || val < 0) {
			MessageBox(Fmt(LoadStr(IDS_TOOMANYLRU), MAX_LRUUSER));
			return	FALSE;
		}
	}
	else if (resId == IMAGE_SHEET) {
	}
	else if (resId == LOG_SHEET) {
		GetDlgItemTextU8(LOG_EDIT, buf, sizeof(buf));
		if (GetDriveType(NULL) == DRIVE_REMOTE
				&& !IsDlgButtonChecked(LOG_CHECK) && strchr(buf, '\\')) {
			MessageBox(LoadStr(IDS_LOGALERT));
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
			MessageBoxU8(LoadStrU8(IDS_NOTSAMEPASS));
			return	FALSE;
		}

		if (!CheckPassword(cfg->PasswordStr, buf1)) {
			if (*buf1 || *buf2 || *buf3) {
				SetDlgItemTextU8(PASSWORD_EDIT, "");
				MessageBoxU8(LoadStrU8(IDS_CANTAUTH));
				return	FALSE;
			}
		}
	}
//	else if (resId == URL_SHEET) {
//	}
	else if (resId == REMOTE_SHEET) {
	}
	else if (resId == LINK_SHEET) {
	}
	else if (resId == BACKUP_SHEET) {
	}
	else if (resId == LABTEST_SHEET) {
	}
#ifdef IPMSG_PRO
	else if (resId == SERVER_SHEET) {
	}
#else
	else if (resId == UPDATE_SHEET) {
	}
	else if (resId == DIRMODE_SHEET) {
		if (cfg->IPv6Mode != 1 && cfg->IPDictEnabled()) {
			if (IsDlgButtonChecked(DIR_NONE_RADIO)) {
			}
			else if (IsDlgButtonChecked(DIR_USER_RADIO)) {
				GetDlgItemTextU8(MASTER_EDIT, buf, sizeof(buf));
				if (*buf) {
					Addr	addr;
					addr.Set(buf);
					if ((!addr.IsEnabled() && !addr.Resolve(buf)) || !addr.IsIPv4()) {
						MessageBoxU8(LoadStrU8(IDS_CANTRESOLVE));
						return	FALSE;
					}
					else if (auto mainWin = GetMain()) {
						auto	allAddrs = mainWin->GetAllSelfAddrs();
						auto	itr = find_if(allAddrs.begin(), allAddrs.end(), [&](auto &ad) {
							return	ad == addr;
						});
						if (itr != allAddrs.end()) {
							MessageBoxU8(LoadStrU8(IDS_SELMASTERERR));
							return	FALSE;
						}
					}
				}
				else {
					MessageBoxU8(LoadStrU8(IDS_NEEDMASTERADDR));
					return	FALSE;
				}
			}
			else if (IsDlgButtonChecked(DIR_MASTER_RADIO)) {
				int	val = GetDlgItemInt(DIRSPAN_EDIT);
				if (val <= 0 || val > 30000) {
					MessageBoxU8(LoadStrU8(IDS_INVALIDVAL));
					return	FALSE;
				}
			}
		}

//		val = GetDlgItemInt(POLL_EDIT);
//		if (val != 0 && val < 5) {
//			MessageBoxU8(LoadStrU8(IDS_TOOSMALL));
//			return	FALSE;
//		}
	}
#endif
	return	TRUE;
}

void TSetupSheet::ShowHelp()
{
	const char	*section = "#settings";

	if (resId == MAIN_SHEET) {
		section = "#lan_settings";
	}
	else if (resId == DETAIL_SHEET) {
		section = "#detail_settings";
	}
	else if (resId == SENDRECV_SHEET) {
		section = "#sendrecv_settings";
	}
	else if (resId == IMAGE_SHEET) {
		section = "#capture_settings";
	}
	else if (resId == LOG_SHEET) {
		section = "#log_settings";
	}
	else if (resId == AUTOSAVE_SHEET) {
		section = "#auto_save";
	}
	else if (resId == PASSWORD_SHEET) {
	}
//	else if (resId == URL_SHEET) {
//	}
	else if (resId == REMOTE_SHEET) {
		section = "#remote_settings";
	}
	else if (resId == LINK_SHEET) {
		section = "#url_settings";
	}
	else if (resId == BACKUP_SHEET) {
		section = "#backup_settings";
	}
	else if (resId == LABTEST_SHEET) {
		section = "#labtest_settings";
	}
#ifdef IPMSG_PRO
	else if (resId == SERVER_SHEET) {
	}
#else
	else if (resId == UPDATE_SHEET) {
		section = "#update";
	}
	else if (resId == DIRMODE_SHEET) {
		section = "#master";
	}
#endif

	::PostMessage(GetMainWnd(), WM_IPMSG_HELP, 0, (LPARAM)"#settings"); // for open index
	::PostMessage(GetMainWnd(), WM_IPMSG_HELP, 0, (LPARAM)section);
}

void TSetupSheet::ReflectDisp()
{
	if (resId == DETAIL_SHEET) {
		static const char	*HotKeyOrg = NULL;
		char				buf[MAX_PATH_U8] = "";

		if (!HotKeyOrg) {
			GetDlgItemTextU8(HOTKEY_CHECK, buf, sizeof(buf));
			HotKeyOrg = strdup(buf);
		}
		if (HotKeyOrg) {
			sprintf(buf, "%s  (", HotKeyOrg);
			if (cfg->HotKeyModify & MOD_CONTROL) strcat(buf, "Ctrl+");
			if (cfg->HotKeyModify & MOD_ALT)     strcat(buf, "Alt+");
			if (cfg->HotKeyModify & MOD_SHIFT)   strcat(buf, "Shift+");
			sprintf(buf + strlen(buf), "%c/%c/%c)",
				cfg->HotKeySend, cfg->HotKeyRecv, cfg->HotKeyLogView);
			SetDlgItemTextU8(HOTKEY_CHECK, buf);
		}
	}
	else if (resId == LABTEST_SHEET) {
		int hook_ids[]  = { HOOKURL_STATIC,   HOOKURL_EDIT,   HOOKBODY_STATIC, HOOKBODY_EDIT };
		int slack_ids[] = { SLACKCHAN_STATIC, SLACKCHAN_EDIT, SLACKKEY_STATIC, SLACKKEY_EDIT };

		int hs = IsDlgButtonChecked(HOOKGENERAL_RADIO) ? SW_SHOW : SW_HIDE;
		for (auto &id: hook_ids) {
			::ShowWindow(GetDlgItem(id), hs);
		}
		int ss = IsDlgButtonChecked(HOOKSLACK_RADIO) ? SW_SHOW : SW_HIDE;
		for (auto &id: slack_ids) {
			::ShowWindow(GetDlgItem(id), ss);
		}
	}
}

BOOL TSetupSheet::SetData()
{
	if (resId == MAIN_SHEET) {
		SetDlgItemTextU8(GROUP_COMBO, cfg->GroupNameStr);
		SetDlgItemTextU8(NICKNAME_EDIT, cfg->NickNameStr);

		for (auto obj=cfg->brList.TopObj(); obj; obj=cfg->brList.NextObj(obj))
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

//		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_LIMITED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_DIRECTED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_BOTH));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_SETCURSEL, cfg->ExtendBroadcast -1, 0);

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
		SetDlgItemInt(OPENTIME_EDIT, cfg->OpenCheck     ? cfg->OpenMsgTime / 1000 : 0);
		SetDlgItemInt(RECVTIME_EDIT, cfg->BalloonNotify ? cfg->RecvMsgTime / 1000 : 0);
		CheckDlgButton(BALLOONNOINFO_CHECK, cfg->BalloonNoInfo);

		CheckDlgButton(HOTKEY_CHECK, cfg->HotKeyCheck);

		CheckDlgButton(CTRL_CHECK,  cfg->HotKeyModify & MOD_CONTROL);
		CheckDlgButton(ALT_CHECK,   cfg->HotKeyModify & MOD_ALT);
		CheckDlgButton(SHIFT_CHECK, cfg->HotKeyModify & MOD_SHIFT);

		UINT combo[] = { SEND_COMBO, RECV_COMBO, LOG_COMBO, 0 };
		int  val[]   = {  cfg->HotKeySend, cfg->HotKeyRecv, cfg->HotKeyLogView, 0 };

		for (int i=0; combo[i]; i++) {
			SendDlgItemMessage(combo[i], CB_ADDSTRING, 0, (LPARAM)"-");
			for (char c[2]="A"; *c <= 'Z'; (*c)++) {
				SendDlgItemMessage(combo[i], CB_ADDSTRING, 0, (LPARAM)c);
			}
			SendDlgItemMessage(combo[i], CB_SETCURSEL, val[i] ? (val[i]-'A')+1 : 0, 0);
			::EnableWindow(GetDlgItem(combo[i]),  cfg->HotKeyCheck);
		}
		::EnableWindow(GetDlgItem(CTRL_CHECK),  cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(ALT_CHECK),   cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(SHIFT_CHECK), cfg->HotKeyCheck);
		::EnableWindow(GetDlgItem(CTRL_CHECK),  cfg->HotKeyCheck);

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

//		::ShowWindow(GetDlgItem(NOTIFY_BTN), IsWin8() ? SW_SHOW : SW_HIDE);
	}
	else if (resId == SENDRECV_SHEET) {
		CheckDlgButton(QUOTE_CHECK, (cfg->QuoteCheck & 0x1) ? TRUE : FALSE);
		CheckDlgButton(QUOTEREDUCE_CHECK, (cfg->QuoteCheck & 0x2) ? FALSE : TRUE);
		CheckDlgButton(QUOTECARET_CHECK, (cfg->QuoteCheck & 0x10) ? TRUE : FALSE);

		CheckDlgButton(SECRET_CHECK, cfg->SecretCheck);
		CheckDlgButton(ONECLICK_CHECK, cfg->OneClickPopup);
		CheckDlgButton(REPFIL_CHECK, cfg->ReplyFilter);
		SetDlgItemTextU8(LRUUSER_EDIT, Fmt("%d", cfg->lruUserMax));
		// ControlIME ... 0:off, 1:senddlg on (finddlg:off), 2:always on
		CheckDlgButton(CONTROLIME_CHECK, cfg->ControlIME ? 1 : 0);
		//CheckDlgButton(FINDDLGIME_CHECK, cfg->ControlIME >= 2 ? 0 : 1);
		CheckDlgButton(LISTCONFIRM_CHECK, cfg->ListConfirm);
		SetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr);
		U8toW(cfg->QuoteStr, cfg->QuoteStrW, wsizeof(cfg->QuoteStrW));

		CheckDlgButton(NOPOPUP_CHECK, cfg->NoPopupCheck);
		//CheckDlgButton(ABSENCENONPOPUP_CHECK, cfg->AbsenceNonPopup);
		CheckDlgButton(LOGVIEWRECV_CHECK, cfg->logViewAtRecv);

		CheckDlgButton(NOBEEP_CHECK, cfg->NoBeep);
		CheckDlgButton(RECVLOGON_CHECK, cfg->RecvLogonDisp);
		CheckDlgButton(RECVIPADDR_CHECK, cfg->RecvIPAddr);
		CheckDlgButton(NOERASE_CHECK, cfg->NoErase);
		CheckDlgButton(REPROMSG_CHECK, cfg->ReproMsg);

		SetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile);
	}
	else if (resId == IMAGE_SHEET) {
//		CheckDlgButton(CLIPMODE_CHECK, (cfg->ClipMode & CLIP_ENABLE));

		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_CLIPALWAYS));
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_CLIPNORMAL));
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_CLIPSTRICT));

		int idx =   (cfg->ClipMode & CLIP_CONFIRM_STRICT) ? 2 :
					(cfg->ClipMode & CLIP_CONFIRM_NORMAL) ? 1 : 0;
		SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_SETCURSEL, idx, 0);

		CheckDlgButton(MINIMIZE_CHECK, cfg->CaptureMinimize);
		CheckDlgButton(CLIPBORAD_CHECK, cfg->CaptureClip);
		CheckDlgButton(SAVE_CHECK, cfg->CaptureSave);
//		SetDlgItemInt(DELAY_EDIT, cfg->CaptureDelay);
//		SetDlgItemInt(DELAYEX_EDIT, cfg->CaptureDelayEx);

		if ((cfg->ClipMode & CLIP_ENABLE) == 0) {
			::EnableWindow(GetDlgItem(CLIPCONFIRM_COMBO), FALSE);
			::EnableWindow(GetDlgItem(MINIMIZE_CHECK), FALSE);
			::EnableWindow(GetDlgItem(CLIPBORAD_CHECK), FALSE);
			::EnableWindow(GetDlgItem(SAVE_CHECK), FALSE);
			::EnableWindow(GetDlgItem(DELAY_EDIT), FALSE);
			::EnableWindow(GetDlgItem(DELAYEX_EDIT), FALSE);
		}
	}
	else if (resId == LOG_SHEET) {
		CheckDlgButton(LOG_CHECK, cfg->LogCheck);
//		CheckDlgButton(LOGONLOG_CHECK, cfg->LogonLog);
//		CheckDlgButton(IPADDR_CHECK, cfg->IPAddrCheck);
//		CheckDlgButton(IMAGESAVE_CHECK, (cfg->ClipMode & CLIP_SAVE) ? 1 : 0);
//		CheckDlgButton(LOGUTF8_CHECK, cfg->LogUTF8);
		SetDlgItemTextU8(LOG_EDIT, cfg->LogFile);

//		if ((cfg->ClipMode & CLIP_ENABLE) == 0) {
//			::EnableWindow(GetDlgItem(IMAGESAVE_CHECK), FALSE);
//		}
	}
	else if (resId == AUTOSAVE_SHEET) {
		CheckDlgButton(AUTOSAVE_CHECK, !!(cfg->autoSaveFlags & AUTOSAVE_ENABLED));
		SetDlgItemTextU8(DIR_EDIT, cfg->autoSaveDir);
		SetDlgItemInt(TOUT_EDIT, cfg->autoSaveTout);

		SendDlgItemMessage(PRIORITY_COMBO, CB_ADDSTRING, 0,
			(LPARAM)LoadStr(IDS_DISPPRI_NOLIMIT));
		for (int i=1; i < cfg->PriorityMax; i++) {
			char	buf[MAX_BUF];
			char	*fmt = LoadStr(IDS_DISPPRI_LIMIT);
			snprintfz(buf, sizeof(buf), fmt, i);
			SendDlgItemMessage(PRIORITY_COMBO, CB_ADDSTRING, 0, (LPARAM)buf);
		}
		SendDlgItemMessage(PRIORITY_COMBO, CB_SETCURSEL, cfg->autoSaveLevel, 0);

		SetDlgItemInt(PRIORITY_COMBO, cfg->autoSaveLevel);
		SetDlgItemInt(SIZE_EDIT, cfg->autoSaveMax);
		CheckDlgButton(DIR_CHECK, !!(cfg->autoSaveFlags & AUTOSAVE_INCDIR));
	}
	else if (resId == PASSWORD_SHEET) {
		if (*cfg->PasswordStr == 0) {
			::EnableWindow(GetDlgItem(OLDPASSWORD_EDIT), FALSE);
		}
		::ShowWindow(GetDlgItem(PASSWDLOG_CHECK), SW_SHOW);
		CheckDlgButton(PASSWDLOG_CHECK, cfg->PasswdLogCheck);
	}
//	else if (resId == URL_SHEET) {
//		CheckDlgButton(DEFAULTURL_CHECK, cfg->DefaultUrl);
//
//		for (auto obj = cfg->urlList.TopObj(); obj; obj = cfg->urlList.NextObj(obj)) {
//			auto tmp_obj = new UrlObj;
//			strcpy(tmp_obj->protocol, obj->protocol);
//			strcpy(tmp_obj->program, obj->program);
//			tmpUrlList.AddObj(tmp_obj);
//			SendDlgItemMessage(URL_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)obj->protocol);
//		}
//		if ((curUrl = tmpUrlList.TopObj())) {
//			SetDlgItemTextU8(URL_EDIT, curUrl->program);
//		}
//		CheckDlgButton(SHELLEXEC_CHECK, cfg->ShellExec);
//		SendDlgItemMessage(URL_LIST, LB_SETCURSEL, 0, 0);
//		PostMessage(WM_COMMAND, URL_LIST, 0);
//	}
	else if (resId == REMOTE_SHEET) {
		char	buf[MAX_BUF];

		if (!*cfg->RemoteReboot)    GenRemoteKey(cfg->RemoteReboot);
		if (!*cfg->RemoteExit)      GenRemoteKey(cfg->RemoteExit);
		if (!*cfg->RemoteIPMsgExit) GenRemoteKey(cfg->RemoteIPMsgExit);

		::EnableWindow(GetDlgItem(REBOOT_EDIT), cfg->RemoteRebootMode ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(REBOOT_BUTTON), cfg->RemoteRebootMode ? TRUE : FALSE);
		CheckDlgButton(REBOOT_CHECK, cfg->RemoteRebootMode ? TRUE : FALSE);
		snprintfz(buf, sizeof(buf), REMOTE_FMT, cfg->RemoteReboot);
		SetDlgItemTextU8(REBOOT_EDIT, buf);

		::EnableWindow(GetDlgItem(EXIT_EDIT), cfg->RemoteExitMode ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(EXIT_BUTTON), cfg->RemoteExitMode ? TRUE : FALSE);
		CheckDlgButton(EXIT_CHECK, cfg->RemoteExitMode ? TRUE : FALSE);
		snprintfz(buf, sizeof(buf), REMOTE_FMT, cfg->RemoteExit);
		SetDlgItemTextU8(EXIT_EDIT, buf);

		::EnableWindow(GetDlgItem(IPMSGEXIT_EDIT), cfg->RemoteIPMsgExitMode ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(IPMSGEXIT_BUTTON), cfg->RemoteIPMsgExitMode ? TRUE : FALSE);
		CheckDlgButton(IPMSGEXIT_CHECK, cfg->RemoteIPMsgExitMode ? TRUE : FALSE);
		snprintfz(buf, sizeof(buf), REMOTE_FMT, cfg->RemoteIPMsgExit);
		SetDlgItemTextU8(IPMSGEXIT_EDIT, buf);
	}
	else if (resId == LINK_SHEET) {
		SetDlgItemTextU8(DIRECTEXT_EDIT, cfg->directOpenExt);
		CheckDlgButton(ONECLICKOPEN_CHECK, (cfg->clickOpenMode & 3) ? TRUE : FALSE);
		CheckDlgButton(ONECLICKVIEW_CHECK, (cfg->clickOpenMode & 4) ? TRUE : FALSE);
		::EnableWindow(GetDlgItem(ONECLICKVIEW_CHECK), (cfg->clickOpenMode & 3) ? TRUE : FALSE);

		CheckDlgButton(LINKDETECT_CHECK, (cfg->linkDetectMode & 1) ? TRUE : FALSE);
	}
	else if (resId == BACKUP_SHEET) {
		CheckDlgButton(FIREWALL_CHECK, (cfg->FwCheckMode & 1) ? TRUE : FALSE);

		CheckDlgButton(MINIRECVACTION_CHECK, cfg->RecvIconMode ? TRUE : FALSE);
		CheckDlgButton(USELOCK_CHECK, cfg->PasswordUseNext ? TRUE : FALSE);
		CheckDlgButton(USELOCKNAME_CHK, cfg->useLockName ? TRUE : FALSE);
	}
	else if (resId == LABTEST_SHEET) {
		for (UINT id=IDS_HOOKNONE; id <= IDS_HOOKTRANSALL; id++) {
			SendDlgItemMessage(HOOK_CMB, CB_ADDSTRING, 0, (LPARAM)LoadStr(id));
		}
		SendDlgItemMessage(HOOK_CMB, CB_SETCURSEL, cfg->hookMode, 0);

		CheckDlgButton(cfg->hookKind ? HOOKSLACK_RADIO : HOOKGENERAL_RADIO, TRUE);
		SetDlgItemTextU8(HOOKURL_EDIT, cfg->hookUrl.s());
		SetDlgItemTextU8(HOOKBODY_EDIT, cfg->hookBody.s());

		SetDlgItemTextU8(SLACKCHAN_EDIT, cfg->slackChan.s());
		SetDlgItemTextU8(SLACKKEY_EDIT, cfg->slackKey.s());

		ReflectDisp();
	}
#ifdef IPMSG_PRO
#define SETUP_SETDATA
#include "miscext.dat"
#undef SETUP_SETDATA
#else
	else if (resId == UPDATE_SHEET) {
		CheckDlgButton(UPDATE_CHK, (cfg->updateFlag & Cfg::UPDATE_ON) ? TRUE : FALSE);
		CheckDlgButton(MANUAL_CHK, (cfg->updateFlag & Cfg::UPDATE_MANUAL) ? TRUE : FALSE);
	}
	else if (resId == DIRMODE_SHEET) {
		if (cfg->IPv6Mode == 1 || !cfg->IPDictEnabled()) {
			CheckDlgButton(DIR_NONE_RADIO, TRUE);
			SetDlgItemTextU8(MASTER_EDIT, "");
			::EnableWindow(GetDlgItem(DIR_NONE_RADIO), FALSE);
			::EnableWindow(GetDlgItem(DIR_USER_RADIO), FALSE);
			::EnableWindow(GetDlgItem(DIR_MASTER_RADIO), FALSE);
			::EnableWindow(GetDlgItem(MASTER_EDIT), FALSE);
			::EnableWindow(GetDlgItem(DIRSPAN_EDIT), FALSE);
		}
		else {
			switch (cfg->DirMode) {
			case DIRMODE_USER: 
				CheckDlgButton(DIR_USER_RADIO, TRUE);
				break;

			case DIRMODE_MASTER: 
				CheckDlgButton(DIR_MASTER_RADIO, TRUE);
				break;

			case DIRMODE_NONE:
			default:
				CheckDlgButton(DIR_NONE_RADIO, TRUE);
				break;
			}

			SetDlgItemTextU8(MASTER_EDIT, cfg->masterSvr);
			if (auto mainWin = GetMain()) {
				Addr	selfAddr = mainWin->GetSelfAddr();
				if (selfAddr.IsEnabled()) {
					SetDlgItemTextU8(SELFADDR_EDIT, selfAddr.S());
				}
			}
			SetDlgItemInt(DIRSPAN_EDIT, cfg->dirSpan / 60);

			EvCommand(0, 0, 0);
		}

		//SetDlgItemInt(POLL_EDIT, cfg->pollTick / 1000);
	}
#endif

	return	TRUE;
}

BOOL TSetupSheet::GetData()
{
	char	buf[MAX_PATH_U8];

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

		for (int i=0; SendDlgItemMessage(BROADCAST_LIST, LB_GETTEXT, i, (LPARAM)buf) != LB_ERR;
			i++) {
			cfg->brList.SetHostRaw(buf, ResolveAddr(buf));
		}

		cfg->DialUpCheck = IsDlgButtonChecked(DIALUP_CHECK);

		cfg->ExtendBroadcast = (int)SendDlgItemMessage(EXTBROADCAST_COMBO, CB_GETCURSEL, 0, 0) + 1;
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
		if (GetDlgItemInt(OPENTIME_EDIT) > 0) {
			cfg->OpenMsgTime = GetDlgItemInt(OPENTIME_EDIT) * 1000;
			cfg->OpenCheck = TRUE;
		} else {
			cfg->OpenCheck = FALSE;
		}
		if (GetDlgItemInt(RECVTIME_EDIT) > 0) {
			cfg->RecvMsgTime = GetDlgItemInt(RECVTIME_EDIT) * 1000;
			cfg->BalloonNotify = TRUE;
		}
		else {
			cfg->BalloonNotify = FALSE;
		}
		cfg->BalloonNoInfo = IsDlgButtonChecked(BALLOONNOINFO_CHECK);

		cfg->HotKeyCheck = IsDlgButtonChecked(HOTKEY_CHECK) ?
								(cfg->HotKeyCheck ? cfg->HotKeyCheck : 1) : 0;
		if (cfg->HotKeyCheck) {
			cfg->HotKeyModify = 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(CTRL_CHECK)  ? MOD_CONTROL : 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(ALT_CHECK)   ? MOD_ALT     : 0;
			cfg->HotKeyModify |= IsDlgButtonChecked(SHIFT_CHECK) ? MOD_SHIFT   : 0;

			UINT combo[] = { SEND_COMBO, RECV_COMBO, LOG_COMBO, 0 };
			int  *val[] =  { &cfg->HotKeySend, &cfg->HotKeyRecv, &cfg->HotKeyLogView, 0 };

			for (int i=0; combo[i]; i++) {
				*val[i] = (int)SendDlgItemMessage(combo[i], CB_GETCURSEL, 0, 0);
				if (*val[i]) {
					*val[i] += 'A' - 1;
				}
			}
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
		cfg->QuoteCheck |= (IsDlgButtonChecked(QUOTEREDUCE_CHECK) ? 0 : 0x2);
		cfg->QuoteCheck |= (IsDlgButtonChecked(QUOTECARET_CHECK) ? 0x10 : 0);

		cfg->SecretCheck = IsDlgButtonChecked(SECRET_CHECK);
		cfg->OneClickPopup = IsDlgButtonChecked(ONECLICK_CHECK);
		cfg->ReplyFilter = IsDlgButtonChecked(REPFIL_CHECK);
		cfg->lruUserMax = GetDlgItemInt(LRUUSER_EDIT);
	// ControlIME ... 0:off, 1:senddlg on (finddlg:off), 2:always on
		cfg->ControlIME = IsDlgButtonChecked(CONTROLIME_CHECK);
		cfg->ListConfirm = IsDlgButtonChecked(LISTCONFIRM_CHECK);
		GetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr, sizeof(cfg->QuoteStr));

	//	if (cfg->ControlIME && !IsDlgButtonChecked(FINDDLGIME_CHECK)) {
	//		cfg->ControlIME = 2;
	//	}
		if (cfg->NoPopupCheck != 2) {
			cfg->NoPopupCheck = IsDlgButtonChecked(NOPOPUP_CHECK);
		}
		cfg->NoBeep = IsDlgButtonChecked(NOBEEP_CHECK);
		//cfg->AbsenceNonPopup = IsDlgButtonChecked(ABSENCENONPOPUP_CHECK);
		cfg->logViewAtRecv = IsDlgButtonChecked(LOGVIEWRECV_CHECK);

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
		if (cfg->ClipMode & CLIP_ENABLE) {
			cfg->ClipMode &= ~CLIP_CONFIRM_ALL;
			switch ((int)SendDlgItemMessage(CLIPCONFIRM_COMBO, CB_GETCURSEL, 0, 0)) {
			case 1: cfg->ClipMode |= CLIP_CONFIRM_NORMAL; break;
			case 2: cfg->ClipMode |= CLIP_CONFIRM_STRICT; break;
			}
			cfg->CaptureMinimize = IsDlgButtonChecked(MINIMIZE_CHECK);
			cfg->CaptureClip = IsDlgButtonChecked(CLIPBORAD_CHECK);
			cfg->CaptureSave = IsDlgButtonChecked(SAVE_CHECK);
//			cfg->CaptureDelay = GetDlgItemInt(DELAY_EDIT);
//			cfg->CaptureDelayEx = GetDlgItemInt(DELAYEX_EDIT);
		}
	}
	else if (resId == LOG_SHEET) {
		cfg->LogCheck = IsDlgButtonChecked(LOG_CHECK);
//		cfg->LogonLog = IsDlgButtonChecked(LOGONLOG_CHECK);
//		cfg->IPAddrCheck = IsDlgButtonChecked(IPADDR_CHECK);

//		if (cfg->ClipMode & CLIP_ENABLE) {
//			if (IsDlgButtonChecked(IMAGESAVE_CHECK)) {
//				cfg->ClipMode |=  CLIP_SAVE;
//			} else {
//				cfg->ClipMode &= ~CLIP_SAVE;
//			}
//		}
//		cfg->LogUTF8 = IsDlgButtonChecked(LOGUTF8_CHECK);

		GetDlgItemTextU8(LOG_EDIT, cfg->LogFile, sizeof(cfg->LogFile));
		LogMng::StrictLogFile(cfg->LogFile);
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
		cfg->PasswdLogCheck = IsDlgButtonChecked(PASSWDLOG_CHECK);
	}
//	else if (resId == URL_SHEET) {
//		cfg->DefaultUrl = IsDlgButtonChecked(DEFAULTURL_CHECK);
//		if (curUrl) GetDlgItemTextU8(URL_EDIT, curUrl->program, sizeof(curUrl->program));
//
//		for (auto tmp_obj = tmpUrlList.TopObj(); tmp_obj; tmp_obj = tmpUrlList.NextObj(tmp_obj)) {
//			auto obj = SearchUrlObj(&cfg->urlList, tmp_obj->protocol);
//
//			if (!obj) {
//				obj = new UrlObj;
//				cfg->urlList.AddObj(obj);
//				strcpy(obj->protocol, tmp_obj->protocol);
//			}
//			strcpy(obj->program, tmp_obj->program);
//		}
//		cfg->ShellExec = IsDlgButtonChecked(SHELLEXEC_CHECK);
//	}
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

		if (GetDlgItemTextU8(IPMSGEXIT_EDIT, buf, sizeof(buf)) > REMOTE_HEADERLEN) {
			strcpy(cfg->RemoteIPMsgExit, buf + REMOTE_HEADERLEN);
		}
		cfg->RemoteIPMsgExitMode = IsDlgButtonChecked(IPMSGEXIT_CHECK);
	}
	else if (resId == LINK_SHEET) {
		GetDlgItemTextU8(DIRECTEXT_EDIT, cfg->directOpenExt, sizeof(cfg->directOpenExt));

		if (IsDlgButtonChecked(ONECLICKOPEN_CHECK)) {
			cfg->clickOpenMode |= 3;
		} else {
			cfg->clickOpenMode &= ~3;
		}

		if (IsDlgButtonChecked(ONECLICKVIEW_CHECK)) {
			cfg->clickOpenMode |= 4;
		} else {
			cfg->clickOpenMode &= ~4;
		}

		int sv_linkmode = cfg->linkDetectMode;
		if (IsDlgButtonChecked(LINKDETECT_CHECK)) {
			cfg->linkDetectMode |= 1;
		} else {
			cfg->linkDetectMode &= ~1;
		}
		if (sv_linkmode != cfg->linkDetectMode) {
			::SendMessage(GetMainWnd(), WM_LOGVIEW_RESETCACHE, 0, 0);
		}
	}
	else if (resId == BACKUP_SHEET) {
		if (IsDlgButtonChecked(FIREWALL_CHECK)) {
			cfg->FwCheckMode |= 1;
		} else {
			cfg->FwCheckMode &= ~1;
		}

		BOOL chk = IsDlgButtonChecked(MINIRECVACTION_CHECK);
		if (cfg->RecvIconMode != chk) {
			cfg->RecvIconMode = chk;
			::PostMessage(GetMainWnd(), WM_IPMSG_INITICON, 0, 0);
		}
		cfg->PasswordUseNext = IsDlgButtonChecked(USELOCK_CHECK);
		cfg->useLockName = IsDlgButtonChecked(USELOCKNAME_CHK);
	}
	else if (resId == LABTEST_SHEET) {
		cfg->hookMode = (int)SendDlgItemMessage(HOOK_CMB, CB_GETCURSEL, 0, 0);
		cfg->hookKind = IsDlgButtonChecked(HOOKSLACK_RADIO) ? 1 : 0;

		if (cfg->hookKind == 0) {
			if (GetDlgItemTextU8(HOOKURL_EDIT, buf, sizeof(buf))) {
				cfg->hookUrl = buf;
			}
			if (GetDlgItemTextU8(HOOKBODY_EDIT, buf, sizeof(buf))) {
				cfg->hookBody = buf;
			}
		}
		else if (cfg->hookKind == 1) {
			if (GetDlgItemTextU8(SLACKCHAN_EDIT, buf, sizeof(buf))) {
				cfg->slackChan = buf;
			}
			if (GetDlgItemTextU8(SLACKKEY_EDIT, buf, sizeof(buf))) {
				cfg->slackKey = buf;
			}
		}
	}
#ifdef IPMSG_PRO
	else if (resId == SERVER_SHEET) {
		//cfg->autoUpdateMode = IsDlgButtonChecked(AUTOUPDATE_CHECK);
	}
#else
	else if (resId == UPDATE_SHEET) {
		if (IsDlgButtonChecked(UPDATE_CHK)) {
			cfg->updateFlag |= Cfg::UPDATE_ON;
		} else {
			cfg->updateFlag &= ~Cfg::UPDATE_ON;
		}
		if (IsDlgButtonChecked(MANUAL_CHK)) {
			cfg->updateFlag |= Cfg::UPDATE_MANUAL;
		} else {
			cfg->updateFlag &= ~Cfg::UPDATE_MANUAL;
		}
	}
	else if (resId == DIRMODE_SHEET) {
		if (cfg->IPv6Mode == 1 || !cfg->IPDictEnabled()) {
			cfg->DirMode = DIRMODE_NONE;
			cfg->masterAddr.Init();
		}
		else {
			if (IsDlgButtonChecked(DIR_NONE_RADIO)) {
				cfg->DirMode = DIRMODE_NONE;
				cfg->masterAddr.Init();
			}
			else if (IsDlgButtonChecked(DIR_USER_RADIO)) {
				cfg->DirMode = DIRMODE_USER;
				GetDlgItemTextU8(MASTER_EDIT, cfg->masterSvr, sizeof(cfg->masterSvr));
				if (*cfg->masterSvr) {
					if (!cfg->masterAddr.Set(cfg->masterSvr)) {
						cfg->masterAddr.Resolve(cfg->masterSvr);
					}
				}
				else {
					cfg->masterAddr.Init();
				}
			}
			else if (IsDlgButtonChecked(DIR_MASTER_RADIO)) {
				cfg->DirMode = DIRMODE_MASTER;
				cfg->masterAddr.Init();

				cfg->dirSpan = GetDlgItemInt(DIRSPAN_EDIT) * 60;
				if (cfg->dirSpan < 0) {
					cfg->dirSpan = 180;
				}
			}
		}
		//cfg->pollTick = 60000; //GetDlgItemInt(POLL_EDIT) * 1000;

		::PostMessage(GetMainWnd(), WM_IPMSG_DIRMODE_SHEET, 0, 0);
	}
#endif
	return	TRUE;
}

BOOL TSetupSheet::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	char	buf[MAX_PATH_U8] = "";
	char	buf2[MAX_PATH_U8] = "";
	char	protocol[MAX_LISTBUF] = "";
	int		ret = 0;

	if (wID == IDCANCEL || wID == HELP_BTN) {
		parent->PostMessage(WM_COMMAND, wID, 0);
		return	TRUE;
	}

	if (resId == MAIN_SHEET) {
		switch (wID) {
		case ADD_BUTTON:
			if (GetDlgItemText(BROADCAST_EDIT, buf, sizeof(buf)) <= 0) return TRUE;
			if (!ResolveAddr(buf).IsEnabled()) {
				return MessageBox(LoadStr(IDS_CANTRESOLVE)), TRUE;
			}
			for (int i=0;
				SendDlgItemMessage(BROADCAST_LIST, LB_GETTEXT, i, (LPARAM)buf2) != LB_ERR; i++) {
				if (stricmp(buf, buf2) == 0) return	TRUE;
			}
			SendDlgItemMessage(BROADCAST_LIST, LB_ADDSTRING, 0, (LPARAM)buf);
			SetDlgItemText(BROADCAST_EDIT, "");
			return	TRUE;

		case DEL_BUTTON:
			while ((int)SendDlgItemMessage(BROADCAST_LIST, LB_GETSELCOUNT, 0, 0) > 0)
			{
				int i;
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
					LoadStrU8(IDS_OPENFILEICON), LoadStrAsFilterU8(IDS_OPENFILEICONFLTR));
			return	TRUE;

		case HOTKEY_CHECK:
			ret = IsDlgButtonChecked(HOTKEY_CHECK);
			::EnableWindow(GetDlgItem(CTRL_CHECK),  ret);
			::EnableWindow(GetDlgItem(ALT_CHECK),   ret);
			::EnableWindow(GetDlgItem(SHIFT_CHECK), ret);
			::EnableWindow(GetDlgItem(CTRL_CHECK),  ret);
			::EnableWindow(GetDlgItem(SEND_COMBO),  ret);
			::EnableWindow(GetDlgItem(RECV_COMBO),  ret);
			::EnableWindow(GetDlgItem(LOG_COMBO),   ret);
			return	TRUE;

#define FMT_NOTIFY_SETTINGS	L"shell32.dll,Options_RunDLL %d"

		case NOTIFY_BTN:
			::ShellExecuteW(hWnd, 0, L"rundll32.exe", FmtW(FMT_NOTIFY_SETTINGS, 5), 0, SW_SHOW);
			return	TRUE;

		case TRAY_BTN:
			::ShellExecuteW(hWnd, 0, L"rundll32.exe", FmtW(FMT_NOTIFY_SETTINGS, 1), 0, SW_SHOW);
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
			OpenFileDlg(this).Exec(SOUND_EDIT, LoadStrU8(IDS_OPENFILESND),
									LoadStrAsFilterU8(IDS_OPENFILESNDFLTR), buf);
			SetCurrentDirectoryU8(buf2);
			return	TRUE;

		case SOUNDPLAY_BUTTON:
			if (GetDlgItemTextU8(SOUND_EDIT, buf, sizeof(buf)) > 0) {
				PlaySoundU8(buf, 0, SND_FILENAME|SND_ASYNC);
			}
			return	TRUE;

		case LOGVIEWRECV_CHECK:
			if (IsDlgButtonChecked(LOGVIEWRECV_CHECK)) {
				TSetupSheet *sheet = parentDlg->GetSheet(LOG_SHEET);
				if (!sheet->IsDlgButtonChecked(LOG_CHECK)) {
					MessageBox(LoadStr(IDS_NEEDLOGCHECK));
				}
			}
			return	TRUE;
		}
	}
	else if (resId == IMAGE_SHEET) {
	}
	else if (resId == LOG_SHEET) {
		switch (wID) {
		case LOGFILE_BUTTON:
			cfg->GetBaseDir(buf);
			MakeDirAllU8(buf);
			OpenFileDlg(this, OpenFileDlg::SAVE).Exec(LOG_EDIT,
				LoadStrU8(IDS_OPENFILELOG), LoadStrAsFilterU8(IDS_OPENFILELOGFLTR));
			return	TRUE;
		}
	}
	else if (resId == AUTOSAVE_SHEET) {
		switch (wID) {
		case DIR_BUTTON:
			MakeAutoSaveDir(cfg, buf);
			MakeDirAllU8(buf);
			if (BrowseDirDlg(this, LoadStrU8(IDS_FOLDERATTACH), buf, buf, sizeof(buf))) {
				SetDlgItemTextU8(DIR_EDIT, buf);
			}
			return	TRUE;

		case OPEN_BUTTON:
			MakeAutoSaveDir(cfg, buf);
			MakeDirAllU8(buf);
			ShellExecuteU8(NULL, NULL, buf, 0, 0, SW_SHOW);
			return	TRUE;
		}
	}
	else if (resId == PASSWORD_SHEET) {
	}
//	else if (resId == URL_SHEET) {
//		UrlObj	*obj;
//		switch (wID) {
//		case ADD_BUTTON:
//			if ((i = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR &&
//				SendDlgItemMessage(URL_LIST, LB_GETTEXT, (WPARAM)i, (LPARAM)protocol) != LB_ERR &&
//				(obj = SearchUrlObj(&tmpUrlList, protocol))) {
//				sprintf(buf, LoadStrU8(IDS_EXECPROGRAM), protocol);
//				OpenFileDlg(this).Exec(URL_EDIT, buf, LoadStrAsFilterU8(IDS_OPENFILEPRGFLTR));
//			}
//			return	TRUE;
//
//		case URL_LIST:
//			if ((i = (int)SendDlgItemMessage(URL_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR) {
//				if (curUrl) {
//					GetDlgItemTextU8(URL_EDIT, curUrl->program, sizeof(curUrl->program));
//					curUrl = NULL;
//				}
//				if (SendDlgItemMessage(URL_LIST, LB_GETTEXT, i, (LPARAM)protocol) != LB_ERR &&
//					(obj = SearchUrlObj(&tmpUrlList, protocol))) {
//					curUrl = obj;
//					SetDlgItemTextU8(URL_EDIT, obj->program);
//				}
//				
//			}
//			return	TRUE;
//		}
//	}
	else if (resId == REMOTE_SHEET) {
		char	buf[MAX_BUF];
		char	buf2[MAX_BUF];
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

		case IPMSGEXIT_CHECK:
			ret = IsDlgButtonChecked(IPMSGEXIT_CHECK);
			::EnableWindow(GetDlgItem(IPMSGEXIT_EDIT), ret);
			::EnableWindow(GetDlgItem(IPMSGEXIT_BUTTON), ret);
			break;

		case REBOOT_BUTTON: case EXIT_BUTTON: case IPMSGEXIT_BUTTON:
			GenRemoteKey(buf);
			snprintfz(buf2, sizeof(buf2), REMOTE_FMT, buf);
			SetDlgItemTextU8(wID == REBOOT_BUTTON ? REBOOT_EDIT :
				wID == EXIT_BUTTON ? EXIT_EDIT : IPMSGEXIT_EDIT, buf2);
			break;

		case REBOOT_EDIT: case EXIT_EDIT: case IPMSGEXIT_EDIT:
			SendDlgItemMessage(wID, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
			break;
		}
	}
	else if (resId == LINK_SHEET) {
		switch (wID) {
		case ONECLICKOPEN_CHECK:
			{
				BOOL	ret = IsDlgButtonChecked(ONECLICKOPEN_CHECK);
				::EnableWindow(GetDlgItem(ONECLICKVIEW_CHECK), ret);
				if (!ret) {
					CheckDlgButton(ONECLICKVIEW_CHECK, FALSE);
				}
			}
			break;
		}
	}
	else if (resId == BACKUP_SHEET) {
		switch (wID) {
		case SAVE_BUTTON:
			{
				char	path[MAX_PATH_U8] = "";
				MakePathU8(path, cfg->lastSaveDir, "ipmsg.reg");

				OpenFileDlg	dlg(this, OpenFileDlg::SAVE, NULL, OFN_OVERWRITEPROMPT);
				if (dlg.Exec(path, sizeof(path), NULL,"Reg file(*.reg)\0*.reg\0\0", NULL, "reg")) {
					char	regName[MAX_PATH_U8] = "";
					cfg->GetSelfRegName(regName);
					snprintfz(buf, sizeof(buf),
						"/E \"%s\" HKEY_CURRENT_USER\\Software\\HSTools\\%s", path, regName);
					ShellExecuteU8(hWnd, "runas", "regedit.exe", buf, 0, SW_SHOW);
				}
			}
			break;

		case DBG_BTN:
			if (DebugConsoleEnabled()) {
				CloseDebugConsole();
			} else {
				OpenDebugConsole();
				DebugW(LoadStrW(IDS_CONSOLEWARN));
			}
			break;
		}
	}
	else if (resId == LABTEST_SHEET) {
		switch (wID) {
		case HOOKTEST_BTN:
			if (IsDlgButtonChecked(HOOKGENERAL_RADIO)) {
				DynBuf	url(MAX_URLBUF);
				DynBuf	body(MAX_URLBUF);
				U8str	json;
				DynBuf	reply;
				U8str	errMsg;

				GetDlgItemTextU8(HOOKBODY_EDIT, (char *)body.Buf(), (int)body.Size());
				GetDlgItemTextU8(HOOKURL_EDIT, (char *)url.Buf(), (int)url.Size());

				if (!*body.s() || !*url.s()) {
					break;
				}
				std::map<U8str, U8str> dict = {
					{ "$(sender)", IP_MSG },
					{ "$(icon)",   cfg->slackIcon.s() },
					{ "$(msg)",    "Transfer test to hook host from IPMsg" }
				};
				U8str	out(MAX_URLBUF);
				ReplaseKeyword(body.s(), &out, &dict);
				TInetRequest(url.s(), NULL, (BYTE *)out.s(), out.Len(), &reply, &errMsg);
				U8str	res(reply.UsedSize() ? reply.s() : errMsg.s());
				if (res.LineNum() > 2) {
					U8str	bak = res;
					res = "Error Message from HookHost\r\n";
					res += bak.s();
				}
				SetDlgItemTextU8(HOOKTEST_EDIT, res.s());
			}
			else {
				char	key[MAX_PATH];
				char	chan[MAX_PATH];
				U8str	json;
				DynBuf	reply;
				U8str	errMsg;

				GetDlgItemTextU8(SLACKCHAN_EDIT, chan, sizeof(chan));
				GetDlgItemTextU8(SLACKKEY_EDIT, key, sizeof(key));

				if (!*key || !*chan) {
					break;
				}
				SlackMakeJson(chan, IP_MSG,
					"Transfer test to slack from IPMsg", cfg->slackIcon.s(), &json);
				SlackRequest(cfg->slackHost.s(), key, json.s(), &reply, &errMsg);
				U8str	res(reply.UsedSize() ? reply.s() : errMsg.s());
				if (res.LineNum() > 2) {
					U8str	bak = res;
					res = "Error Message from Slack\r\n";
					res += bak.s();
				}
				SetDlgItemTextU8(HOOKTEST_EDIT, res.s());
			}
			break;

		case HOOKGENERAL_RADIO:
		case HOOKSLACK_RADIO:
			ReflectDisp();
		}
	}
#ifdef IPMSG_PRO
	else if (resId == SERVER_SHEET) {
#define SETUP_EVCMD
#include "miscext.dat"
#undef SETUP_EVCMD
	}
#else
	else if (resId == UPDATE_SHEET) {
		switch (wID) {
		case UPDATE_BTN:
			if (auto mainWin = GetMain()) {
				SetDlgItemTextU8(UPDATE_EDIT, "");

				if (updateReady) {
					auto	idle_state = mainWin->CheckUpdateIdle();
					if (idle_state != UPDI_TRANS) {
						mainWin->UpdateCheck(UPD_NONE, hWnd);
					}
					else {
						SetDlgItemTextU8(UPDATE_EDIT, LoadStrU8(IDS_UPDBUSYTRANS));
					}
				}
				else {
					if (!mainWin->UpdateCheck(UPD_STEP, hWnd)) {
						SetDlgItemTextU8(UPDATE_EDIT, "UpdateCheck error");
					}
				}
			}
			break;
		}
	}
	else if (resId == DIRMODE_SHEET) {
		if (cfg->IPv6Mode != 1 && cfg->IPDictEnabled()) {
			int mode =  IsDlgButtonChecked(DIR_USER_RADIO)   ? DIRMODE_USER :
						IsDlgButtonChecked(DIR_MASTER_RADIO) ? DIRMODE_MASTER : DIRMODE_NONE;

			::EnableWindow(GetDlgItem(MASTER_EDIT), mode == DIRMODE_USER);
			::EnableWindow(GetDlgItem(DIRSPAN_EDIT), mode == DIRMODE_MASTER);
		}
	}
#endif

	return	FALSE;
}

BOOL TSetupSheet::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifndef IPMSG_PRO
	if (resId == UPDATE_SHEET) {
		BOOL	ret = (BOOL)wParam;
		auto	ur = (UpdateReply *)lParam;

		if (!ret) {
			SetDlgItemTextU8(UPDATE_EDIT, ur->errMsg.s());
			return	TRUE;
		}

		if (uMsg == WM_IPMSG_UPDINFORESULT) {
			if (!ur->needUpdate) {
				SetDlgItemTextU8(UPDATE_EDIT, LoadStrU8(IDS_LASTESTVER));
			}
			else if (!updateReady) {
				SetDlgItemTextU8(UPDATE_BTN, LoadStrU8(IDS_UPDATE));
				SetDlgItemTextU8(UPDATE_EDIT,
					Fmt("%s\r\n (v%s)", LoadStrU8(IDS_NEEDUPDATE), ur->ver.s()));
				updateReady = TRUE;
			}
		}
		else if (uMsg == WM_IPMSG_UPDDATARESULT) {
			if (ur->errMsg.Len()) {
				SetDlgItemTextU8(UPDATE_EDIT, ur->errMsg.s());
			}
			else {
				SetDlgItemTextU8(UPDATE_EDIT, "");
			}
		}
	}
#endif
	return	TRUE;
}

/*
	Setup Dialog初期化処理
*/
TSetupDlg::TSetupDlg(Cfg *_cfg, THosts *_hosts, BOOL is_first, TWin *_parent)
	: TDlg(SETUP_DIALOG, _parent), tipWnd(this)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg		= _cfg;
	hosts	= _hosts;
	curIdx	= -1;
	isFirstMode = is_first;
	//isFirstMode = TRUE;

	idVec.push_back(MAIN_SHEET);
#ifndef IPMSG_PRO
	idVec.push_back(DIRMODE_SHEET);
#endif
	idVec.push_back(DETAIL_SHEET);
	idVec.push_back(SENDRECV_SHEET);
	idVec.push_back(IMAGE_SHEET);
	idVec.push_back(LINK_SHEET);
	if (cfg->PasswordUse) {
		idVec.push_back(PASSWORD_SHEET);
	}
	idVec.push_back(LOG_SHEET);
	idVec.push_back(AUTOSAVE_SHEET);
	idVec.push_back(REMOTE_SHEET);
	idVec.push_back(LABTEST_SHEET);
#ifdef IPMSG_PRO
	idVec.push_back(SERVER_SHEET);
#else
	idVec.push_back(UPDATE_SHEET);
#endif
	idVec.push_back(BACKUP_SHEET);

	sheet = new TSetupSheet[idVec.size()];
};

TSetupDlg::~TSetupDlg()
{
	delete [] sheet;
}

/*
	Window 生成時の CallBack
*/
BOOL TSetupDlg::EvCreate(LPARAM lParam)
{
	setupList.AttachWnd(GetDlgItem(SETUP_LIST));

	for (int i=0; i < idVec.size(); i++) {
		idxMap[idVec[i]] = i;
		sheet[i].Create(idVec[i], cfg, hosts, this);
		setupList.SendMessage(LB_ADDSTRING, 0,
			(LPARAM)LoadStr(IDS_SETUP_SHEET1 + (idVec[i] - SETUP_SHEET1)));
	}
	SetSheetIdx();

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

	if (isFirstMode) {
	//	FirstMode();
		isFirstMode = FALSE;
	}

	return	TRUE;
}

//BOOL TSetupDlg::FirstMode()
//{
//	TRect	rc;
//
//	::GetWindowRect(sheet[0].GetDlgItem(NICKNAME_EDIT), &rc);
//	TPoint	pt(rc.x() - 20, rc.bottom + 20);
//	//::ScreenToClient(hWnd, &pt);

//	tipWnd.Create(L"最初にユーザ名を設定しましょう", pt.x, pt.y, 1, TRUE);
//	return	TRUE;
//}

BOOL TSetupDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case APPLY_BUTTON:
		{
			if (!sheet[curIdx].CheckData()) {
				return TRUE;
			}
			for (int i=0; i < idVec.size(); i++) {
				sheet[i].GetData();
			}
			cfg->WriteRegistry(CFG_GENERAL|CFG_BROADCAST|CFG_LINKACT|CFG_SLACKOPT
				|CFG_HOOKOPT|CFG_UPDATEOPT);
			if (wID == IDOK) {
				EndDialog(wID);
			}
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case HELP_BTN:
		sheet[curIdx].ShowHelp();
		return	TRUE;

	case SETUP_LIST:
		if (wNotifyCode == LBN_SELCHANGE) {
			SetSheetIdx();
		}
		return	TRUE;
	}
	return	FALSE;
}

void TSetupDlg::SetSheetIdx(int idx)
{
	if (curIdx >= 0 && curIdx == idx) return;
	if (idx >= (ssize_t)idVec.size()) return;

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
	for (int i=0; i < idVec.size(); i++) {
		sheet[i].Show(i == idx ? SW_SHOW : SW_HIDE);
	}
	curIdx = idx;
}

void TSetupDlg::SetSheet(int sheet_id)
{
	auto	itr = idxMap.find(sheet_id);

	if (itr == idxMap.end()) {
		return;
	}

	int	idx = itr->second;
	SetSheetIdx(idx);

	if (idx == curIdx) {
		setupList.SendMessage(LB_SETCURSEL, idx, 0);
	}
}

TSetupSheet *TSetupDlg::GetSheet(int sheet_id)
{
	auto	itr = idxMap.find(sheet_id);

	if (itr == idxMap.end()) {
		return NULL;
	}

	return sheet + itr->second;
}

