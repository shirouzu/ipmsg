static char *ipmsg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2015   ipmsg.cpp	Ver3.51";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Application Class
	Create					: 1996-06-01(Sat)
	Update					: 2015-06-21(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"

#define IPMSG_USAGE \
	"ipmsg.exe [portno] [/MSG   [/LOG] [/SEAL] <hostname or IP addr> <message>]\r\n" \
	"ipmsg.exe [portno] [/MSGEX [/LOG] [/SEAL] <hostname or IP addr> " \
												"<msg_line1 \\n msg_line2...>]\r\n" \
	"ipmsg.exe [portno] [/NIC nic_addr] [/NICLIST] [/NICID n] <hostname or IP addr>"

TMsgApp::TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
	LoadLibrary("RICHED20.DLL");
	srand((UINT)Time());
}

TMsgApp::~TMsgApp()
{
}

BOOL SendCmdlineMessage(Addr nicAddr, int port_no, char *msg, BOOL is_multiline)
{
	MsgMng	msgMng(nicAddr, port_no);
	ULONG	command = IPMSG_SENDMSG|IPMSG_NOADDLISTOPT|IPMSG_NOLOGOPT;
	Addr	destAddr;
	char	*tok, *p;

	for (tok=separate_token(msg, ' ', &p) ; tok && *tok == '/';
		 tok=separate_token(NULL, ' ', &p)) {
		if (stricmp(tok, "/LOG") == 0)
			command &= ~IPMSG_NOLOGOPT;
		else if (stricmp(tok, "/SEAL") == 0)
			command |= IPMSG_SECRETOPT;
	}

	if (!(destAddr = ResolveAddr(tok)).IsEnabled()) return FALSE;

	if (!(msg = separate_token(NULL, 0, &p))) return FALSE;

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

	return	msgMng.Send(destAddr, ::htons(port_no), command, msg);
}

void NicListMessage(bool is_v6, const char *msg=NULL)
{
	int			num       = 0;
	AddrInfo	*addrs    = GetIPAddrs(GIA_NOBROADCAST, &num, is_v6);
	char		buf[8192] = "", *p = buf;

	if (msg)  p += sprintf(p, "%s\n\n", msg);
	if (!num) p += sprintf(p, "No Nic\n");

	for (int i=0; addrs && i < num && sizeof(buf) - (p - buf) > 100; i++) {
		p += sprintf(p, " NIC(%d) = %s\n", i+1, addrs[i].addr.ToStr());
	}
	MessageBox(0, buf, IP_MSG, MB_OK);
	delete [] addrs;
}

void TMsgApp::InitWindow(void)
{
	HWND		hWnd;
	char		class_name[MAX_PATH_U8] = IPMSG_CLASS, *tok, *p;
	char		*class_ptr = NULL;
	Addr		nicAddr = 0;
	int			port_no = 0;
	BOOL		show_history = FALSE;
	enum Stat { ST_NORMAL, ST_TASKBARUI_MSG, ST_EXIT, ST_ERR } status = ST_NORMAL;
	int			taskbar_msg = 0;
	int			taskbar_cmd = 0;

	if (*cmdLine == '"') cmdLine = strchr(cmdLine+1, '"');
	if (cmdLine)         cmdLine = strchr(cmdLine, ' ');
	if (!cmdLine)        cmdLine = "";

	if ((port_no = atoi(cmdLine)) == 0) port_no = IPMSG_DEFAULT_PORT;

	MsgMng::WSockStartup(); // for Addr::ToStr and other sock functions

	if ((tok = strchr(cmdLine, '/'))) {
		DWORD	exit_status = 0xffffffff;

		for (tok=separate_token(tok, ' ', &p); tok && *tok == '/';
				tok=separate_token(NULL, ' ', &p)) {
			if (stricmp(tok, "/NICLIST") == 0 || stricmp(tok, "/NICLIST6") == 0) {
				bool		is_v6  = stricmp(tok, "/NICLIST6") == 0;
				NicListMessage(is_v6);
				status = ST_EXIT;
			}
			else if (stricmp(tok, "/NICID") == 0 || stricmp(tok, "/NICID6") == 0) {	// NICID 指定
				bool is_v6 = stricmp(tok, "/NICID6") == 0;

				status = ST_EXIT;
				if ((tok = separate_token(NULL, ' ', &p))) {
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
				if ((tok = separate_token(NULL, ' ', &p)) &&
					(nicAddr = ResolveAddr(tok)).IsEnabled()) {
					status = ST_NORMAL;
				}
				if (status == ST_EXIT) NicListMessage(is_v6, "NIC addr not found");
			}
			else if (stricmp(tok, "/MSG") == 0) {	// コマンドラインモード
				tok = separate_token(NULL, 0, &p);
				status = SendCmdlineMessage(nicAddr, port_no, tok, FALSE) ? ST_EXIT : ST_ERR;
			}
			else if (stricmp(tok, "/MSGEX") == 0) {	// コマンドラインモード（複数行）
				tok = separate_token(NULL, 0, &p);
				status = SendCmdlineMessage(nicAddr, port_no, tok, TRUE) ? ST_EXIT : ST_ERR;
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

	if (port_no != IPMSG_DEFAULT_PORT || nicAddr.IsEnabled()) {
		char	*p = class_name + wsprintf(class_name, "%s_%d", IPMSG_CLASS, port_no);
		if (nicAddr.IsEnabled()) {
			*p++ = '_';
			nicAddr.ToStr(p);
		}
	}

	HANDLE	hMutex = ::CreateMutex(NULL, FALSE, class_name);
	::WaitForSingleObject(hMutex, INFINITE);

	if ((hWnd = FindWindowU8(class_name)) ||
		!TRegisterClassU8(class_name, CS_DBLCLKS, ::LoadIcon(hI, (LPCSTR)IPMSG_ICON),
						::LoadCursor(NULL, IDC_ARROW), (HBRUSH)::GetStockObject(WHITE_BRUSH))) {
		if (hWnd) ::SetForegroundWindow(hWnd);
		::ExitProcess(0xffffffff);
		return;
	}

	mainWnd = new TMainWin(nicAddr, port_no);
	mainWnd->CreateU8(class_name);
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

	if (!IsWinXP()) {
		MessageBox(0, "Please use old version (v3.42 or earlier)",
					"Win2000 is not supported", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TMsgApp	app(hI, WtoU8(GetCommandLineW()), nCmdShow);

	return	app.Run();
}

