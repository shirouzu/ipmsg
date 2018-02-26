static char *mainwinev_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   mainwinev.cpp	Ver4.61";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window Event
	Create					: 1996-06-01(Sat)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include "instdata/instcmn.h"
#include <process.h>
#include <gdiplus.h>

using namespace std;

#include "toast/toast.h"

/*
	MainWindow 生成時の CallBack
	Packet 受信開始、Entry Packetの送出
*/
BOOL TMainWin::EvCreate(LPARAM lParam)
{
	SetMainWnd(hWnd);
	if (IsWin10() /* && !IsWin10Fall()*/) {	// Fall以降では不要に
		InitToastDll();
	}

	if (IsWinVista() && ::IsUserAnAdmin() && TIsEnableUAC()) {
		RemoveMessageFilters();
	}

#ifdef IPMSG_PRO
#define MAIN_EVCREATE_1
#include "mainext.dat"
#undef  MAIN_EVCREATE_1
#endif

	if (!msgMng->GetStatus()) {
		return TRUE;
	}

	if (cfg->TaskbarUI) {
		Show(SW_MINIMIZE);
	} else {
		Show(SW_HIDE);
	}

	if (cfg->lastWnd) {
		HWND	bak = hWnd;
		hWnd = (HWND)cfg->lastWnd;
		TaskTray(NIM_DELETE);
		hWnd = bak;
	}
	cfg->lastWnd = (int64)hWnd;
	cfg->WriteRegistry(CFG_LASTWND);

	TaskTray(NIM_ADD, hCycleIcon[0], IP_MSG);

	TaskBarCreateMsg = ::RegisterWindowMessage("TaskbarCreated");
	TaskBarButtonMsg = ::RegisterWindowMessage("TaskbarButtonCreated");
	TaskBarNotifyMsg = ::RegisterWindowMessage(IP_MSG);

	if (IsWinVista()) { // user32.dll の遅延ロード必須
		::RegisterPowerSettingNotification(hWnd, &GUID_MONITOR_POWER_ON,
			DEVICE_NOTIFY_WINDOW_HANDLE);
	//	&GUID_CONSOLE_DISPLAY_STATE
	}

	SetIcon(cfg->AbsenceCheck);
	SetCaption();
	if (!SetupCryptAPI(cfg)) {
		MessageBoxU8("CryptoAPI can't be used.");
	}
	GetSelfHost(&cfg->selfHost);

	if (!cfg->IPDictEnabled()) {
		cfg->DirMode = DIRMODE_NONE;
		cfg->masterAddr.Init();
	}

	msgMng->AsyncSelectRegister(hWnd);
	SetHotKey(cfg);

	if (msgMng->GetStatus()) {
		if (cfg->DirMode == DIRMODE_MASTER) {
			isFirstBroad = TRUE;
		}
		EntryHost();
		isFirstBroad = FALSE;
	}

	if (param.isUpdateErr) {
		UpdateFileCleanup();
		BalloonWindow(TRAY_NORMAL, "Update Failed", IP_MSG, 10000);
	}

#ifdef IPMSG_PRO
#define MAIN_EVCREATE_2
#include "mainext.dat"
#undef  MAIN_EVCREATE_2
#endif
	PollSend();

	if (param.isFirstRun) {
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), "#usage");
	}

//	if (!*cfg->NickNameStr) {
//		BalloonWindow(TRAY_NORMAL, LoadStrU8(IDS_FIRSTRUN_TRAY), IP_MSG, 60000000, TRUE);
//	}

	if (param.isHistory) {
		PostMessage(WM_COMMAND, MENU_HELP_HISTORY, 0);
	}

	if (param.isInstalled || param.isUpdated) {
		char	title[MAX_BUF];
		snprintfz(title, sizeof(title), IPMSG_FULLNAME " ver %s", GetVersionStr());
		BalloonWindow(TRAY_INST, LoadStrU8(
			param.isUpdated ? IDS_UPDATEDONE_TRAY : IDS_INSTALLED_TRAY), title, 10000000, TRUE);
	}

	LoadStoredPacket();
	shareMng->LoadShare();

	if (cfg->viewEpoch) {
		if (auto view=logViewList.TopObj()) {
			view->SetStatusCheckEpoch(cfg->viewEpoch);
		}
	}
	logmng->InitDB(); // logmng自体にはアクセス可能
	if (LogDb *logDb = logmng->GetLogDb()) {
		logDb->PrefetchCache(hWnd, WM_LOGFETCH_DONE);
	}


	SetTimer(IPMSG_CLEANUP_TIMER, 1000); // 1sec
	SetTimer(IPMSG_CLEANUPDIRTCP_TIMER, 2000); // 2sec

#ifndef IPMSG_PRO
	if (cfg->DirMode == DIRMODE_MASTER) {
		dirPhase = DIRPH_START;
		SetTimer(IPMSG_DIR_TIMER, DIR_BASE_TICK);

#if 0
//		time_t	t = time(NULL);
//		for (int i=0; i < 1; i++) {
//			Host	*host = new Host;
//			host->hostSub.addr.Set(Fmt("10.0.%d.%d", i / 100 + 100, (i % 100)+100));
//			Debug("%s", host->hostSub.addr.S());
//			host->hostSub.portNo = portNo;
//			sprintf(host->hostSub.u.userName, "user_%03d", i);
//			sprintf(host->hostSub.u.hostName, "host_%03d", i);
//			sprintf(host->groupName, "group_%03d", i);
//			sprintf(host->nickName, "nick_%03d", i);
//			host->hostStatus = IPMSG_CAPUTF8OPT|IPMSG_ENCRYPTOPT|IPMSG_CAPFILEENCOPT|IPMSG_FILEATTACHOPT;
//			host->updateTime = host->updateTimeDirect = t - 180;
//			host->active = TRUE;
//			allHosts.AddHost(host);
//		}
#endif
	}
#endif

#if 0
// 署名速度確認
	DynBuf	b(1500);
	IPDict	d;
	d.put_bytes("data", b, 1500);

	TTick	t;
	for (int i=0; i < 5000; i++) {
	//	memset(b.Buf(), i, 1500);
		msgMng->SignIPDict(&d);
	}
	Debug("elaps=%.2f\n", t.elaps() / 1000.0);
#endif

	switch (fwMode) {
	case FW_NEED:
		PostMessage(WM_IPMSG_DELAY_FWDLG, 0, 0);
		break;

	case FW_PENDING:
		SetTimer(IPMSG_FWCHECK_TIMER, 15000); // 15sec
		break;

	default:
		break;
	}

	return	TRUE;
}

void TMainWin::LoadStoredPacket()
{
	for (int i=0; i < cfg->RecvMax; i++) {
		char		head[MAX_LISTBUF];
		char		auto_saved[MAX_PATH_U8];
		ULONG		img_base;
		DynMsgBuf	dmsg;
		MsgBuf		&msg = *dmsg.msg;

		if (!cfg->LoadPacket(msgMng, i, &msg, head, &img_base, auto_saved)) {
			break;
		}
		if (cfg->viewEpoch == 0 || msg.timestamp < cfg->viewEpoch) {
			cfg->viewEpoch = msg.timestamp;
		}
		if (!RecvDlgOpen(&msg, head, img_base, auto_saved)) {
			cfg->DeletePacket(&msg);
		}
	}
}

void TMainWin::RemoveMessageFilters()
{
	TChangeWindowMessageFilter(WM_DROPFILES, 1);
	TChangeWindowMessageFilter(WM_COPYDATA, 1);
	TChangeWindowMessageFilter(WM_COPYGLOBALDATA, 1);
	TChangeWindowMessageFilter(WM_CLOSE, 1);

	TChangeWindowMessageFilter(WM_IPMSG_CMDVER, 1);
	TChangeWindowMessageFilter(WM_IPMSG_CMDVERRES, 1);
	TChangeWindowMessageFilter(WM_IPMSG_CMD, 1);
	TChangeWindowMessageFilter(WM_IPMSG_CMDRES, 1);
	TChangeWindowMessageFilter(WM_CMDWIN_MAIN, 1);
}


/*
	Task Managerなどからの終了要求
*/
BOOL TMainWin::EvDestroy(void)
{
	Terminate();
	return	TRUE;
}

/*
	Timer Callback
	NonPopup受信時の icon反転
	ListGet Modeにおける、HostList要求/受信処理
*/
BOOL TMainWin::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (terminateFlg) {
		KillTimer(timerID);
		return	TRUE;
	}

/*	if (timerID == 0x0200) {
		KillTimer(timerID);
		LogOpen();
		return	TRUE;
	}
*/
	switch (timerID) {
	case IPMSG_REVERSEICON:
		ReverseIcon(FALSE);
		return	TRUE;

	case IPMSG_LISTGET_TIMER:
		KillTimer(IPMSG_LISTGET_TIMER);
		entryTimerStatus = 0;

		if (entryStartTime != IPMSG_GETLIST_FINISH) {
			entryStartTime = time(NULL);
			if (SetTimer(IPMSG_LISTGETRETRY_TIMER, cfg->ListGetMSec)) {
				entryTimerStatus = IPMSG_LISTGETRETRY_TIMER;
			}
			BroadcastEntry(IPMSG_BR_ISGETLIST2 | IPMSG_RETRYOPT);
		}
		return	TRUE;

	case IPMSG_LISTGETRETRY_TIMER:
		KillTimer(IPMSG_LISTGETRETRY_TIMER);
		entryTimerStatus = 0;

		if (entryStartTime != IPMSG_GETLIST_FINISH) {
			entryStartTime = IPMSG_GETLIST_FINISH;
			if (cfg->ListGet) {
				BroadcastEntry(IPMSG_BR_ENTRY);
			}
		}
		return	TRUE;

	case IPMSG_ANS_TIMER:
		KillTimer(IPMSG_ANS_TIMER);
		ansTimerID = 0;
		ExecuteAnsQueue();
		return	TRUE;

	case IPMSG_CLEANUP_TIMER:
		CleanupProc();
		return	TRUE;

#ifndef IPMSG_PRO
	case IPMSG_CLEANUPDIRTCP_TIMER:
		CleanupDirTcp();
		return	TRUE;
#endif

	case IPMSG_BALLOON_RECV_TIMER:
	case IPMSG_BALLOON_OPEN_TIMER:
	case IPMSG_BALLOON_INST_TIMER:
		KillTimer(timerID);
		if (timerID == IPMSG_BALLOON_RECV_TIMER && trayMode == TRAY_RECV ||
			timerID == IPMSG_BALLOON_OPEN_TIMER && trayMode == TRAY_OPENMSG ||
			timerID == IPMSG_BALLOON_INST_TIMER &&
				(trayMode == TRAY_INST || trayMode == TRAY_UPDATE)) {
			trayMode = TRAY_NORMAL;
			BalloonWindow(TRAY_NORMAL);
		}
		return	TRUE;

	case IPMSG_BALLOON_DELAY_TIMER:
		if (::GetForegroundWindow()) {
			KillTimer(timerID);
			if (BalloonWindow(TRAY_OPENMSG, trayMsg, LoadStrU8(IDS_DELAYOPEN),
								cfg->OpenMsgTime + IPMSG_DELAYMSG_OFFSETTIME)) {
				*trayMsg = 0;
			}
		}
		return	TRUE;

	case IPMSG_BALLOON_RESET_TIMER:
		KillTimer(timerID);
		::SystemParametersInfo(SPI_SETMESSAGEDURATION, 0, (void *)(UINT_PTR)msgDuration, 0);
		return	TRUE;

	case IPMSG_ENTRY_TIMER:
		KillTimer(IPMSG_ENTRY_TIMER);
		KillTimer(IPMSG_DELAYENTRY_TIMER);

		for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
			BroadcastEntrySub(brobj->Addr(), portNo, IPMSG_BR_ENTRY);
		}

		for (auto obj = cfg->DialUpList.TopObj(); obj; obj = cfg->DialUpList.NextObj(obj)) {
			BroadcastEntrySub(obj->addr, obj->portNo, IPMSG_BR_ENTRY);
		}
		return	TRUE;

	case IPMSG_DELAYENTRY_TIMER:
		if (TryBroadcastForResume()) {
			KillTimer(IPMSG_DELAYENTRY_TIMER);
		}
		return	TRUE;

	case IPMSG_FIRSTRUN_TIMER:
		KillTimer(IPMSG_FIRSTRUN_TIMER);
		FirstSetup();
		return	TRUE;

	case IPMSG_FWCHECK_TIMER:
		FwCheckProc();
		return	TRUE;

#ifdef IPMSG_PRO
#define MAIN_EVTIMER
#include "mainext.dat"
#undef  MAIN_EVTIMER
#else
	case IPMSG_POLL_TIMER:
		PollSend();
		return	TRUE;

	case IPMSG_BRDIR_TIMER:
		ReplyDirBroadcast();
		return	TRUE;

	case IPMSG_DIR_TIMER:
		DirTimerMain();
		return	TRUE;

//	case IPMSG_UPDATE_TIMER:
//		UpdateProc();
//		return	TRUE;

#endif
	}

	return	FALSE;
}

/*
	WM_COMMAND CallBack
*/
BOOL TMainWin::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case MENU_SETUP:
		MiscDlgOpen(setupDlg);
		return	TRUE;

	case MENU_LOGVIEWER:
		LogViewOpen();
		return	TRUE;

	case MENU_LOGOPEN:
		LogOpen();
		return	TRUE;

	case MENU_LOGIMGOPEN:
		{
			char	path[MAX_PATH_U8];
			if (MakeImageFolderName(cfg, path)) {
				MakeDirAllU8(path);
				ShellExecuteU8(NULL, NULL, path, 0, 0, SW_SHOW);
			}
		}
		return	TRUE;

	case MENU_DLLINKS:
		{
			WCHAR	path[MAX_PATH_U8];
			if (MakeDownloadLinkRootDirW(cfg, path)) {
				MakeDirAllW(path);
				ShellExecuteW(NULL, NULL, path, 0, 0, SW_SHOW);
			}
		}
		return	TRUE;

	case MENU_AUTOSAVE:
		{
			char	path[MAX_PATH_U8];
			if (MakeAutoSaveDir(cfg, path)) {
				MakeDirAllU8(path);
				ShellExecuteU8(NULL, NULL, path, 0, 0, SW_SHOW);
			}
		}
		return	TRUE;

	case MENU_ABOUT:
		MiscDlgOpen(aboutDlg);
		return	TRUE;

	case MENU_SHARE:
		MiscDlgOpen(shareStatDlg);
		return	TRUE;

	case MENU_ACTIVEWIN:
		ActiveChildWindow(activeToggle = TRUE);
		return	TRUE;

	case MENU_ABSENCE:
		cfg->AbsenceCheck = !cfg->AbsenceCheck;
		BroadcastEntry(IPMSG_BR_NOTIFY);
		SetIcon(cfg->AbsenceCheck);
		return	TRUE;

	case MENU_ABSENCEEX:
		MiscDlgOpen(absenceDlg);
		return	TRUE;

	case MENU_HELP:
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), "#usage");
		return	TRUE;

	case MENU_HELP_HISTORY:
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), "#history");
		return	TRUE;

	case MENU_HELP_LOGVIEW:
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), "#logview");
		return	TRUE;

	case MENU_HELP_TIPS:
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), "#tips");
		return	TRUE;

	case HIDE_ACCEL:
		PostMessage(WM_HIDE_CHILDWIN, 0, 0);
		return	TRUE;

	case MISC_ACCEL:
		msgList.DeleteListDlg();
		return	TRUE;

	case MENU_OPENHISTDLG:
		histDlg->SetMode(histDlg->UnOpenedNum() ? FALSE : TRUE);
		MiscDlgOpen(histDlg);
		return	TRUE;

	case MENU_SUPPORTBBS:
		ShellExecuteU8(NULL, NULL, LoadStrU8(IDS_SUPPORTBBS), 0, 0, SW_SHOW);
		return	TRUE;

	case MENU_BINFOLDER:
		ShellExecuteU8(NULL, NULL, cfg->execDir, 0, 0, SW_SHOW);
		return	TRUE;

	case MENU_LOGFOLDER:
		{
			char	dir[MAX_PATH_U8] = "";
			if (cfg->GetBaseDir(dir)) {
				ShellExecuteU8(NULL, NULL, dir, 0, 0, SW_SHOW);
			}
		}
		return	TRUE;

#ifndef IPMSG_PRO
	case MENU_UPDATE:
		UpdateCheck(UPD_CONFIRM|UPD_BUSYCONFIRM|UPD_SHOWERR|UPD_SKIPNOTIFY);
		return	TRUE;
#endif

	case MENU_URL:
		ShellExecuteU8(NULL, NULL, LoadStrU8(IDS_IPMSGURL), 0, 0, SW_SHOW);
		return	TRUE;

	case MENU_BETAURL:
		ShellExecuteU8(NULL, NULL, LoadStrU8(IDS_IPMSGBETAURL), 0, 0, SW_SHOW);
		return	TRUE;

	case MENU_EXIT:
	case IDCANCEL:
		TApp::Exit(0);
		return	TRUE;

	default:
		if (wID >= (UINT)MENU_ABSENCE_START && wID < (UINT)MENU_ABSENCE_START + cfg->AbsenceMax) {
			cfg->AbsenceChoice = wID - MENU_ABSENCE_START;
			cfg->AbsenceCheck = FALSE;
			EvCommand(0, MENU_ABSENCE, 0);
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	System Menu Callback
*/
BOOL TMainWin::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	switch (uCmdType) {
	case SC_RESTORE: case SC_MAXIMIZE:
		if (cfg->TaskbarUI) {
			if (!PopupCheck()) {
				SendDlgOpen();
			}
		}
		return	TRUE;

	case MENU_SETUP:
	case MENU_LOGVIEWER:
	case MENU_LOGOPEN:
	case MENU_LOGIMGOPEN:
	case MENU_DLLINKS:
	case MENU_AUTOSAVE:
	case MENU_ABOUT:
	case MENU_SHARE:
	case MENU_ACTIVEWIN:
	case MENU_OPENHISTDLG:
	case MENU_ABSENCE:
	case MENU_ABSENCEEX:
	case MENU_EXIT:
	case MENU_SUPPORTBBS:
	case MENU_HELP_TIPS:
		return	EvCommand(0, (WORD)uCmdType, 0);

	default:
		if (uCmdType >= MENU_ABSENCE_START &&
			(int)uCmdType < MENU_ABSENCE_START + cfg->AbsenceMax) {
			return	EvCommand(0, (WORD)uCmdType, 0);
		}
		break;
	}
	return	FALSE;
}

/*
	Logout時などの終了通知 CallBack
*/
BOOL TMainWin::EvEndSession(BOOL nSession, BOOL nLogOut)
{
	if (nSession) Terminate();
	return	TRUE;
}

/*
	サスペンド等のイベント通知
*/
BOOL TMainWin::EvPowerBroadcast(WPARAM pbtEvent, LPARAM pbtData)
{
	switch (pbtEvent) {
	case PBT_APMSUSPEND:
		lastExitTick = GetTick();
		BroadcastEntry(IPMSG_BR_EXIT);
		break;

	case PBT_APMRESUMESUSPEND:
		SetTimer(IPMSG_DELAYENTRY_TIMER, IPMSG_DELAYENTRY_SPAN);
		lastExitTick = 0;
		break;

	case PBT_POWERSETTINGCHANGE:
		if (POWERBROADCAST_SETTING *pbs = (POWERBROADCAST_SETTING *)pbtData) {
			if (pbs->PowerSetting == GUID_MONITOR_POWER_ON) {
				Debug("GUID_MONITOR_POWER_ON=%d\n", pbs->Data[0]);
				monitorState = pbs->Data[0];
			}
//			else if (pbs->PowerSetting == GUID_CONSOLE_DISPLAY_STATE) {
//				Debug("GUID_CONSOLE_DISPLAY_STATE=%d\n", pbs->Data[0]);
//			}
		}
		break;
	}
	return	TRUE;
}

BOOL TMainWin::TryBroadcastForResume()
{
	auto brobj = brListOrg.TopObj();

	if (!brobj || BroadcastEntrySub(brobj->Addr(), portNo, IPMSG_NOOPERATION)) {
		Debug("TryBroadcastForResume br_obj=%s success\n", brobj ? brobj->addr.S() : "null");
		BroadcastEntry(IPMSG_BR_ENTRY);
		return	TRUE;
	}

	Debug("TryBroadcastForResume br_obj=%s retry...\n", brobj ? brobj->addr.S() : "null");
	return	FALSE;
}

/*
	iconを通常Windowに戻してよいかどうかの問い合わせ CallBack
*/
BOOL TMainWin::EvQueryOpen(void)
{
	if (cfg->TaskbarUI) SendDlgOpen();
	return	TRUE;
}

BOOL TMainWin::EvHotKey(int hotKey)
{
	switch (hotKey) {
	case WM_SENDDLG_OPEN:
	case WM_DELMISCDLG:
	case WM_RECVDLG_OPEN:
	case WM_LOGVIEW_OPEN:
		return	PostMessage(hotKey, 0, 0), TRUE;
	}
	return	FALSE;
}

/*
	MouseButton Event CallBack
*/
BOOL TMainWin::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg) {
//	case WM_RBUTTONDOWN:
//	case WM_NCRBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_NCRBUTTONUP:
		Popup(MAIN_MENU);
		return	TRUE;

	case WM_LBUTTONDBLCLK:
	case WM_NCLBUTTONDBLCLK:
		if (!cfg->OneClickPopup) {
			SendDlgOpen();
		}
		return	TRUE;

	case WM_LBUTTONDOWN:
	case WM_NCLBUTTONDOWN:
		if (reverseTimerStatus && cfg->OneClickPopup) {
			PopupCheck();
			break;
		}

		SetForegroundWindow();

		BOOL ctl_on = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
		BOOL shift_on = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
		BOOL menu_on = (GetAsyncKeyState(VK_MENU) & 0x8000) ? TRUE : FALSE;

//		if (ctl_on && !menu_on)
//			return	PostMessage(WM_COMMAND, MENU_ABSENCE, 0), TRUE;
		if (shift_on && !menu_on) {
			return PostMessage(WM_COMMAND, MENU_ACTIVEWIN, 0), TRUE;
		}

		if (cfg->TaskbarUI) {
			msgList.ActiveListDlg();
		}

		for (auto dlg = sendList.TopObj(); dlg; dlg = sendList.NextObj(dlg)) {
			if (dlg->IsSending()) {
				dlg->SetForegroundWindow();	// 再送信確認ダイアログを前に
			}
		}

		if (PopupCheck()) {
			return TRUE;
		}

		if (cfg->OneClickPopup) {
			PostMessage(WM_SENDDLG_OPEN, 0, 0);
		}

		return	FALSE;
	}
	return	FALSE;
}


/*
	Menu Event CallBack
*/
BOOL TMainWin::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	switch (uMsg) {
	case WM_INITMENU:
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainWin::AddAbsenceMenu(HMENU hTargetMenu, int insertOffset)
{
	HMENU	hSubMenu = ::CreateMenu();
	UINT	index = ::GetMenuItemCount(hTargetMenu) - insertOffset;

	if (hSubMenu == NULL) {
		return FALSE;
	}

	for (int i=cfg->AbsenceMax -1; i >= 0; i--) {
		AppendMenuU8(hSubMenu, MF_STRING, MENU_ABSENCE_START + i, cfg->AbsenceHead[i]);
	}
	AppendMenuU8(hSubMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hSubMenu, MF_STRING, MENU_ABSENCEEX, LoadStrU8(IDS_ABSENCESET));
	InsertMenuU8(hTargetMenu, index++, MF_BYPOSITION|MF_POPUP, (UINT_PTR)hSubMenu,
					LoadStrU8(IDS_ABSENCEMENU));

	if (cfg->AbsenceCheck) {
		char buf[MAX_BUF];
		snprintfz(buf, sizeof(buf), "%s(%s)",
				LoadStrU8(IDS_ABSENCELIFT), cfg->AbsenceHead[cfg->AbsenceChoice]);
		InsertMenuU8(hTargetMenu, index, MF_BYPOSITION|MF_STRING, MENU_ABSENCE, buf);
	}
	return	TRUE;
}

/*
	App定義 Event CallBack
*/
BOOL TMainWin::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_SENDDLG_OPEN:
		SendDlgOpen((DWORD)wParam, (ReplyInfo *)lParam);
		return	TRUE;

	case WM_SENDDLG_EXIT:
		SendDlgExit((DWORD)lParam);
		return	TRUE;

	case WM_SENDDLG_HIDE:
		SendDlgHide((DWORD)lParam);
		return	TRUE;

	case WM_SENDDLG_FONTCHANGED:
		if (histDlg->hWnd) {
			histDlg->SetFont();
			::InvalidateRect(histDlg->hWnd, NULL, TRUE);
		}
		return	TRUE;

	case WM_RECVDLG_OPEN:
		PopupCheck();
		return	TRUE;

	case WM_RECVDLG_EXIT:
		RecvDlgExit((DWORD)lParam);
		return	TRUE;

	case WM_RECVDLG_BYVIEWER:
		{
			MsgIdent	*mi = (MsgIdent *)lParam;
			RecvDlgOpenByViewer(mi, (wParam & RDLG_FILESAVE) ? TRUE : FALSE);
			if (wParam & RDLG_FREE_MI) {
				delete mi;
			}
		}
		return	TRUE;

	case WM_NOTIFY_TRAY:		// TaskTray
		switch (lParam) {
		case NIN_BALLOONHIDE:
			return	TRUE;

		case NIN_BALLOONTIMEOUT:
			trayMode = TRAY_NORMAL;
			return	TRUE;

		case NIN_BALLOONSHOW:
			return	TRUE;

		case NIN_BALLOONUSERCLICK:
			ToastHide();
			if (trayMode == TRAY_INST) {
				PostMessage(WM_COMMAND, MENU_HELP_HISTORY, 0);
			}
#ifndef IPMSG_PRO
			else if (trayMode == TRAY_UPDATE) {
				ShowUpdateDlg();
			}
#endif
			else if (trayMode != TRAY_OPENMSG) {
				SendMessage(WM_RECVDLG_OPEN, 0, 0);
			}
			trayMode = TRAY_NORMAL;
			return	TRUE;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			ToastHide();
			break;

		default:
			break;
		}
		PostMessage((UINT)lParam, 0, 0);
		return	TRUE;

	case WM_HIDE_CHILDWIN:
		ActiveChildWindow(activeToggle = !activeToggle);
		return	TRUE;

	case WM_UDPEVENT:
		while (UdpEvent(lParam))
			;
		return	TRUE;

	case WM_TCPEVENT:
		TcpEvent(wParam, lParam);
		return	TRUE;

#ifndef IPMSG_PRO
	case WM_IPMSG_UPDATERES:
		UpdateCheckRes((TInetReqReply *)lParam);
		return	TRUE;

	case WM_IPMSG_UPDATEDLRES:
		UpdateDlRes((TInetReqReply *)lParam);
		return	TRUE;

	case WM_TCPDIREVENT:
		TcpDirEvent(wParam, lParam);
		return	TRUE;
#endif

	case WM_STOPTRANS:
		StopSendFile((int)lParam);
		return	TRUE;

	case WM_REFRESH_HOST:
		RefreshHost((BOOL)wParam);
		return	TRUE;

	case WM_MSGDLG_EXIT:
		MsgDlgExit((DWORD)lParam);
		return	TRUE;

	case WM_DELMISCDLG:
		if (cfg->HotKeyCheck >= 2 && cfg->OpenCheck < 2) {
			LockWorkStation();
		}
		else if (msgList.TopObj()) {
			msgList.DeleteListDlg();
		}
		return	TRUE;

	case WM_IPMSG_INITICON:
		InitIcon();
		SetIcon(cfg->AbsenceCheck);
		return	TRUE;

	case WM_IPMSG_CHANGE_MCAST:
		ChangeMulticastMode((int)wParam);
		return	TRUE;

	case WM_HISTDLG_OPEN:
		if (wParam == 1) histDlg->SetMode(TRUE);
		MiscDlgOpen(histDlg);
		return	TRUE;

	case WM_HISTDLG_HIDE:
		histDlg->Show(SW_HIDE);
		return	TRUE;

	case WM_HISTDLG_NOTIFY:
		{
			HistNotify *hn = (HistNotify *)wParam;
			histDlg->SendNotify(hn->hostSub, hn->packetNo, hn->msg);
			SetCaption();
		}
		return	TRUE;

	case WM_FORCE_TERMINATE:
		TaskTray(NIM_DELETE);
		::ExitProcess(0xffffffff);
		return	TRUE;

	case WM_IPMSG_IMECTRL:
		ControlIME((TWin *)lParam, (BOOL)wParam);
		return	TRUE;

	case WM_IPMSG_BRNOTIFY:
		if (lParam == IPMSG_DEFAULT_PORT) {
			BroadcastEntry(IPMSG_BR_NOTIFY);
		}
		return	TRUE;

	case WM_IPMSG_REMOTE:
		remoteDlg->Start((TRemoteDlg::Mode)wParam);
		return	TRUE;

	case WM_LOGVIEW_UPDATE:
		for (auto view=logViewList.TopObj(); view; view=logViewList.NextObj(view)) {
			view->UpdateLog(MakeMsgId(wParam, lParam));
		}
		return	TRUE;

	case WM_LOGVIEW_RESETCACHE:
		for (auto view=logViewList.TopObj(); view; view=logViewList.NextObj(view)) {
			if ((TLogView *)lParam != view) {
				view->ResetCache();
			}
		}
		return	TRUE;

	case WM_LOGVIEW_RESETFIND:
		for (auto view=logViewList.TopObj(); view; view=logViewList.NextObj(view)) {
			if ((TLogView *)lParam != view) {
				view->ResetFindBodyCombo();
			}
		}
		return	TRUE;

	case WM_RECVDLG_READCHECK:
		{
			MsgIdent	*mi = (MsgIdent *)lParam;
			logmng->ReadCheckStatus(mi, TRUE, FALSE);
			if (wParam & 1) {
				delete mi;
			}
		}
		return	TRUE;

	case WM_LOGVIEW_OPEN:
		ToastHide();
		if (wParam == 0) {
			if ((cfg->logViewAtRecv && cfg->LogCheck) && reverseTimerStatus) {
				PopupCheck();
			}
			else {
				LogViewOpen(TRUE);
			}
		}
		else {	// new logviewer
			TLogView	*view = new TLogView(cfg, logmng, FALSE);
			Wstr		uid;
			if (lParam) {
				uid.Init((const char *)lParam);
			}
			view->Create();
			if (uid) {
				view->SetUser(uid);
			}
			logViewList.AddObj(view);
		}
		return	TRUE;

//	case WM_LOGVIEW_PRECLOSE:
//		if (auto *view = (TLogView *)lParam) {
//			for (auto *dlg = sendList.TopObj(); dlg; dlg = sendList.NextObj(dlg)) {
//				dlg->DetachParent(view->hWnd);
//			}
//		}
//		return	TRUE;

	case WM_LOGVIEW_CLOSE:
		if (auto view = logViewList.TopObj()) {	// 2番目以降のみ削除
			while ((view = logViewList.NextObj(view))) {
				if (view == (TLogView *)lParam) {
					logViewList.DelObj(view);
					delete view;
					break;
				}
			}
		}
		return	TRUE;

	case WM_LOGFETCH_DONE:
		if (LogDb *logDb = logmng->GetLogDb()) {
			VBVec<MsgVec>	dummyVec;
			dummyVec.Init(0, 256, 256);
			logDb->PostInit();
			logDb->SelectMsgIdList(&dummyVec, FALSE, 0);
		}
		return	TRUE;

	case WM_IPMSG_SETUPDLG:
		MiscDlgOpen(setupDlg);
		setupDlg->SetSheet((int)wParam);
		return	TRUE;

	case WM_IPMSG_SLACKRES:
		{
			auto	*irr = (TInetReqReply *)lParam;
			Debug("slack ret=%d reply=%s err=%s\n", irr->code, irr->reply.s(), irr->errMsg.s());
			delete irr;
		}
		return	TRUE;

	case WM_IPMSG_DELAY_FWDLG:
		if (!fwDlg) {
			fwDlg = new TFwDlg(this);
			fwDlg->Create();
			fwDlg->SetForceForegroundWindow();
		}
		return	TRUE;

	case WM_IPMSG_SETFWRES:
		if (wParam) {
			FwStatus	fs;
			GetFwStatusEx(NULL, &fs);

			if (fs.IsAllowed()) {
				fwMode = FW_OK;
				BroadcastEntry(IPMSG_BR_NOTIFY);
			}
		}
		delete fwDlg;
		fwDlg = NULL;
		return	TRUE;

	case WM_IPMSG_CMD:
		Cmd((HWND)wParam);
		return	TRUE;

	case WM_IPMSG_CMDVER:
		return	IPMSG_CMD_VER1; // ...

	case WM_IPMSG_HELP:
		hHelp = ShowHelpU8(0, cfg->execDir, LoadStrU8(IDS_IPMSGHELP), (char *)lParam);
		return	TRUE;

#ifdef IPMSG_PRO
#define MAIN_EVAPP
#include "mainext.dat"
#undef  MAIN_EVAPP

#else
	case WM_IPMSG_DIRMODE_SHEET:
		MakeBrListEx();
		PollSend();

		if (cfg->DirMode == DIRMODE_MASTER) {
			isFirstBroad = TRUE;
			EntryHost();
			isFirstBroad = FALSE;

			if (dirPhase == DIRPH_NONE) {
				dirPhase = DIRPH_START;
				dirTick = 0;
				SetTimer(IPMSG_DIR_TIMER, DIR_BASE_TICK);
				ClearDirAll();
			}
			DirRefreshAgent();
			DirSendAgentBroad(TRUE);
		}
		return	TRUE;

//	case WM_IPMSG_SVRDETECT:
//		if (cfg->DirMode == DIRMODE_MASTER) {
//			if (dirPhase == DIRPH_NONE) {
//				dirPhase = DIRPH_START;
//				SetTimer(IPMSG_DIR_TIMER, 1000);
//			}
//		}
//		return	MasterDetect((const char *)wParam);
//		return	TRUE;

#endif

	}
	return	FALSE;
}

BOOL TMainWin::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == TaskBarCreateMsg) {
		TaskTray(NIM_ADD, hCycleIcon[0], IP_MSG);
		SetIcon(cfg->AbsenceCheck);
		SetCaption();
		return	TRUE;
	}
	else if (uMsg == TaskBarButtonMsg) {
//		Debug("TaskBarButtonMsg wParam=%x lParam=%d\n", wParam, lParam);
		return	TRUE;
	}
	else if (uMsg == TaskBarNotifyMsg) {
		Popup(MAIN_MENU);
		return	TRUE;
	}
	return	FALSE;
}

/*
	UDP Packet 受信処理
*/
BOOL TMainWin::UdpEvent(LPARAM lParam)
{
	DynMsgBuf	dmsg;

	if (WSAGETSELECTERROR(lParam) || !msgMng->Recv(dmsg.msg)) {
		return	FALSE;
	}

	return	UdpEventCore(dmsg.msg);
}

BOOL TMainWin::UdpEventCore(MsgBuf *msg)
{
	ULONG	cmd = GET_MODE(msg->command);

	if (cfg->PriorityReject) {
		Host *host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
		if (host && (host->priority < 0 || cfg->PriorityReject >= 2 && host->priority == 0) &&
			!IsSameHost(&msg->hostSub, msgMng->GetLocalHost())) {

			if (cmd != IPMSG_BR_EXIT && cmd != IPMSG_NOOPERATION) {
				BroadcastEntrySub(&msg->hostSub, IPMSG_BR_EXIT);
			}
			if (cmd != IPMSG_ANSENTRY) {
				return	TRUE;
			}
		}
	}

	switch (cmd) {
	case IPMSG_BR_ENTRY:
		MsgBrEntry(msg);
		return	TRUE;

	case IPMSG_BR_EXIT:
		MsgBrExit(msg);
		return	TRUE;

	case IPMSG_ANSENTRY:
		MsgAnsEntry(msg);
		return	TRUE;

	case IPMSG_BR_ABSENCE: // == IPMSG_BR_NOTIFY
		MsgBrAbsence(msg);
		return	TRUE;

	case IPMSG_SENDMSG:
		MsgSendMsg(msg);
		break;

	case IPMSG_RECVMSG:
		MsgRecvMsg(msg);
		break;

	case IPMSG_READMSG:
	case IPMSG_ANSREADMSG:
		MsgReadMsg(msg);
		break;

	case IPMSG_BR_ISGETLIST2:
		MsgBrIsGetList(msg);
		return	TRUE;

	case IPMSG_OKGETLIST:
		MsgOkGetList(msg);
		return	TRUE;

	case IPMSG_GETLIST:
		MsgGetList(msg);
		return	TRUE;

	case IPMSG_ANSLIST:
		MsgAnsList(msg);
		return	TRUE;

#ifndef IPMSG_PRO
	case IPMSG_ANSLIST_DICT:
		MsgAnsListDict(msg);
		return	TRUE;
#endif

	case IPMSG_GETINFO:
		MsgGetInfo(msg);
		break;

	case IPMSG_SENDINFO:
		MsgSendInfo(msg);
		break;

	case IPMSG_GETPUBKEY:
		MsgGetPubKey(msg);
		break;

	case IPMSG_ANSPUBKEY:
		MsgAnsPubKey(msg);
		break;

	case IPMSG_GETABSENCEINFO:
		MsgGetAbsenceInfo(msg);
		break;

	case IPMSG_SENDABSENCEINFO:
		MsgSendAbsenceInfo(msg);
		break;

	case IPMSG_RELEASEFILES:
		MsgReleaseFiles(msg);
		break;

#ifdef IPMSG_PRO
#define MAIN_UDPEV
#include "mainext.dat"
#undef  MAIN_UDPEV
#else
	case IPMSG_DIR_BROADCAST:
		MsgDirBroadcast(msg);
		break;

	case IPMSG_DIR_POLLAGENT:
		MsgDirPollAgent(msg);
		break;

	case IPMSG_DIR_AGENTREJECT:
		MsgDirAgentReject(msg);
		break;

	case IPMSG_DIR_REQUEST:
		MsgDirRequest(msg);
		break;

	case IPMSG_DIR_PACKET:
		MsgDirPacket(msg);
		break;

	case IPMSG_DIR_AGENTPACKET:
		MsgDirAgentPacket(msg);
		break;

	case IPMSG_DIR_POLL:
		MsgDirPoll(msg);
		break;

	case IPMSG_DIR_ANSBROAD:
		MsgDirAnsBroad(msg, TRUE);
		break;

	case IPMSG_DIR_EVBROAD:
		MsgDirAnsBroad(msg, FALSE);
		break;

#endif

	}

	packetLog[packetLogCnt].no = msg->packetNo;
	packetLog[packetLogCnt].u  = msg->hostSub.u;
	packetLogCnt = (packetLogCnt + 1) % MAX_PACKETLOG;

	return	TRUE;
}

BOOL TMainWin::TcpEvent(SOCKET sd, LPARAM lParam)
{
	if (WSAGETSELECTERROR(lParam))
		return	FALSE;

	switch (LOWORD(lParam)) {
	case FD_ACCEPT:
		{
			ConnectInfo *info = new ConnectInfo;

			if (msgMng->Accept(hWnd, info)) {
				info->startTick = GetTick();
				connList.AddObj(info);
				info->buf.Alloc(MAX_UDPBUF);
			}
			else {
				delete info;
			}
		}
		break;

	case FD_READ:
		RecvTcpMsg(sd);
		break;

	case FD_CLOSE:
		{
			SendFileObj *obj;
			if ((obj = FindSendFileObj(sd))) {
				EndSendFile(obj);
			}
			else {
				for (auto conInfo=connList.TopObj(); conInfo; conInfo=connList.NextObj(conInfo)) {
					if (conInfo->sd == sd) {
						Debug("fd_close\n");
						char c;
						::recv(sd, &c, 1, 0);
						connList.DelObj(conInfo);
						break;
					}
				}
				::closesocket(sd);
			}
		}
		break;
	}
	return	TRUE;
}

