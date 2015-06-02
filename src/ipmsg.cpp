static char *ipmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   ipmsg.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Application Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <time.h>
#include "resource.h"
#include "ipmsg.h"

#define IPMSG_CLASS	"ipmsg_class"
#define IPMSG_USAGE "ipmsg.exe [portno] [/MSG [/LOG] [/SEAL] <hostname or IP addr> <message>]\r\nipmsg.exe [portno] [/NIC nic_addr]"

TMsgApp::TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
	LoadLibrary("RICHED20.DLL");
	srand((UINT)Time());
	TLibInit_AdvAPI32();
	TLibInit_Crypt32();
}

TMsgApp::~TMsgApp()
{
}

void TMsgApp::InitWindow(void)
{
	HWND		hWnd;
	char		class_name[MAX_PATH_U8] = IPMSG_CLASS, *tok, *msg, *p;
	ULONG		nicAddr = 0;
	int			port_no = atoi(cmdLine);
	BOOL		show_history = FALSE;
	enum Stat { ST_NORMAL, ST_EXIT, ST_ERR } status = ST_NORMAL;

	if (port_no == 0) port_no = IPMSG_DEFAULT_PORT;

	if ((tok = strchr(cmdLine, '/'))) {
		DWORD	exit_status = 0xffffffff;

		for (tok=separate_token(tok, ' ', &p); tok && *tok == '/';
				tok=separate_token(NULL, ' ', &p)) {
			if (stricmp(tok, "/NIC") == 0) {	// NIC 指定
				if (!(tok = separate_token(NULL, ' ', &p)) || !(nicAddr = ResolveAddr(tok))) {
					status = ST_ERR;
					break;
				}
			}
			else if (stricmp(tok, "/MSG") == 0) {	// コマンドラインモード
				MsgMng	msgMng(nicAddr, port_no);
				ULONG	command = IPMSG_SENDMSG|IPMSG_NOADDLISTOPT|IPMSG_NOLOGOPT, destAddr;

				while ((tok = separate_token(NULL, ' ', &p)) && *tok == '/') {
					if (stricmp(tok, "/LOG") == 0)
						command &= ~IPMSG_NOLOGOPT;
					else if (stricmp(tok, "/SEAL") == 0)
						command |= IPMSG_SECRETOPT;
				}
				if ((msg = separate_token(NULL, 0, &p)) && (destAddr = ResolveAddr(tok))) {
					exit_status = msgMng.Send(destAddr, htons(port_no), command, msg) ? 0 : -1;
				}
				else status = ST_ERR;
			}
			else if (stricmp(tok, "/SHOW_HISTORY") == 0) {	// インストーラからの起動
				show_history = TRUE;
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

	if (port_no != IPMSG_DEFAULT_PORT || nicAddr) {
		wsprintf(class_name, nicAddr ? "%s_%d_%s" : "%s_%d",
			IPMSG_CLASS, port_no, inet_ntoa(*(in_addr *)&nicAddr));
	}

	HANDLE	hMutex = ::CreateMutex(NULL, FALSE, class_name);
	::WaitForSingleObject(hMutex, INFINITE);

	if ((hWnd = FindWindowU8(class_name)) ||
		TRegisterClass(class_name, CS_DBLCLKS, ::LoadIcon(hI, (LPCSTR)IPMSG_ICON),
						::LoadCursor(NULL, IDC_ARROW)) == 0) {
		if (hWnd) ::SetForegroundWindow(hWnd);
		::ExitProcess(0xffffffff);
		return;
	}

	mainWnd = new TMainWin(nicAddr, port_no);
	mainWnd->Create(class_name, IP_MSG, WS_OVERLAPPEDWINDOW | (IsNewShell() ? WS_MINIMIZE : 0));
	::ReleaseMutex(hMutex);
	::CloseHandle(hMutex);

	if (show_history) mainWnd->SendMessage(WM_COMMAND, MENU_HELP_HISTORY, 0);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	if (IsWin95()) {
		MessageBox(0, "Please use old version (v2.06 or earlier)",
					"Win95/98/Me is not support", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TMsgApp	app(hI, cmdLine, nCmdShow);

	return	app.Run();
}

