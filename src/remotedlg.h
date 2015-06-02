/*static char *remotedlg_id = 
	"@(#)Copyright (C) H.Shirouzu 2013-2014   remotedlg.h	Ver3.50"; */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Remote Dialog
	Create					: 2013-09-29(Sun)
	Update					: 2014-04-14(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef REMOTEDLG_H
#define REMOTEDLG_H

#define IPMSG_REMOTE_TIMER 100

class TRemoteDlg : public TDlg {
public:
	enum Mode { INIT=0, REBOOT=1, EXIT=2, STANDBY=3, HIBERNATE=4 };

	TRemoteDlg(Cfg *_cfg, TWin *_parent);
	BOOL Start(Mode _mode);
	BOOL StartCore();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvTimer(WPARAM timerID, TIMERPROC proc);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);

protected:
	Cfg		*cfg;
	Mode	mode;
	int		count;
	int		graceSec;
	LPCSTR	title;
	LPCSTR	notifyFmt;
	DWORD	startTick;
};

#endif

