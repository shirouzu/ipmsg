/* @(#)Copyright (C) H.Shirouzu 1998-2015   install.h	Ver4.61 */
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Main Header
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#define WM_IPMSG_HIDE			(WM_APP + 100)
#define WM_IPMSG_QUIT			(WM_APP + 101)
#define WM_IPMSG_INSTALL		(WM_APP + 102)

class TInstDlg : public TDlg
{
protected:
	BOOL	runasImm;
	HWND	runasWnd;
	BOOL	isFirst;
	BOOL	isSilent;
	BOOL	isExt64;
	BOOL	isExtract;
	BOOL	isInternal;

	BOOL	programLink;
	BOOL	desktopLink;
	BOOL	startupLink;
	BOOL	isAppReg;
	BOOL	isExec;
	BOOL	isSubDir;
	BOOL	isAuto;

	int		fwCheckMode;

	IPDict	ipDict;
	U8str	ver;

	WCHAR	**orgArgv;
	int		orgArgc;

	enum Stat { INST_INIT, INST_RUN, INST_RETRY, INST_END } stat;

	char	setupDir[MAX_PATH_U8];
	char	defaultDir[MAX_PATH_U8];
	char	extractDir[MAX_PATH_U8];
	enum Mode { SIMPLE, HISTORY, FIRST, INSTALLED, INTERNAL, INTERNAL_ERR };

	TSubClassCtl runasBtn;
	TSubClassCtl progBar;
	TSubClassCtl msgStatic;

	BOOL	VerifyDict();
	BOOL	InitDir();
	void	SetupDlg();
	void	CreateShortCut();
	void	RegisterAppInfo();
	BOOL	CheckFw(BOOL *third_fw);
	BOOL	ParseCmdLine();
	BOOL	SetStat(Stat _stat);
	void	ErrMsg(const WCHAR *body, const WCHAR *title);
	BOOL	MakeExtractDir(char *extractDir);
	BOOL	ExtractCore(char *dir);
	BOOL	Extract();
	BOOL	ExtractAll(const char *dir);

public:
	TInstDlg(char *cmdLine);
	virtual ~TInstDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
	BOOL	Install(void);
	BOOL	AppKick(Mode mode);

	BOOL	Progress();
};

class TInstApp : public TApp
{
public:
	TInstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TInstApp();

	void InitWindow(void);
};

class TLaunchDlg : public TDlg
{
protected:
	char *msg;
	BOOL isFirst;

public:
	TLaunchDlg(LPCSTR _msg, BOOL isFirst ,TWin *_win);
	virtual ~TLaunchDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);

	BOOL	needHist;
};

class TInputDlg : public TDlg
{
protected:
	char	*dirBuf;

public:
	TInputDlg(char *_dirBuf, TWin *_win);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

class TBrowseDirDlg : public TSubClass
{
protected:
	char	*fileBuf = NULL;
	int		fileBufSize = 0;
	BOOL	dirtyFlg = FALSE;
	BOOL	*is_x64 = NULL;

public:
	TBrowseDirDlg(char *_fileBuf, int _fileBufSize, BOOL *_is_x64=NULL) :
		fileBuf(_fileBuf),
		fileBufSize(_fileBufSize),
		is_x64(_is_x64)
		{}
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	BOOL	IsDirty(void) { return dirtyFlg; };
};

enum CreateStatus { CS_OK, CS_BROKEN, CS_ACCESS };
CreateStatus CreateFileBySelf(const char *path, const char *fname);

#define FDATA_KEY	"fdata"
#define MTIME_KEY	"mtime"
#define FSIZE_KEY	"fsize"
#define VER_KEY		"ver"
#define SHA256_KEY	"sha256"

