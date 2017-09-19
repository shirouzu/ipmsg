static char *ipcmd_id = 
	"@(#)Copyright (C) H.Shirouzu 2017   ipcmd.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Command
	Create					: 2017-05-28(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../ipmsg.h"
#include "cmdwin.h"

using namespace std;

#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "imm32.lib")
#ifndef _WIN64
#pragma comment (lib, "Shlwapi.lib")
#endif
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "Ws2_32.lib")

class TCmdApp : public TApp {
	TCmdWin	*qwnd;
	int		argc;
	WCHAR	**argv;

public:
	TCmdApp(int argc, WCHAR **argv);
	virtual ~TCmdApp();

	virtual void	InitWindow(void);
	virtual BOOL	PreProcMsg(MSG *msg);

	int	Result() {
		return qwnd ? qwnd->result : -1;
	}
	BOOL UseStat() {
		return qwnd ? qwnd->useStat : TRUE;
	}
};


TCmdApp::TCmdApp(int _argc, WCHAR **_argv)
	: TApp(::GetModuleHandle(NULL), "", SW_HIDE)
{
	::SetDllDirectory("");

	if (!TSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
		TLoadLibraryExW(L"iertutil.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cryptbase.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"urlmon.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cscapi.dll", TLT_SYSDIR);
	}

	qwnd = NULL;
	WSADATA	wsaData;
	::WSAStartup(MAKEWORD(2,2), &wsaData);

	argc = _argc;
	argv = _argv;
}

TCmdApp::~TCmdApp()
{
}

BOOL TCmdApp::PreProcMsg(MSG *msg)
{
	return	TApp::PreProcMsg(msg);
}

void TCmdApp::InitWindow(void)
{
	if (IsWinVista() && ::IsUserAnAdmin() && TIsEnableUAC()) {
		TChangeWindowMessageFilter(WM_IPMSG_CMDVER, 1);
		TChangeWindowMessageFilter(WM_IPMSG_CMDVERRES, 1);
		TChangeWindowMessageFilter(WM_IPMSG_CMD, 1);
		TChangeWindowMessageFilter(WM_IPMSG_CMDRES, 1);
		TChangeWindowMessageFilter(WM_CMDWIN_MAIN, 1);
	}

	qwnd = new TCmdWin;

	for (int i=1; i < argc; i++) {
		qwnd->arg.push_back(argv[i]);
	}

	mainWnd = qwnd;

	OpenDebugConsole(ODC_PARENT);
	qwnd->Create();
	qwnd->PostMessage(WM_CMDWIN_MAIN, 0, 0);
}

int wmain(int argc, WCHAR **argv)
{
	if (IsWin2003Only()) {
		U8Out("Sorry, ipcmd is not supported in Windows Server 2003 (or WinXP(x64))\n");
		return	-1;
	}

	TCmdApp	app(argc, argv);

	app.Run();

//	Debug("fin\n");
	if (app.UseStat()) {
		U8Out("stat=%d\n", app.Result());
	}

	return app.Result();
}

void U8Out(const char *fmt,...)
{
	char buf[8192];

	va_list	ap;
	va_start(ap, fmt);
	vsnprintfz(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	Wstr	w(buf);
	DWORD wlen = (DWORD)w.Len();

	static HANDLE hStdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);

	::WriteConsoleW(hStdOut, w.s(), wlen, &wlen, 0);
}

