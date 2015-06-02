static char *mainwin_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   mainwin.cpp	Ver3.21";
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: Main Window
	Create					: 1996-06-01(Sat)
	Update					: 2011-06-27(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include "resource.h"
#include "ipmsg.h"

HICON		TMainWin::hMainIcon = NULL;
HICON		TMainWin::hRevIcon = NULL;
TMainWin	*TMainWin::mainWin = NULL;
static HWND	hMainWnd = NULL;

/*
	MainWindow の初期化
*/
TMainWin::TMainWin(ULONG nicAddr, int _portNo, TWin *_parent) : TWin(_parent)
{
	char	title[100];
	sprintf(title, "IPMsg ver%s", GetVersionStr());
	InstallExceptionFilter(title, GetLoadStr(IDS_EXCEPTIONLOG));

	hosts.Enable(THosts::NAME, TRUE);
	hosts.Enable(THosts::ADDR, TRUE);
	cfg = new Cfg(nicAddr, portNo = _portNo);
	if (!(msgMng = new MsgMng(nicAddr, portNo, cfg))->GetStatus())
	{
		::ExitProcess(0xffffffff);
		return;
	}
	cfg->ReadRegistry();
	if (cfg->lcid > 0) TSetDefaultLCID(cfg->lcid);

	setupDlg = new TSetupDlg(cfg, &hosts);
	aboutDlg = new TAboutDlg();
	absenceDlg = new TAbsenceDlg(cfg, this);
	logmng = new LogMng(cfg);
	ansList = new TRecycleList(MAX_ANSLIST, sizeof(AnsQueueObj));
	shareMng = new ShareMng(cfg);
	shareStatDlg = new TShareStatDlg(shareMng, cfg);
	histDlg = new THistDlg(cfg, &hosts);
	refreshStartTime = 0;
	entryStartTime = IPMSG_GETLIST_FINISH;

	memset(packetLog, 0, sizeof(packetLog));
	packetLogCnt = 0;

	entryTimerStatus = 0;
	reverseTimerStatus = 0;
	reverseCount = 0;
	ansTimerID = 0;
	terminateFlg = FALSE;
	activeToggle = TRUE;
	writeRegFlags = CFG_HOSTINFO|CFG_DELCHLDHOST|CFG_LRUUSER;
	trayMode = TRAY_NORMAL;
	*trayMsg = 0;

	InitIcon();
	MakeBrListEx();

	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
}

/*
	MainWindow 終了
*/
TMainWin::~TMainWin()
{
#if 0	// 無駄
	delete msgMng;
	delete cfg;
	delete ansList;
#endif
}

/*
	子Window および socketの終了と所有Memoryの開放
*/
void TMainWin::Terminate(void)
{
	if (terminateFlg)
		return;
	terminateFlg = TRUE;

	if (!msgMng->GetStatus())
		return;

	ExitHost();
	DeleteListDlg(&sendList);
	DeleteListDlg(&recvList);
	DeleteListDlg(&msgList);
	delete histDlg;
	delete shareStatDlg;
	delete absenceDlg;
	delete aboutDlg;
	delete setupDlg;

	if (IsNewShell())
		TaskTray(NIM_DELETE);

	Time_t	now_time = Time();
	for (int cnt=0; cnt < hosts.HostCnt(); cnt++)
		hosts.GetHost(cnt)->updateTime = now_time;
	cfg->WriteRegistry(writeRegFlags);

	msgMng->CloseSocket();

	for (int i=0; i < MAX_KEY; i++) {
		if (cfg->priv[i].hKey)	pCryptDestroyKey(cfg->priv[i].hKey);
	}

#if 0	// 無駄
	delete logmng;

	Host *host;
	while (hosts.HostCnt() > 0)
	{
		hosts.DelHost(host = hosts.GetHost(0));
//		delete host;
	}

	AddrObj *brObj;
	while ((brObj = (AddrObj *)cfg->DialUpList.TopObj()))
	{
		cfg->DialUpList.DelObj(brObj);
		delete brObj;
	}
#endif
}

/*
	MainWindow 生成時の CallBack
	New Shell なら、TaskTray に icon登録、そうでないなら icon化
	Packet 受信開始、Entry Packetの送出
*/
BOOL TMainWin::EvCreate(LPARAM lParam)
{
	hMainWnd = hWnd;
	mainWin = this;

	if (IsWinVista() && TIsUserAnAdmin() && TIsEnableUAC()) {
		TChangeWindowMessageFilter(WM_DROPFILES, 1);
		TChangeWindowMessageFilter(WM_COPYDATA, 1);
		TChangeWindowMessageFilter(WM_COPYGLOBALDATA, 1);
		TChangeWindowMessageFilter(WM_CLOSE, 1);
	}

	if (!msgMng->GetStatus())
		return	TRUE;

	if (IsNewShell())
	{
		Show(SW_HIDE);
		while (!TaskTray(NIM_ADD, hMainIcon, IP_MSG))
			Sleep(1000);	// for logon script
	}
	else
		Show(SW_MINIMIZE);
	TaskBarCreateMsg= ::RegisterWindowMessage("TaskbarCreated");

	SetIcon(cfg->AbsenceCheck ? hRevIcon : hMainIcon);
	SetCaption();
	if (!SetupCryptAPI(cfg, msgMng)) MessageBoxU8("CryptoAPI can't be used. Setup New version IE");

	msgMng->AsyncSelectRegister(hWnd);
	SetHotKey(cfg);

	if (msgMng->GetStatus())
		EntryHost();

	::SetTimer(hWnd, IPMSG_CLEANUP_TIMER, 60000, NULL); // 1min
	return	TRUE;
}

/*
	Task Managerなどからの終了要求
*/
BOOL TMainWin::EvClose(void)
{
	Terminate();
	::PostQuitMessage(0);
	return	TRUE;
}

/*
	Timer Callback
	NonPopup受信時の icon反転
	ListGet Modeにおける、HostList要求/受信処理
*/
BOOL TMainWin::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (terminateFlg)
	{
		::KillTimer(hWnd, timerID);
		return	TRUE;
	}

	switch (timerID)
	{
	case IPMSG_REVERSEICON:
		ReverseIcon(FALSE);
		return	TRUE;

	case IPMSG_LISTGET_TIMER:
		::KillTimer(hWnd, IPMSG_LISTGET_TIMER);
		entryTimerStatus = 0;
		if (entryStartTime != IPMSG_GETLIST_FINISH)
		{
			entryStartTime = Time();
			if (::SetTimer(hWnd, IPMSG_LISTGETRETRY_TIMER, cfg->ListGetMSec, NULL))
				entryTimerStatus = IPMSG_LISTGETRETRY_TIMER;
			BroadcastEntry(IPMSG_BR_ISGETLIST2 | IPMSG_RETRYOPT);
		}
		return	TRUE;

	case IPMSG_LISTGETRETRY_TIMER:
		::KillTimer(hWnd, IPMSG_LISTGETRETRY_TIMER);
		entryTimerStatus = 0;
		if (entryStartTime != IPMSG_GETLIST_FINISH)
		{
			entryStartTime = IPMSG_GETLIST_FINISH;
			if (cfg->ListGet)
				BroadcastEntry(IPMSG_BR_ENTRY);
		}
		return	TRUE;

	case IPMSG_ANS_TIMER:
		::KillTimer(hWnd, IPMSG_ANS_TIMER);
		ansTimerID = 0;
		ExecuteAnsQueue();
		return	TRUE;

	case IPMSG_CLEANUP_TIMER:
		shareMng->Cleanup();
		return	TRUE;

	case IPMSG_BALLOON_RECV_TIMER:
	case IPMSG_BALLOON_OPEN_TIMER:
		::KillTimer(hWnd, timerID);
		if (timerID == IPMSG_BALLOON_RECV_TIMER && trayMode == TRAY_RECV ||
			timerID == IPMSG_BALLOON_OPEN_TIMER && trayMode == TRAY_OPENMSG) {
			trayMode = TRAY_NORMAL;
			BalloonWindow(TRAY_NORMAL);
		}
		return	TRUE;

	case IPMSG_BALLOON_DELAY_TIMER:
		if (::GetForegroundWindow()) {
			::KillTimer(hWnd, timerID);
			if (BalloonWindow(TRAY_OPENMSG, trayMsg, GetLoadStrU8(IDS_DELAYOPEN),
								IPMSG_DELAYMSG_TIME)) {
				*trayMsg = 0;
			}
		}
		return	TRUE;

	case IPMSG_ENTRY_TIMER:
		::KillTimer(hWnd, IPMSG_ENTRY_TIMER);

		for (TBrObj *brobj=brListEx.Top(); brobj; brobj=brListEx.Next(brobj))
			BroadcastEntrySub(brobj->Addr(), htons(portNo), IPMSG_BR_ENTRY);

		for (AddrObj *obj = (AddrObj *)cfg->DialUpList.TopObj(); obj; obj = (AddrObj *)cfg->DialUpList.NextObj(obj))
			BroadcastEntrySub(obj->addr, obj->portNo, IPMSG_BR_ENTRY);
		return	TRUE;
	}

	return	FALSE;
}

/*
	WM_COMMAND CallBack
*/
BOOL TMainWin::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case MENU_SETUP:
		MiscDlgOpen(setupDlg);
		return	TRUE;

	case MENU_LOGOPEN:
		LogOpen();
		return	TRUE;

	case MENU_LOGIMGOPEN:
		{
			char	path[MAX_PATH];
			if (MakeImageFolder(cfg, path)) {
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
		BroadcastEntry(IPMSG_BR_ABSENCE); 
		SetIcon(cfg->AbsenceCheck ? hRevIcon : hMainIcon);
		return	TRUE;

	case MENU_ABSENCEEX:
		MiscDlgOpen(absenceDlg);
		return	TRUE;

	case MENU_HELP:
		ShowHelpU8(hWnd, cfg->execDir, GetLoadStrU8(IDS_IPMSGHELP), "#usage");
		return	TRUE;

	case MENU_HELP_HISTORY:
		ShowHelpU8(hWnd, cfg->execDir, GetLoadStrU8(IDS_IPMSGHELP), "#history");
		return	TRUE;

	case HIDE_ACCEL:
		PostMessage(WM_HIDE_CHILDWIN, 0, 0);
		return	TRUE;

	case MISC_ACCEL:
		DeleteListDlg(&msgList);
		return	TRUE;

	case MENU_OPENHISTDLG:
		histDlg->SetMode(FALSE);
		MiscDlgOpen(histDlg);
		return	TRUE;

	case MENU_EXIT:
	case IDCANCEL:
		Terminate();
		::PostQuitMessage(0);
		return	TRUE;

	default:
		if (wID >= (UINT)MENU_ABSENCE_START && wID < (UINT)MENU_ABSENCE_START + cfg->AbsenceMax)
		{
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
	switch (uCmdType)
	{
	case SC_RESTORE: case SC_MAXIMIZE:
		if (!IsNewShell())
			SendDlgOpen();
		return	TRUE;

	case MENU_SETUP:
	case MENU_LOGOPEN:
	case MENU_LOGIMGOPEN:
	case MENU_ABOUT:
	case MENU_SHARE:
	case MENU_ACTIVEWIN:
	case MENU_OPENHISTDLG:
	case MENU_ABSENCE:
	case MENU_ABSENCEEX:
	case MENU_EXIT:
		return	EvCommand(0, uCmdType, 0);

	default:
		if (uCmdType >= MENU_ABSENCE_START && (int)uCmdType < MENU_ABSENCE_START + cfg->AbsenceMax)
			return	EvCommand(0, uCmdType, 0);
		break;
	}
	return	FALSE;
}

/*
	Logout時などの終了通知 CallBack
*/
BOOL TMainWin::EvEndSession(BOOL nSession, BOOL nLogOut)
{
	if (nSession)
		Terminate();
	return	TRUE;
}

/*
	iconを通常Windowに戻してよいかどうかの問い合わせ CallBack
*/
BOOL TMainWin::EvQueryOpen(void)
{
	if (!IsNewShell())
		SendDlgOpen();
	return	TRUE;
}

BOOL TMainWin::EvHotKey(int hotKey)
{
	switch (hotKey)
	{
	case WM_SENDDLG_OPEN:
	case WM_DELMISCDLG:
	case WM_RECVDLG_OPEN:
		return	PostMessage(hotKey, 0, 0), TRUE;
	}
	return	FALSE;
}

/*
	MouseButton Event CallBack
*/
BOOL TMainWin::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
//	case WM_RBUTTONDOWN:
//	case WM_NCRBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_NCRBUTTONUP:
		Popup(MAIN_MENU);
		return	TRUE;

	case WM_LBUTTONDBLCLK:
	case WM_NCLBUTTONDBLCLK:
		if (!cfg->OneClickPopup) SendDlgOpen();
		return	TRUE;

	case WM_LBUTTONDOWN:
	case WM_NCLBUTTONDOWN:
		if (trayMode == TRAY_RECV && cfg->OneClickPopup) {
			trayMode = TRAY_NORMAL;
			break;
		}

		SetForegroundWindow();

		BOOL ctl_on = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
		BOOL shift_on = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
		BOOL menu_on = (GetAsyncKeyState(VK_MENU) & 0x8000) ? TRUE : FALSE;

//		if (ctl_on && !menu_on)
//			return	PostMessage(WM_COMMAND, MENU_ABSENCE, 0), TRUE;
		if (shift_on && !menu_on)
			return	PostMessage(WM_COMMAND, MENU_ACTIVEWIN, 0), TRUE;

		if (!IsNewShell())
			ActiveListDlg(&msgList);

		for (TSendDlg *dlg = (TSendDlg *)sendList.TopObj(); dlg; dlg = (TSendDlg *)sendList.NextObj(dlg)) {
			if (dlg->IsSending())
				dlg->SetForegroundWindow();	// 再送信確認ダイアログを前に
		}

		if (PopupCheck())
			return	TRUE;

		if (cfg->OneClickPopup)
			PostMessage(WM_SENDDLG_OPEN, 0, 0);
		return	FALSE;
	}
	return	FALSE;
}


/*
	Menu Event CallBack
*/
BOOL TMainWin::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	switch (uMsg)
	{
	case WM_INITMENU:
		return	TRUE;
	}
	return	FALSE;
}

BOOL TMainWin::AddAbsenceMenu(HMENU hTargetMenu, int insertOffset)
{
	char	buf[MAX_LISTBUF];
	HMENU	hSubMenu = ::CreateMenu();
	UINT	index = ::GetMenuItemCount(hTargetMenu) - insertOffset;

	if (hSubMenu == NULL)
		return	FALSE;

	for (int cnt=cfg->AbsenceMax -1; cnt >= 0; cnt--)
		AppendMenuU8(hSubMenu, MF_STRING, MENU_ABSENCE_START + cnt, cfg->AbsenceHead[cnt]);
	AppendMenuU8(hSubMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hSubMenu, MF_STRING, MENU_ABSENCEEX, GetLoadStrU8(IDS_ABSENCESET));
	InsertMenuU8(hTargetMenu, index++, MF_BYPOSITION|MF_POPUP, (UINT)hSubMenu, GetLoadStrU8(IDS_ABSENCEMENU));

	if (cfg->AbsenceCheck)
	{
		wsprintf(buf, "%s(%s)", GetLoadStrU8(IDS_ABSENCELIFT), cfg->AbsenceHead[cfg->AbsenceChoice]);
		InsertMenuU8(hTargetMenu, index, MF_BYPOSITION|MF_STRING, MENU_ABSENCE, buf);
	}
	return	TRUE;
}

/*
	User定義 Event CallBack
*/
BOOL TMainWin::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SENDDLG_OPEN:
		SendDlgOpen((HWND)wParam, (MsgBuf *)lParam);
		return	TRUE;

	case WM_SENDDLG_EXIT:
		SendDlgExit((TSendDlg *)lParam);
		return	TRUE;

	case WM_SENDDLG_HIDE:
		SendDlgHide((TSendDlg *)lParam);
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
		RecvDlgExit((TRecvDlg *)lParam);
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
			if (trayMode != TRAY_OPENMSG) {
				SendMessage(WM_RECVDLG_OPEN, 0, 0);
			}
			trayMode = TRAY_NORMAL;
			return	TRUE;

		default:
			break;
		}
		PostMessage(lParam, 0, 0);
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

	case WM_REFRESH_HOST:
		RefreshHost((BOOL)wParam);
		return	TRUE;

	case WM_MSGDLG_EXIT:
		MsgDlgExit((TMsgDlg *)lParam);
		return	TRUE;

	case WM_DELMISCDLG:
		if (msgList.TopObj()) {
			DeleteListDlg(&msgList);
		}
		return	TRUE;

	case WM_IPMSG_INITICON:
		InitIcon();
		SetIcon(cfg->AbsenceCheck ? hRevIcon : hMainIcon);
		return	TRUE;

// refresh時に更新すれば十分
//	case WM_IPMSG_BRRESET:
//		MakeBrListEx();
//		return	TRUE;

	case WM_HISTDLG_OPEN:
		if (wParam == 1) histDlg->SetMode(TRUE);
		MiscDlgOpen(histDlg);
		return	TRUE;

	case WM_HISTDLG_HIDE:
		histDlg->Show(SW_HIDE);
		return	TRUE;

	case WM_HISTDLG_NOTIFY:
		histDlg->SendNotify((HostSub *)wParam, (ULONG)lParam);
		SetCaption();
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
			BroadcastEntry(IPMSG_BR_ABSENCE); 
		}
		return	TRUE;

	default:
		if (uMsg == TaskBarCreateMsg)
		{
			TaskTray(NIM_ADD, hMainIcon, IP_MSG);
			SetCaption();
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	参加 Message Packet送出
*/
void TMainWin::EntryHost(void)
{
	Time_t	now_time = Time();

	if (entryStartTime + (Time_t)cfg->ListGetMSec / 1000 > now_time)
		return;
	entryStartTime = now_time;

	if (cfg->ListGet)
	{
		if (::SetTimer(hWnd, IPMSG_LISTGET_TIMER, cfg->ListGetMSec, NULL))
			entryTimerStatus = IPMSG_LISTGET_TIMER;
		BroadcastEntry(IPMSG_BR_ISGETLIST2);
	}
	else
		BroadcastEntry(IPMSG_BR_ENTRY);
}

/*
	脱出 Message Packet送出
*/
void TMainWin::ExitHost(void)
{
	BroadcastEntry(IPMSG_BR_EXIT);
	this->Sleep(100);
	BroadcastEntry(IPMSG_BR_EXIT);
}

/*
	重複 Packet調査（簡易...もしくは手抜きとも言う(^^;）
*/				
BOOL TMainWin::IsLastPacket(MsgBuf *msg)
{
	for (int cnt=0; cnt < MAX_PACKETLOG; cnt++)
	{
		if (packetLog[cnt].addr == msg->hostSub.addr && packetLog[cnt].port == msg->hostSub.portNo && packetLog[cnt].no == msg->packetNo
		&& msg->packetNo != IPMSG_DEFAULT_PORT // 大昔の Mac版のバグ避け...
		)
		return	TRUE;
	}
	return	FALSE;
}

/*
	UDP Packet 受信処理
*/
BOOL TMainWin::UdpEvent(LPARAM lParam)
{
	MsgBuf	msg;

	if (WSAGETSELECTERROR(lParam) || !msgMng->Recv(&msg))
		return	FALSE;

	if (cfg->PriorityReject)
	{
		Host *host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
		if (host && (host->priority < 0 || cfg->PriorityReject >= 2 && host->priority == 0) &&
			!IsSameHost(&msg.hostSub, msgMng->GetLocalHost()))
		{
			ULONG	cmd = GET_MODE(msg.command);
			if (cmd != IPMSG_BR_EXIT && cmd != IPMSG_NOOPERATION)
				BroadcastEntrySub(&msg.hostSub, IPMSG_BR_EXIT);
			if (GET_MODE(msg.command) != IPMSG_ANSENTRY)
				return	TRUE;
		}
	}

	switch (GET_MODE(msg.command))
	{
	case IPMSG_BR_ENTRY:
		MsgBrEntry(&msg);
		return	TRUE;

	case IPMSG_BR_EXIT:
		MsgBrExit(&msg);
		return	TRUE;

	case IPMSG_ANSENTRY:
		MsgAnsEntry(&msg);
		return	TRUE;

	case IPMSG_BR_ABSENCE:
		MsgBrAbsence(&msg);
		return	TRUE;

	case IPMSG_SENDMSG:
		MsgSendMsg(&msg);
		break;

	case IPMSG_RECVMSG:
		MsgRecvMsg(&msg);
		break;

	case IPMSG_READMSG:
	case IPMSG_ANSREADMSG:
		MsgReadMsg(&msg);
		break;

	case IPMSG_BR_ISGETLIST2:
		MsgBrIsGetList(&msg);
		return	TRUE;

	case IPMSG_OKGETLIST:
		MsgOkGetList(&msg);
		return	TRUE;

	case IPMSG_GETLIST:
		MsgGetList(&msg);
		return	TRUE;

	case IPMSG_ANSLIST:
		MsgAnsList(&msg);
		return	TRUE;

	case IPMSG_GETINFO:
		MsgGetInfo(&msg);
		break;

	case IPMSG_SENDINFO:
		MsgSendInfo(&msg);
		break;

	case IPMSG_GETPUBKEY:
		MsgGetPubKey(&msg);
		break;

	case IPMSG_ANSPUBKEY:
		MsgAnsPubKey(&msg);
		break;

	case IPMSG_GETABSENCEINFO:
		MsgGetAbsenceInfo(&msg);
		break;

	case IPMSG_SENDABSENCEINFO:
		MsgSendAbsenceInfo(&msg);
		break;

	case IPMSG_RELEASEFILES:
		MsgReleaseFiles(&msg);
		break;
	}

	packetLog[packetLogCnt].no = msg.packetNo;
	packetLog[packetLogCnt].addr = msg.hostSub.addr;
	packetLog[packetLogCnt].port = msg.hostSub.portNo;
	packetLogCnt = (packetLogCnt + 1) % MAX_PACKETLOG;

	return	TRUE;
}

/*
	TCP Packet 受信処理
*/
inline SendFileObj *TMainWin::FindSendFileObj(SOCKET sd)
{
	for (SendFileObj *obj = (SendFileObj *)sendFileList.TopObj(); obj; obj = (SendFileObj *)sendFileList.NextObj(obj))
		if (obj->conInfo->sd == sd)
			return	obj;
	return	NULL;
}

BOOL TMainWin::TcpEvent(SOCKET sd, LPARAM lParam)
{
	if (WSAGETSELECTERROR(lParam))
		return	FALSE;

	switch (LOWORD(lParam)) {
	case FD_ACCEPT:
		{
			ConnectInfo	tmpInfo, *info;
			if (msgMng->Accept(hWnd, &tmpInfo))
			{
				if (CheckConnectInfo(&tmpInfo))
				{
					info = new ConnectInfo(tmpInfo);
					connectList.AddObj(info);
				}
				else ::closesocket(tmpInfo.sd);
			}
		}
		break;

	case FD_READ:
		StartSendFile(sd);
		break;

	case FD_CLOSE:
		{
			SendFileObj *obj;
			if ((obj = FindSendFileObj(sd)))
				EndSendFile(obj);
			else
				::closesocket(sd);
		}
		break;
	}
	return	TRUE;
}

BOOL TMainWin::CheckConnectInfo(ConnectInfo *conInfo)
{
	for (ShareInfo *info=shareMng->Top(); info; info=shareMng->Next(info))
	{
		for (int cnt=0; cnt < info->hostCnt; cnt++)
			if (info->host[cnt]->hostSub.addr == conInfo->addr/* && info->host[cnt]->hostSub.portNo == conInfo->port*/)
				return	conInfo->port = info->host[cnt]->hostSub.portNo, TRUE;
	}
	return	FALSE;
}

/*
	ファイル送受信開始処理
*/
BOOL TMainWin::StartSendFile(SOCKET sd)
{
	ConnectInfo 	*conInfo;
	AcceptFileInfo	fileInfo;

	for (conInfo=(ConnectInfo *)connectList.TopObj(); conInfo && conInfo->sd != sd; conInfo=(ConnectInfo *)connectList.NextObj(conInfo))
		;
	if (conInfo == NULL)
		return	::closesocket(sd), FALSE;
	else {
		msgMng->ConnectDone(hWnd, conInfo);	// 非同期メッセージの抑制

// すでに read 要求がきているので、固まる事は無い...はず
// 一度の recv で読めない場合、エラーにしてしまう（手抜き）
		char	buf[MAX_PATH_U8];
		int		size;
		if ((size = ::recv(conInfo->sd, buf, sizeof(buf) -1, 0)) > 0)
			buf[size] = 0;
		if (size <= 0 || !shareMng->GetAcceptableFileInfo(conInfo, buf, &fileInfo))
		{
			connectList.DelObj(conInfo);
			::closesocket(conInfo->sd);
			delete conInfo;
			return	FALSE;
		}
	}

	SendFileObj	*obj = new SendFileObj;
	memset(obj, 0, sizeof(SendFileObj));
	obj->conInfo = conInfo;
	obj->hFile = INVALID_HANDLE_VALUE;
	obj->fileInfo = fileInfo.fileInfo;
	obj->offset = fileInfo.offset;
	obj->packetNo = fileInfo.packetNo;
	obj->host = fileInfo.host;
	obj->command = fileInfo.command;
	obj->conInfo->startTick = obj->conInfo->lastTick = ::GetTickCount();
	obj->attachTime = fileInfo.attachTime;
	connectList.DelObj(conInfo);
	sendFileList.AddObj(obj);

	BOOL	ret = FALSE;

	if (obj->fileInfo->MemData()) {
		obj->isDir	= FALSE;
		obj->status	= FS_MEMFILESTART;
		ret = TRUE;
	}
	else {
		if (!GetFileInfomationU8(obj->fileInfo->Fname(), &obj->fdata))
			return	EndSendFile(obj), FALSE;

		obj->isDir = (obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
		obj->status = obj->isDir || GET_MODE(obj->command) == IPMSG_GETDIRFILES ? FS_DIRFILESTART : FS_TRANSFILE;

		if (*obj->fdata.cFileName == 0)
			ForcePathToFname(obj->fileInfo->Fname(), obj->fdata.cFileName);

		if (obj->isDir)
		{
			ret = GET_MODE(obj->command) == IPMSG_GETDIRFILES ? TRUE : FALSE;
			obj->hDir = (HANDLE *)malloc((MAX_PATH_U8/2) * sizeof(HANDLE));
		}
		else {
			if ((cfg->fileTransOpt & FT_STRICTDATE) && *(_int64 *)&obj->fdata.ftLastWriteTime > *(_int64 *)&obj->attachTime)
				ret = FALSE, obj->status = FS_COMPLETE;		// 共有情報から消去
			else if (GET_MODE(obj->command) == IPMSG_GETDIRFILES)
				ret = TRUE;
			else
				ret = OpenSendFile(obj->fileInfo->Fname(), obj);
		}
	}
	if (!ret)	return	EndSendFile(obj), FALSE;

	DWORD	id;	// 使わず（95系で error になるのを防ぐだけ）
	obj->hThread = (HANDLE)~0;	// 微妙な領域を避ける
	// thread 内では MT 対応が必要な crt は使わず
	if ((obj->hThread = ::CreateThread(NULL, 0, SendFileThread, obj, 0, &id)) == NULL)
		return	EndSendFile(obj), FALSE;

	return	TRUE;
}

BOOL TMainWin::OpenSendFile(const char *fname, SendFileObj *obj)
{
	DWORD	lowSize, highSize, viewSize;

	if ((obj->hFile = CreateFileU8(fname, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE)
	{
		lowSize = ::GetFileSize(obj->hFile, &highSize);
		if ((obj->fileSize = (_int64)highSize << 32 | lowSize) == 0)
			return	TRUE;
		obj->hMap = ::CreateFileMapping(obj->hFile, 0, PAGE_READONLY, highSize, lowSize, 0);
		viewSize = (int)(obj->fileSize > cfg->ViewMax ? cfg->ViewMax : obj->fileSize);
		highSize = (int)(obj->offset >> 32);
		lowSize = (int)((obj->offset / cfg->ViewMax) * cfg->ViewMax);
		obj->mapAddr = (char *)::MapViewOfFile(obj->hMap, FILE_MAP_READ, highSize, lowSize, viewSize);

		if (obj->mapAddr && ::IsBadReadPtr(obj->mapAddr, 1))
		{
			CloseSendFile(obj);
			return	FALSE;
		}
	}
	return	obj->mapAddr ? TRUE : FALSE;
}

BOOL TMainWin::CloseSendFile(SendFileObj *obj)
{
	if (obj == NULL)
		return	FALSE;

	::UnmapViewOfFile(obj->mapAddr);obj->mapAddr= NULL;
	::CloseHandle(obj->hMap);		obj->hMap	= NULL;
	::CloseHandle(obj->hFile);		obj->hFile	= INVALID_HANDLE_VALUE;
	obj->offset = 0;

	return	TRUE;
}

DWORD WINAPI TMainWin::SendFileThread(void *_sendFileObj)
{
	SendFileObj	*obj = (SendFileObj *)_sendFileObj;
	fd_set		fds;
	fd_set		*rfds = NULL, *wfds = &fds;
	timeval		tv;
	int			sock_ret;
	BOOL		ret = FALSE, completeWait = FALSE;
	BOOL		(TMainWin::*SendFileFunc)(SendFileObj*) =
				obj->status == FS_MEMFILESTART ? &TMainWin::SendMemFile :
				GET_MODE(obj->command) == IPMSG_GETDIRFILES ? &TMainWin::SendDirFile : &TMainWin::SendFile;

	FD_ZERO(&fds);
	FD_SET(obj->conInfo->sd, &fds);

	for (int waitCnt=0; waitCnt < 180 && obj->hThread; waitCnt++)
	{
		tv.tv_sec = 1, tv.tv_usec = 0;

		if ((sock_ret = ::select(obj->conInfo->sd + 1, rfds, wfds, NULL, &tv)) > 0)
		{
			waitCnt = 0;

			if (completeWait)
			{	// dummy read により、相手側の socket クローズによる EOF を検出
				if (::recv(obj->conInfo->sd, (char *)&ret, sizeof(ret), 0) >= 0)
					ret = TRUE;
				break;
			}
			else if (!(mainWin->*SendFileFunc)(obj))
				break;
			else if (obj->status == FS_COMPLETE)
			{
				completeWait = TRUE, rfds = &fds, wfds = NULL;
				// 過去β7以前の互換性のため
				if (obj->fileSize == 0) { ret = TRUE; break; }
			}
		}
		else if (sock_ret == 0) {
			FD_ZERO(&fds);
			FD_SET(obj->conInfo->sd, &fds);
		}
		else if (sock_ret == SOCKET_ERROR) {
			break;
		}
	}

	if (obj->isDir)
	{
		mainWin->CloseSendFile(obj);
		while (--obj->dirCnt >= 0)
			::FindClose(obj->hDir[obj->dirCnt]);
	}

	obj->status = ret ? FS_COMPLETE : FS_ERROR;
	mainWin->PostMessage(WM_TCPEVENT, obj->conInfo->sd, FD_CLOSE);
	::ExitThread(0);
	return	0;
}

int MakeDirHeader(SendFileObj *obj, BOOL find)
{
	int					len;
	WIN32_FIND_DATA_U8	*dat = &obj->fdata;
	DWORD				attr = dat->dwFileAttributes, ipmsg_attr;
	char				cFileName[MAX_PATH_U8];
	WCHAR				wbuf[MAX_PATH];

	if (obj->command & IPMSG_UTF8OPT) {
		strncpyz(cFileName, dat->cFileName, MAX_PATH_U8);
	}
	else {
		U8toW(dat->cFileName, wbuf, MAX_PATH);
		WtoA(wbuf, cFileName, MAX_PATH_U8);
	}

	ipmsg_attr = (!find ? IPMSG_FILE_RETPARENT : (attr & FILE_ATTRIBUTE_DIRECTORY)
			? IPMSG_FILE_DIR : IPMSG_FILE_REGULAR) |
		(attr & FILE_ATTRIBUTE_READONLY ? IPMSG_FILE_RONLYOPT : 0) |
		(attr & FILE_ATTRIBUTE_HIDDEN ? IPMSG_FILE_HIDDENOPT : 0) |
		(attr & FILE_ATTRIBUTE_SYSTEM ? IPMSG_FILE_SYSTEMOPT : 0);

	if (find)
		len = wsprintf(obj->header, "0000:%s:%x%08x:%x:%x=%x:%x=%x:", cFileName,
				dat->nFileSizeHigh, dat->nFileSizeLow, ipmsg_attr,
				IPMSG_FILE_MTIME, FileTime2UnixTime(&dat->ftLastWriteTime),
				IPMSG_FILE_CREATETIME, FileTime2UnixTime(&dat->ftCreationTime));
	else if (*(_int64 *)&dat->ftLastWriteTime)
		len = wsprintf(obj->header, "0000:.:0:%x:%x=%x:%x=%x:", ipmsg_attr,
				IPMSG_FILE_MTIME, FileTime2UnixTime(&dat->ftLastWriteTime),
				IPMSG_FILE_CREATETIME, FileTime2UnixTime(&dat->ftCreationTime));
	else
		len = wsprintf(obj->header, "0000:.:0:%x:", ipmsg_attr);

	obj->header[wsprintf(obj->header, "%04x", len)] = ':';

	return	len;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendDirFile(SendFileObj *obj)
{
	BOOL	find = FALSE;

	if (obj->status == FS_OPENINFO)
	{
		char	buf[MAX_BUF];
		if (obj->dirCnt == 0)
			strncpyz(buf, obj->fileInfo->Fname(), MAX_PATH_U8);
		else if (MakePath(buf, obj->path, *obj->fdata.cAlternateFileName ? obj->fdata.cAlternateFileName : obj->fdata.cFileName) >= MAX_PATH_U8)
			return	FALSE;
		strncpyz(obj->path, buf, MAX_PATH_U8);
		obj->dirCnt++;
		obj->status = FS_FIRSTINFO;
	}

	if (obj->status == FS_FIRSTINFO || obj->status == FS_NEXTINFO)
	{
		if (obj->status == FS_FIRSTINFO)
		{
			char	buf[MAX_BUF];
			MakePath(buf, obj->path, "*");
			find = (obj->hDir[obj->dirCnt -1] = FindFirstFileU8(buf, &obj->fdata)) == INVALID_HANDLE_VALUE ? FALSE : TRUE;
		}
		else find = FindNextFileU8(obj->hDir[obj->dirCnt -1], &obj->fdata);

		while (find && (strcmp(obj->fdata.cFileName, ".") == 0 || strcmp(obj->fdata.cFileName, "..") == 0))
			find = FindNextFileU8(obj->hDir[obj->dirCnt -1], &obj->fdata);
		obj->status = FS_MAKEINFO;
	}

	if (obj->status == FS_DIRFILESTART || obj->status == FS_MAKEINFO)
	{
		if (obj->status == FS_DIRFILESTART)
			find = TRUE;
		if (find && (obj->dirCnt > 0 || !obj->isDir) && (obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			char	buf[MAX_BUF];
			int		len = obj->isDir ? MakePath(buf, obj->path, *obj->fdata.cAlternateFileName ? obj->fdata.cAlternateFileName : obj->fdata.cFileName) : wsprintf(buf, "%s", obj->fileInfo->Fname());
			BOOL	modifyCheck = (cfg->fileTransOpt & FT_STRICTDATE) && *(_int64 *)&obj->fdata.ftLastWriteTime > *(_int64 *)&obj->attachTime;
			if (len >= MAX_PATH_U8 || modifyCheck || !OpenSendFile(buf, obj))
			{
				len = strlen(obj->fdata.cFileName);
				strncpyz(obj->fdata.cFileName + len, " (Can't open)", MAX_PATH_U8 - len);
				obj->fdata.nFileSizeHigh = obj->fdata.nFileSizeLow = 0;
			}
		}
		if (!find && obj->isDir)
			GetFileInfomationU8(obj->path, &obj->fdata);

		obj->headerOffset = 0;
		obj->headerLen = MakeDirHeader(obj, find);
		if (!find)
		{
			if (--obj->dirCnt >= 0 && obj->isDir)
			{
				::FindClose(obj->hDir[obj->dirCnt]);
				if (!PathToDir(obj->path, obj->path) && obj->dirCnt > 0)
					return	FALSE;
			}
			if (obj->dirCnt <= 0)
				obj->dirCnt--;	// 終了
		}
		obj->status = FS_TRANSINFO;
	}

	if (obj->status == FS_TRANSINFO)
	{
		int	size = ::send(obj->conInfo->sd, obj->header + obj->headerOffset, obj->headerLen - obj->headerOffset, 0);
		if (size < 0)
			return	FALSE;
		else {
			if ((obj->headerOffset += size) < obj->headerLen)
				return	TRUE;
			obj->status = obj->dirCnt < 0 ? FS_COMPLETE : !find ? FS_NEXTINFO : (obj->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FS_OPENINFO : FS_TRANSFILE;
		}
	}

	if (obj->status == FS_TRANSFILE)
	{
		if (obj->mapAddr && !SendFile(obj))
		{
			CloseSendFile(obj);
			return	FALSE;
		}
		else if (obj->mapAddr == NULL || obj->status == FS_ENDFILE)
		{
			CloseSendFile(obj);
			obj->status = obj->isDir ? FS_NEXTINFO : FS_MAKEINFO;
		}
	}
	return	TRUE;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendFile(SendFileObj *obj)
{
	if (obj == NULL || obj->hFile == INVALID_HANDLE_VALUE)
		return	FALSE;

	int		size = 0;
	_int64	remain = obj->fileSize - obj->offset;
	int		transMax = cfg->TransMax - (int)(obj->offset % cfg->TransMax);

	if (remain > 0 && (size = ::send(obj->conInfo->sd, obj->mapAddr + (obj->offset % cfg->ViewMax), (int)(remain > transMax ? transMax : remain), 0)) < 0)
		return	FALSE;

	obj->offset += size;

	if (obj->offset == obj->fileSize)
		obj->status = GET_MODE(obj->command) == IPMSG_GETDIRFILES ? FS_ENDFILE : FS_COMPLETE;
	else if ((obj->offset % cfg->ViewMax) == 0)	// 再マップの必要
	{
		::UnmapViewOfFile(obj->mapAddr);
		remain = obj->fileSize - obj->offset;
		obj->mapAddr = (char *)::MapViewOfFile(obj->hMap, FILE_MAP_READ, (int)(obj->offset >> 32), (int)obj->offset, (int)(remain > cfg->ViewMax ? cfg->ViewMax : remain));
	}

	obj->conInfo->lastTick = ::GetTickCount();

	return	TRUE;
}

/*
	ファイル送受信
*/
BOOL TMainWin::SendMemFile(SendFileObj *obj)
{
	if (!obj || !obj->fileInfo || !obj->fileInfo->MemData())
		return	FALSE;

	int		size = 0;
	_int64	remain = obj->fileInfo->Size() - obj->offset;
	if (remain > cfg->TransMax) remain = cfg->TransMax;

	if (remain > 0 && (size = ::send(obj->conInfo->sd, (const char *)obj->fileInfo->MemData() + obj->offset, (int)remain, 0)) < 0)
		return	FALSE;

	obj->offset += size;

	if (obj->offset == obj->fileInfo->Size()) {
		obj->status = FS_COMPLETE;
	}

	obj->conInfo->lastTick = ::GetTickCount();

	return	TRUE;
}

BOOL TMainWin::EndSendFile(SendFileObj *obj)
{
	if (obj == NULL)
		return	FALSE;

	if (obj->hThread)
	{
		HANDLE	hThread = obj->hThread;
		obj->hThread = 0;	// 中断の合図
		::WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
	}
	if (::closesocket(obj->conInfo->sd) != 0)
		obj->status = FS_ERROR;	// error 扱いにする

	CloseSendFile(obj);

	if (obj->isDir)
		free(obj->hDir);

	shareMng->EndHostShare(obj->packetNo, &obj->host->hostSub, obj->fileInfo, obj->status == FS_COMPLETE ? TRUE : FALSE);
	sendFileList.DelObj(obj);
	delete obj->conInfo;
	delete obj;
	return	TRUE;
}

/*
	Entry Packet受信処理
*/
void TMainWin::MsgBrEntry(MsgBuf *msg)
{
	AnsQueueObj *obj = (AnsQueueObj *)ansList->GetObj(FREE_LIST);
	int			command = IPMSG_ANSENTRY|HostStatus();

	if (msg->command & IPMSG_CAPUTF8OPT) command |= IPMSG_UTF8OPT;

	if (!VerifyUserNameExtension(cfg, msg)) return;

	if (obj) {
		obj->hostSub = msg->hostSub;
		obj->command = command;
		ansList->PutObj(USED_LIST, obj);
	}
	if (obj == NULL || !SetAnswerQueue(obj)) {
		msgMng->Send(&msg->hostSub, command, GetNickNameEx(), cfg->GroupNameStr);
	}

	AddHost(&msg->hostSub, msg->command, msg->msgBuf, msg->exBuf);
}

BOOL TMainWin::SetAnswerQueue(AnsQueueObj *obj)
{
	if (ansTimerID)
		return	TRUE;

	int		hostCnt = hosts.HostCnt();
	DWORD	spawn;
	DWORD	rand_val;

	TGenRandom(&rand_val, sizeof(rand_val));

	if (hostCnt < 50 || ((msgMng->GetLocalHost()->addr ^ obj->hostSub.addr) << 8) == 0)
		spawn = 1023 & rand_val;
	else if (hostCnt < 300)
		spawn = 2047 & rand_val;
	else
		spawn = 4095 & rand_val;

	if ((ansTimerID = ::SetTimer(hWnd, IPMSG_ANS_TIMER, spawn, NULL)) == 0)
		return	FALSE;

	return	TRUE;
}

void TMainWin::ExecuteAnsQueue(void)
{
	AnsQueueObj *obj;

	while ((obj = (AnsQueueObj *)ansList->GetObj(USED_LIST))) {
		msgMng->Send(&obj->hostSub, obj->command, GetNickNameEx(), cfg->GroupNameStr);
		ansList->PutObj(FREE_LIST, obj);
	}
}

/*
	Exit Packet受信処理
*/
void TMainWin::MsgBrExit(MsgBuf *msg)
{
	Host *host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
	if (host)
		host->updateTime = Time();
	DelHost(&msg->hostSub);

	for (ShareInfo *info=shareMng->Top(),*next; info; info = next)
	{
		next = shareMng->Next(info);
		shareMng->EndHostShare(info->packetNo, &msg->hostSub);
	}
}

/*
	AnsEntry Packet受信処理
*/
void TMainWin::MsgAnsEntry(MsgBuf *msg)
{
	if (!VerifyUserNameExtension(cfg, msg)) return;

	AddHost(&msg->hostSub, msg->command, msg->msgBuf, msg->exBuf);
}

/*
	Absence Packet受信処理
*/
void TMainWin::MsgBrAbsence(MsgBuf *msg)
{
	if (!VerifyUserNameExtension(cfg, msg)) return;

	AddHost(&msg->hostSub, msg->command, msg->msgBuf, msg->exBuf);
}

/*
	Send Packet受信処理
*/
void TMainWin::MsgSendMsg(MsgBuf *msg)
{
	TRecvDlg	*recvDlg;

	if (!VerifyUserNameExtension(cfg, msg) &&
	((msg->command &  IPMSG_AUTORETOPT) == 0 ||
	(msg->command & (IPMSG_PASSWORDOPT|IPMSG_SENDCHECKOPT|IPMSG_SECRETEXOPT|IPMSG_FILEATTACHOPT))))
		return;

	if (TRecvDlg::GetCreateCnt() >= cfg->RecvMax)
		return;

	for (recvDlg = (TRecvDlg *)recvList.TopObj(); recvDlg; recvDlg = (TRecvDlg *)recvList.NextObj(recvDlg)) {
		if (recvDlg->IsSamePacket(msg))
			break;
	}

	if (recvDlg || IsLastPacket(msg)) {
		if ((msg->command & IPMSG_SENDCHECKOPT) &&
			(msg->command & (IPMSG_BROADCASTOPT | IPMSG_AUTORETOPT)) == 0)
			msgMng->Send(&msg->hostSub, IPMSG_RECVMSG, msg->packetNo);
		return;
	}

	if (!RecvDlgOpen(msg)) return;

	if ((msg->command & IPMSG_BROADCASTOPT) == 0 && (msg->command & IPMSG_AUTORETOPT) == 0) {
		if ((msg->command & IPMSG_SENDCHECKOPT))
			msgMng->Send(&msg->hostSub, IPMSG_RECVMSG, msg->packetNo);

		if (cfg->AbsenceCheck && *cfg->AbsenceStr[cfg->AbsenceChoice])
			msgMng->Send(&msg->hostSub, IPMSG_SENDMSG|IPMSG_AUTORETOPT,
							cfg->AbsenceStr[cfg->AbsenceChoice]);
		if ((msg->command & IPMSG_NOADDLISTOPT) == 0 && hosts.GetHostByAddr(&msg->hostSub) == NULL)
			BroadcastEntrySub(&msg->hostSub, IPMSG_BR_ENTRY);
	}
}

/*
	Recv Packet受信処理
*/
void TMainWin::MsgRecvMsg(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	Read Packet受信処理
*/
void TMainWin::MsgReadMsg(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	HostList 送出可能問合せ Packet受信処理
*/
void TMainWin::MsgBrIsGetList(MsgBuf *msg)
{
	if (cfg->AllowSendList
		&& (entryStartTime + ((ULONG)cfg->ListGetMSec / 1000) < (ULONG)Time())
			&& (!cfg->ListGet || (IPMSG_RETRYOPT & msg->command)))
		msgMng->Send(&msg->hostSub, IPMSG_OKGETLIST);
}

/*
	HostList 送出可能通知 Packet受信処理
*/
void TMainWin::MsgOkGetList(MsgBuf *msg)
{
	if (entryStartTime != IPMSG_GETLIST_FINISH)
		msgMng->Send(&msg->hostSub, IPMSG_GETLIST);
}

/*
	HostList 送出要求 Packet受信処理
*/
void TMainWin::MsgGetList(MsgBuf *msg)
{
	if (cfg->AllowSendList)
		SendHostList(msg);
}

/*
	HostList 送出 Packet受信処理
*/
void TMainWin::MsgAnsList(MsgBuf *msg)
{
	if (entryStartTime != IPMSG_GETLIST_FINISH)
		AddHostList(msg);
}

/*
	Version Information 要求 Packet受信処理
*/
void TMainWin::MsgGetInfo(MsgBuf *msg)
{
	char	buf[MAX_LISTBUF];

	wsprintf(buf, "%sVer%s", GetLoadStrU8(IDS_WINEDITION), GetVersionStr());
	msgMng->Send(&msg->hostSub, IPMSG_SENDINFO, buf);
}

/*
	Version Information 通知 Packet受信処理
*/
void TMainWin::MsgSendInfo(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	Public Key 要求 Packet受信処理
*/
void TMainWin::MsgGetPubKey(MsgBuf *msg)
{
	int		capa = strtoul(msg->msgBuf, 0, 16);
	int		local_capa = GetLocalCapa(cfg);
	char	buf[MAX_BUF];

//	if (!VerifyUserNameExtension(cfg, msg)) return;

	if (GetUserNameDigestField(msg->hostSub.userName)) {
		if ((capa & IPMSG_RSA_2048) == 0 || (capa & IPMSG_SIGN_SHA1) == 0) {
			return;	// 成り済まし？
		}
	}

	if ((capa &= local_capa) == 0)
		return;

	PubKey&	pubKey = (capa & IPMSG_RSA_2048) ? cfg->pub[KEY_2048] :
					 (capa & IPMSG_RSA_1024) ? cfg->pub[KEY_1024] :
					 						   cfg->pub[KEY_512];

	// 署名検証用公開鍵を要求（IPMSG_AUTORETOPT でピンポン抑止）
	if ((capa & IPMSG_SIGN_SHA1) && (msg->command & IPMSG_AUTORETOPT) == 0) {
		Host	*host = hosts.GetHostByName(&msg->hostSub);
		if (!host) host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
		if (!host || !host->pubKey.Key() || !host->pubKeyUpdated) {
			wsprintf(buf, "%x", local_capa);
			msgMng->Send(&msg->hostSub, IPMSG_GETPUBKEY|IPMSG_AUTORETOPT, buf);
		}
	}

	// 自分自身の鍵を送信
	wsprintf(buf, "%X:%X-", local_capa, pubKey.Exponent());
	bin2hexstr_revendian(pubKey.Key(), pubKey.KeyLen(), buf + strlen(buf));
	msgMng->Send(&msg->hostSub, IPMSG_ANSPUBKEY, buf);
}

/*
	Public Key 送信 Packet受信処理
*/
void TMainWin::MsgAnsPubKey(MsgBuf *msg)
{
	BYTE	key[MAX_BUF];
	int		key_len, e, capa;
	char	*capa_hex, *e_hex, *key_hex, *p;

	if (GetLocalCapa(cfg) == 0) return;

	if ((capa_hex = separate_token(msg->msgBuf, ':', &p)) == NULL)
		return;
	if ((e_hex = separate_token(NULL, '-', &p)) == NULL)
		return;
	if ((key_hex = separate_token(NULL, ':', &p)) == NULL)
		return;

	capa = strtoul(capa_hex, 0, 16);
	e = strtoul(e_hex, 0, 16);
	hexstr2bin_revendian(key_hex, key, sizeof(key), &key_len);

	if (IsUserNameExt(cfg) && !VerifyUserNameDigest(msg->hostSub.userName, key)) {
		return; // Illegal public key
	}

	Host	*host = hosts.GetHostByName(&msg->hostSub);
	if (!host) host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
	if (host && (!host->pubKey.Key() || !host->pubKeyUpdated)) {
		host->pubKey.Set(key, key_len, e, capa);
		host->pubKeyUpdated = TRUE;
	}

	for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg)) {
		if (sendDlg->SendPubKeyNotify(&msg->hostSub, key, key_len, e, capa))
			break;
	}
}

/*
	Information 通知処理
*/
void TMainWin::MsgInfoSub(MsgBuf *msg)
{
	int	cmd = GET_MODE(msg->command);
	int	packet_no = (cmd == IPMSG_RECVMSG || cmd == IPMSG_ANSREADMSG || cmd == IPMSG_READMSG)
					? atol(msg->msgBuf) : 0;

	if (cmd == IPMSG_READMSG)
	{
		if (GET_OPT(msg->command) & IPMSG_READCHECKOPT)
			msgMng->Send(&msg->hostSub, IPMSG_ANSREADMSG, msg->packetNo);
	}
	else {
		if (cmd == IPMSG_ANSREADMSG) {
			for (TRecvDlg *recvDlg = (TRecvDlg *)recvList.TopObj(); recvDlg; recvDlg = (TRecvDlg *)recvList.NextObj(recvDlg))
				if (recvDlg->SendFinishNotify(&msg->hostSub, packet_no))
					break;
			return;
		}
		TSendDlg *sendDlg;
		for (sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
			if (sendDlg->SendFinishNotify(&msg->hostSub, packet_no))
				break;
		if (sendDlg == NULL)
			return;
	}
	if (IsLastPacket(msg))		//重複チェック
		return;

	char	title[MAX_LISTBUF], *msg_text = msg->msgBuf;
	int		show_mode = cfg->OpenCheck == 3 && cmd == IPMSG_READMSG ? SW_MINIMIZE : SW_SHOW;
	Host	*host = cfg->priorityHosts.GetHostByName(&msg->hostSub);

	if (host && *host->alterName) {
		strcpy(title, host->alterName);
	} else {
		strcpy(title, host && *host->nickName ? host->nickName : msg->hostSub.userName);
	}

	switch (cmd)
	{
	case IPMSG_READMSG:
		histDlg->OpenNotify(&msg->hostSub, packet_no);
		SetCaption();
		if (cfg->OpenCheck == 1) {
			BalloonWindow(TRAY_OPENMSG, title, GetLoadStrU8(IDS_OPENFIN), IPMSG_OPENMSG_TIME);
			return;
		}
		else if (cfg->OpenCheck == 0) {
			return;
		}
		else {
			char *p =  strchr(Ctime(), ' ');
			if (p) p++;
			sprintf(msg_text, "%s\r\n%s", GetLoadStrU8(IDS_OPENFIN), p);
			if ((p = strrchr(msg_text, ' '))) *p = 0;
		}
		break;

	case IPMSG_SENDINFO:
		histDlg->OpenNotify(&msg->hostSub, msg->packetNo, msg->msgBuf);
		return;

	case IPMSG_SENDABSENCEINFO:
		show_mode = SW_SHOW;
		break;

	default:
		return;
	}

	if (cmd == IPMSG_SENDABSENCEINFO) {	//将来的には TMsgDlgで処理
		static int msg_cnt = 0;	// TMsgDlg 化した後は TMsgDlg::createCnt に移行
		if (msg_cnt >= cfg->RecvMax)
			return;
		msg_cnt++;
		MessageBoxU8(msg_text, title);
		msg_cnt--;
	}
	else {
		if (TMsgDlg::GetCreateCnt() >= cfg->RecvMax * 4)
			return;
		TMsgDlg	*msgDlg = new TMsgDlg(IsNewShell() ? this : 0);
		msgDlg->Create(msg_text, title, show_mode);
		if (cmd == IPMSG_SENDINFO || cmd == IPMSG_SENDABSENCEINFO)
			ActiveDlg(msgDlg);
		msgList.AddObj(msgDlg);
	}
}

/*
	不在通知 Information 要求 Packet受信処理
*/
void TMainWin::MsgGetAbsenceInfo(MsgBuf *msg)
{
	msgMng->Send(&msg->hostSub, IPMSG_SENDABSENCEINFO, cfg->AbsenceCheck ? cfg->AbsenceStr[cfg->AbsenceChoice] : GetLoadStrU8(IDS_NOTABSENCE));
}

/*
	不在通知 Information 通知 Packet受信処理
*/
void TMainWin::MsgSendAbsenceInfo(MsgBuf *msg)
{
	MsgInfoSub(msg);
}

/*
	添付ファイル破棄通知 Packet受信処理
*/
void TMainWin::MsgReleaseFiles(MsgBuf *msg)
{
	int	packet_no = atoi(msg->msgBuf);

	shareMng->EndHostShare(packet_no, &msg->hostSub);
}

/*
	送信Dialog生成。ただし、同一の迎撃送信Dialogが開いている場合は、
	そのDialogを Activeにするのみ。
*/
BOOL TMainWin::SendDlgOpen(HWND hRecvWnd, MsgBuf *msg)
{
	TSendDlg *sendDlg;

	if (hRecvWnd)
	{
		for (sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
			if (sendDlg->GetRecvWnd() == hRecvWnd && !sendDlg->IsSending())
				return	ActiveDlg(sendDlg), TRUE;
	}

	if ((sendDlg = new TSendDlg(msgMng, shareMng, &hosts, cfg, logmng, hRecvWnd, msg)) == NULL)
		return	FALSE;

	sendList.AddObj(sendDlg);
	sendDlg->Create();
	sendDlg->Show();
	sendDlg->SetForceForegroundWindow();

	ControlIME(sendDlg, TRUE);

// test
	if (hosts.HostCnt() == 0 && !cfg->ListGet)
		BroadcastEntrySub(inet_addr("127.0.0.1"), htons(portNo), IPMSG_BR_ENTRY);

	return	TRUE;
}

/*
	送信Dialog Hide通知(WM_SENDDLG_HIDE)処理。
	伝えてきた送信Dialogに対応する、受信Dialogを破棄
*/
void TMainWin::SendDlgHide(TSendDlg *sendDlg)
{
	ControlIME(sendDlg, FALSE);

	if (sendDlg->GetRecvWnd() && !cfg->NoErase) {
		TRecvDlg *recvDlg;

		for (recvDlg = (TRecvDlg *)recvList.TopObj();
				recvDlg; recvDlg = (TRecvDlg *)recvList.NextObj(recvDlg)) {
			if (recvDlg->hWnd == sendDlg->GetRecvWnd() && recvDlg->IsClosable()) {
				recvDlg->EvCommand(0, IDCANCEL, 0);
				break;
			}
		}
	}
}

/*
	送信Dialog Exit通知(WM_SENDDLG_EXIT)処理。
	伝えてきた送信Dialog および対応する受信Dialogの破棄
*/
void TMainWin::SendDlgExit(TSendDlg *sendDlg)
{
	if (!sendDlg->IsSending())	// 送信中の場合は HIDE で実行済み
		ControlIME(sendDlg, FALSE);
	sendList.DelObj(sendDlg);
	delete sendDlg;
}

/*
	受信Dialogを生成
*/
BOOL TMainWin::RecvDlgOpen(MsgBuf *msg)
{
	TRecvDlg *recvDlg;

	if ((recvDlg = new TRecvDlg(msgMng, msg, &hosts, cfg, logmng)) == NULL)
		return	FALSE;
	if (recvDlg->Status() == TRecvDlg::ERR)		// 暗号文の復号に失敗した
	{
		delete recvDlg;
		return	FALSE;
	}

	recvList.AddObj(recvDlg);

	if (!cfg->NoBeep)
	{
		char	*soundFile = cfg->SoundFile;
#if 0
		Host	*host = hosts.GetHostByAddr(&msg->hostSub);

		if (host)
		{
			int priorityLevel = (host->priority - DEFAULT_PRIORITY) / PRIORITY_OFFSET;

			if (priorityLevel >= 0 && priorityLevel < cfg->PriorityMax)
			{
				if (cfg->PrioritySound[priorityLevel])
					soundFile = cfg->PrioritySound[priorityLevel];
			}
		}
#endif
		if (*soundFile == '\0' || !PlaySoundU8(soundFile, 0, SND_FILENAME|SND_ASYNC))
			if (!MessageBeep(MB_OK))
				MessageBeep((UINT)~0);
	}

	if (cfg->NoPopupCheck || (cfg->AbsenceNonPopup && cfg->AbsenceCheck))
	{
		if (cfg->BalloonNotify) {
			Host *host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
			char buf1[MAX_LISTBUF], buf2[MAX_LISTBUF], buf3[MAX_LISTBUF*2 + 1];
			if (host && *host->alterName) {
				strcpy(buf1, host->alterName);
			}
			else {
				MakeListString(cfg, &(msg->hostSub), &hosts, buf1);
			}
			SYSTEMTIME rt = recvDlg->GetRecvTime();
			wsprintf(buf2, "at %s", Ctime(&rt));
			wsprintf(buf3, "%s\n%s", buf1, buf2);
			BalloonWindow(TRAY_RECV, buf3, GetLoadStrU8(IDS_RECVMSG), IPMSG_RECVMSG_TIME);
		}
		if (reverseTimerStatus == 0)
		{
			reverseCount = 0;
			ReverseIcon(TRUE);
			if (::SetTimer(hWnd, IPMSG_REVERSEICON, IPMSG_TIMERINTERVAL, NULL))
				reverseTimerStatus = IPMSG_REVERSEICON;
		}
		if (recvDlg->UseClipboard()) recvDlg->Create();
	}
	else {
		recvDlg->Create();
		recvDlg->SetStatus(TRecvDlg::SHOW);
		recvDlg->Show();
		recvDlg->SetForceForegroundWindow();
	}

	return	TRUE;
}

/*
	受信Dialogを破棄
*/
void TMainWin::RecvDlgExit(TRecvDlg *recvDlg)
{
	recvList.DelObj(recvDlg);
	delete recvDlg;
}

/*
	確認Dialogを破棄
*/
void TMainWin::MsgDlgExit(TMsgDlg *msgDlg)
{
	msgList.DelObj(msgDlg);
	delete msgDlg;
}

/*
	Setup/About/Absence Dialogを生成
*/
void TMainWin::MiscDlgOpen(TDlg *dlg)
{
	if (dlg->hWnd == NULL)
		dlg->Create(), dlg->Show();
	else
		ActiveDlg(dlg);
}

/*
	TaskTrayに指定iconを登録
*/
BOOL TMainWin::TaskTray(int nimMode, HICON hSetIcon, LPCSTR tip)
{
	NOTIFYICONDATA2W	tn = { sizeof(NOTIFYICONDATA2W) };

	tn.hWnd = hWnd;
	tn.uID = WM_NOTIFY_TRAY;
	tn.uFlags = NIF_MESSAGE|(hSetIcon ? NIF_ICON : 0)|(tip ? NIF_TIP : 0);
	tn.uCallbackMessage = WM_NOTIFY_TRAY;
	tn.hIcon = hSetIcon;
	if (tip) {
		U8toW(tip, tn.szTip, sizeof(tn.szTip) / sizeof(WCHAR));
	}

	return	::Shell_NotifyIconW(nimMode, (NOTIFYICONDATAW *)&tn);
}

inline int strcharcount(const char *s, char c) {
	int ret = 0;
	while (*s) if (*s++ == c) ret++;
	return	ret;
}
/*
	通知ミニウィンドウ表示
*/
BOOL TMainWin::BalloonWindow(TrayMode _tray_mode, LPCSTR msg, LPCSTR title, DWORD timer)
{
	NOTIFYICONDATA2W	tn = { sizeof(tn) };

	tn.hWnd = hWnd;
	tn.uID = WM_NOTIFY_TRAY;
	tn.uFlags = NIF_INFO|NIF_MESSAGE|(_tray_mode == TRAY_RECV ? NIF_ICON : 0);
	tn.uCallbackMessage = WM_NOTIFY_TRAY;
	tn.hIcon = TMainWin::hMainIcon;

	if (msg && title) {
		U8toW(msg,   tn.szInfo,      sizeof(tn.szInfo) / sizeof(WCHAR));
		U8toW(title, tn.szInfoTitle, sizeof(tn.szInfoTitle) / sizeof(WCHAR));
	}
	tn.uTimeout     = 10000;
	tn.dwInfoFlags  = (_tray_mode == TRAY_RECV ? NIIF_USER : NIIF_INFO) | NIIF_NOSOUND;

	if (msg) {
		if (trayMode != _tray_mode && trayMode != TRAY_NORMAL) {
			::KillTimer(hWnd,
				trayMode == TRAY_RECV ? IPMSG_BALLOON_RECV_TIMER : IPMSG_BALLOON_OPEN_TIMER);
		}
		trayMode = _tray_mode;
		if (trayMode == TRAY_RECV) {
			::SetTimer(hWnd, IPMSG_BALLOON_RECV_TIMER, timer, NULL);
		}
		else {
			if (!::GetForegroundWindow()) {
				::SetTimer(hWnd, IPMSG_BALLOON_DELAY_TIMER, 1000, NULL);
				if (timer != IPMSG_DELAYMSG_TIME) {
					int	len = strlen(trayMsg);
					int	msg_len = strlen(msg);
					int	msg_cnt = strcharcount(msg, '\n');
					if (len + msg_len + 10 < sizeof(trayMsg) && msg_cnt <= 5) {
						if (len > 0) trayMsg[len++] = '\n';
						if (msg_cnt <= 4) memcpy(trayMsg + len, msg, msg_len + 1);
						else              strcpy(trayMsg + len, " ... ");
					}
				}
				return	FALSE;
			}
			if (timer) ::SetTimer(hWnd, IPMSG_BALLOON_OPEN_TIMER, timer, NULL);
		}
	}
	else {
		trayMode = TRAY_NORMAL;
	}

	return	::Shell_NotifyIconW(NIM_MODIFY, (NOTIFYICONDATAW *)&tn);
}

/*
	MainWindow or TaskTray Icon を clickした時の Popup Menu生成
*/
void TMainWin::Popup(UINT resId)
{
	HMENU	hMenu = ::LoadMenu(TApp::GetInstance(), (LPCSTR)resId);
	HMENU	hSubMenu = ::GetSubMenu(hMenu, 0);	//かならず、最初の項目に定義
	POINT	pt = {0};
	char	buf[MAX_LISTBUF];

	if (IsWinVista()) {
		::GetCursorPos(&pt);
	}
	else {
		DWORD	val = ::GetMessagePos();
		pt.x = LOWORD(val);
		pt.y = HIWORD(val);
	}

	if (hMenu == NULL || hSubMenu == NULL)
		return;

	ShareCntInfo	info;
	shareMng->GetShareCntInfo(&info);

	strcpy(buf, GetLoadStrU8(IDS_DOWNLOAD));
	if (info.packetCnt)
		wsprintf(buf + strlen(buf), "(%d/%d)", info.fileCnt, info.transferCnt);
	InsertMenuU8(hSubMenu, 0, MF_BYPOSITION|MF_STRING, MENU_SHARE, buf);

	strcpy(buf, GetLoadStrU8(IDS_UNOPENED));
	if (histDlg->UnOpenedNum())
		wsprintf(buf + strlen(buf), "(%d)", histDlg->UnOpenedNum());
	InsertMenuU8(hSubMenu, 1, MF_BYPOSITION|MF_STRING, MENU_OPENHISTDLG, buf);

	InsertMenuU8(hSubMenu, 2, MF_BYPOSITION|MF_SEPARATOR, NULL, NULL);

	AddAbsenceMenu(hSubMenu, 2);

	SetForegroundWindow();		//とっても大事！

	::TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

	::DestroyMenu(hMenu);
}

/*
	NonPopup状態になっていた受信Dialogがあれば、出現させる
*/
BOOL TMainWin::PopupCheck(void)
{
	BOOL		result = FALSE; 
	TRecvDlg	*recvDlg;

	for (recvDlg = (TRecvDlg *)recvList.TopObj(); recvDlg; recvDlg = (TRecvDlg *)recvList.NextObj(recvDlg)) {
		if (!recvDlg->hWnd) recvDlg->Create();
		if (recvDlg->Status() == TRecvDlg::INIT) {
			recvDlg->SetStatus(TRecvDlg::SHOW);
			recvDlg->Show();
			recvDlg->SetForceForegroundWindow();
			result = TRUE;
		}
	}

	::KillTimer(hWnd, IPMSG_REVERSEICON);
	reverseTimerStatus = 0;
	SetIcon(cfg->AbsenceCheck ? hRevIcon : hMainIcon);
	BalloonWindow(TRAY_NORMAL);

	return	result;
}

/*
	全Windowを前面にもってくる。hide == TRUEの場合、全Windowを hideに
*/
void TMainWin::ActiveChildWindow(BOOL active)
{
	ActiveDlg(aboutDlg, active);
	ActiveDlg(setupDlg, active);
	ActiveDlg(absenceDlg, active);
	ActiveDlg(shareStatDlg, active);
	ActiveDlg(histDlg, active);
	ActiveListDlg(&recvList, active);
	ActiveListDlg(&sendList, active);
	ActiveListDlg(&msgList, active);

	if (!active)
		SetForegroundWindow();
}

/*
	HostDataのcopy
*/
inline void TMainWin::SetHostData(Host *destHost, HostSub *hostSub, ULONG command, Time_t update_time, char *nickName, char *groupName, int priority)
{
	destHost->hostStatus = GET_OPT(command);
	destHost->hostSub = *hostSub;
	destHost->updateTime = update_time;
	destHost->priority = priority;
	strncpyz(destHost->nickName, nickName, sizeof(destHost->nickName));
	strncpyz(destHost->groupName, groupName, sizeof(destHost->groupName));
}

/*
	Host追加処理
*/
void TMainWin::AddHost(HostSub *hostSub, ULONG command, char *nickName, char *groupName)
{
	Host	*host, *tmp_host, *priorityHost;
	Time_t	now_time = Time();
	int		priority = DEFAULT_PRIORITY;

	if (GET_MODE(command) == IPMSG_BR_ENTRY && (command & IPMSG_DIALUPOPT) && !IsSameHost(hostSub, msgMng->GetLocalHost()))
	{
		AddrObj *obj;
		for (obj = (AddrObj *)cfg->DialUpList.TopObj(); obj; obj = (AddrObj *)cfg->DialUpList.NextObj(obj))
			if (obj->addr == hostSub->addr && obj->portNo == hostSub->portNo)
				break;

		if (obj == NULL)
		{
			obj = new AddrObj;
			obj->addr	= hostSub->addr;
			obj->portNo	= hostSub->portNo;
			cfg->DialUpList.AddObj(obj);
		}
	}

	if ((priorityHost = cfg->priorityHosts.GetHostByName(hostSub)))
	{
		priority = priorityHost->priority;
//		command |= priorityHost->hostStatus & IPMSG_ENCRYPTOPT;

	}	// 従来ユーザ名での優先度設定を拡張ユーザ名に引き継ぐ
	else {
		char	*p = (char *)GetUserNameDigestField(hostSub->userName);

		if (p) {
			*p = 0;
			priorityHost = cfg->priorityHosts.GetHostByName(hostSub);
			*p = '-';
			if (priorityHost) {
				priority = priorityHost->priority;
// 古いエントリを削除にする処理は、様子見
//				cfg->priorityHosts.DelHost(priorityHost);
//				if (priorityHost->RefCnt() == 0)
//					delete priorityHost;
//				writeRegFlags |= CFG_DELHOST;
// 代わりに、古いエントリの有効期間を短く（30日後に expire）
				Time_t	t = Time() - (cfg->KeepHostTime - 3600 * 24 * 30);
				if (priorityHost->updateTime > t) priorityHost->updateTime = t;

				priorityHost = NULL;
			}
		}
	}

	if ((host = hosts.GetHostByName(hostSub)))
	{
		if (host->hostSub.addr != hostSub->addr || host->hostSub.portNo != hostSub->portNo)
		{
			if ((tmp_host = hosts.GetHostByAddr(hostSub)))
			{
				for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg)) {
					sendDlg->DelHost(tmp_host);
				}
				hosts.DelHost(tmp_host);
				if (tmp_host->RefCnt() == 0)
					delete tmp_host;
			}
			hosts.DelHost(host);
			host->hostSub.addr = hostSub->addr;
			host->hostSub.portNo = hostSub->portNo;
			hosts.AddHost(host);
		}
		if (((command ^ host->hostStatus) & (IPMSG_ABSENCEOPT|IPMSG_FILEATTACHOPT|IPMSG_ENCRYPTOPT)) || strcmp(host->nickName, nickName) || strcmp(host->groupName, groupName))
		{
			SetHostData(host, hostSub, command, now_time, nickName, groupName, priority);
			for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
				sendDlg->ModifyHost(host);
		}
		else {
			host->hostStatus = GET_OPT(command);
			host->updateTime = now_time;
		}
		return;
	}

	if ((host = hosts.GetHostByAddr(hostSub)))
	{
		for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg)) {
			sendDlg->DelHost(host);
		}
		hosts.DelHost(host);
		if (host->RefCnt() == 0)
			delete host;
	}

	if ((host = priorityHost) == NULL)
		host = new Host;

	SetHostData(host, hostSub, command, now_time, nickName, groupName, priority);
	hosts.AddHost(host);
	if (priorityHost == NULL) {
		cfg->priorityHosts.AddHost(host);
	}

	SetCaption();

	for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
		sendDlg->AddHost(host);
}

/*
	全Hostの削除処理
*/
void TMainWin::DelAllHost(void)
{
	for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
		sendDlg->DelAllHost();

	while (hosts.HostCnt() > 0)
		DelHostSub(hosts.GetHost(0));
}

/*
	特定Hostの削除処理
*/
void TMainWin::DelHost(HostSub *hostSub)
{
	Host *host;

	if ((host = hosts.GetHostByAddr(hostSub)))
		DelHostSub(host);
}

/*
	特定Hostの削除処理Sub
*/
void TMainWin::DelHostSub(Host *host)
{
	for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg; sendDlg = (TSendDlg *)sendList.NextObj(sendDlg))
		sendDlg->DelHost(host);

	for (AddrObj *obj = (AddrObj *)cfg->DialUpList.TopObj(); obj; obj = (AddrObj *)cfg->DialUpList.NextObj(obj))
	{
		if (obj->addr == host->hostSub.addr && obj->portNo == host->hostSub.portNo)
		{
			cfg->DialUpList.DelObj(obj);
			delete obj;
			break;
		}
	}
	hosts.DelHost(host);
	if (host->RefCnt() == 0)
		delete host;
	else if (host->pubKey.Key()) {
		 if (!(host->pubKey.Capa() & IPMSG_RSA_2048)) {
			host->pubKey.UnSet();
		}
		host->pubKeyUpdated = FALSE;
	}
	SetCaption();
}

/*
	更新Button用処理。所有HostListの削除、および再Entry Packet送出。
	なお、unRemoveFlg == TRUEの場合は、削除処理を行わない
*/
void TMainWin::RefreshHost(BOOL unRemoveFlg)
{
	Time_t	now_time = Time();

	MakeBrListEx();

	if (cfg->ListGet && entryTimerStatus != 0 || refreshStartTime + IPMSG_ENTRYMINSEC >= now_time)
		return;

	if (!unRemoveFlg)
	{
		if (cfg->UpdateTime == 0 || refreshStartTime + cfg->UpdateTime < now_time) {
			DelAllHost();
		}
		else {
			for (int cnt=0; cnt < hosts.HostCnt(); )
			{
				if (hosts.GetHost(cnt)->updateTime + cfg->UpdateTime < now_time)
					DelHostSub(hosts.GetHost(cnt));
				else
					cnt++;
			}
		}
	}
	refreshStartTime = now_time;
	EntryHost();
}

/*
	Main Window or TaskTray用 Captionの設定
*/
void TMainWin::SetCaption(void)
{
	char	buf[MAX_LISTBUF];
	int		len;

	len = wsprintf(buf, GetLoadStrU8(IDS_CAPTION), hosts.HostCnt());

	if (histDlg->UnOpenedNum()) {
		wsprintf(buf + len, GetLoadStrU8(IDS_CAPTIONADD), histDlg->UnOpenedNum());
	}

	if (IsNewShell())
		TaskTray(NIM_MODIFY, NULL, buf);
	else
		SetWindowTextU8(buf);
}

/*
	IPMSG_ENTRY/IPMSG_EXIT/IPMSG_ABSENCEパケット送出
*/
void TMainWin::BroadcastEntry(ULONG mode)
{
	TBrObj *brobj;
	for (brobj=brListEx.Top(); brobj; brobj=brListEx.Next(brobj))
		BroadcastEntrySub(brobj->Addr(), htons(portNo), IPMSG_NOOPERATION);

	this->Sleep(cfg->DelayTime);

	UINT host_status = mode | HostStatus();

	for (brobj=brListEx.Top(); brobj; brobj=brListEx.Next(brobj))
		BroadcastEntrySub(brobj->Addr(), htons(portNo), host_status);

	for (AddrObj *obj = (AddrObj *)cfg->DialUpList.TopObj(); obj; obj = (AddrObj *)cfg->DialUpList.NextObj(obj))
		BroadcastEntrySub(obj->addr, obj->portNo, host_status);

	if (mode == IPMSG_BR_ENTRY && cfg->ExtendEntry)
		::SetTimer(hWnd, IPMSG_ENTRY_TIMER, IPMSG_ENTRYMINSEC * 1000, NULL);
}

/*
	IPMSG_ENTRY/IPMSG_EXIT/IPMSG_ABSENCEパケット送出Sub
*/
void TMainWin::BroadcastEntrySub(ULONG addr, int port_no, ULONG mode)
{
	if (addr == 0)
		return;

	msgMng->Send(addr, port_no, mode | (cfg->DialUpCheck ? IPMSG_DIALUPOPT : 0) | HostStatus(),
				 GetNickNameEx(), cfg->GroupNameStr);
}

void TMainWin::BroadcastEntrySub(HostSub *hostSub, ULONG mode)
{
	BroadcastEntrySub(hostSub->addr, hostSub->portNo, mode);
}

/*
	icon 反転処理。startFlg == TRUEの場合、逆iconに Resetされる
*/
void TMainWin::ReverseIcon(BOOL startFlg)
{
	static int cnt = 0;

	if (startFlg)
		cnt = 0;

	SetIcon(cnt++ % 2 ? hMainIcon : hRevIcon);
}

/*
	指定iconを MainWindow or TaskTray にセット
*/
void TMainWin::SetIcon(HICON hSetIcon)
{
	if (IsNewShell())
		TaskTray(NIM_MODIFY, hSetIcon);
	else {
		::SetClassLong(hWnd, GCL_HICON, (LONG)hSetIcon);
		::FlashWindow(hWnd, FALSE);
	}
}

/*
	HostList送出処理
	手抜き ... デリミタやデータがない場合に特殊文字('\a','\b')を使用
*/
void TMainWin::SendHostList(MsgBuf *msg)
{
	int		start_no, len, total_len = 0, host_cnt = 0;
	char	*buf = new char [MAX_UDPBUF];
	char	tmp[MAX_BUF], *tmp_p = tmp;
	char	tmp_mbcs[MAX_BUF];
	int		utf8opt = (msg->command & (IPMSG_CAPUTF8OPT|IPMSG_UTF8OPT)) ? IPMSG_UTF8OPT : 0;

	if ((start_no = atoi(msg->msgBuf)) < 0)
		start_no = 0;

	total_len = wsprintf(buf, "%5d%c%5d%c", 0, HOSTLIST_SEPARATOR, 0, HOSTLIST_SEPARATOR);

	for (int cnt=start_no; cnt < hosts.HostCnt(); cnt++)
	{
		Host	*host = hosts.GetHost(cnt);

		len = wsprintf(tmp, "%s%c%s%c%ld%c%s%c%d%c%s%c%s%c",
			host->hostSub.userName, HOSTLIST_SEPARATOR,
			host->hostSub.hostName, HOSTLIST_SEPARATOR,
			host->hostStatus, HOSTLIST_SEPARATOR,
			inet_ntoa(*(LPIN_ADDR)&host->hostSub.addr), HOSTLIST_SEPARATOR,
			host->hostSub.portNo, HOSTLIST_SEPARATOR,
			*host->nickName ? host->nickName : HOSTLIST_DUMMY, HOSTLIST_SEPARATOR,
			*host->groupName ? host->groupName : HOSTLIST_DUMMY, HOSTLIST_SEPARATOR);

		if (!utf8opt) { // MBCS mode
			len = sprintf(tmp_mbcs, "%s", U8toA(tmp));
			tmp_p = tmp_mbcs;
		}

		if (len + total_len +80 >= MAX_UDPBUF)	// +80 : ipmsg protocol header reserve
		{
			break;
		}
		memcpy(buf + total_len, tmp_p, len +1);
		total_len += len;
		host_cnt++;
	}
	len = wsprintf(tmp, "%5d%c%5d", (start_no + host_cnt == hosts.HostCnt()) ? 0 : start_no + host_cnt, HOSTLIST_SEPARATOR, host_cnt);
	memcpy(buf, tmp, len);
	msgMng->Send(&msg->hostSub, IPMSG_ANSLIST|utf8opt, buf);
	delete [] buf;
}

/*
	HostList受信処理
*/
void TMainWin::AddHostList(MsgBuf *msg)
{
	char	*tok, *nickName, *groupName, *p;
	HostSub	hostSub;
	ULONG	host_status;
	int		total_num, continue_cnt;

	if ((tok = separate_token(msg->msgBuf, HOSTLIST_SEPARATOR, &p)) == NULL)
		return;
	continue_cnt = atoi(tok);

	if ((tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
		return;
	total_num = atoi(tok);

	int host_cnt;
	for (host_cnt=0; (tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)); host_cnt++)
	{
		nickName = groupName = NULL;
		strncpyz(hostSub.userName, tok, sizeof(hostSub.userName));

		if ((tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		strncpyz(hostSub.hostName, tok, sizeof(hostSub.hostName));

		if ((tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		host_status = GET_OPT(atol(tok));

		if (GetUserNameDigestField(hostSub.userName) && (host_status & IPMSG_ENCRYPTOPT) == 0) {
			continue; // 成り済まし？
		}

		if ((tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		hostSub.addr = inet_addr(tok);

		if ((tok = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		hostSub.portNo = atoi(tok);

		if ((nickName = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		if (strcmp(nickName, HOSTLIST_DUMMY) == 0)
			nickName = "";

		if ((groupName = separate_token(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		if (strcmp(groupName, HOSTLIST_DUMMY) == 0)
			groupName = "";

		AddHost(&hostSub, IPMSG_BR_ENTRY|host_status, nickName, groupName);
	}

	if (continue_cnt && continue_cnt >= host_cnt) {
		msgMng->Send(&msg->hostSub, IPMSG_GETLIST, abs(continue_cnt));
		if (::SetTimer(hWnd, IPMSG_LISTGETRETRY_TIMER, cfg->ListGetMSec, NULL))
			entryTimerStatus = IPMSG_LISTGETRETRY_TIMER;
	}
	else {
		entryStartTime = IPMSG_GETLIST_FINISH;
		if (cfg->ListGet)
			BroadcastEntry(IPMSG_BR_ENTRY);
	}
}

/*
	Logを開く
*/
void TMainWin::LogOpen(void)
{
	SHELLEXECUTEINFO	shellExecInfo = { sizeof(SHELLEXECUTEINFO) };

	shellExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shellExecInfo.hwnd = NULL;
	shellExecInfo.lpFile = cfg->LogFile;
	shellExecInfo.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteExU8(&shellExecInfo) || (int)shellExecInfo.hInstApp <= WINEXEC_ERR_MAX)
	{
		switch ((int)shellExecInfo.hInstApp)
		{
		case SE_ERR_NOASSOC: case SE_ERR_ASSOCINCOMPLETE:
			MessageBoxU8(GetLoadStrU8(IDS_FILEEXTERR));
			break;
		default:
			MessageBoxU8(GetLoadStrU8(IDS_CANTOPENLOG));
			break;
		}
	}
//	WaitForSingleObject(shellExecInfo.hProcess, INFINITE);
}

/*
	自Hostの Status (dialup mode / absence mode)
*/
ULONG TMainWin::HostStatus(void)
{
	return	(cfg->DialUpCheck ? IPMSG_DIALUPOPT : 0)
			| (cfg->AbsenceCheck ? IPMSG_ABSENCEOPT : 0)
			| (msgMng->IsAvailableTCP() ?
			(IPMSG_FILEATTACHOPT | ((cfg->ClipMode & CLIP_ENABLE) ? IPMSG_CLIPBOARDOPT : 0)) : 0)
			| (GetLocalCapa(cfg) ? IPMSG_ENCRYPTOPT : 0)
			| IPMSG_CAPUTF8OPT
			| IPMSG_ENCEXTMSGOPT;
}

/*
	IPMSG用 icon初期化
*/
void  TMainWin::InitIcon(void)
{
	if (*cfg->IconFile == 0 || !(hMainIcon = ::ExtractIconW(TApp::GetInstance(), U8toW(cfg->IconFile), 0)))
		hMainIcon = ::LoadIcon(TApp::GetInstance(), (LPCSTR)IPMSG_ICON);
	if (*cfg->RevIconFile == 0 || !(hRevIcon = ::ExtractIconW(TApp::GetInstance(), U8toW(cfg->RevIconFile), 0)))
		hRevIcon = ::LoadIcon(TApp::GetInstance(), (LPCSTR)RECV_ICON);
}

/*
	MainWindow iconを教えてあげる。
	この routineは static member function であり、SendDlgなどから呼ばれる。
*/
HICON TMainWin::GetIPMsgIcon(void)
{
	return	hMainIcon;
}

/*
	ListDlg を Active or Hideに（単なる簡易ルーチン）
*/
void TMainWin::ActiveListDlg(TList *list, BOOL active)
{
	for (TListDlg *dlg = (TListDlg *)list->TopObj(); dlg; dlg = (TListDlg *)list->NextObj(dlg))
		ActiveDlg(dlg, active);
}

/*
	ListDlg を Delete する（単なる簡易ルーチン）
*/
void TMainWin::DeleteListDlg(TList *list)
{
	TListDlg *dlg;

	while ((dlg = (TListDlg *)list->TopObj()))
	{
		list->DelObj(dlg);
		delete dlg;
	}
}

/*
	Dlg を Active or Hideに
*/
void TMainWin::ActiveDlg(TDlg *dlg, BOOL active)
{
	if (dlg->hWnd != 0)
		active ? dlg->Show(), dlg->SetForegroundWindow() : dlg->Show(SW_HIDE);
}

/*
	Extend NickName
*/
char *TMainWin::GetNickNameEx(void)
{
	static char buf[MAX_LISTBUF];

	if (cfg->AbsenceCheck && *cfg->AbsenceHead[cfg->AbsenceChoice])
		wsprintf(buf, "%s[%s]", *cfg->NickNameStr ? cfg->NickNameStr : msgMng->GetOrgLocalHost()->userName, cfg->AbsenceHead[cfg->AbsenceChoice]);
	else
		strcpy(buf, *cfg->NickNameStr ? cfg->NickNameStr : msgMng->GetOrgLocalHost()->userName);

	return	buf;
}

void TMainWin::ControlIME(TWin *win, BOOL open)
{
//	HWND	targetWnd = (win && win->hWnd) ? win->GetDlgItem(SEND_EDIT) : hWnd;
//
//	if (!targetWnd) targetWnd = hWnd;
//
//	if (win && win->hWnd && !open)
//		::SetFocus(targetWnd);		// ATOK残像 暫定対策...

	if (!cfg->ControlIME) return;

	if (!open) {
		for (TSendDlg *sendDlg = (TSendDlg *)sendList.TopObj(); sendDlg;
				sendDlg = (TSendDlg *)sendList.NextObj(sendDlg)) {
			if (sendDlg != win && !sendDlg->IsSending()) return;
		}
	}

	if (GetImeOpenStatus(hWnd) != open) {
		SetImeOpenStatus(hWnd, open);
	}
}

void TMainWin::MakeBrListEx()
{
	brListEx.Reset();

	int			num = 0, i, j;
	AddrInfo	*info = cfg->ExtendBroadcast > 0 ? GetIPAddrs(TRUE, &num) : NULL;
	TBrObj		*obj;

	if (info && num > 0) {
		for (i=num-1; i >= 0; i--) brListEx.SetHostRaw(NULL, info[i].br_addr);
	}

	if ((cfg->ExtendBroadcast & 0x1) == 0 || !info || num == 0) {
		brListEx.SetHostRaw(NULL, ~0);
	}

	for (obj=cfg->brList.Top(); obj; obj=cfg->brList.Next(obj)) {
		if (obj->addr == 0) {
//			if (obj->Addr(TRUE) == 0) {
//				MessageBox(FmtStr("Can't resolve <%s> in broadcast list", obj->host), "IPMsg");
				continue;
//			}
		}
		for (j=0; j < num; j++) {
			if (obj->Addr() == info[j].br_addr) break;
		}
		if (j == num) {
			brListEx.SetHostRaw(NULL, obj->Addr());
		}
	}
//	for (obj=brListEx.Top(); obj; obj=brListEx.Next(obj)) {
//		Debug("%s\n", inet_ntoa(*(struct in_addr *)&obj->addr));
//	}
	delete [] info;
}

/*
BOOL TMainDlg::RunAsAdmin(DWORD flg)
{
	SHELLEXECUTEINFO	sei = { sizeof(SHELLEXECUTEINFO) };
	char				buf[MAX_PATH];

	char	self[MAX_PATH];
	GetModuleFileName(NULL, self, sizeof(self));
	ShellExecute(hWnd, "runas", self, "/runas", "", SW_NORMAL);

	sei.lpVerb = "runas";
	sei.lpFile = self;
	sei.lpDirectory = "";
	sei.nShow = SW_NORMAL;
	sei.lpParameters = "/runas";
	return	::ShellExecuteEx(&sei);
}
*/

/*
	MainWindow を教えてあげる。
*/
HWND GetMainWnd(void)
{
	return	hMainWnd;
}

