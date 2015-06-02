/* @(#)Copyright (C) H.Shirouzu 1998-2011   install.h	Ver3.00 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2011-04-10(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

enum InstMode { SETUP_MODE, RESETUP_MODE, UNINSTALL_MODE };

struct InstallCfg {
	InstMode	mode;
	int			portNo;
	BOOL		startupLink;
	BOOL		programLink;
	BOOL		desktopLink;
	BOOL		delPubkey;
};

class TInstSheet : public TDlg
{
	InstallCfg	*cfg;

public:
	TInstSheet(TWin *_parent, InstallCfg *_cfg);

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);

	void	Paste(void);
	void	GetData(void);
	void	PutData(void);
};

class TInstDlg : public TDlg
{
protected:
	TInstSheet		*propertySheet;
	InstallCfg		cfg;

public:
	TInstDlg(char *cmdLine);
	virtual ~TInstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
#if 0
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
	BOOL	Install(void);
	BOOL	UnInstall(void);
	void	ChangeMode(void);
	BOOL	TerminateIPMsg(void);
	BOOL	RemoveSameLink(const char *dir, char *remove_path=NULL);
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

#define IPMSG_CLASS				"ipmsg_class"
#define IPMSG_NAME				"IPMSG for Win32"
#define IPMSG_FULLNAME			"IP Messenger for Win"
#define IPMSG_STR				"IPMsg"
#define HSTOOLS_STR				"HSTools"
#define IPMSG_DEFAULT_PORT		0x0979

#define IPMSG_EXENAME			"ipmsg.exe"
#define INSTALL_EXENAME			"setup.exe"
#define IPMSGHELP_NAME			"ipmsg.chm"

#define UNINSTALL_CMDLINE		"/r"
#define IPMSG_SHORTCUT_NAME		IPMSG_NAME ".lnk"
#define UNC_PREFIX				"\\\\"
#define MAX_WRAPPER_IPMSGSIZE	50000
#define RESOLVE_WRAPPER_IPMSG	"org\\ipmsg.exe"
#define MAX_BUF					1024

#define REGSTR_SHELLFOLDERS		REGSTR_PATH_EXPLORER "\\Shell Folders"
#define REGSTR_STARTUP			"Startup"
#define REGSTR_DESKTOP			"Desktop"
#define REGSTR_PROGRAMS			"Programs"
#define REGSTR_PATH				"Path"
#define REGSTR_PROGRAMFILES		"ProgramFilesDir"

#define INSTALL_STR				"Install"
#define UNINSTALL_STR			"UnInstall"

// function prototype
BOOL CALLBACK TerminateIPMsgProc(HWND hWnd, LPARAM lParam);
BOOL SymLink(LPCSTR src, LPSTR dest, LPCSTR arg="");
BOOL ReadLink(LPCSTR src, LPSTR dest, LPSTR arg);
BOOL DeleteLink(LPCSTR path);
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title);
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data);
BOOL GetParentDirU8(const char *srcfile, char *dir);
int MakePath(char *dest, const char *dir, const char *file);
UINT GetDriveTypeEx(const char *file);

// inline function
inline BOOL IsUncFile(const char *path) { return strnicmp(path, UNC_PREFIX, 2) == 0; }

