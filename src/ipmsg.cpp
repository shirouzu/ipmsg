static char *ipmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   ipmsg.cpp	Ver3.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Application Class
	Create					: 1996-06-01(Sat)
	Update					: 2011-04-20(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <time.h>
#include "resource.h"
#include "ipmsg.h"

#define IPMSG_CLASS	"ipmsg_class"

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

	if (port_no == 0)
		port_no = IPMSG_DEFAULT_PORT;

	if ((tok = strchr(cmdLine, '/')) && separate_token(tok, ' ', &p))
	{
		BOOL	diag = TRUE;
		DWORD	status = 0xffffffff;

		if (stricmp(tok, "/NIC") == 0)	// NIC 指定
		{
			if (tok = separate_token(NULL, ' ', &p))
				nicAddr = ResolveAddr(tok);
		}
		else if (stricmp(tok, "/MSG") == 0)	// コマンドラインモード
		{
			MsgMng	msgMng(nicAddr, port_no);
			ULONG	command = IPMSG_SENDMSG|IPMSG_NOADDLISTOPT|IPMSG_NOLOGOPT, destAddr;

			while ((tok = separate_token(NULL, ' ', &p)) && *tok == '/') {
				if (stricmp(tok, "/LOG") == 0)
					command &= ~IPMSG_NOLOGOPT;
				else if (stricmp(tok, "/SEAL") == 0)
					command |= IPMSG_SECRETOPT;
			}

			if ((msg = separate_token(NULL, 0, &p)))
			{
				diag = FALSE;
				 if ((destAddr = ResolveAddr(tok)))
					status = msgMng.Send(destAddr, htons(port_no), command, msg) ? 0 : -1;
			}
		}
		if (nicAddr == 0)
		{
			if (diag)
				MessageBox(0, "ipmsg.exe [portno] [/MSG [/LOG] [/SEAL] <hostname or IP addr> <message>]\r\nipmsg.exe [portno] [/NIC nic_addr]", IP_MSG, MB_OK);
			::ExitProcess(status);
			return;
		}
	}

	if (port_no != IPMSG_DEFAULT_PORT || nicAddr)
		wsprintf(class_name, nicAddr ? "%s_%d_%s" : "%s_%d", IPMSG_CLASS, port_no, inet_ntoa(*(in_addr *)&nicAddr));

	HANDLE	hMutex = ::CreateMutex(NULL, FALSE, class_name);
	::WaitForSingleObject(hMutex, INFINITE);

	if ((hWnd = FindWindowU8(class_name)) || TRegisterClass(class_name, CS_DBLCLKS, ::LoadIcon(hI, (LPCSTR)IPMSG_ICON), ::LoadCursor(NULL, IDC_ARROW)) == 0)
	{
		if (hWnd)
			::SetForegroundWindow(hWnd);
		::ExitProcess(0xffffffff);
		return;
	}

	mainWnd = new TMainWin(nicAddr, port_no);
	mainWnd->Create(class_name, IP_MSG, WS_OVERLAPPEDWINDOW | (IsNewShell() ? WS_MINIMIZE : 0));
	::ReleaseMutex(hMutex);
	::CloseHandle(hMutex);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	if (IsWin95()) {
		MessageBox(0, "Please use old version (v2.06 or earlier)", "Win95/98/Me is not support", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TMsgApp	app(hI, cmdLine, nCmdShow);

	return	app.Run();
}

