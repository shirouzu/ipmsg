static char *ipmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2012   ipmsg.cpp	Ver3.42";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Application Class
	Create					: 1996-06-01(Sat)
	Update					: 2012-06-10(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <time.h>
#include "resource.h"
#include "ipmsg.h"

#define IPMSG_CLASS	"ipmsg_class"
#define IPMSG_USAGE "ipmsg.exe [portno] [/MSG [/LOG] [/SEAL] <hostname or IP addr> <message>]\r\nipmsg.exe [portno] [/NICLIST] [/NIC nic_addr]"

TMsgApp::TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
	LoadLibrary("RICHED20.DLL");
	srand((UINT)Time());
	TLibInit_AdvAPI32();
	TLibInit_Crypt32();
	TLibInit_WinSock();
}

TMsgApp::~TMsgApp()
{
}

void TMsgApp::InitWindow(void)
{
	HWND		hWnd;
	char		class_name[MAX_PATH_U8] = IPMSG_CLASS, *tok, *msg, *p;
	char		*class_ptr = NULL;
	ULONG		nicAddr = 0;
	int			port_no = atoi(cmdLine);
	BOOL		show_history = FALSE;
	enum Stat { ST_NORMAL, ST_TASKBARUI_MSG, ST_EXIT, ST_ERR } status = ST_NORMAL;
	int			taskbar_msg = 0;
	int			taskbar_cmd = 0;

	if (port_no == 0) port_no = IPMSG_DEFAULT_PORT;

	if ((tok = strchr(cmdLine, '/'))) {
		DWORD	exit_status = 0xffffffff;

		for (tok=separate_token(tok, ' ', &p); tok && *tok == '/';
				tok=separate_token(NULL, ' ', &p)) {
			if (stricmp(tok, "/NICLIST") == 0) {
				int			num = 0;
				AddrInfo	*addrs = GetIPAddrs(FALSE, &num);
				char		buf[8192] = "No NIC", *p = buf;

				for (int i=0; addrs && i < num; i++) {
					p += sprintf(p, " NIC(%d) = %s\n", i+1, Tinet_ntoa(*(LPIN_ADDR)&addrs[i].addr));
				}
				MessageBox(0, buf, IP_MSG, MB_OK);
				delete [] addrs;
				status = ST_EXIT;
			}
			else if (stricmp(tok, "/NICID") == 0) {	// NICID 指定
				status = ST_ERR;
				if ((tok = separate_token(NULL, ' ', &p))) {
					int			target = atoi(tok) - 1;
					int			num = 0;
					AddrInfo	*addrs = GetIPAddrs(FALSE, &num);
					if (addrs && target > 0 && target < num) {
						nicAddr = addrs[target].addr;
						status = ST_NORMAL;
					}
					delete [] addrs;
				}
				if (status == ST_ERR) break;
			}
			else if (stricmp(tok, "/NIC") == 0) {	// NIC 指定
				if (!(tok = separate_token(NULL, ' ', &p)) || !(nicAddr = ResolveAddr(tok))) {
					status = ST_ERR;
					break;
				}
			}
			else if (stricmp(tok, "/MSG") == 0) {	// コマンドラインモード
				MsgMng	msgMng(nicAddr, port_no);
				ULONG	command = IPMSG_SENDMSG|IPMSG_NOADDLISTOPT|IPMSG_NOLOGOPT, destAddr;
				status  = ST_EXIT;

				while ((tok = separate_token(NULL, ' ', &p)) && *tok == '/') {
					if (stricmp(tok, "/LOG") == 0)
						command &= ~IPMSG_NOLOGOPT;
					else if (stricmp(tok, "/SEAL") == 0)
						command |= IPMSG_SECRETOPT;
				}
				if ((msg = separate_token(NULL, 0, &p)) && (destAddr = ResolveAddr(tok))) {
					exit_status = msgMng.Send(destAddr, Thtons(port_no), command, msg) ? 0 : -1;
				}
				else status = ST_ERR;
			}
			else if (stricmp(tok, "/SHOW_HISTORY") == 0) {	// インストーラからの起動
				show_history = TRUE;
			}
			else if (stricmp(tok, "/TASKBAR_MSG") == 0) {	// TaskbarUI用
				if (!(class_ptr = separate_token(NULL, ' ', &p))
				|| !(tok = separate_token(NULL, ' ', &p))
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

	if (port_no != IPMSG_DEFAULT_PORT || nicAddr) {
		wsprintf(class_name, nicAddr ? "%s_%d_%s" : "%s_%d",
			IPMSG_CLASS, port_no, Tinet_ntoa(*(in_addr *)&nicAddr));
	}

	HANDLE	hMutex = ::CreateMutex(NULL, FALSE, class_name);
	::WaitForSingleObject(hMutex, INFINITE);

	if ((hWnd = FindWindowU8(class_name)) ||
		!TRegisterClassU8(class_name, CS_DBLCLKS, ::LoadIcon(hI, (LPCSTR)IPMSG_ICON),
						::LoadCursor(NULL, IDC_ARROW))) {
		if (hWnd) ::SetForegroundWindow(hWnd);
		::ExitProcess(0xffffffff);
		return;
	}

	mainWnd = new TMainWin(nicAddr, port_no);
	mainWnd->Create(class_name);
	::ReleaseMutex(hMutex);
	::CloseHandle(hMutex);

	if (show_history) mainWnd->SendMessage(WM_COMMAND, MENU_HELP_HISTORY, 0);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	if (IsWin95()) {
		MessageBox(0, "Please use old version (v2.06 or earlier)",
					"Win95/98/Me is not supported", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TMsgApp	app(hI, cmdLine, nCmdShow);

	return	app.Run();
}

