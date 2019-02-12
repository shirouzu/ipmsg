static char *ipmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2019   ipmsg.cpp	Ver4.71";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Application Class
	Create					: 1996-06-01(Sat)
	Update					: 2019-02-12(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "imm32.lib")
#pragma comment (lib, "Iphlpapi.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "Ws2_32.lib")

#define IPMSG_EXE IP_MSG ".exe"

#define IPMSG_USAGE \
	IPMSG_EXE " [portno] [/MSG   [/LOG] [/SEAL] <hostname or IP addr> <message>]\r\n" \
	IPMSG_EXE " [portno] [/MSGEX [/LOG] [/SEAL] <hostname or IP addr> " \
												"<msg_line1 \\n msg_line2...>]\r\n" \
	IPMSG_EXE " [portno] [/NIC nic_addr] [/NICLIST] [/NICID n] <hostname or IP addr>"


TMsgApp::TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_CHECK_CRT_DF|_CRTDBG_LEAK_CHECK_DF );

	::SetDllDirectory("");

	if (!TSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
		TLoadLibraryExW(L"iertutil.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cryptbase.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"urlmon.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cscapi.dll", TLT_SYSDIR);
	}

	TLoadLibraryExW(L"riched20.dll", TLT_SYSDIR);
	TLoadLibraryExW(L"msftedit.dll", TLT_SYSDIR);

	TLoadLibraryExW(L"Dbghelp.dll", TLT_SYSDIR);

	srand((UINT)time(NULL));
	ipmsgWnd = NULL;
}

TMsgApp::~TMsgApp()
{
}

BOOL TMsgApp::PreProcMsg(MSG *msg)
{
	if (ipmsgWnd->PreProcMsgFilter(msg)) {
		return	TRUE;
	}
	return	TApp::PreProcMsg(msg);
}

BOOL SendCmdlineMessage(Addr nicAddr, int port_no, char *msg, BOOL is_multiline)
{
	MsgMng	msgMng(nicAddr, port_no);
	ULONG	command = IPMSG_SENDMSG|IPMSG_NOADDLISTOPT|IPMSG_NOLOGOPT;
	Addr	destAddr;
	char	*tok, *p;

	for (tok=sep_tok(msg, ' ', &p) ; tok && *tok == '/';
		 tok=sep_tok(NULL, ' ', &p)) {
		if (stricmp(tok, "/LOG") == 0)
			command &= ~IPMSG_NOLOGOPT;
		else if (stricmp(tok, "/SEAL") == 0)
			command |= IPMSG_SECRETOPT;
	}

	if (!(destAddr = ResolveAddr(tok)).IsEnabled()) return FALSE;

	if (!(msg = sep_tok(NULL, 0, &p))) return FALSE;

	if (is_multiline) {
		char prior_char=0;
		for (char *c=msg; *c; c++) {
			if (prior_char == '\\' && *c == 'n') {
				*(c -1) = '\r';
				*c      = '\n';
			}
			prior_char = *c;
		}
	}

	return	msgMng.Send(destAddr, port_no, command, msg);
}

void NicListMessage(bool is_v6, const char *msg=NULL)
{
	int			num       = 0;
	AddrInfo	*addrs    = GetIPAddrs(GIA_NOBROADCAST, &num, is_v6);
	char		buf[8192] = "";
	int			len =0;

	if (msg)  len += snprintfz(buf + len, sizeof(buf)-len, "%s\n\n", msg);
	if (!num) len += snprintfz(buf + len, sizeof(buf)-len, "No Nic\n");

	for (int i=0; addrs && i < num && sizeof(buf) - len > 100; i++) {
		len += snprintfz(buf + len, sizeof(buf)-len, " NIC(%d) = %s\n", i+1, addrs[i].addr.S());
	}
	MessageBox(0, buf, IP_MSG, MB_OK);
	delete [] addrs;
}

BOOL SetFirewallExceptArg(const char *arg)
{
	BOOL	result = TRUE;
	HWND	hParentWnd = NULL;

	if (*(arg+9) == '=') {
		hParentWnd = (HWND)strtoull(arg+10, 0, 16);
	}

	if (TIsEnableUAC() && !::IsUserAnAdmin()) {
		TFwDlg::SetFirewallExcept(hParentWnd);	// こちらを通ることは原則ない
	}
	else {
		result = SetFwStatusEx(NULL, IP_MSG_W);
		if (hParentWnd) {
			::PostMessage(hParentWnd, WM_IPMSG_SETFWRES, result, 0);
		}
	}
	return	result;
}

void TMsgApp::InitWindow(void)
{
	TMainWin::Param	param;

	HWND		hWnd;
	char		class_name[MAX_PATH_U8] = IPMSG_CLASS;
	char		*tok = NULL;
	char		*p = NULL;
	char		*class_ptr = NULL;
	enum Stat { ST_NORMAL, ST_TASKBARUI_MSG, ST_EXIT, ST_ERR } status = ST_NORMAL;

	Addr&		nicAddr     = param.nicAddr;
	int	&		portNo      = param.portNo;
	BOOL&		isFirstRun  = param.isFirstRun;
	BOOL&		isInstalled = param.isInstalled;
	BOOL&		isUpdated   = param.isUpdated;
	BOOL&		isUpdateErr = param.isUpdateErr;
	BOOL&		isHistory   = param.isHistory;

	if (*cmdLine == '"') cmdLine = strchr(cmdLine+1, '"');
	if (cmdLine)         cmdLine = strchr(cmdLine, ' ');
	if (!cmdLine)        cmdLine = "";

	if ((portNo = atoi(cmdLine)) == 0) portNo = IPMSG_DEFAULT_PORT;

	MsgMng::WSockStartup(); // for Addr::S and other sock functions

	if ((tok = strchr(cmdLine, '/'))) {
		DWORD	exit_status = 0xffffffff;

		for (tok=sep_tok(tok, ' ', &p); tok && *tok == '/';
				tok=sep_tok(NULL, ' ', &p)) {
			if (stricmp(tok, "/NICLIST") == 0 || stricmp(tok, "/NICLIST6") == 0) {
				bool		is_v6  = stricmp(tok, "/NICLIST6") == 0;
				NicListMessage(is_v6);
				status = ST_EXIT;
			}
			else if (stricmp(tok, "/NICID") == 0 || stricmp(tok, "/NICID6") == 0) {	// NICID 指定
				bool is_v6 = stricmp(tok, "/NICID6") == 0;

				status = ST_EXIT;
				if ((tok = sep_tok(NULL, ' ', &p))) {
					int			target = atoi(tok) - 1;
					int			num = 0;
					AddrInfo	*addrs = GetIPAddrs(GIA_NOBROADCAST, &num, is_v6);
					if (addrs && target >= 0 && target < num) {
						nicAddr = addrs[target].addr;
						status = ST_NORMAL;
					}
					delete [] addrs;
				}
				if (status == ST_EXIT) NicListMessage(is_v6, "NICID not found");
			}
			else if (stricmp(tok, "/NIC") == 0 || stricmp(tok, "/NIC6") == 0) {	// NIC 指定
				bool is_v6 = stricmp(tok, "/NIC6") == 0;

				status = ST_EXIT;
				if ((tok = sep_tok(NULL, ' ', &p)) &&
					(nicAddr = ResolveAddr(tok)).IsEnabled()) {
					status = ST_NORMAL;
				}
				if (status == ST_EXIT) NicListMessage(is_v6, "NIC addr not found");
			}
			else if (stricmp(tok, "/MSG") == 0) {	// コマンドラインモード
				tok = sep_tok(NULL, 0, &p);
				status = SendCmdlineMessage(nicAddr, portNo, tok, FALSE) ? ST_EXIT : ST_ERR;
			}
			else if (stricmp(tok, "/MSGEX") == 0) {	// コマンドラインモード（複数行）
				tok = sep_tok(NULL, 0, &p);
				status = SendCmdlineMessage(nicAddr, portNo, tok, TRUE) ? ST_EXIT : ST_ERR;
			}
			else if (stricmp(tok, "/FIRST_RUN") == 0) {	// インストーラからの起動
				isFirstRun = TRUE;
			}
			else if (stricmp(tok, "/INSTALLED") == 0) {	// 手動インストールからの起動
				isInstalled = TRUE;
			}
			else if (stricmp(tok, "/UPDATED") == 0) {	// 自動アップデートからの起動
				isUpdated = TRUE;
			}
			else if (stricmp(tok, "/UPDATE_ERR") == 0) {	// 自動アップデートからの起動
				isUpdateErr = TRUE;
			}
			else if (stricmp(tok, "/SHOW_HISTORY") == 0) {	// インストーラからの起動
				isHistory = TRUE;
			}
			else if (strnicmp(tok, "/FIREWALL", 9) == 0) {	// firewall除外設定
				SetFirewallExceptArg(tok);
				::ExitProcess(0xffffffff);
				return;
			}
			else if (stricmp(tok, "/TASKBAR_MSG") == 0) {	// TaskbarUI用
				int		taskbar_msg = 0;
				int		taskbar_cmd = 0;

				if (!(class_ptr = sep_tok(NULL, ' ', &p))
				|| !(tok = sep_tok(NULL, ' ', &p))
				|| !(taskbar_cmd = atoi(tok))
				|| !(taskbar_msg = ::RegisterWindowMessage(IP_MSG))) {
					status = ST_ERR;
					break;
				}
				if ((hWnd = FindWindowU8(class_ptr))) {
					::PostMessage(hWnd, taskbar_msg, taskbar_cmd, 0);
				}
				::ExitProcess(0xffffffff);
				return;
			}
			else status = ST_ERR;
		}
		if (status != ST_NORMAL) {
			if (status == ST_ERR) {
				MessageBox(0, IPMSG_USAGE, IP_MSG, MB_OK);
			}
			::ExitProcess(exit_status);
			return;
		}
	}

	if (portNo != IPMSG_DEFAULT_PORT || nicAddr.IsEnabled()) {
		char	*p = class_name + sprintf(class_name, "%s_%d", IPMSG_CLASS, portNo);
		if (nicAddr.IsEnabled()) {
			*p++ = '_';
			nicAddr.S(p);
		}
	}

	HANDLE	hMutex = ::CreateMutex(NULL, FALSE, class_name);
	::WaitForSingleObject(hMutex, INFINITE);

	if ((hWnd = FindWindowU8(class_name)) ||
		!TRegisterClassU8(class_name, CS_DBLCLKS, ::LoadIcon(TApp::hInst(),
			(LPCSTR)IPMSG_ICON), ::LoadCursor(NULL, IDC_ARROW),
			(HBRUSH)::GetStockObject(WHITE_BRUSH))) {
		if (hWnd) {
			::PostMessage(hWnd, WM_SENDDLG_OPEN, 0, 0);
		}
		::ExitProcess(0xffffffff);
		return;
	}

//	OpenDebugConsole();

	ipmsgWnd = new TMainWin(&param);
	mainWnd = ipmsgWnd;
	mainWnd->CreateU8(class_name);
	::ReleaseMutex(hMutex);
	::CloseHandle(hMutex);
}

#ifdef _DEBUG
void addr_test()
{
	MsgMng::WSockStartup();

	Addr	a1;
	Addr	a2;
	a1.Set("192.168.1.5");
	a2.Set("192.168.1.91");

	Debug("a1 net = %s\n", a1.S());
	Debug("a2 net = %s\n", a2.S());
	Debug("a1 net = %s\n", a1.ToNet(24).S());
	Debug("a2 net = %s\n", a2.ToNet(24).S());
	Debug("a1net == a2net = %d\n", a1.ToNet(24) == a2.ToNet(24));
	Debug("sizeof host=%zd, hostSub=%zd, Addr=%zd\n", sizeof(Host), sizeof(HostSub), sizeof(Addr));
}

void thosts_test()
{
	THosts	all_hosts;
	THosts	seg_hosts;

	all_hosts.Enable(THosts::NAME, TRUE);
	all_hosts.Enable(THosts::ADDR, TRUE);
	all_hosts.SetLruIdx(ALL_LRU);
	seg_hosts.Enable(THosts::NAME, TRUE);
	seg_hosts.Enable(THosts::ADDR, TRUE);
	seg_hosts.SetLruIdx(SEG_LRU);

	for (int i=0; i < 10; i++) {
		Host *host = new Host();
		host->hostSub.addr.Set(Fmt("192.168.0.%d", i+1));
		host->hostSub.portNo = 2425;
		host->hostSub.SetUserName(Fmt("user_%d", i));
		host->hostSub.SetHostName(Fmt("host_%d", i));
		all_hosts.AddHost(host);
		seg_hosts.AddHost(host);
	}

	all_hosts.UpdateLru(all_hosts.GetHost(1));
	all_hosts.UpdateLru(all_hosts.GetHost(0));
	seg_hosts.UpdateLru(seg_hosts.GetHost(5));
	seg_hosts.UpdateLru(seg_hosts.GetHost(4));

	all_hosts.DelHost(all_hosts.GetHost(5));
	seg_hosts.DelHost(seg_hosts.GetHost(3));

	for (auto itr=all_hosts.lruList.rbegin(); itr != all_hosts.lruList.rend(); itr++) {
		Debug("all_hosts %s\n", (*itr)->hostSub.u.userName);
	}
	for (auto itr=seg_hosts.lruList.rbegin(); itr != seg_hosts.lruList.rend(); itr++) {
		Debug("seg_hosts %s\n", (*itr)->hostSub.u.userName);
	}
}
#endif

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
#ifdef _DEBUG
	//ipdic_test();
	//addr_test();
	//thosts_test();

//	Sleep(100000);
#endif

#if 0
	int		max_num = 1024;
	size_t	sz = (size_t)1024 * 1024 * 1024 * 1024;
	VBuf	**d = new VBuf *[max_num];
	for (int i = 0; i < max_num; i++) {
		d[i] = new VBuf();
	}
	for (int i = 0; i < max_num; i++) {
		if (!d[i]->AllocBuf(0, sz)) {
			Debug("i=%d error=%x", i, GetLastError());
			break;
		}
		Debug("%d ", i);
	}
	for (int i = 0; i < max_num; i++) {
		d[i]->FreeBuf();
		delete d[i];
	}
	delete[] d;
#endif

	if (!IsWinXP()) {
		MessageBox(0, "Please use old version (v3.42 or earlier)",
					"Win2000 is not supported", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TMsgApp	app(hI, WtoU8(GetCommandLineW()), nCmdShow);

	return	app.Run();
}

