/* static char *cmdwin_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		cmdwin.h	Ver4.61"; */
/* ========================================================================
	Project  Name			: IP Messenger Command
	Create					: 2017-05-24(Wed)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	======================================================================== */
#ifndef CMDWIN_H
#define CMDWIN_H

class TCmdWin : public TWin {
	HWND	hIPMsgWnd;
	HANDLE	hIPMsgProc;

	IPDict	in;
	IPDict	out;
	HANDLE	hRecv;

	HWND FindIPMsgWindow(int port, Addr nic);

	BOOL SendCmd();
	BOOL PostCmd();

	BOOL CmdGetAbs();
	BOOL CmdSetAbs();
	BOOL CmdList();
	BOOL CmdRefresh();
	BOOL CmdSendMsg();
	BOOL CmdRecvMsg();
	BOOL CmdGetState();
	BOOL CmdTerminate();

	BOOL PostSendMsg();
	BOOL PostRecvMsg();
	BOOL PostTerminate();

	void Exit(int _result, const char *msg=NULL);

public:
	TCmdWin() : TWin() {
		hIPMsgWnd = NULL;
		hIPMsgProc = NULL;
		result = -1;
	}
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);

	BOOL DoCmd();
	int		result;
	BOOL	useStat;
	std::vector<U8str>	arg;
};

void U8Out(const char *fmt,...);

#endif

