/* @(#)Copyright (C) H.Shirouzu 1998-2011   install.h	Ver3.00 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2011-04-10(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

class TUninstDlg : public TDlg
{
protected:
	HWND	runasWnd;

public:
	TUninstDlg(char *cmdLine);
	virtual ~TUninstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL	UnInstall(void);
	int		TerminateIPMsg(void);
};

class TUninstApp : public TApp
{
public:
	TUninstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TUninstApp();

	void InitWindow(void);
};

#define IPMSG_QUIT_MESSAGE		(WM_USER + 100)

#define IPMSG_CLASS				"ipmsg_class"
#define IPMSG_NAME				"IPMSG for Win32"
#define HSTOOLS_STR				"HSTools"
#define IPMSG_DEFAULT_PORT		0x0979

#define IPMSG_EXENAME			"ipmsg.exe"

#define IPMSG_SHORTCUT_NAME		IPMSG_NAME ".lnk"
#define UNC_PREFIX				"\\\\"
#define MAX_BUF					1024

#define REGSTR_SHELLFOLDERS		REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP			"Startup"
#define REGSTR_DESKTOP			"Desktop"
#define REGSTR_PROGRAMS			"Programs"
#define REGSTR_PATH				"Path"
#define REGSTR_PROGRAMFILES		"ProgramFilesDir"

#define UNINSTALL_STR			"UnInstall"

// function prototype
BOOL CALLBACK TerminateIPMsgProc(HWND hWnd, LPARAM lParam);
BOOL DeleteLink(LPCSTR path);
BOOL GetParentDirU8(const char *srcfile, char *dir);
int MakePath(char *dest, const char *dir, const char *file);
UINT GetDriveTypeEx(const char *file);
BOOL RemoveSameLink(const char *dir, char *remove_path=NULL);
inline BOOL IsUncFile(const char *path) { return strnicmp(path, UNC_PREFIX, 2) == 0; }

