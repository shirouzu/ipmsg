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
	BOOL	isInternal;
	int		fwCheckMode;

	enum Stat { INST_INIT, INST_RUN, INST_RETRY, INST_END } stat;

	char	setupDir[MAX_PATH_U8];
	enum Mode { SIMPLE, HISTORY, FIRST, INSTALLED, INTERNAL, INTERNAL_ERR };

	TSubClassCtl runasBtn;
	TSubClassCtl progBar;
	TSubClassCtl msgStatic;

	BOOL	InitDir();
	void	CreateShortCut();
	BOOL	ParseCmdLine();
	BOOL	SetStat(Stat _stat);

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
	char	*fileBuf;
	int		fileBufSize;
	BOOL	dirtyFlg;

public:
	TBrowseDirDlg(char *_fileBuf, int _fileBufSize) {
		fileBuf		= _fileBuf;
		fileBufSize	= _fileBufSize;
	}
	virtual BOOL	AttachWnd(HWND _hWnd);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	SetFileBuf(LPARAM list);
	BOOL	IsDirty(void) { return dirtyFlg; };
};

enum CreateStatus { CS_OK, CS_BROKEN, CS_ACCESS };
CreateStatus CreateFileBySelf(const char *path, const char *fname);

