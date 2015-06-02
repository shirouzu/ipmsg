static char *setupdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   setupdlg.cpp	Ver3.30";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Setup Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2011-07-31(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "resource.h"
#include "ipmsg.h"
#include "plugin.h"

/*
	SetupópSheet
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
	SetData();

	RECT	rc;
	POINT	pt;
	::GetWindowRect(parent->GetDlgItem(SETUP_LIST), &rc);
	pt.x = rc.right;
	pt.y = rc.top;
	ScreenToClient(parent->hWnd, &pt);
	SetWindowPos(0, pt.x + 10, pt.y - 10, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	SetWindowLong(GWL_EXSTYLE, GetWindowLong(GWL_EXSTYLE)|WS_EX_CONTROLPARENT);
	SetDlgIcon(hWnd);

	return	TRUE;
}

BOOL TSetupSheet::EvNcDestroy(void)
{
	if (resId == SETUP_SHEET6) {
		UrlObj *urlObj;

		while ((urlObj = (UrlObj *)tmpUrlList.TopObj())) {
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

	if (resId == SETUP_SHEET3) {
		GetDlgItemTextU8(LRUUSER_EDIT, buf, sizeof(buf));
		if ((val = atoi(buf)) > MAX_LRUUSER || val < 0) {
			MessageBox(Fmt(GetLoadStr(IDS_TOOMANYLRU), MAX_LRUUSER));
			return	FALSE;
		}
	}
	else if (resId == SETUP_SHEET4) {
		GetDlgItemTextU8(LOG_EDIT, buf, sizeof(buf));
		if (GetDriveType(NULL) == DRIVE_REMOTE
				&& !IsDlgButtonChecked(LOG_CHECK) && strchr(buf, '\\')) {
			MessageBox(GetLoadStr(IDS_LOGALERT));
			return	FALSE;
		}
	}
	else if (resId == SETUP_SHEET5) {
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

	return	TRUE;
}

BOOL TSetupSheet::SetData()
{
	if (resId == SETUP_SHEET1) {
		SetDlgItemTextU8(GROUP_COMBO, cfg->GroupNameStr);
		SetDlgItemTextU8(NICKNAME_EDIT, cfg->NickNameStr);

		for (TBrObj *obj=cfg->brList.Top(); obj; obj=cfg->brList.Next(obj))
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

		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_LIMITED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_DIRECTED));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_ADDSTRING, 0, (LPARAM)GetLoadStr(IDS_BOTH));
		SendDlgItemMessage(EXTBROADCAST_COMBO, CB_SETCURSEL, cfg->ExtendBroadcast, 0);
	}
	else if (resId == SETUP_SHEET2) {
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
		CheckDlgButton(ABNORMALBTN_CHECK, cfg->AbnormalButton);

		if (cfg->lcid != -1 || GetSystemDefaultLCID() == 0x411) {
			::ShowWindow(GetDlgItem(LCID_CHECK), SW_SHOW);
			::EnableWindow(GetDlgItem(LCID_CHECK), TRUE);
			CheckDlgButton(LCID_CHECK, cfg->lcid == -1 || cfg->lcid == 0x411 ? FALSE : TRUE);
		}

		SetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile);
		SetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile);
	}
	else if (resId == SETUP_SHEET3) {
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

		SetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile);
	}
	else if (resId == SETUP_SHEET4) {
		CheckDlgButton(CLIPMODE_CHECK, (cfg->ClipMode & CLIP_ENABLE));
		CheckDlgButton(CLIPCONFIRM_CHECK, (cfg->ClipMode & CLIP_CONFIRM) ? 1 : 0);
		CheckDlgButton(MINIMIZE_CHECK, cfg->CaptureMinimize);
		CheckDlgButton(CLIPBORAD_CHECK, cfg->CaptureClip);
		CheckDlgButton(SAVE_CHECK, cfg->CaptureSave);
		SetDlgItemInt(DELAY_EDIT, cfg->CaptureDelay);
		SetDlgItemInt(DELAYEX_EDIT, cfg->CaptureDelayEx);
	}
	else if (resId == SETUP_SHEET5) {
		CheckDlgButton(LOG_CHECK, cfg->LogCheck);
		CheckDlgButton(LOGONLOG_CHECK, cfg->LogonLog);
		CheckDlgButton(IPADDR_CHECK, cfg->IPAddrCheck);
		CheckDlgButton(IMAGESAVE_CHECK, (cfg->ClipMode & 2) ? 1 : 0);
		CheckDlgButton(LOGUTF8_CHECK, cfg->LogUTF8);
		CheckDlgButton(PASSWDLOG_CHECK, cfg->PasswdLogCheck);
		SetDlgItemTextU8(LOG_EDIT, cfg->LogFile);
	}
	else if (resId == SETUP_SHEET6) {
		if (*cfg->PasswordStr == 0)
			::EnableWindow(GetDlgItem(OLDPASSWORD_EDIT), FALSE);
	}
	else if (resId == SETUP_SHEET7) {
		CheckDlgButton(DEFAULTURL_CHECK, cfg->DefaultUrl);

		for (UrlObj *obj = (UrlObj *)cfg->urlList.TopObj(); obj;
					 obj = (UrlObj *)cfg->urlList.NextObj(obj)) {
			UrlObj *tmp_obj = new UrlObj;
			strcpy(tmp_obj->protocol, obj->protocol);
			strcpy(tmp_obj->program, obj->program);
			tmpUrlList.AddObj(tmp_obj);
			SendDlgItemMessage(URL_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)obj->protocol);
		}
		if ((curUrl = (UrlObj *)tmpUrlList.TopObj())) {
			SetDlgItemTextU8(URL_EDIT, curUrl->program);
		}
		CheckDlgButton(SHELLEXEC_CHECK, cfg->ShellExec);
		SendDlgItemMessage(URL_LIST, LB_SETCURSEL, 0, 0);
		PostMessage(WM_COMMAND, URL_LIST, 0);
	}
	return	TRUE;
}

BOOL TSetupSheet::GetData()
{
	char	buf[MAX_PATH_U8];
	int		i;

	if (resId == SETUP_SHEET1) {
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
	}
	else if (resId == SETUP_SHEET2) {
		cfg->BalloonNotify = IsDlgButtonChecked(BALLOONNOTIFY_CHECK);
		cfg->OpenCheck = (int)SendDlgItemMessage(OPEN_COMBO, CB_GETCURSEL, 0, 0);
//		cfg->OpenCheck = IsDlgButtonChecked(OPEN_CHECK);
		cfg->OpenMsgTime = GetDlgItemInt(OPENTIME_EDIT) * 1000;
		cfg->RecvMsgTime = GetDlgItemInt(RECVTIME_EDIT) * 1000;
		cfg->BalloonNoInfo = IsDlgButtonChecked(BALLOONNOINFO_CHECK);

		GetDlgItemTextU8(QUOTE_EDIT, cfg->QuoteStr, sizeof(cfg->QuoteStr));

		cfg->HotKeyCheck = IsDlgButtonChecked(HOTKEY_CHECK);
		cfg->AbnormalButton = IsDlgButtonChecked(ABNORMALBTN_CHECK);
		if (::IsWindowEnabled(GetDlgItem(LCID_CHECK))) {
			cfg->lcid = IsDlgButtonChecked(LCID_CHECK) ? 0x409 : -1;
		}

		GetDlgItemTextU8(MAINICON_EDIT, cfg->IconFile, sizeof(cfg->IconFile));
		GetDlgItemTextU8(REVICON_EDIT, cfg->RevIconFile, sizeof(cfg->RevIconFile));
		::SendMessage(GetMainWnd(), WM_IPMSG_INITICON, 0, 0);
		SetDlgIcon(hWnd);
		SetHotKey(cfg);
	}
	else if (resId == SETUP_SHEET3) {
		cfg->QuoteCheck = IsDlgButtonChecked(QUOTE_CHECK);
		cfg->SecretCheck = IsDlgButtonChecked(SECRET_CHECK);
		cfg->OneClickPopup = IsDlgButtonChecked(ONECLICK_CHECK);
		GetDlgItemTextU8(LRUUSER_EDIT, buf, sizeof(buf));
		cfg->lruUserMax = atoi(buf);
	// ControlIME ... 0:off, 1:senddlg on (finddlg:off), 2:always on
		cfg->ControlIME = IsDlgButtonChecked(CONTROLIME_CHECK);
		if (cfg->ControlIME && !IsDlgButtonChecked(FINDDLGIME_CHECK)) {
			cfg->ControlIME = 2;
		}
		cfg->NoPopupCheck = IsDlgButtonChecked(NOPOPUP_CHECK);
		cfg->NoBeep = IsDlgButtonChecked(NOBEEP_CHECK);
		cfg->AbsenceNonPopup = IsDlgButtonChecked(ABSENCENONPOPUP_CHECK);
		cfg->RecvLogonDisp = IsDlgButtonChecked(RECVLOGON_CHECK);
		cfg->RecvIPAddr = IsDlgButtonChecked(RECVIPADDR_CHECK);
		cfg->NoErase = IsDlgButtonChecked(NOERASE_CHECK);

		GetDlgItemTextU8(SOUND_EDIT, cfg->SoundFile, sizeof(cfg->SoundFile));
	}
	else if (resId == SETUP_SHEET4) {
		if (IsDlgButtonChecked(CLIPMODE_CHECK)) {
			cfg->ClipMode |=  CLIP_ENABLE;
		} else {
			cfg->ClipMode &= ~CLIP_ENABLE;
		}
		if (IsDlgButtonChecked(CLIPCONFIRM_CHECK)) {
			cfg->ClipMode |=  CLIP_CONFIRM;
		} else {
			cfg->ClipMode &= ~CLIP_CONFIRM;
		}
		cfg->CaptureMinimize = IsDlgButtonChecked(MINIMIZE_CHECK);
		cfg->CaptureClip = IsDlgButtonChecked(CLIPBORAD_CHECK);
		cfg->CaptureSave = IsDlgButtonChecked(SAVE_CHECK);
		cfg->CaptureDelay = GetDlgItemInt(DELAY_EDIT);
		cfg->CaptureDelayEx = GetDlgItemInt(DELAYEX_EDIT);
	}
	else if (resId == SETUP_SHEET5) {
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
	else if (resId == SETUP_SHEET6) {
		char	buf[MAX_NAMEBUF];
		GetDlgItemTextU8(OLDPASSWORD_EDIT, buf, sizeof(buf));
		if (CheckPassword(cfg->PasswordStr, buf)) {
			GetDlgItemTextU8(NEWPASSWORD_EDIT, buf, sizeof(buf));
			MakePassword(buf, cfg->PasswordStr);
		}
	}
	else if (resId == SETUP_SHEET7) {
		cfg->DefaultUrl = IsDlgButtonChecked(DEFAULTURL_CHECK);
		if (curUrl) GetDlgItemTextU8(URL_EDIT, curUrl->program, sizeof(curUrl->program));

		for (UrlObj *tmp_obj = (UrlObj *)tmpUrlList.TopObj(); tmp_obj;
			tmp_obj = (UrlObj *)tmpUrlList.NextObj(tmp_obj)) {
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

	return	TRUE;
}

BOOL TSetupSheet::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	char	buf[MAX_PATH_U8] = "", buf2[MAX_PATH_U8] = "", protocol[MAX_LISTBUF] = "";
	int		i;
	UrlObj	*obj;

	if (resId == SETUP_SHEET1) {
		switch (wID) {
		case ADD_BUTTON:
			if (GetDlgItemText(BROADCAST_EDIT, buf, sizeof(buf)) <= 0) return TRUE;
			if (ResolveAddr(buf) == 0) return MessageBox(GetLoadStr(IDS_CANTRESOLVE)), TRUE;
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
		}
	}
	else if (resId == SETUP_SHEET2) {
		switch (wID) {
		case MAINICON_BUTTON: case REVICON_BUTTON:
			OpenFileDlg(this).Exec(wID == MAINICON_BUTTON ? MAINICON_EDIT : REVICON_EDIT,
					GetLoadStrU8(IDS_OPENFILEICON), GetLoadStrAsFilterU8(IDS_OPENFILEICONFLTR));
			return	TRUE;
		}
	}
	else if (resId == SETUP_SHEET3) {
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
	else if (resId == SETUP_SHEET4) {
		switch (wID) {
		case LOGFILE_BUTTON:
			OpenFileDlg(this, OpenFileDlg::SAVE).Exec(LOG_EDIT, GetLoadStrU8(IDS_OPENFILELOG),
													GetLoadStrAsFilterU8(IDS_OPENFILELOGFLTR));
			return	TRUE;
		}
	}
	else if (resId == SETUP_SHEET6) {
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

	return	FALSE;
}

/*
	Setup Dialogèâä˙âªèàóù
*/
TSetupDlg::TSetupDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent) : TDlg(SETUP_DIALOG, _parent)
{
	cfg		= _cfg;
	hosts	= _hosts;
	curIdx	= -1;
}

/*
	Window ê∂ê¨éûÇÃ CallBack
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


