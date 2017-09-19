static char *cmdwin_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		cmdwin.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: IP Messenger Inter-Process Communication
	Create					: 2017-05-24(Wed)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	======================================================================== */

#include "../ipmsg.h"
#include "cmdwin.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <conio.h>

using namespace std;

BOOL TCmdWin::EvCreate(LPARAM lParam)
{
	hIPMsgWnd = NULL;
	useStat = TRUE;
	hRecv = NULL;

//	for (auto &u: arg) {
//		DebugU8(" %s\n", u.s());
//	};

	return	TRUE;
}

void TCmdWin::Exit(int _result, const char *msg)
{
	result = _result;
	if (msg) {
		U8Out(msg);
	}
	TApp::Exit(result);
}

bool IsConsole()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (!GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE), &csbi)) {
		return	false;
	}
	return	(csbi.dwCursorPosition.X || csbi.dwCursorPosition.Y) ? true : false;
}

void exit_usage()
{
	bool	is_console = IsConsole();

	fprintf(stderr,
		"ipcmd [/port=N] [/nic=ipaddr] [/nostat]\n"
		"    getabs                 ... Get absence mode\n"
		"    setabs (0|1|...|N)     ... Set absence mode\n"
		"    list  ... get userlist\n"
		"    refresh [/noremove]    ... Refresh user list\n"
		"    send [/file=path1 /file=path2...] [/noseal]\n"
		"         \"(uid|ipaddr|ALL)[,uid...]\"\n"
		"         (\"msg_body\" | /msgfile=path)\n"
		"                           ... Send message\n"
		"    recv [/msgfile=path]   ... Receive message\n"
		"    state                  ... Get misc status\n"
		"    terminate              ... Terminate ipmsg\n"
	);

	if (!is_console) {
		fprintf(stderr, "\n  Push Any Key ... ");
		(void)_getch();
	}

	::ExitProcess(-1);
}

HWND TCmdWin::FindIPMsgWindow(int port, Addr nic)
{
	char	class_name[MAX_BUF];
	char	*p = class_name;

	p += strcpyz(p, IPMSG_CLASS);
	if (port || nic.IsEnabled()) {
		if (!port) {
			port = IPMSG_DEFAULT_PORT;
		}
		p += sprintf(p, "_%d", port);
		if (nic.IsEnabled()) {
			p += sprintf(p, "_%s", nic.S());
		}
	}
	return	FindWindowU8(class_name);
}

BOOL TCmdWin::DoCmd()
{
	auto	i = arg.begin();

	int		port = 0;
	Addr	nic;

	while (i != arg.end()) {
		auto	&s = *i;
		if (strnicmp(s.s(), "/port=", 6) == 0) {
			U8str	p(&s[6]);
			port = atoi(p.s());
			i = arg.erase(i);
		}
		else if (strnicmp(s.s(), "/nic=", 5) == 0) {
			U8str	p(&s[5]);
			nic.Set(p.s());
			i = arg.erase(i);
		}
		else if (strnicmp(s.s(), "/nostat", 7) == 0) {
			useStat = FALSE;
			i = arg.erase(i);
		}
		else {
			break;
		}
	}
	if (i == arg.end()) {
		exit_usage();
	}

	hIPMsgWnd = FindIPMsgWindow(port, nic);

	auto	c = *i;
	i = arg.erase(i);

	if (!strcmpi(c.s(), "getabs")) {
		CmdGetAbs();
	}
	else if (!strcmpi(c.s(), "setabs")) {
		CmdSetAbs();
	}
	else if (!strcmpi(c.s(), "list")) {
		CmdList();
	}
	else if (!strcmpi(c.s(), "refresh")) {
		CmdRefresh();
	}
	else if (!strcmpi(c.s(), "send")) {
		CmdSendMsg();
	}
	else if (!strcmpi(c.s(), "recv")) {
		CmdRecvMsg();
	}
	else if (!strcmpi(c.s(), "state")) {
		CmdGetState();
	}
	else if (!strcmpi(c.s(), "terminate")) {
		CmdTerminate();
	}
	else {
		exit_usage();
	}

	return	TRUE;
}

BOOL TCmdWin::SendCmd()
{
	if (!hIPMsgWnd) {
		Exit(-1, "ipmsg doesn't exist\n");
		return	FALSE;
	}
	DWORD	pid = 0;
	if (::GetWindowThreadProcessId(hIPMsgWnd, &pid)) {
		hIPMsgProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
	}

	SetTimer(IPMSG_CMD_TIMER, 1000);	// watch dog

	LRESULT lr = ::SendMessage(hIPMsgWnd, WM_IPMSG_CMDVER, (DWORD_PTR)hWnd, 0);
	if (lr != IPMSG_CMD_VER1) {
		Exit(-1);
		return	FALSE;
	}

	IPIpc	ipc;

	if (!ipc.SaveDictToMap(hWnd, TRUE, in)) {
		Exit(-1);
		return	FALSE;
	}

	::SendMessage(hIPMsgWnd, WM_IPMSG_CMD, (DWORD_PTR)hWnd, 0);

	return	TRUE;
}

BOOL TCmdWin::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CMDWIN_MAIN:
		DoCmd();
		return	TRUE;

	case WM_IPMSG_CMDRES:
		{
			IPIpc	ipc;
			ipc.LoadDictFromMap(hWnd, FALSE, &out);
			PostMessage(WM_IPMSG_POSTCMD, 0, 0);
		}
		return	TRUE;

	case WM_IPMSG_POSTCMD:
		PostCmd();
		TApp::Exit(result);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TCmdWin::EvTimer(WPARAM timerID, TIMERPROC proc)
{
	if (!::IsWindow(hIPMsgWnd)) {
		KillTimer(timerID);
		U8Out("ipmsg doesn't exist\n");
		TApp::Exit(result);
	}
	return	TRUE;
}

BOOL TCmdWin::PostCmd()
{
	int64	val = 0;
	if (out.get_int(IPMSG_STAT_KEY, &val)) {
		result = INT_RDC(val);
	}

	U8str	u;
	if (out.get_str(IPMSG_ERRINFO_KEY, &u)) {
		U8Out("%s\n", u.s());
	}

	int64	cmd = 0;
	in.get_int(IPMSG_CMD_KEY, &cmd);

	switch (cmd) {
	case IPMSG_CMD_SENDMSG:
		PostSendMsg();
		break;

	case IPMSG_CMD_RECVMSG:
		PostRecvMsg();
		break;

	case IPMSG_CMD_TERMINATE:
		PostTerminate();
		break;
	}

	return	TRUE;
}

BOOL TCmdWin::CmdGetAbs()
{
	auto	i = arg.begin();

	if (arg.end() - i != 0) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_GETABSENCE);

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::CmdSetAbs()
{
	auto	i = arg.begin();

	if (arg.end() - i != 1) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_SETABSENCE);
	in.put_int(IPMSG_ABSMODE_KEY, atoi(i->s()));

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::CmdList()
{
	auto	i = arg.begin();

	if (arg.end() - i != 0) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_GETHOSTLIST);

	SendCmd();

	IPDictList	dlist;

	if (out.get_dict_list(IPMSG_HOSTLIST_KEY, &dlist)) {
		for (auto &h: dlist) {
			Host	host;
			if (DictToHost(h, &host)) {
				U8Out("%s\n", host.S());
			}
		}
	}

	return	TRUE;
}

BOOL TCmdWin::CmdRefresh()
{
	auto	i = arg.begin();

	int64	flags = IPMSG_REMOVE_FLAG;

	if (i != arg.end()) {
		auto	&s = *i++;
		if (stricmp(s.s(), "/noremove") == 0) {
			flags &= ~IPMSG_REMOVE_FLAG;
		}
		else {
			exit_usage();
		}
	}

	if (i != arg.end()) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_REFRESH);
	in.put_int(IPMSG_FLAGS_KEY, flags);

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::CmdSendMsg()
{
	auto	i = arg.begin();
	int64	flags = IPMSG_SEAL_FLAG;

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_SENDMSG);

	IPDictStrList	files;
	while (i != arg.end()) {
		auto	&s = *i;
		if (strnicmp(s.s(),  "/file=", 6) == 0) {
			char	path[MAX_PATH_U8];
			char	*fname;

			if (!GetFullPathNameU8(&s[6], MAX_PATH_U8, path, &fname) ||
				GetFileAttributesU8(path) == 0xffffffff) {
				U8Out("path not found (%s)\n", path);
				Exit(-1);
				return	FALSE;
			}
			files.push_back(make_shared<U8str>(path));
			i++;
		}
		else if (stricmp(s.s(), "/noseal") == 0) {
			flags &= ~IPMSG_SEAL_FLAG;
			i++;
		}
		else {
			break;
		}
	}
	in.put_int(IPMSG_FLAGS_KEY, flags);

	if (files.size()) {
		in.put_str_list(IPMSG_FILELIST_KEY, files);
	}
	if (arg.end() - i != 2) exit_usage();

	IPDictStrList	to;
	auto			&s = *i++;
	char			*tok, *p;

	for (tok=sep_tok(s.Buf(), ',', &p); tok; tok=sep_tok(NULL, ',', &p)) {
		to.push_back(make_shared<U8str>(tok));
	}
	if (to.size() == 0) exit_usage();
	in.put_str_list(IPMSG_TOLIST_KEY, to);

	auto	&body = *i++;
	if (strnicmp(body.s(), "/msgfile=", 9) == 0) {
		auto path = body.s() + 9;
		auto fh = CreateFileU8(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 
					0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (fh == INVALID_HANDLE_VALUE) {
			Exit(-1, Fmt("msgfile can't open (%s) %d\n", path, ::GetLastError()));
			return	FALSE;
		}
		DWORD	size = ::GetFileSize(fh, 0);
		U8str	data(size + 1);
		if (!::ReadFile(fh, data.Buf(), size, &size, 0)) {
			Exit(-1, Fmt("msgfile can't read(%s) %d\n", path, ::GetLastError()));
			return	FALSE;
		}
		data[size] = 0;
		if (!IsUTF8(data.Buf())) {
			Exit(-1,
				Fmt("msgfile is required as UTF-8 encoding (%s) %d\n", path, ::GetLastError()));
			return	FALSE;
		}
		in.put_str(IPMSG_BODY_KEY, data.s());
	}
	else {
		in.put_str(IPMSG_BODY_KEY, body.s());
	}

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::PostSendMsg()
{
	IPDictStrList	tlist;

	if (out.get_str_list(IPMSG_TOLIST_KEY, &tlist)) {
		for (auto &u: tlist) {
			U8Out("%s\n", u->s());
		}
	}
	return	TRUE;
}

BOOL TCmdWin::CmdRecvMsg()
{
	auto	i = arg.begin();
	int64	flags = 0;

	while (i != arg.end()) {
		auto	&s = *i;
		if (strnicmp(s.s(), "/msgfile=", 9) == 0) {
			const char *path = s.s() + 9;
			hRecv = CreateFileU8(path, GENERIC_WRITE, 0, 
						0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
			if (hRecv == INVALID_HANDLE_VALUE) {
				Exit(-1, Fmt("msgfile can't open (%s) %d\n", path, ::GetLastError()));
				return FALSE;
			}
			i++;
		}
		else if (stricmp(s.s(), "/proc") == 0) {
			flags |= IPMSG_RECVPROC_FLAG;
			i++;
		}
		else {
			break;
		}
	}
	if (i != arg.end()) exit_usage();

	in.put_int(IPMSG_FLAGS_KEY, flags);
	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_RECVMSG);

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::PostRecvMsg()
{
	U8str	s;

	if (out.get_str(IPMSG_BODY_KEY, &s)) {
		if (hRecv) {
			DWORD	size = s.Len();
			::WriteFile(hRecv, s.s(), size, &size, 0);
			::CloseHandle(hRecv);
			hRecv = NULL;
		}
		else {
			U8Out("%s\n", s.s());
		}
	}
	return	TRUE;
}

BOOL TCmdWin::CmdGetState()
{
	auto	i = arg.begin();

	if (arg.end() - i != 0) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_GETSTATE);

	SendCmd();

	U8str	body;

	if (out.get_str(IPMSG_BODY_KEY, &body)) {
		U8Out("%s\n", body.s());
	}

	return	TRUE;
}

BOOL TCmdWin::CmdTerminate()
{
	auto	i = arg.begin();

	if (i != arg.end()) exit_usage();

	in.put_int(IPMSG_CMD_KEY, IPMSG_CMD_TERMINATE);

	SendCmd();

	return	TRUE;
}

BOOL TCmdWin::PostTerminate()
{
	if (result == 0) {
		::WaitForSingleObject(hIPMsgProc, INFINITE);
	}
	return	TRUE;
}

