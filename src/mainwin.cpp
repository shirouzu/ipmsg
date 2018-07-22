static char *mainwin_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   mainwin.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Main Window
	Create					: 1996-06-01(Sat)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include <process.h>
#include <gdiplus.h>
using namespace Gdiplus;

static HICON hMainIcon = NULL;
static HICON hMainMstIcon = NULL;
static HICON hMainBigIcon = NULL;
static TMainWin *mainWin = NULL;
static HWND	hMainWnd = NULL;

HICON TMainWin::hRevIcon = NULL;
HICON TMainWin::hRevBigIcon = NULL;

using namespace std;

#include "toast/toast.h"

#pragma warning (push)
#pragma warning (disable : 4091) // dbghelp.h(1540): warning C4091: 'typedef ' ignored...
#include <dbghelp.h>
#pragma warning (pop)

/*
	MainWindow の初期化
*/
TMainWin::TMainWin(Param *_param, TWin *_parent) : TWin(_parent)
{
	char	title[100];
	char	dir[MAX_PATH_U8];
	char	exception_log[MAX_PATH_U8];
	char	dump_log[MAX_PATH_U8];

	if (_param) {
		param = *_param;
	}

	if (SHGetSpecialFolderPathU8(0, dir, CSIDL_DESKTOPDIRECTORY, FALSE)) {
		SetCurrentDirectoryU8(dir);
	}
	if (SHGetSpecialFolderPathU8(0, dir, CSIDL_MYDOCUMENTS, FALSE)) {
		MakePathU8(exception_log, dir, "ipmsg_exception.log");
		MakePathU8(dump_log, dir, "ipmsg_exception.dmp");
	}

	sprintf(title, "IPMsg ver%s", GetVersionStr());
	InstallExceptionFilter(title, LoadStr(IDS_EXCEPTIONLOG), exception_log, dump_log,
		MiniDumpWithPrivateReadWriteMemory | 
		MiniDumpWithDataSegs | 
		MiniDumpWithHandleData |
		MiniDumpWithFullMemoryInfo | 
		MiniDumpWithThreadInfo | 
		MiniDumpWithUnloadedModules);
//	Debug("mscver=%d %d\n", _MSC_VER, _MSC_FULL_VER);

	InitExTrace(1024 * 1024);

	TGdiplusInit();

	hosts.Enable(THosts::NAME, TRUE);
	hosts.Enable(THosts::ADDR, TRUE);

#ifndef IPMSG_PRO
	allHosts.Enable(THosts::NAME, TRUE);
	allHosts.Enable(THosts::ADDR, TRUE);
	allHosts.SetLruIdx(ALL_LRU);
	dirPhase = DIRPH_NONE;
#endif

	portNo = param.portNo;
	cfg = new Cfg(param.nicAddr, portNo);
	cfg->ReadRegistry();

	if ((cfg->Debug & 0x1) == 0) {
		TGsFailureHack();
	}

	if (cfg->Debug & 0x4) {
		OpenDebugConsole();
		OpenDebugFile("C:\\temp\\ipmsg_dbg.log");
	}

	fwDlg = NULL;
	fwMode = FW_OK;
	if ((cfg->FwCheckMode & 1) == 0) {
		FwInitCheck();
	}

	if (!(msgMng = new MsgMng(param.nicAddr, portNo, cfg, &hosts))->GetStatus()) {
		::ExitProcess(0xffffffff);
		return;
	}

	if (cfg->lcid > 0) {
		TSetDefaultLCID(cfg->lcid);
	}

	TInetSetUserAgent(Fmt("IPMsg v%s", GetVersionStr()));

	setupDlg = new TSetupDlg(cfg, &hosts, param.isFirstRun);
	aboutDlg = new TAboutDlg(cfg);
	absenceDlg = new TAbsenceDlg(cfg, this);
	logmng = new LogMng(cfg);
	ansList = new TRecycleListEx<AnsQueueObj>(MAX_ANSLIST);
	dosHost = new TRecycleListEx<DosHost>(MAX_DOSHOST);
	shareMng = new ShareMng(cfg, msgMng);
	shareStatDlg = new TShareStatDlg(shareMng, cfg);
	histDlg = new THistDlg(cfg, &hosts);
	logViewList.AddObj(new TLogView(cfg, logmng, TRUE));

	remoteDlg = new TRemoteDlg(cfg, this);
	msgBox = new TMsgBox(this);

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

	brDirAgentTick = 0;
	brDirAgentLast = 0;
	brDirAgentLimit = 0;
	lastExitTick = 0;

	desktopLockCnt = 0;
	monitorState = 0x1;

	isFirstBroad = FALSE;

#ifdef IPMSG_PRO
#define MAIN_INIT
#include "mainext.dat"
#undef  MAIN_INIT
#else
	updConfirm = new TUpdConfim(NULL);
	updRetry = FALSE;
#endif

	if (cfg->viewEpochSave) {
		cfg->viewEpochSave = 0;
		cfg->WriteRegistry(CFG_GENERAL);
	}

	memset(hCycleIcon, 0, sizeof(hCycleIcon));
	InitIcon();
	MakeBrListEx();

	msgDuration = 5;
	if (IsWinVista()) {
		TRegistry	reg(HKEY_CURRENT_USER);
		if (reg.OpenKey("Control Panel\\Accessibility")) {
			reg.GetInt("MessageDuration", &msgDuration);
		}
	}

	hHelp = NULL;
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	hHelpAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)HELP_ACCEL);

	mainWin = this;
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

	ToastHide();
	UnInitToastDll();

	terminateFlg = TRUE;

	if (!msgMng->GetStatus())
		return;
	shareMng->Cleanup(); // SaveShare

	ExitHost();
//	sendList.DeleteListDlg();
	recvList.DeleteListDlg();
	msgList.DeleteListDlg();
	while (TLogView *view=logViewList.TopObj()) {
		logViewList.DelObj(view);
		delete view;
	}
	delete histDlg;
	delete shareStatDlg;
	delete absenceDlg;
	delete aboutDlg;
	delete setupDlg;
	delete remoteDlg;
	delete fwDlg;

	UnInitShowHelp();

//	if (!cfg->TaskbarUI)
		TaskTray(NIM_DELETE);
	cfg->lastWnd = 0;

#if 0
	if (IsWin7() && cfg->TaskbarUI) {
		DeleteJumpList();
	}
#endif

	time_t	now_time = time(NULL);
	for (int i=0; i < hosts.HostCnt(); i++) {
		hosts.GetHost(i)->updateTime = now_time;
	}
	cfg->WriteRegistry(writeRegFlags|CFG_LASTWND);

	msgMng->CloseSocket();

	for (int i=0; i < MAX_KEY; i++) {
		if (cfg->priv[i].hKey) ::CryptDestroyKey(cfg->priv[i].hKey);
	}

	::ExitProcess(0);
}

#define TOASTDLL	L"iptoast.dll"
#define IPMSG_PNG	L"ipmsg.png"
#define IPEXC_PNG	L"ipexc.png"
static BOOL (*_ToastMgrInit)(HWND, UINT);
static BOOL (*_ToastShow)(const WCHAR *, const WCHAR *, const WCHAR*);
static BOOL (*_ToastHide)();
static BOOL (*_ToastIsAlive)();
static WCHAR *ipmsgPng;
static WCHAR *ipexcPng;
static HMODULE hToast;

void TMainWin::InitToastDll(void)
{
	if ((hToast = TLoadLibraryExW(TOASTDLL, TLT_EXEDIR)) == NULL) {
		return;
	}

	_ToastMgrInit = (BOOL (*)(HWND, UINT))::GetProcAddress(hToast, "ToastMgrInit");
	_ToastShow = (BOOL (*)(const WCHAR *, const WCHAR *, const WCHAR*))
		::GetProcAddress(hToast, "ToastShow");
	_ToastHide = (BOOL (*)())::GetProcAddress(hToast, "ToastHide");
	_ToastIsAlive = (BOOL (*)())::GetProcAddress(hToast, "ToastIsAlive");

	if (_ToastMgrInit && _ToastShow && _ToastHide && _ToastIsAlive) {
		if (_ToastMgrInit(hWnd, WM_NOTIFY_TRAY)) {
			WCHAR	path[MAX_PATH];

			MakePathW(path, TGetExeDirW(), IPMSG_PNG);
			ipmsgPng = wcsdup(path);

			MakePathW(path, TGetExeDirW(), IPEXC_PNG);
			ipexcPng = wcsdup(path);
		}
	}
	if (!ipmsgPng) {
		::FreeLibrary(hToast);
		hToast = NULL;
	}
}

void TMainWin::ToastHide(void)
{
	if (hToast && _ToastIsAlive()) {
		_ToastHide();
	}
}

void TMainWin::UnInitToastDll(void)
{
//	if (hToast) {
//		::FreeLibrary(hToast);
//	}
}


BOOL TMainWin::PreProcMsgFilter(MSG *msg)
{
	if (!hHelp) {
		return	FALSE;
	}
	if (!::IsWindow(hHelp)) {
		hHelp = NULL;
		return	FALSE;
	}
	if (TransMsgHelp(msg)) {
		return	TRUE;
	}
	if (::GetForegroundWindow() == hHelp && ::TranslateAcceleratorW(hHelp, hHelpAccel, msg)) {
		::PostMessage(hHelp, WM_SYSCOMMAND, SC_CLOSE, 0);
		hHelp = NULL;
		return	TRUE;
	}

	return	FALSE;
}

BOOL TMainWin::CreateU8(LPCSTR class_name, LPCSTR title, DWORD, DWORD, HMENU)
{
	className = strdup(class_name);
	DWORD style = WS_OVERLAPPEDWINDOW|(cfg->TaskbarUI ? 0 : WS_MINIMIZE);
	return	TWin::CreateU8(className, IP_MSG, style, 0, 0);
}

BOOL TMainWin::FwInitCheck()
{
	fwMode = FW_OK;

	if (!IsWinVista()) {
		return	TRUE;
	}
	if (Is3rdPartyFwEnabled()) {
		if (cfg->Fw3rdParty != 1) {
			cfg->Fw3rdParty = 1;
			cfg->WriteRegistry(CFG_GENERAL);
		}
		return	TRUE;
	}
	if (cfg->Fw3rdParty == 1) {	// 有効になるまでに時間が掛かる環境
		for (int i=0; i < 10; i++) {
			Sleep(1000);
			if (Is3rdPartyFwEnabled()) {
				break;
			}
		}
		if (!Is3rdPartyFwEnabled()) {
			cfg->Fw3rdParty = 0;
			cfg->WriteRegistry(CFG_GENERAL);
		}
	}

	FwStatus	fs;
	GetFwStatusEx(NULL, &fs);

	if (!fs.fwEnable || fs.IsAllowed()) {
		return	TRUE;
	}
	if (IsUserAnAdmin()) {
		SetFwStatusEx(NULL, IP_MSG_W);
	}
	else if (fs.enableCnt == 0) {	// 初回起動設定
		if (IsDomainEnviron()) {
			fwMode = FW_PENDING;
		}
		else {
			TFwDlg	dlg;
			dlg.Exec();
			GetFwStatusEx(NULL, &fs);
			fwMode = fs.IsAllowed() ? FW_OK : FW_CANCEL;
		}
	}
	else {
		fwMode = FW_NEED; // すでに拒否登録済み
	}

	return	TRUE;
}

BOOL TMainWin::FwCheckProc()
{
	KillTimer(IPMSG_FWCHECK_TIMER);

	if (fwMode == FW_PENDING) {	// Windowsの例外設定DLGが出ている間は延期したいが...
		FwStatus	fs;
		GetFwStatusEx(NULL, &fs);

		if (IsUserAnAdmin()) {
			SetFwStatusEx(NULL, IP_MSG_W);
			fwMode = FW_OK;
			return	TRUE;
		}
		if (!fs.fwEnable || fs.IsAllowed()) {
			fwMode = FW_OK;
			return	TRUE;
		}
		if (fs.IsBlocked()) { // block entry 存在
			PostMessage(WM_IPMSG_DELAY_FWDLG, 0, 0);
		}
	}
	return	TRUE;
}

BOOL TMainWin::FirstSetup()
{
	if (!setupDlg->hWnd) {
		setupDlg->Create();
		setupDlg->Show();
	}

	return	TRUE;
}

//void TMainWin::test_func()
//{
//	static int	max_host = 2000;
//	static HostSub *test_host = [&]() {
//		auto hosts = new HostSub[max_host];
//
//		int i=0;
//		for (int i=0; i < max_host; i++) {
//			auto	&h = hosts[i];
//			auto	v = (i%600);
//			h.addr = Fmt("192.168.%d.%d", i/254 + 1, i%254+1);
//			h.portNo = 2425;
//			strcpy(h.u.userName, Fmt("u%d", i%601));
//			strcpy(h.u.hostName, Fmt("h%d", i%601));
//		}
//		return	hosts;
//	}();
//
//	static int v = 0;
//	Debug("test_func(%d)\n", v);
//	for (int i=0; i < max_host; i++) {
//		AddHost(&test_host[(i+v) % max_host], IPMSG_BR_NOTIFY, "", "", "", TRUE);
//	}
//	SetCaption();
//	v++;
//}

BOOL TMainWin::CleanupProc()
{
	static DWORD	last;
	DWORD			cur = GetTick();

	if (cfg->useLockName || cfg->hookMode) {
		BOOL	is_rdp = ::GetSystemMetrics(SM_REMOTESESSION);
		BOOL	lock = (IsLockedScreen() || (!is_rdp && monitorState == 0)) ? TRUE : FALSE;
		BOOL	event = FALSE;

		if (lock && desktopLockCnt < DESKTOP_LOCKMAX) {
			if (++desktopLockCnt >= DESKTOP_LOCKMAX) {
				event = TRUE;
			}
		}
		else if (!lock && desktopLockCnt > 0) {
			event = (desktopLockCnt >= DESKTOP_LOCKMAX) ? TRUE : FALSE;
			desktopLockCnt = 0;
		}
		if (event) {
			Debug("Change to %s\n", desktopLockCnt ? "Lock" : "UnLock");
			if (cfg->useLockName) {
				BroadcastEntry(IPMSG_BR_NOTIFY);
			}
			if (cfg->hookMode) {
				FlushToHook();
			}
		}
	}

#ifndef IPMSG_PRO
	if (updRetry && (updData.flags & UPD_CONFIRM) == 0 && CheckUpdateIdle() == UPDI_IDLE) {
		updRetry = FALSE;
		UpdateCheck(UPD_SHOWERR);
	}
#endif

	if (cur - last < 10000) {
		return	TRUE;
	}
	last = cur;

//	test_func();

	ConnectInfo	*connInfo = connList.TopObj();

	if (connInfo) {

		while (connInfo) {
			ConnectInfo	*next = connList.NextObj(connInfo);
			if (cur - connInfo->startTick > 10000) {
				connList.DelObj(connInfo);
				::closesocket(connInfo->sd);
				delete connInfo;
			}
			connInfo = next;
		}
	}
	shareMng->Cleanup();

#ifdef IPMSG_PRO
#define MAIN_CLEANUP
#include "mainext.dat"
#undef  MAIN_CLEANUP
#else

	if (cfg->updateFlag & Cfg::UPDATE_ON) {
		UpdateCheckTimer();
	}

#endif

	return	TRUE;
}

/*
	参加 Message Packet送出
*/
void TMainWin::EntryHost(void)
{
	time_t	now_time = time(NULL);

	if (entryStartTime + (time_t)cfg->ListGetMSec / 1000 > now_time) return;

	entryStartTime = now_time;

	if (cfg->ListGet) {
		if (SetTimer(IPMSG_LISTGET_TIMER, cfg->ListGetMSec)) {
			entryTimerStatus = IPMSG_LISTGET_TIMER;
		}
		BroadcastEntry(IPMSG_BR_ISGETLIST2);
	}
	else {
		BroadcastEntry(IPMSG_BR_ENTRY);
	}
}

/*
	脱出 Message Packet送出
*/
void TMainWin::ExitHost(void)
{
	lastExitTick = GetTick();

	BroadcastEntry(IPMSG_BR_EXIT);
	this->Sleep(100);
	BroadcastEntry(IPMSG_BR_EXIT);
}

/*
	重複 Packet調査（簡易...もしくは手抜きとも言う(^^;）
*/				
BOOL TMainWin::IsLastPacket(MsgBuf *msg)
{
	for (int i=0; i < MAX_PACKETLOG; i++) {
		if (packetLog[i].no == msg->packetNo && packetLog[i].u == msg->hostSub.u) {
			return	TRUE;
		}
	}
	return	FALSE;
}

BOOL TMainWin::CheckSelfAlias(MsgBuf *msg)
{
	if (selfAddr.IsEnabled() && IsSameHost(&msg->hostSub, msgMng->GetLocalHost())) {
		if (selfAddr != msg->hostSub.addr) {
			return TRUE;
		}
	}
	return	FALSE;
}

/*
	送信Dialog生成。ただし、同一の迎撃送信Dialogが開いている場合は、
	そのDialogを Activeにするのみ。
*/
BOOL TMainWin::SendDlgOpen(DWORD recvId, ReplyInfo *_rInfo)
{
	TRecvDlg *recvDlg = recvId ? recvList.Search(recvId) : NULL;
	TSendDlg *sendDlg = NULL;

	if (recvDlg) {
		for (sendDlg = sendList.TopObj(); sendDlg; sendDlg = sendList.NextObj(sendDlg)) {
			if (sendDlg->GetRecvId() == recvDlg->twinId && !sendDlg->IsSending())
				return	ActiveDlg(sendDlg), TRUE;
		}
	}
	ReplyInfo	tmp;
	ReplyInfo	&rInfo = _rInfo ? *_rInfo : tmp;
	if ((rInfo.recvDlg = recvDlg)) {
		rInfo.isMultiRecv = (recvDlg->GetMsgBuf()->command & IPMSG_MULTICASTOPT) ? TRUE : FALSE;
	}

	if (!(sendDlg = new TSendDlg(msgMng, shareMng, &hosts, cfg, logmng,
		 &rInfo, cfg->TaskbarUI ? (TWin *)this : 0))) {
		return	FALSE;
	}

	sendList.AddObj(sendDlg);
	sendDlg->Create();
//	sendDlg->Exec();

	if (!rInfo.cmdHWnd) {
		sendDlg->Show();
		sendDlg->SetForceForegroundWindow();
		ControlIME(sendDlg, TRUE);

// test
		if (hosts.HostCnt() == 0 && !cfg->ListGet)
			BroadcastEntrySub(Addr("127.0.0.1"), portNo, IPMSG_BR_ENTRY);
	}

	return	TRUE;
}

/*
	送信Dialog Hide通知(WM_SENDDLG_HIDE)処理。
	伝えてきた送信Dialogに対応する、受信Dialogを破棄
*/
void TMainWin::SendDlgHide(DWORD sendid)
{
	TSendDlg *sendDlg = sendList.Search(sendid);
	if (!sendDlg) return;

	ControlIME(sendDlg, FALSE);

	if (sendDlg->GetRecvId() && !cfg->NoErase) {
		TRecvDlg	*recvdlg = recvList.Search(sendDlg->GetRecvId());
		if (recvdlg && recvdlg->IsClosable() && recvdlg->modalCount == 0) {
			recvdlg->PostMessage(WM_COMMAND, IDCANCEL, 0);
		}
	}
}

/*
	送信Dialog Exit通知(WM_SENDDLG_EXIT)処理。
	伝えてきた送信Dialog および対応する受信Dialogの破棄
*/
void TMainWin::SendDlgExit(DWORD sendid)
{
	TSendDlg *sendDlg = sendList.Search(sendid);
	if (!sendDlg || sendDlg->modalCount) return;

	if (!sendDlg->IsSending())	// 送信中の場合は HIDE で実行済み
		ControlIME(sendDlg, FALSE);
	sendList.DelObj(sendDlg);
	delete sendDlg;
}

BOOL TMainWin::RecvDlgOpenCore(TRecvDlg *recvDlg, MsgBuf *msg, const char *rep_head, BOOL is_rproc)
{
	if (cfg->NoPopupCheck || (cfg->AbsenceNonPopup && cfg->AbsenceCheck)) {
		if (cfg->NoPopupCheck == 2) {
			recvDlg->Create();
			recvDlg->ShowWindow(SW_MINIMIZE);
			recvDlg->SetStatus(TRecvDlg::SHOW);
			recvDlg->Show(SW_MINIMIZE);
		}
		else if (cfg->BalloonNotify) {
			Host	*host = cfg->priorityHosts.GetHostByName(&msg->hostSub);
			char	buf[MAX_LISTBUF];

			if (host && *host->alterName) {
				strcpy(buf, host->alterName);
			}
			else {
				if (rep_head) {
					strcpy(buf, rep_head);
				}
				else {
					MakeListString(cfg, &(msg->hostSub), &hosts, buf);
				}
			}
			if (cfg->BalloonNoInfo) {
				strcpy(buf, " ");
			}

			char	tbuf[MAX_LISTBUF];
			time_t	t = recvDlg->GetRecvTime();
			sprintf(tbuf, "at %s", Ctime(&t));

			char	body[MAX_LISTBUF*2 + 1];
			snprintfz(body, sizeof(body), "%s%s%s", buf, cfg->BalloonNoInfo ? "" : "\n", tbuf);

			char	title[MAX_LISTBUF];
			snprintfz(title, sizeof(title), "%s%s", rep_head ? LoadStrU8(IDS_RESTORE) : "",
				(TRecvDlg::GetCreateCnt() >= int(cfg->RecvMax * 0.9)) ?
				Fmt(LoadStrU8(IDS_RECVMAXALERT_TRAY), TRecvDlg::GetCreateCnt(), cfg->RecvMax) : 
				LoadStrU8(IDS_RECVMSG));
			BalloonWindow(TRAY_RECV, body, title, cfg->RecvMsgTime);
		}
		if (reverseTimerStatus == 0) {
			reverseCount = 0;
			ReverseIcon(TRUE);
			if (SetTimer(IPMSG_REVERSEICON,
				/*cfg->RecvIconMode ? IPMSG_RECVICONTICK2 :*/ IPMSG_RECVICONTICK)) {
				reverseTimerStatus = IPMSG_REVERSEICON;
			}
		}
		if (recvDlg->UseClipboard() ||
			(recvDlg->FileAttached() && (cfg->autoSaveFlags & AUTOSAVE_ENABLED))) {
			if (!recvDlg->hWnd) recvDlg->Create();
		}
	}
	else if (cfg->logViewAtRecv && cfg->LogCheck) {
		PopupCheck();
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
	受信Dialogを生成
*/
BOOL TMainWin::RecvDlgOpen(MsgBuf *msg, const char *rep_head, ULONG img_base,
	const char *auto_saved)
{
	auto recvDlg = new TRecvDlg(msgMng, &hosts, cfg, logmng, cfg->TaskbarUI ? this : 0);

	if (!recvDlg) {
		return	FALSE;
	}

	switch (recvDlg->Init(msg, rep_head, img_base, auto_saved)) {
	case TRecvDlg::ERR: case TRecvDlg::REMOTE:	// 暗号文の復号に失敗 or 遠隔コマンドメッセージ
		delete recvDlg;
		return	FALSE;
	}

	recvList.AddObj(recvDlg);

	if (!cfg->NoBeep) {
		char	*soundFile = cfg->SoundFile;
#if 0
		Host	*host = hosts.GetHostByAddr(&msg->hostSub);

		if (host) {
			int priorityLevel = (host->priority - DEFAULT_PRIORITY) / PRIORITY_OFFSET;

			if (priorityLevel >= 0 && priorityLevel < cfg->PriorityMax) {
				if (cfg->PrioritySound[priorityLevel])
					soundFile = cfg->PrioritySound[priorityLevel];
			}
		}
#endif
		if (*soundFile == '\0' || !PlaySoundU8(soundFile, 0, SND_FILENAME|SND_ASYNC)) {
			if (!MessageBeep(MB_OK)) MessageBeep((UINT)~0);
		}
	}

	auto itr = find_if(recvCmdVec.begin(), recvCmdVec.end(), [&](auto &r) {
		return	(r.flags & IPMSG_RECVPROC_FLAG) ? true : false;
	});
	BOOL	is_rproc = (itr != recvCmdVec.end()) ? TRUE : FALSE;

	RecvDlgOpenCore(recvDlg, msg, rep_head, is_rproc);

	if (recvCmdVec.size() > 0) {
		IPDict	out;
		U8str	u;

		recvDlg->GetLogStr(&u);
		out.put_str(IPMSG_BODY_KEY, u.s());
		out.put_int(IPMSG_STAT_KEY, 0);

		for (auto rcmd: recvCmdVec) {
			if (!::IsWindow(rcmd.hWnd)) continue;

			IPIpc	ipc;
			ipc.SaveDictToMap(rcmd.hWnd, FALSE, out);
			::SendMessage(rcmd.hWnd, WM_IPMSG_CMDRES, 0, 0);
		}
		recvCmdVec.clear();

		if (is_rproc) {
		}
	}

	if (!recvDlg->IsRep()) {
		if (RequireHookTrans()) {
			SendToHook(recvDlg->GetMsgBuf(), recvDlg->FileAttached());
			recvDlg->hookCheck = TRUE;
		}
	}

	return	TRUE;
}

void TMainWin::SendToHook(MsgBuf *msg, BOOL is_attached)
{
	char	nick[MAX_LISTBUF] = "";

	MakeNick(cfg, &msg->hostSub, &hosts, nick);
	if (is_attached) {
		strcat(nick, " (with attach)");
	}

	U8str	json;
	char	*body = *msg->msgBuf ? msg->msgBuf : "(no body text)";

	if (cfg->hookKind == 0) {
		std::map<U8str, U8str> dict = {
			{ "$(sender)", nick           },
			{ "$(msg)",    body },
			{ "$(icon)",   cfg->slackIcon },
		};
		U8str	out;
		DynBuf	reply;
		U8str	errMsg;
		ReplaseKeyword(cfg->hookBody.s(), &out, &dict);
		TInetRequest(cfg->hookUrl.s(), NULL, (BYTE *)out.s(), out.Len(), &reply, &errMsg);
		U8str	res(reply.UsedSize() ? reply.s() : errMsg.s());
		DebugU8("SendToHook=%s\n", res.s());
	}
	else if (cfg->hookKind == 1) {
		SlackMakeJson(cfg->slackChan.s(), nick, body, cfg->slackIcon.s(), &json);
		SlackRequestAsync(cfg->slackHost.s(), cfg->slackKey.s(), json.s(), hWnd,
			WM_IPMSG_SLACKRES);
	}
}

void TMainWin::FlushToHook()
{
	for (auto dlg = recvList.TopObj(); dlg; dlg = recvList.NextObj(dlg)) {
		if (dlg->hookCheck || dlg->IsOpened() || dlg->IsRep()) continue;

		SendToHook(dlg->GetMsgBuf(), dlg->FileAttached());
		dlg->hookCheck = TRUE;
	}
}

BOOL TMainWin::RequireHookTrans()
{
	return	(cfg->hookMode == 2 || cfg->hookMode == 1 && IsLockDetected()) ? TRUE : FALSE;
}


/*
	受信Dialogを破棄
*/
void TMainWin::RecvDlgExit(DWORD recvid)
{
	auto recvDlg = recvList.Search(recvid);
	if (!recvDlg || recvDlg->modalCount) return;

	recvList.DelObj(recvDlg);
	delete recvDlg;
}

void TMainWin::RecvDlgOpenByViewer(MsgIdent *mi, BOOL is_file_save)
{
	TRecvDlg *recvDlg = NULL;
	U8str	uid  = mi->uid.s();
	U8str	host = mi->host.s();
	BOOL	unread_count = 0;
	BOOL	done = FALSE;

	for (auto dlg = recvList.TopObj(); dlg; dlg = recvList.NextObj(dlg)) {
		MsgBuf	*msg = dlg->GetMsgBuf();
		if ((mi->msgId && mi->msgId == msg->msgId) ||
			(msg->packetNo == mi->packetNo &&
			 uid == msg->hostSub.u.userName && host == msg->hostSub.u.hostName)) {
			recvDlg = dlg;
			done = TRUE;
		}
		else if (dlg->Status() == TRecvDlg::INIT) {
			unread_count++;
		}
	}

	if (recvDlg) {	// target found
		if (recvDlg->hWnd == 0) {
			recvDlg->Create();
		}
		if (recvDlg->Status() == TRecvDlg::INIT) {
			recvDlg->SetStatus(TRecvDlg::SHOW);
		}
		if (recvDlg->FileAttacheRemain() || recvDlg->IsAutoSaved()) {
			recvDlg->Show();
		}

		POINT	pt;
		TRect	rc, scRect;
		GetCurrentScreenSize(&scRect);
		GetCursorPos(&pt);
		recvDlg->GetWindowRect(&rc);
		pt.x -= rc.cx() / 2;
		pt.y -= rc.cy() / 2;

		if (pt.x + rc.cx() > scRect.cx()) {
			pt.x = scRect.cx() - rc.cx();
		}
		if (pt.y + rc.cy() > scRect.cy()) {
			pt.y = scRect.cy() - rc.cy();
		}
		recvDlg->EvCommand(0, OPEN_BUTTON, 0);

		if (recvDlg->FileAttacheRemain() || recvDlg->IsAutoSaved()) {
			recvDlg->MoveWindow(pt.x, max(0, pt.y), rc.cx(), rc.cy(), TRUE);
			recvDlg->SetForceForegroundWindow();
			::SetFocus(recvDlg->GetDlgItem(IDOK));
			if (is_file_save) {
				recvDlg->PostMessage(WM_COMMAND, FILE_BUTTON, 0);
			}
		}
		else {
			if (recvDlg->IsOpenMsgSending()) {
				::ShowWindow(recvDlg->hWnd, SW_HIDE);
			}
			else {
				PostMessage(WM_RECVDLG_EXIT, 0, recvDlg->twinId);
			}
		}
	}

	if (done && unread_count == 0) {	// 余計な点滅を消す
		PopupCheck();
	}
}

/*
	Setup/About/Absence Dialogを生成
*/
void TMainWin::MiscDlgOpen(TDlg *dlg)
{
	if (dlg->hWnd == NULL) {
		dlg->Create();
		dlg->Show();
	}
	else {
		ActiveDlg(dlg);
	}
}

/*
	Dlg を Active or Hideに
*/
void TMainWin::ActiveDlg(TDlg *dlg, BOOL active)
{
	if (dlg->hWnd != 0) {
		if (active) {
			dlg->Show();
			dlg->SetForegroundWindow();
		}
		else {
			dlg->Show(SW_HIDE);
		}
	}
}


/*
	TaskTrayに指定iconを登録
*/
BOOL TMainWin::TaskTray(int nimMode, HICON hSetIcon, LPCSTR tip)
{
	NOTIFYICONDATAW	tn = { IsWinVista() ? sizeof(tn) : NOTIFYICONDATAW_V2_SIZE };

	tn.hWnd = hWnd;
	tn.uID = WM_NOTIFY_TRAY;
	tn.uFlags = NIF_MESSAGE|(hSetIcon ? NIF_ICON : 0)|(tip ? NIF_TIP : 0);
	tn.uCallbackMessage = WM_NOTIFY_TRAY;
	tn.hIcon = hSetIcon;

	if (tip) {
		U8toW(tip, tn.szTip, wsizeof(tn.szTip));
		if (IsWinVista()) {
			tn.uVersion = NOTIFYICON_VERSION_4; // union: uTimeout
			tn.uFlags |= NIF_SHOWTIP;
		}
	}

	BOOL ret = ::Shell_NotifyIconW(nimMode, &tn);

	if (nimMode != NIM_DELETE && cfg->TrayIcon) {
		if (!ret && nimMode == NIM_ADD) {
			for (int i=0; i < 20; i++) {
				::Sleep(1000);
				if ((ret = ::Shell_NotifyIconW(NIM_MODIFY, &tn))) {
					break;
				}
				if ((ret = ::Shell_NotifyIconW(NIM_ADD, &tn))) {
					break;
				}
			}
		}
		if (ret) {
			static BOOL once_result = ForceSetTrayIcon(hWnd, WM_NOTIFY_TRAY);
		}
	}

	return	ret;
}

inline int strcharcount(const char *s, char c) {
	int ret = 0;
	while (*s) {
		if (*s++ == c) ret++;
	}
	return	ret;
}
/*
	通知ミニウィンドウ表示
*/
BOOL TMainWin::BalloonWindow(TrayMode _tray_mode, LPCSTR msg, LPCSTR title, DWORD timer,
	BOOL force_icon)
{
	NOTIFYICONDATAW	tn = { IsWinVista() ? sizeof(tn) : NOTIFYICONDATAW_V2_SIZE };

	tn.hWnd = hWnd;
	tn.uID = WM_NOTIFY_TRAY;
	tn.uFlags = NIF_INFO|NIF_MESSAGE| ((_tray_mode == TRAY_RECV || force_icon) ? NIF_ICON : 0);
	tn.uCallbackMessage = WM_NOTIFY_TRAY;
	tn.hIcon = hCycleIcon[0];
	tn.hIcon = hMainBigIcon;

	if (msg && title) {
		U8toW(msg,   tn.szInfo,      wsizeof(tn.szInfo));
		U8toW(title, tn.szInfoTitle, wsizeof(tn.szInfoTitle));
	}

	tn.uTimeout		= timer;
	tn.dwInfoFlags	= ((_tray_mode == TRAY_RECV || force_icon) ? NIIF_USER : NIIF_INFO) |
		NIIF_NOSOUND;

	if (msg) {
		if (trayMode != _tray_mode && trayMode != TRAY_NORMAL) {
			KillTimer(trayMode == TRAY_RECV ? IPMSG_BALLOON_RECV_TIMER :
				 (trayMode == TRAY_INST || trayMode == TRAY_UPDATE) ? IPMSG_BALLOON_INST_TIMER :
				  IPMSG_BALLOON_OPEN_TIMER);
		}
		trayMode = _tray_mode;
		if (trayMode == TRAY_RECV) {
			SetTimer(IPMSG_BALLOON_RECV_TIMER, timer);
		}
		else if (trayMode == TRAY_INST || trayMode == TRAY_UPDATE) {
			SetTimer(IPMSG_BALLOON_INST_TIMER, timer);
		}
		else {
			if (!::GetForegroundWindow()) {
				SetTimer(IPMSG_BALLOON_DELAY_TIMER, 1000);
				if (msg != trayMsg) {
					int	len = (int)strlen(trayMsg);
					int	msg_len = (int)strlen(msg);
					int	msg_cnt = strcharcount(msg, '\n');

					if (len + msg_len + 10 < sizeof(trayMsg) && msg_cnt <= 5) {
						if (len > 0) {
							trayMsg[len++] = '\n';
						}
						if (msg_cnt <= 4) {
							memcpy(trayMsg + len, msg, msg_len + 1);
						}
						else {
							strcpy(trayMsg + len, " ... ");
						}
					}
				}
				return	FALSE;
			}
			SetTimer(IPMSG_BALLOON_OPEN_TIMER, timer);
		}
	}
	else {
		trayMode = TRAY_NORMAL;
	}

	if (!IsWinVista()) {
		return	::Shell_NotifyIconW(NIM_MODIFY, &tn);
	}

	tn.uVersion = NOTIFYICON_VERSION_4; // union: uTimeout
	tn.uFlags |= NIF_SHOWTIP;

	if (timer) {	// Win8.1以前は半透明秒数を加算＋タイマーで強制消去
		DWORD new_val = (timer + (hToast ? 0 : 4999)) / 1000;
		::SystemParametersInfo(SPI_SETMESSAGEDURATION, 0, (void *)(LONG_PTR)new_val, 0);
	}

	BOOL ret = TRUE;

	if (hToast) {
		if (msg) {
			ret = _ToastShow(U8toWs(title), U8toWs(msg),
				(trayMode == TRAY_RECV || trayMode == TRAY_INST || trayMode == TRAY_UPDATE)
					? ipmsgPng : ipexcPng);
		}
		else {
			ret = _ToastHide();
		}
	} else {
		ret = ::Shell_NotifyIconW(NIM_MODIFY, &tn);
	}

	if (timer) {
		if (hToast) {
			SetTimer(IPMSG_BALLOON_RESET_TIMER, 500);
		} else {
			::SystemParametersInfo(SPI_SETMESSAGEDURATION, 0, (void *)(UINT_PTR)msgDuration, 0);
		}
	}
	return	ret;
}

/*
	MainWindow or TaskTray Icon を clickした時の Popup Menu生成
*/
void TMainWin::Popup(UINT resId)
{
	HMENU	hMenu = ::LoadMenu(TApp::hInst(), (LPCSTR)(DWORD_PTR)resId);
	HMENU	hSubMenu = ::GetSubMenu(hMenu, 0);	//かならず、最初の項目に定義
	POINT	pt = {};
	char	buf[MAX_LISTBUF];
	int		top_pos = 0;

	::GetCursorPos(&pt);

	if (hMenu == NULL || hSubMenu == NULL) {
		return;
	}

	// ダウンロードモニタ
	ShareCntInfo	info;
	shareMng->GetShareCntInfo(&info);

	if (info.packetCnt) {
		int len = strcpyz(buf, LoadStrU8(IDS_DOWNLOAD));
		snprintfz(buf + len, sizeof(buf)-len, "(%d/%d)", info.fileCnt, info.transferCnt);
		InsertMenuU8(hSubMenu, top_pos++, MF_BYPOSITION|MF_STRING, MENU_SHARE, buf);
	}

	// 未開封モニタ
	int len = strcpyz(buf, LoadStrU8(histDlg->UnOpenedNum() ? IDS_UNOPENED : IDS_OPENED));
	if (histDlg->UnOpenedNum()) {
		snprintfz(buf + len, sizeof(buf)-len, "(%d)", histDlg->UnOpenedNum());
	}
	InsertMenuU8(hSubMenu, top_pos++, MF_BYPOSITION|MF_STRING, MENU_OPENHISTDLG, buf);
	InsertMenuU8(hSubMenu, top_pos++, MF_BYPOSITION|MF_SEPARATOR, NULL, NULL);

	AddAbsenceMenu(hSubMenu, 2);

	if (cfg->HotKeyCheck && cfg->HotKeyLogView &&
		(cfg->HotKeyModify & (MOD_CONTROL|MOD_ALT|MOD_SHIFT)))  {
		char	tmp[MAX_LISTBUF] = "";
		if (cfg->HotKeyModify & MOD_CONTROL) strcat(tmp, "Ctrl+");
		if (cfg->HotKeyModify & MOD_ALT)     strcat(tmp, "Alt+");
		if (cfg->HotKeyModify & MOD_SHIFT)   strcat(tmp, "Shift+");

		snprintfz(buf, sizeof(buf), "%s (%s%c)", LoadStrU8(IDS_LOGOPEN), tmp, cfg->HotKeyLogView);
		::ModifyMenuU8(hSubMenu, MENU_LOGVIEWER, MF_BYCOMMAND|MF_STRING, MENU_LOGVIEWER, buf);
	}

#ifdef IPMSG_PRO
	::DeleteMenu(hSubMenu, MENU_UPDATE, MF_BYCOMMAND);
	::DeleteMenu(hSubMenu, MENU_URL, MF_BYCOMMAND);
	::DeleteMenu(hSubMenu, MENU_BETAURL, MF_BYCOMMAND);
#endif

//	if ((cfg->ClipMode & CLIP_ENABLE) == 0) {
//		DeleteMenu(hSubMenu, MENU_LOGIMGOPEN, MF_BYCOMMAND);
//	}

	SetForegroundWindow();		//とっても大事！

	::TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

	::DestroyMenu(hMenu);
}

/*
	NonPopup状態になっていた受信Dialogがあれば、出現させる
*/
BOOL TMainWin::PopupCheck(void)
{
	BOOL	result = FALSE; 
	BOOL	open_logview = (cfg->logViewAtRecv && cfg->LogCheck) ? TRUE : FALSE;

	KillTimer(IPMSG_REVERSEICON);
	reverseTimerStatus = 0;
	SetIcon(cfg->AbsenceCheck);
	BalloonWindow(TRAY_NORMAL);

	TRecvDlg	*next = NULL;
	for (auto dlg=recvList.TopObj(); dlg; dlg=next) {
		next = recvList.NextObj(dlg);
		if (!dlg->hWnd) {
			dlg->Create();
		}
		if (dlg->Status() == TRecvDlg::INIT) {
			dlg->SetStatus(TRecvDlg::SHOW);
			BOOL	is_nologopt = (dlg->GetMsgBuf()->command & IPMSG_NOLOGOPT) ? TRUE : FALSE;

			if (!cfg->LogCheck || !cfg->logViewAtRecv || is_nologopt ||
				((dlg->FileAttacheRemain() || dlg->IsAutoSaved()) && dlg->IsOpened())) {
				if (open_logview && !is_nologopt) {
					LogViewOpen(TRUE);
					open_logview = FALSE;
					result = TRUE;
				}
				dlg->Show(SW_NORMAL);
				dlg->SetForceForegroundWindow();
			}
			else {
				if (dlg->IsOpened()) {
					PostMessage(WM_RECVDLG_EXIT, 0, dlg->twinId);
				}
				result = TRUE;
			}
		}
	}

	if (result && open_logview) {
		LogViewOpen(TRUE);
	}

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
	recvList.ActiveListDlg(active);
	sendList.ActiveListDlg(active);
	msgList.ActiveListDlg(active);

	for (auto view=logViewList.TopObj(); view; view=logViewList.NextObj(view)) {
		ActiveDlg(view, active);
	}

	if (!active) {
		SetForegroundWindow();
	}
}


/*
	HostDataのcopy
*/
inline void TMainWin::SetHostData(Host *destHost, HostSub *hostSub,
	ULONG command, time_t update_time, const char *nickName, const char *groupName, int priority)
{
	destHost->hostStatus = GET_OPT(command) & IPMSG_ALLSTAT;
	if (hostSub) {
		destHost->hostSub = *hostSub;
	}
	destHost->updateTime = update_time;
	destHost->priority = priority;
	strncpyz(destHost->nickName, nickName, sizeof(destHost->nickName));
	strncpyz(destHost->groupName, groupName, sizeof(destHost->groupName));
}

/*
	Host追加処理
*/
Host *TMainWin::AddHost(HostSub *hostSub, ULONG command, const char *nickName,
	const char *groupName, const char *verInfo, BOOL byHostList)
{
	Host	*host;
	Host	*priorityHost;
	time_t	now_time = time(NULL);
	int		priority = DEFAULT_PRIORITY;

	if (GET_MODE(command) == IPMSG_BR_ENTRY && (command & IPMSG_DIALUPOPT) &&
		!IsSameHost(hostSub, msgMng->GetLocalHost())) {
		AddrObj *obj;
		for (obj = cfg->DialUpList.TopObj(); obj; obj = cfg->DialUpList.NextObj(obj)) {
			if (obj->addr == hostSub->addr && obj->portNo == hostSub->portNo) {
				break;
			}
		}

		if (!obj) {
			obj = new AddrObj;
			obj->addr	= hostSub->addr;
			obj->portNo	= hostSub->portNo;
			cfg->DialUpList.AddObj(obj);
		}
	}

	if ((priorityHost = cfg->priorityHosts.GetHostByName(hostSub))) {
		priority = priorityHost->priority;
//		command |= priorityHost->hostStatus & IPMSG_ENCRYPTOPT;
	}

	if ((host = hosts.GetHostByName(hostSub))) {
		BOOL	need_dlgupdate = FALSE;

		if (host->hostSub.addr != hostSub->addr || host->hostSub.portNo != hostSub->portNo) {
			if (host->hostSub.addr.IsIPv6() && hostSub->addr.IsIPv4()
				|| abs(now_time - host->updateTime) >= 7) {
				if (Host *tmp_host = hosts.GetHostByAddr(hostSub)) {
					for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
						dlg->DelHost(tmp_host, 0, !byHostList);
					}
					Debug("tmp_del (%s) %s %p\n", tmp_host->S(), tmp_host);
					hosts.DelHost(tmp_host);
					tmp_host->SafeRelease();
				}
				Debug("addr changed old=%s  new=%s %p\n", host->S(), hostSub->S(), host);
				hosts.DelHost(host);
				host->hostSub.addr = hostSub->addr;
				host->hostSub.portNo = hostSub->portNo;
				hosts.AddHost(host);
				need_dlgupdate = TRUE;
			}
			else {
				Debug("skip old=%s new=%s %p\n", host->S(), hostSub->S(), host);
				hostSub = NULL;	// SetHostDataされてもAddrを変更しない
			}
		}

		if (((command ^ host->hostStatus) &
				(IPMSG_ABSENCEOPT|IPMSG_FILEATTACHOPT|IPMSG_ENCRYPTOPT)) ||
			strcmp(host->nickName, nickName) || strcmp(host->groupName, groupName) ||
			need_dlgupdate) {

			SetHostData(host, hostSub, command, now_time, nickName, groupName, priority);
			for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
				dlg->ModifyHost(host, FALSE);
			}
		}
		else {
			host->hostStatus = GET_OPT(command) & IPMSG_ALLSTAT;
			host->updateTime = now_time;
		}
		PostAddHost(host, verInfo, now_time, byHostList);
		return	host;
	}

	if ((host = hosts.GetHostByAddr(hostSub))) {
		for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
			dlg->DelHost(host, 0, FALSE);
		}
		hosts.DelHost(host);
		host->SafeRelease();
	}

	if ((host = priorityHost) == NULL) {
		host = new Host;
	}

	SetHostData(host, hostSub, command, now_time, nickName, groupName, priority);

	hosts.AddHost(host);
	if (priorityHost == NULL) {
		cfg->priorityHosts.AddHost(host);
	}

	for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
		dlg->AddHost(host, FALSE, FALSE);
	}

	if (!byHostList) {
		SetCaption();
	}

	PostAddHost(host, verInfo, now_time, byHostList);

	return	host;
}

void TMainWin::PostAddHost(Host *host, const char *verInfo, time_t now_time, BOOL byHostList)
{
	host->active = TRUE;

	if (host->hostStatus & IPMSG_CAPIPDICTOPT) {
		if ( (host->pubKey.Capa() & IPMSG_SIGN_SHA1)
		 && !(host->pubKey.Capa() & IPMSG_SIGN_SHA256)) {
			host->pubKey.SetCapa(host->pubKey.Capa() | IPMSG_SIGN_SHA256);
			Debug("add IPMSG_SIGN_SHA256 %s\n", host->hostSub.addr.S());
		}
	}

	if (!byHostList) {
		host->updateTimeDirect = now_time;
		if (verInfo && *verInfo) {
			strncpyz(host->verInfoRaw, verInfo, sizeof(host->verInfoRaw));
		} else {
			*host->verInfoRaw = 0;
		}
	}
#ifdef IPMSG_PRO
#else
	if (cfg->DirMode == DIRMODE_MASTER) {
		if (!byHostList) {
			DirAddHost(host, FALSE);
		}
	}
	else {
		MasterPubKey(host);
	}
#endif
}


/*
	全Hostの削除処理
*/
void TMainWin::DelAllHost(void)
{
	for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
		dlg->DelAllHost();
	}

	int	max_num = hosts.HostCnt();

	while (max_num > 0) {
		DelHostSub(hosts.GetHost(--max_num));
	}
	SetCaption();
}

/*
	指定Hostの削除処理
*/
void TMainWin::DelHost(HostSub *hostSub, BOOL caption_upd)
{
	Host *host;

	if ((host = hosts.GetHostByAddr(hostSub))) {

#ifndef IPMSG_PRO
		if (cfg->DirMode == DIRMODE_MASTER) {
			DirDelHost(host);
		}
#endif
		DelHostSub(host);

		if (caption_upd) {
			SetCaption();
		}
	}
}

/*
	指定Hostの削除処理Sub
*/
BOOL TMainWin::DelHostSub(Host *host)
{
	for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
		dlg->DelHost(host, 0, FALSE);
	}

	for (auto obj = cfg->DialUpList.TopObj(); obj; obj = cfg->DialUpList.NextObj(obj)) {
		if (obj->addr == host->hostSub.addr && obj->portNo == host->hostSub.portNo) {
			cfg->DialUpList.DelObj(obj);
			delete obj;
			break;
		}
	}
	if (!hosts.DelHost(host)) {
		return	FALSE;
	}

	if (host->SafeRelease()) {
		;
	}
	else if (host->pubKey.Key()) {
		 if (!(host->pubKey.Capa() & IPMSG_RSA_2048)) {
			host->pubKey.UnSet();
		}
	}

	return	TRUE;
}

/*
	更新Button用処理。所有HostListの削除、および再Entry Packet送出。
	なお、removeFlg == FALSEの場合は、削除処理を行わない
*/
void TMainWin::RefreshHost(BOOL removeFlg)
{
//	test_func();

	time_t	now_time = time(NULL);

	MakeBrListEx();

	PollSend();

	if (cfg->ListGet && entryTimerStatus != 0 || refreshStartTime + IPMSG_ENTRYMINSEC >= now_time)
		return;

	if (removeFlg) {
		BOOL all_del = (cfg->UpdateTime == 0 || refreshStartTime + cfg->UpdateTime < now_time);

		if (all_del && !brDirAgentTick) {
			DelAllHost();
		}
		else {
			for (int i=0; i < hosts.HostCnt(); ) {
				Host	*host = hosts.GetHost(i);
				BOOL	host_del = (host->updateTime + cfg->UpdateTime) < now_time;

				if (all_del) {
					if (brDirAgentTick && !host_del) {
						host_del = host->hostSub.addr.IsInNet(brDirAddr);
					} else {
						host_del = TRUE;
					}
				}
				if (host_del) {
					DelHostSub(host);
				}
				else {
					i++;
				}
			}
		}
	}
	SetCaption();
	refreshStartTime = now_time;
	EntryHost();

#ifndef IPMSG_PRO
	if (cfg->DirMode == DIRMODE_MASTER) {
		for (int i=0; i < allHosts.HostCnt(); i++) {
			Host	*ah = allHosts.GetHost(i);
			AddHost(&ah->hostSub, ah->hostStatus|IPMSG_BR_ENTRY, ah->nickName, ah->groupName,
				ah->verInfoRaw, TRUE);
		}
		SetCaption();
	}
#endif
}

/*
	Main Window or TaskTray用 Captionの設定
*/
void TMainWin::SetCaption(void)
{
	char	buf[MAX_LISTBUF];
	int		len;

	len = snprintfz(buf, sizeof(buf), LoadStrU8(IDS_CAPTION), hosts.HostCnt());

	if (histDlg->UnOpenedNum()) {
		snprintfz(buf + len, sizeof(buf)-len, LoadStrU8(IDS_CAPTIONADD), histDlg->UnOpenedNum());
	}

	for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
		dlg->DispUpdate();
	}

	if (cfg->TaskbarUI) {
		SetWindowTextU8(buf);
	}
	TaskTray(NIM_MODIFY, NULL, buf);
}

/*
	IPMSG_ENTRY/IPMSG_EXIT/IPMSG_ABSENCEパケット送出
*/
void TMainWin::BroadcastEntry(ULONG mode)
{
	for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
		BroadcastEntrySub(brobj->Addr(), portNo, IPMSG_NOOPERATION);
	}

	this->Sleep(cfg->DelayTime);

	ULONG host_status = mode | HostStatus();

	for (auto brobj=brListEx.TopObj(); brobj; brobj=brListEx.NextObj(brobj)) {
		BroadcastEntrySub(brobj->Addr(), portNo, host_status);
	}

	for (auto *obj = cfg->DialUpList.TopObj(); obj; obj = cfg->DialUpList.NextObj(obj)) {
		BroadcastEntrySub(obj->addr, obj->portNo, host_status);
	}

	if (mode == IPMSG_BR_ENTRY && cfg->ExtendEntry) {
		SetTimer(IPMSG_ENTRY_TIMER, IPMSG_ENTRYMINSEC * 1000);
	}
}

/*
	IPMSG_ENTRY/IPMSG_EXIT/IPMSG_ABSENCEパケット送出Sub
*/
BOOL TMainWin::BroadcastEntrySub(Addr addr, int port_no, ULONG mode, IPDict *ipdict)
{
	if (!addr.IsEnabled()) {
		return FALSE;
	}

	IPDict	ipdict_tmp;
	if (cfg->masterAddr.IsEnabled()) {
		if (!ipdict) {
			ipdict = &ipdict_tmp;
		}
		ipdict->put_str(IPMSG_SVRADDR_KEY, cfg->masterAddr.S());
	}
	if (cfg->DirMode == DIRMODE_MASTER && GET_MODE(mode) == IPMSG_BR_ENTRY) {
		if (isFirstBroad) {
			if (!ipdict) {
				ipdict = &ipdict_tmp;
			}
			ipdict->put_int(IPMSG_UPTIME_KEY, 0);
		}
	}

	return msgMng->Send(addr, port_no,
		mode | (cfg->DialUpCheck ? IPMSG_DIALUPOPT : 0) | HostStatus(),
		GetNickNameEx(), cfg->GroupNameStr, ipdict);
}

BOOL TMainWin::BroadcastEntrySub(HostSub *hostSub, ULONG mode, IPDict *ipdict)
{
	return BroadcastEntrySub(hostSub->addr, hostSub->portNo, mode, ipdict);
}

/*
	icon 反転処理。startFlg == TRUEの場合、逆iconに Resetされる
*/
void TMainWin::ReverseIcon(BOOL startFlg)
{
	static DWORD cnt = 0;

	if (startFlg) {
		cnt = 0;
	}

	SetIcon(FALSE, (cnt++ % (cfg->RecvIconMode ? MAX_CYCLE_ICON : 2)));
}

/*
	指定iconを MainWindow or TaskTray にセット
*/
void TMainWin::SetIcon(BOOL is_recv, int cnt)
{
	if (cfg->TaskbarUI) {
		::SetClassLong(hWnd, GCL_HICON, LONG_RDC(is_recv ? hRevBigIcon : hMainBigIcon));
		::SetClassLong(hWnd, GCL_HICONSM, LONG_RDC(is_recv ? hRevIcon : hCycleIcon[cnt]));
//		::FlashWindow(hWnd, FALSE);
	}
	TaskTray(NIM_MODIFY, is_recv ? hRevIcon : hCycleIcon[cnt]);
}

/*
	HostList送出処理
	手抜き ... デリミタやデータがない場合に特殊文字('\a','\b')を使用
*/
void TMainWin::SendHostList(MsgBuf *msg)
{
	int		start_no, len, total_len = 0, host_cnt = 0;
	DynBuf	buf(MAX_UDPBUF);
	char	tmp[MAX_BUF];
	char	*tmp_p = tmp;
	int		utf8opt = (msg->command & (IPMSG_CAPUTF8OPT|IPMSG_UTF8OPT)) ? IPMSG_UTF8OPT : 0;

	if ((start_no = atoi(msg->msgBuf)) < 0) {
		start_no = 0;
	}

#ifdef IPMSG_PRO
	THosts	&targ_hosts = hosts;
#else
	THosts	&targ_hosts = (cfg->DirMode == DIRMODE_MASTER) ? allHosts : hosts;
#endif
	total_len = sprintf(buf, "%5d%c%5d%c", 0, HOSTLIST_SEPARATOR, 0, HOSTLIST_SEPARATOR);

	for (int i=start_no; i < targ_hosts.HostCnt(); i++) {
		Host	*host = targ_hosts.GetHost(i);

		len = MakeHostListStr(tmp, host);

		if (len + total_len +80 >= MAX_UDPBUF) {	// +80 : ipmsg protocol header reserve
			break;
		}
		memcpy((char *)buf + total_len, tmp_p, len +1);
		total_len += len;
		host_cnt++;
	}
	len = sprintf(tmp, "%5d%c%5d", (start_no + host_cnt == targ_hosts.HostCnt()) ?
										0 : start_no + host_cnt, HOSTLIST_SEPARATOR, host_cnt);
	memcpy(buf, tmp, len);
	msgMng->Send(&msg->hostSub, IPMSG_ANSLIST|utf8opt, buf);
}

/*
	HostList受信処理
*/
BOOL TMainWin::AddHostListCore(char *buf, BOOL is_master, int *_cont_cnt, int *_host_cnt)
{
	char	*tok;
	char	*p;
	HostSub	hostSub;
	ULONG	host_status = 0;
	int		cnt_tmp = 0;
	int		cnt_tmp2 = 0;
	int		&cont_cnt  = _cont_cnt ? *_cont_cnt : cnt_tmp;
	int		&host_cnt  = _host_cnt ? *_host_cnt : cnt_tmp2;

	if ((tok = sep_tok(buf, HOSTLIST_SEPARATOR, &p)) == NULL)
		return	FALSE;
	cont_cnt = atoi(tok);

	if ((tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
		return	FALSE;
	int		total_num = atoi(tok);

	host_cnt = 0;
	for ( ; (tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)); host_cnt++) {
		char *nickName = NULL;
		char *groupName = NULL;
		strncpyz(hostSub.u.userName, tok, sizeof(hostSub.u.userName));

		if ((tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		strncpyz(hostSub.u.hostName, tok, sizeof(hostSub.u.hostName));

		if ((tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		host_status = atol(tok);

		if ((tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		hostSub.addr = Addr(tok);

		if ((tok = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		hostSub.portNo = ::htons(atoi(tok));

		if ((nickName = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		if (strcmp(nickName, HOSTLIST_DUMMY) == 0)
			nickName = "";

		if ((groupName = sep_tok(NULL, HOSTLIST_SEPARATOR, &p)) == NULL)
			break;
		if (strcmp(groupName, HOSTLIST_DUMMY) == 0)
			groupName = "";

		if (GET_MODE(host_status) == IPMSG_BR_EXIT || host_status == 0) {
			DelHost(&hostSub, FALSE);
		} else {
			if (GetUserNameDigestField(hostSub.u.userName) &&
				(host_status & IPMSG_ENCRYPTOPT) == 0) {
				continue; // 成り済まし？
			}
			AddHost(&hostSub, IPMSG_BR_ENTRY|host_status, nickName, groupName, NULL, TRUE);
		}
	}
	SetCaption();

	return	TRUE;
}

void TMainWin::AddHostList(MsgBuf *msg)
{
	BOOL	is_master = msg->hostSub.addr == cfg->masterAddr ? TRUE : FALSE;
	int		continue_cnt = 0;
	int		host_cnt = 0;

	if (!AddHostListCore(msg->msgBuf, is_master, &continue_cnt, &host_cnt)) {
		Debug("AddHostList err(%d/%d) %s\n", continue_cnt, host_cnt, msg->hostSub.addr.S());
		return;
	}

	if (continue_cnt && continue_cnt >= host_cnt) {
		Debug("AddHostList cont(%d/%d) %s\n", continue_cnt, host_cnt, msg->hostSub.addr.S());
		msgMng->Send(&msg->hostSub, IPMSG_GETLIST, abs(continue_cnt));
		if (SetTimer(IPMSG_LISTGETRETRY_TIMER, cfg->ListGetMSec)) {
			entryTimerStatus = IPMSG_LISTGETRETRY_TIMER;
		}
	}
	else {
		Debug("AddHostList fin(%d/%d) %s\n", continue_cnt, host_cnt, msg->hostSub.addr.S());

		if (cfg->DirMode == DIRMODE_USER && cfg->masterAddr.IsEnabled() &&
			msg->hostSub.addr != cfg->masterAddr) {
			if (SetTimer(IPMSG_LISTGETRETRY_TIMER, cfg->ListGetMSec)) {
				entryTimerStatus = IPMSG_LISTGETRETRY_TIMER;
			}
		}
		else {
			entryStartTime = IPMSG_GETLIST_FINISH;
		}
		if (cfg->ListGet) {
			BroadcastEntry(IPMSG_BR_ENTRY);
		}
	}
}

/*
	Logを開く
*/
void TMainWin::LogViewOpen(BOOL last_view)
{
	if (auto view=logViewList.TopObj()) {
		if (view->hWnd) {
			view->Show();
			if (last_view) {
				view->LastView();
			}
		}
		else {
			view->Create();
		}
		view->SetForceForegroundWindow();
	}
	return;
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

	if (!LONG_RDC(ShellExecuteExU8(&shellExecInfo))
		|| LONG_RDC(shellExecInfo.hInstApp) <= WINEXEC_ERR_MAX) {
		switch (LONG_RDC(shellExecInfo.hInstApp)) {
		case SE_ERR_NOASSOC: case SE_ERR_ASSOCINCOMPLETE:
			MessageBoxU8(LoadStrU8(IDS_FILEEXTERR));
			break;
		default:
			MessageBoxU8(LoadStrU8(IDS_CANTOPENLOG));
			break;
		}
	}
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
			| (cfg->GetCapa() ? (IPMSG_ENCRYPTOPT|IPMSG_ENCEXTMSGOPT) : 0)
			| IPMSG_CAPUTF8OPT
			| IPMSG_CAPFILEENCOPT
			| ((cfg->DirMode == DIRMODE_MASTER) ? (IPMSG_DIR_MASTER|IPMSG_DIALUPOPT) : 0)
			| (cfg->IPDictEnabled() ? IPMSG_CAPIPDICTOPT : 0);
}

/*
	IPMSG用 icon初期化
*/
void  TMainWin::InitIcon(void)
{
	if (hMainIcon) {
		if (hCycleIcon[0] == hMainIcon) {
			hCycleIcon[0] = NULL;
		}
		::DestroyIcon(hMainIcon);
		hMainIcon = NULL;
	}
	if (hMainMstIcon) {
		if (hCycleIcon[0] == hMainMstIcon) {
			hCycleIcon[0] = NULL;
		}
		::DestroyIcon(hMainMstIcon);
		hMainMstIcon = NULL;
	}
	if (hMainBigIcon) {
		::DestroyIcon(hMainBigIcon);
		hMainBigIcon = NULL;
	}
	if (hRevIcon) {
		if (hCycleIcon[1] == hRevIcon) {
			hCycleIcon[1] = NULL;
		}
		::DestroyIcon(hRevIcon);
		hRevIcon = NULL;
	}
	if (hRevBigIcon) {
		::DestroyIcon(hRevBigIcon);
		hRevBigIcon = NULL;
	}
	for (int i=0; i < MAX_CYCLE_ICON; i++) {
		if (hCycleIcon[i]) {
			::DestroyIcon(hCycleIcon[i]);
			hCycleIcon[i] = NULL;
		}
	}

	if (*cfg->IconFile) {
		hMainIcon = ::ExtractIconW(TApp::hInst(), U8toWs(cfg->IconFile), 0);
		hMainBigIcon = ::ExtractIconW(TApp::hInst(), U8toWs(cfg->IconFile), 0);
	}
	if (!hMainIcon) {
		hMainIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
			16, 16, 0);
		hMainMstIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
			16, 16, 0);
		hMainBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
			32, 32, 0);
	}
	hMainMstIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_MST_ICON, IMAGE_ICON,
			16, 16, 0);

	if (*cfg->RevIconFile) {
		hRevIcon = ::ExtractIconW(TApp::hInst(), U8toWs(cfg->RevIconFile), 0);
		hRevBigIcon = ::ExtractIconW(TApp::hInst(), U8toWs(cfg->RevIconFile), 0);
	}
	if (!hRevIcon) {
		hRevIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)RECV_ICON, IMAGE_ICON,
			16, 16, 0);
		hRevBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)RECV_ICON, IMAGE_ICON,
			32, 32, 0);
	}

	if (cfg->RecvIconMode) {
		hCycleIcon[0] = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON,
			IMAGE_ICON, 16, 16, 0);
		for (int i=1; i < MAX_CYCLE_ICON; i++) {
			hCycleIcon[i] = (HICON)::LoadImage(TApp::hInst(),
				(LPCSTR)(INT_PTR)(IPMSG_ICON1 + i - 1), IMAGE_ICON, 16, 16, 0);
		}
	}
	else {
		hCycleIcon[0] = (cfg->DirMode == DIRMODE_MASTER) ? hMainMstIcon : hMainIcon;
		hCycleIcon[1] = hRevIcon;
	}
}

/*
	MainWindow iconを教えてあげる。
	この routineは static member function であり、SendDlgなどから呼ばれる。
*/
HICON GetIPMsgIcon(HICON *hBigIcon)
{
	if (hBigIcon) {
		*hBigIcon = hMainBigIcon;
	}
	return	hMainIcon;
}


/*
	Extend NickName
*/
char *TMainWin::GetNickNameEx(void)
{
	static char buf[MAX_LISTBUF];

	if (cfg->AbsenceCheck && *cfg->AbsenceHead[cfg->AbsenceChoice]) {
		snprintfz(buf, sizeof(buf), "%s[%s]", *cfg->NickNameStr ?
									cfg->NickNameStr : msgMng->GetOrgLocalHost()->u.userName,
									cfg->AbsenceHead[cfg->AbsenceChoice]);
	}
	else if (IsLockDetected() && cfg->useLockName && *cfg->LockName) {
		snprintfz(buf, sizeof(buf), "%s [%s]", *cfg->NickNameStr ?
									cfg->NickNameStr : msgMng->GetOrgLocalHost()->u.userName,
									cfg->LockName);
	}
	else {
		strcpy(buf, *cfg->NickNameStr ? cfg->NickNameStr : msgMng->GetOrgLocalHost()->u.userName);
	}

	return	buf;
}

void TMainWin::ControlIME(TWin *win, BOOL open)
{
	if (!cfg->ControlIME) return;

	if (!open) {
		for (auto dlg=sendList.TopObj(); dlg; dlg=sendList.NextObj(dlg)) {
			if (dlg != win && !dlg->IsSending()) return;
		}
	}

	HWND	targetWnd = (win && win->hWnd) ? win->GetDlgItem(SEND_EDIT) : hWnd;
	if (!targetWnd && win) {
		targetWnd = win->hWnd;
	}

	if (GetImeOpenStatus(targetWnd) != open) {
		if (!SetImeOpenStatus(targetWnd, open) && open) {
			::SetFocus(hWnd);
			SetImeOpenStatus(targetWnd, FALSE);
			::SetFocus(targetWnd);
			SetImeOpenStatus(targetWnd, TRUE);
		}
	}
}

void TMainWin::ChangeMulticastMode(int new_mcast_mode)
{
	msgMng->MulticastLeave();
	cfg->MulticastMode = new_mcast_mode;
	msgMng->MulticastJoin();
}

void TMainWin::MakeBrListEx()
{
	brListEx.Reset();
	brListOrg.Reset();
	allSelfAddrs.clear();

	int			num = 0;
	AddrInfo	*info = cfg->ExtendBroadcast > 0 ?
							GetIPAddrs(GIA_STRICT, &num, cfg->IPv6Mode) : NULL;

	if (info && num > 0) {
		int		gw_num = 0;
		for (int i=0; i < num; i++) {
			if (info[i].type == AddrInfo::BROADCAST) {
				brListEx.SetHostRaw(NULL, info[i].addr);
				brListOrg.SetHostRaw(NULL, info[i].addr);
				Debug("type=%d len=%d %s/mask=%d\n",
					info[i].type, info[i].addr.size, info[i].addr.S(), info[i].addr.mask);
			}
			else if (info[i].type == AddrInfo::UNICAST) {
				if (info[i].gw.size() > 0) {
					if (gw_num++ == 0) {
						if (cfg->IPv6Mode != 1) {
							selfAddr = info[i].addr;
							Debug("Self IPv4 Addr is %s\n", selfAddr.S());
						}
					}
				}
				allSelfAddrs.push_back(info[i].addr);
			}
		}
	}
	if (cfg->IPv6Mode != 1 &&
		param.nicAddr.IsEnabled() && param.nicAddr.IsIPv4() && param.nicAddr != selfAddr) {
		selfAddr = param.nicAddr;
		Debug("Self IPv4 Addr %s (Force)\n", selfAddr.S());
	}

	if (cfg->IPv6Mode) {
		if ((cfg->MulticastMode & 0x1) == 0) {
			brListEx.SetHostRaw(NULL, cfg->IPv6Multicast);
			brListOrg.SetHostRaw(NULL, cfg->IPv6Multicast);
		}
		if ((cfg->MulticastMode & 0x2) == 0) {
			brListEx.SetHostRaw(NULL, LINK_MULTICAST_ADDR6);
			brListOrg.SetHostRaw(NULL, LINK_MULTICAST_ADDR6);
		}
	}
	if (cfg->IPv6Mode != 1) {
		if (cfg->ExtendBroadcast != 1 || !info || num == 0) {
			brListEx.SetHostRaw(NULL, Addr(IPMSG_LIMITED_BROADCAST));
		//	brListOrg.SetHostRaw(NULL, Addr(IPMSG_LIMITED_BROADCAST));
		}
	}

	if (cfg->masterAddr.IsEnabled()) {
		brListEx.SetHostRaw(NULL, cfg->masterAddr);
	}

	for (auto obj=cfg->brList.TopObj(); obj; obj=cfg->brList.NextObj(obj)) {
		if (!obj->addr.IsEnabled()) {
			continue;
		}
		brListEx.SetHostRaw(NULL, obj->Addr(), TRUE);
	}
	for (auto obj=brListEx.TopObj(); obj; obj=brListEx.NextObj(obj)) {
		Debug("brlist=%s\n", obj->addr.S());
	}

	if (selfAddr.IsEnabled()) {
		msgMng->GetLocalHost()->addr = selfAddr;
		msgMng->GetLocalHostA()->addr = selfAddr;
	}

	delete [] info;
}

void TMainWin::GetSelfHost(LogHost *host)
{
	HostSub		*hostSub = msgMng->GetLocalHost();

	host->uid.Init(hostSub->u.userName);
	host->nick.Init(GetNickNameEx());
	host->gname.Init(cfg->GroupNameStr);
	host->host.Init(hostSub->u.hostName);
	host->addr.Init(hostSub->addr.S());
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

TMainWin *GetMain(void)
{
	return	mainWin;
}

void TMainWin::SetMainWnd(HWND hWnd)
{
	hMainWnd = hWnd;
}


