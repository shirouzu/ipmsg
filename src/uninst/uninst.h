/* @(#)Copyright (C) H.Shirouzu 1998-2011   install.h	Ver4.61 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

class TUninstDlg : public TDlg
{
protected:
	HWND			runasWnd;
	TSubClassCtl	runasBtn;
	BOOL			isSilent;

	void	DeleteShortcut();
	void	RemoveAppRegs();
	BOOL	DeletePubkey();

public:
	TUninstDlg(char *cmdLine);
	virtual ~TUninstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL	UnInstall(void);
};

class TUninstApp : public TApp
{
public:
	TUninstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TUninstApp();

	void InitWindow(void);
};

#define IPMSG_QUIT_MESSAGE		(WM_APP + 100)
#define UNINSTALL_STR			"UnInstall"


