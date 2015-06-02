/* @(#)Copyright (C) H.Shirouzu 1998-2011   install.h	Ver3.20 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

class TInstDlg : public TDlg
{
protected:
	BOOL	runasImm;
	HWND	runasWnd;

public:
	TInstDlg(char *cmdLine);
	virtual ~TInstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL	Install(void);
	BOOL	AppKick(void);
	int		TerminateIPMsg(void);
};

class TInstApp : public TApp
{
public:
	TInstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TInstApp();

	void InitWindow(void);
};

class TBrowseDirDlg : public TSubClass
{
protected:
	char	*fileBuf;
	BOOL	dirtyFlg;

public:
	TBrowseDirDlg(char *_fileBuf) { fileBuf = _fileBuf; }
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	BOOL	IsDirty(void) { return dirtyFlg; };
};

class TInputDlg : public TDlg
{
protected:
	char	*dirBuf;

public:
	TInputDlg(char *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win) { dirBuf = _dirBuf; }
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

#define WM_IPMSG_HIDE			(WM_USER + 100)
#define WM_IPMSG_QUIT			(WM_USER + 101)
#define WM_IPMSG_INSTALL		(WM_USER + 102)

#define IPMSG_CLASS				"ipmsg_class"
#define IPMSG_NAME				"IPMSG for Win32"
#define IPMSG_FULLNAME			"IP Messenger for Win"
#define IPMSG_STR				"IPMsg"
#define HSTOOLS_STR				"HSTools"
#define IPMSG_DEFAULT_PORT		0x0979

#define IPMSG_EXENAME			"ipmsg.exe"
#define SETUP_EXENAME			"setup.exe"
#define IPMSGHELP_NAME			"ipmsg.chm"

#define IPMSG_SHORTCUT_NAME		IPMSG_NAME ".lnk"
#define UNC_PREFIX				"\\\\"
#define MAX_BUF					1024

#define REGSTR_SHELLFOLDERS		REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP			"Startup"
#define REGSTR_DESKTOP			"Desktop"
#define REGSTR_PROGRAMS			"Programs"
#define REGSTR_PATH				"Path"
#define REGSTR_PROGRAMFILES		"ProgramFilesDir"
#define REGSTR_PROGRAMFILESX86	"ProgramFilesDir (x86)"

#define INSTALL_STR				"Install"

// function prototype
BOOL CALLBACK TerminateIPMsgProc(HWND hWnd, LPARAM lParam);
BOOL SymLink(LPCSTR src, LPSTR dest, LPCSTR arg="");
BOOL DeleteLink(LPCSTR path);
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title);
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
BOOL GetParentDirU8(const char *srcfile, char *dir);
int MakePath(char *dest, const char *dir, const char *file);
enum CreateStatus { CS_OK, CS_BROKEN, CS_ACCESS };
CreateStatus CreateFileBySelf(char *path, char *fname);
BOOL RemoveSameLink(const char *dir, char *remove_path=NULL);

