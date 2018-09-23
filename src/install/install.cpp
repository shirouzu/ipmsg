static char *install_id = 
	"@(#)Copyright (C) H.Shirouzu 1998-2017   install.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Installer Application Class
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include "resource.h"
#include "install.h"
#include "../version.h"

#include "instcmn.h"
#include "instcore.h"
#include "../ipmsgdef.h"

using namespace std;

#ifdef DISALBE_HOOK
volatile int64 gEnableHook = 0;
#else
volatile int64 gEnableHook = ENABLE_HOOK;
#endif

#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "winmm.lib")

int ExecInTempDir();

#define TEMPDIR_OPT		L"/TEMPDIR"
#define UPDATE_OPT		L"/UPDATE"
#define SILENT_OPT		L"/SILENT"
#define EXTRACT_OPT		L"/EXTRACT"
#define EXTRACT32_OPT	L"/EXTRACT32"
#define EXTRACT64_OPT	L"/EXTRACT64"
#define INSTDIR_OPT		L"/INSTDIR="
#define DIR_OPT			L"/DIR="
#define NOPROG_OPT		L"/NOPROG"
#define NODESK_OPT		L"/NODESK"
#define NOAPP_OPT		L"/NOAPP"
#define NOSTARTUP_OPT	L"/NOSTARTUP"
#define NOEXEC_OPT		L"/NOEXEC"
#define NOSUBDIR_OPT	L"/NOSUBDIR"
#define UPDATED_OPT		L"/UPDATED"
#define INTERNAL_OPT	L"/INTERNAL"
#define HELP_OPT		L"/h"
#define TEMPDIR_OPT		L"/TEMPDIR"
#define RUNAS_OPT		L"/runas="

#define USAGE_STR	L"\r\n \
USAGE: \r\n \
	/SILENT ... silent install\r\n \
	/DIR=<dir> ... setup/target dir\r\n\r\n \
	/NOPROG ... no create program menu\r\n \
	/NODESK ... no create desktop shortcut\r\n \
	/NOSTARTUP ... no create startup shortcut\r\n \
	/NOAPP  ... no register application to OS\r\n\r\n \
	/EXTRACT   ... extract files\r\n \
	/EXTRACT32 ... extract 32bit files\r\n \
	/EXTRACT64 ... extract 64bit files\r\n \
	/NOSUBDIR  ... no create subdir with /EXTRACT \r\n\r\n \
"

char *SetupFiles [] = {
	IPMSG_EXE, UNINST_EXE, IPCMD_EXE, IPTOAST_DLL, IPMSG_CHM, IPEXC_PNG, IPMSG_PNG
};
char *DualFiles [] = { IPMSG_EXE, UNINST_EXE, IPCMD_EXE, IPTOAST_DLL };

/*
	WinMain
*/
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
//	::SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);
	::SetDllDirectory("");
	::SetCurrentDirectoryW(TGetExeDirW());

	if (!TSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
		TLoadLibraryExW(L"iertutil.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cryptbase.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"urlmon.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cscapi.dll", TLT_SYSDIR);

		WCHAR	*cmdLineW = ::GetCommandLineW();
		if (!wcsstr(cmdLineW, TEMPDIR_OPT) && !wcsstr(cmdLineW, RUNAS_OPT)) {
			return	ExecInTempDir();
		}
	}

	TInstApp	app(hI, cmdLine, nCmdShow);

	return	app.Run();
}


int ExecInTempDir()
{
// テンポラリディレクトリ作成
	WCHAR	temp[MAX_PATH] = L"";
	if (::GetTempPathW(wsizeof(temp), temp) == 0) {
		return	-1;
	}

	WCHAR	dir[MAX_PATH] = L"";
	int	dlen = MakePathW(dir, temp, L"");	// 末尾に \\ が無ければ追加

	while (1) {
		int64	dat;
		TGenRandomMT(&dat, sizeof(dat));
		snwprintfz(dir + dlen, wsizeof(dir) - dlen, L"ipinst-%llx", dat);
		if (::CreateDirectoryW(dir, NULL)) {
			break;
		}
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			return -1;
		}
	}

// テンポラリディレクトリにインストーラをコピー
	WCHAR	self[MAX_PATH] = L"";
	if (::GetModuleFileNameW(NULL, self, wsizeof(self)) == 0) {
		return -1;
	}

	WCHAR *fname = wcsrchr(self, '\\');
	if (!fname) {
		return -1;
	}

	WCHAR	new_self[MAX_PATH] = L"";
	MakePathW(new_self, dir, fname + 1);
	if (!::CopyFileW(self, new_self, TRUE)) {
		return -1;
	}

// テンポラリディレクトリのインストーラを実行
	WCHAR	cmdline[MAX_BUF] = L"";
	snwprintfz(cmdline, wsizeof(cmdline), L"\"%s\" ", new_self);

	int		argc = 0;
	WCHAR	**argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	for (int i=1; i < argc; i++) {
		wcsncatz(cmdline, argv[i], wsizeof(cmdline));
		wcsncatz(cmdline, L" ", wsizeof(cmdline));
	}
	wcsncatz(cmdline, TEMPDIR_OPT, wsizeof(cmdline));

	STARTUPINFOW		sui = { sizeof(sui) };
	PROCESS_INFORMATION pi = {};

	if (!::CreateProcessW(new_self, cmdline, 0, 0, 0, 0, 0, dir, &sui, &pi)) {
		return	-1;
	}

	::WaitForSingleObject(pi.hProcess, INFINITE);
	::CloseHandle(pi.hThread);
	::CloseHandle(pi.hProcess);

// テンポラリインストーラ終了後に、テンポラリファイル＆ディレクトリを削除
	for (int i=0; i < 100; i++) {
		if (::GetFileAttributesW(new_self) != 0xffffffff) {
			::DeleteFileW(new_self);
		}
		if (::GetFileAttributesW(dir) != 0xffffffff) {
			::RemoveDirectoryW(dir);
		}
		else {
			break;
		}
		Sleep(500);
	}

	return 0;
}

/*
	インストールアプリケーションクラス
*/
TInstApp::TInstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
}

TInstApp::~TInstApp()
{
}

void TInstApp::InitWindow(void)
{
	InitCommonControls();

	TDlg *maindlg = new TInstDlg(cmdLine);
	mainWnd = maindlg;
	maindlg->Create();
}


/*
	メインダイアログクラス
*/
TInstDlg::TInstDlg(char *cmdLine) :
	TDlg(INSTALL_DIALOG),
	runasBtn(this),
	progBar(this),
	msgStatic(this)
{
	runasImm = FALSE;
	runasWnd = NULL;
	isFirst = FALSE;
	isSilent = FALSE;
	isExt64 = TOs64();
	isExtract = FALSE;
	isInternal = FALSE;
	stat = INST_INIT;
	*setupDir = 0;
	*defaultDir = 0;
	*extractDir = 0;

	OpenDebugConsole(ODC_PARENT);
	fwCheckMode = 0;

	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.ChangeApp(HSTOOLS_STR, IP_MSG, TRUE)) {
		reg.GetInt(FWCHECKMODE_STR, &fwCheckMode);
	}
}

TInstDlg::~TInstDlg()
{
}

void TInstDlg::ErrMsg(const WCHAR *body, const WCHAR *title)
{
	auto	s = title ? FmtW(L"%s: %s\n", title, body) : body;

	if (isSilent) {
		OutW(s);
		TApp::Exit(-1);
	} else {
		MessageBoxW(s, IP_MSG_W);
	}
}

BOOL TInstDlg::ParseCmdLine()
{
	programLink = TRUE;
	desktopLink = TRUE;
	startupLink = TRUE;
	isAppReg    = TRUE;
	isExec      = TRUE;
	isSubDir    = TRUE;

	isAuto     = FALSE;
	isSilent   = FALSE;
	isExtract  = FALSE;
	isExt64    = TOs64();

	*setupDir   = 0;
	*defaultDir = 0;
	*extractDir = 0;

	orgArgv = CommandLineToArgvExW(::GetCommandLineW(), &orgArgc);

	for (int i=1; orgArgv[i] /*&& orgArgv[i][0] == '/'*/; i++) {
		auto	arg = orgArgv[i];

		if (wcsicmp(arg, UPDATE_OPT) == 0) {
			isAuto = TRUE;
			desktopLink = FALSE;
			startupLink = FALSE;
			programLink = FALSE;
		}
		else if (wcsicmp(arg, SILENT_OPT) == 0) {
			isSilent = TRUE;
		}
		else if (wcsicmp(arg, EXTRACT_OPT) == 0) {
			isExtract = TRUE;
		}
		else if (wcsicmp(arg, EXTRACT32_OPT) == 0) {
			isExtract = TRUE;
			isExt64   = FALSE;
		}
		else if (wcsicmp(arg, EXTRACT64_OPT) == 0) {
			isExtract = TRUE;
			isExt64   = TRUE;
		}
		else if (wcsicmp(arg, NOPROG_OPT) == 0) {
			programLink = FALSE;
		}
		else if (wcsicmp(arg, NODESK_OPT) == 0) {
			desktopLink = FALSE;
		}
		else if (wcsicmp(arg, NOAPP_OPT) == 0) {
			isAppReg = FALSE;
		}
		else if (wcsicmp(arg, NOSTARTUP_OPT) == 0) {
			startupLink = FALSE;
		}
		else if (wcsicmp(arg, NOEXEC_OPT) == 0) {
			isExec = FALSE;
		}
		else if (wcsicmp(arg, NOSUBDIR_OPT) == 0) {
			isSubDir = FALSE;
		}
		else if (wcsnicmp(arg, DIR_OPT, wsizeof(DIR_OPT)-1) == 0) {
			WtoU8(arg + wsizeof(DIR_OPT)-1, setupDir, sizeof(setupDir));
			strcpy(extractDir, setupDir);
		}
		else if (wcsnicmp(arg, INSTDIR_OPT, wsizeof(INSTDIR_OPT)-1) == 0) {
			WtoU8(arg + wsizeof(INSTDIR_OPT)-1, setupDir, sizeof(setupDir));
			strcpy(extractDir, setupDir);
		}
		else if (wcsicmp(arg, INTERNAL_OPT) == 0) {
			isInternal = TRUE;
			isSilent = TRUE;
		}
		else if (wcsnicmp(arg, RUNAS_OPT, wsizeof(RUNAS_OPT)-1) == 0) {
			auto	p = arg + wsizeof(RUNAS_OPT)-1;
			runasWnd = (HWND)wcstoull(p, 0, 16);
			if ((p = wcsstr(p, L",imm="))) {
				runasImm = wcstol(p+5, 0, 10);
			}
		}
		else if (wcsicmp(arg, TEMPDIR_OPT) == 0) {
			// nothing
		}
		else {
			ErrMsg(USAGE_STR, (wcsicmp(arg, HELP_OPT) == 0) ? NULL :
				FmtW(L"Unrecognized option: %s", arg));
			TApp::Exit(-1);
		}
	}
	if (isExtract) {
		isAuto = TRUE;
		isSilent = TRUE;
		isAppReg = FALSE;
		isExec = FALSE;
		desktopLink = FALSE;
		programLink = FALSE;
		startupLink = FALSE;
	}

	if (isInternal) {
		isAuto = TRUE;
		isSilent = TRUE;
		desktopLink = FALSE;
		programLink = FALSE;
		startupLink = FALSE;
	}

	return	TRUE;
}


BOOL TInstDlg::VerifyDict()
{
	GetIPDictBySelf(&ipDict);

	ipDict.get_str(VER_KEY, &ver);

	DynBuf	dict_hash;
	if (!ipDict.get_bytes(SHA256_KEY, &dict_hash)) return FALSE;
	ipDict.del_key(SHA256_KEY);

	DynBuf	dbuf;
	auto	size = ipDict.pack(&dbuf);
	if (size == 0) return FALSE;

	TDigest	digest;
	BYTE	sha256[SHA256_SIZE];

	digest.Init(TDigest::SHA256);
	digest.Update(dbuf.Buf(), (DWORD)size);
	if (!digest.GetVal(sha256)) return FALSE;

	return	memcmp(dict_hash.Buf(), sha256, SHA256_SIZE) == 0 ? TRUE : FALSE;
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TInstDlg::EvCreate(LPARAM lParam)
{
	ParseCmdLine();

	if (!VerifyDict()) {
		if (isSilent) {
			OutW(L"Installer is broken.\n");
		} else {
			MessageBox("Installer is broken.", "IPMsg");
		}
		TApp::Exit(-1);
		return	FALSE;
	}

	SetStat(INST_INIT);

	InitDir();
	SetupDlg();

	if (runasWnd) {
		if (!IsWindow(runasWnd)) {
			TApp::Exit(-1);
			return	FALSE;
		}
		CheckDlgButton(STARTUP_CHECK, ::IsDlgButtonChecked(runasWnd, STARTUP_CHECK));
		CheckDlgButton(DESKTOP_CHECK, ::IsDlgButtonChecked(runasWnd, DESKTOP_CHECK));
		CheckDlgButton(NOFW_CHK,      ::IsDlgButtonChecked(runasWnd, NOFW_CHK));

		WCHAR	wbuf[MAX_PATH] = L"";
		::SendDlgItemMessageW(runasWnd, FILE_EDIT, WM_GETTEXT, MAX_PATH, (LPARAM)wbuf);
		SetDlgItemTextU8(FILE_EDIT, WtoU8s(wbuf));

		::SendMessage(runasWnd, WM_IPMSG_HIDE, 0, 0);
	}

	if (isSilent || runasImm) {
		PostMessage(WM_IPMSG_INSTALL, 0, 0);
	}

	if (!isSilent) {
		GetWindowRect(&rect);
		int		cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int		xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		MoveWindow((cx - xsize)/2, (cy - ysize)/2, xsize, ysize, TRUE);
		EnableWindow(TRUE);
		Show();
	}

	if (!gEnableHook) {
		char	buf[MAX_BUF];
		GetWindowText(buf, sizeof(buf));
		strncatz(buf, " (Non Hook)", sizeof(buf));
		SetWindowText(buf);
	}

	return	TRUE;
}

BOOL TInstDlg::InitDir()
{
// 現在ディレクトリ設定
	char	buf[MAX_PATH_U8] = "";

	SHGetSpecialFolderPathU8(NULL, buf, CSIDL_LOCAL_APPDATA);
	MakePathU8(defaultDir, buf, IP_MSG);

	if (isInternal) {
		GetModuleFileNameU8(NULL, buf, sizeof(buf));
		GetParentDirU8(buf, setupDir);
		return	TRUE;
	}

// 既にセットアップされている場合は、セットアップディレクトリを読み出す
	isFirst = TRUE;
	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.ChangeApp(HSTOOLS_STR, IP_MSG, TRUE)) {
		if (reg.GetStr(REGSTR_PATH, buf, sizeof(buf)) && *buf) {
			isFirst = FALSE;
			strcpy(setupDir, buf);
		}
		reg.CloseKey();
	}

	if (!*setupDir) {
		strcpy(setupDir, defaultDir);
	}

	return	TRUE;
}

void TInstDlg::SetupDlg()
{
	SetDlgItemTextU8(FILE_EDIT, setupDir);
	CheckDlgButton(STARTUP_CHECK, 1);
	CheckDlgButton(DESKTOP_CHECK, 1);
	if (fwCheckMode) {
		CheckDlgButton(NOFW_CHK, 1);
	}

	HICON hBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
		32, 32, LR_DEFAULTCOLOR);
	HICON hSmallIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
		16, 16, LR_DEFAULTCOLOR);

	SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
	SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

	msgStatic.AttachWnd(GetDlgItem(INST_STATIC));

	progBar.AttachWnd(GetDlgItem(INST_PROG));
	progBar.SendMessage(PBM_SETRANGE, 0, MAKELPARAM(0, 9));
	progBar.SendMessage(PBM_SETSTEP, 1, 0);
	progBar.EnableWindow(FALSE);

	char	title[256];
	char	title2[256];

	GetWindowText(title, sizeof(title));
	sprintf(title2, "%s ver%s", title, GetVersionStr());
	SetWindowText(title2);

	::SetClassLong(hWnd, GCL_HICON, LONG_RDC(::LoadIcon(TApp::hInst(), (LPCSTR)SETUP_ICON)));

	WCHAR	buf[128];
	GetDlgItemTextW(SETUP_GRP, buf, wsizeof(buf));
	swprintf(buf + wcslen(buf), L"  (For %s)", isExt64 ? L"x64" : L"x86");
	SetDlgItemTextW(SETUP_GRP, buf);

	if (IsWinVista() && !::IsUserAnAdmin() && TIsEnableUAC()) {
		runasBtn.AttachWnd(GetDlgItem(RUNAS_BUTTON));
		runasBtn.CreateTipWnd(LoadStrW(IDS_LABLE_RUNAS));
		runasBtn.ShowWindow(SW_SHOW);
		runasBtn.SendMessage(BCM_SETSHIELD, 0, 1);
	}
}

BOOL TInstDlg::EvNcDestroy(void)
{
	if (runasWnd) {
		::PostMessage(runasWnd, WM_IPMSG_QUIT, 0, 0);
		runasWnd = NULL;
	}
	return	FALSE;
}

/*
	メインダイアログ用 WM_COMMAND 処理ルーチン
*/
BOOL TInstDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		isExtract = FALSE;
		switch (stat) {
		case INST_INIT:
		case INST_RETRY:
			Install();
			break;

		case INST_RUN:
			SetStat(INST_INIT);
			break;

		case INST_END:
			TApp::Exit(0);
			break;
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case RUNAS_BUTTON:
		RunAsAdmin(hWnd);
		return	TRUE;

	case EXTRACT_BUTTON:
		isExtract = TRUE;
		Extract();
		return	TRUE;

	case FILE_BUTTON:
		BrowseDirDlg(this, FILE_EDIT, "Select Install Directory");
		return	TRUE;

	case INITDIR_BTN:
		SetDlgItemTextU8(FILE_EDIT, defaultDir);
		return	TRUE;

	}
	return	FALSE;
}

BOOL TInstDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_IPMSG_HIDE:
		Show(SW_HIDE);
		return	TRUE;

	case WM_IPMSG_QUIT:
		if (wParam == 1) {
			AppKick((Mode)lParam);
		}
		TApp::Exit(0);
		return	TRUE;

	case WM_IPMSG_INSTALL:
		if (isExtract) {
			Extract();
		} else {
			Install();
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TInstDlg::AppKick(Mode mode)
{
	WCHAR	setupDirW[MAX_PATH];
	WCHAR	setupPathW[MAX_PATH];

	GetDlgItemTextW(FILE_EDIT, setupDirW, wsizeof(setupDirW));

	MakePathW(setupPathW, setupDirW, IPMSG_EXENAME_W);

	WCHAR	cmdLineW[MAX_BUF] = L"";
	snwprintfz(cmdLineW, wsizeof(cmdLineW),
		L"\"%s\" %s",
		setupPathW, 
		mode == FIRST ? L"/FIRST_RUN" :
		mode == HISTORY ? L"/SHOW_HISTORY" :
		mode == INSTALLED ? L"/INSTALLED" :
		mode == INTERNAL ? L"/UPDATED":
		mode == INTERNAL_ERR ? L"/UPDATE_ERR" : L"");

	STARTUPINFOW		sui = { sizeof(sui) };
	PROCESS_INFORMATION pi = {};

	if (!::CreateProcessW(setupPathW, cmdLineW, 0, 0, 0, 0, 0, setupDirW, &sui, &pi)) {
		return	FALSE;
	}
	::CloseHandle(pi.hThread);
	::CloseHandle(pi.hProcess);

	return	TRUE;
}

BOOL TInstDlg::Progress() // call by ExtractAll
{
	if (stat != INST_RUN) {
		return	FALSE;
	}
	progBar.SendMessage(PBM_STEPIT, 0, 0);
	this->Sleep(100);
	return	TRUE;
}

void TInstDlg::CreateShortCut(void)
{
	TRegistry	reg(HKEY_CURRENT_USER);
	char		setupPath[MAX_PATH_U8];
	char		buf[MAX_PATH_U8];
	char		path[MAX_PATH_U8];

	MakePathU8(setupPath, setupDir, IPMSG_EXENAME);

	if (!reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		return;
	}
	if (startupLink) {
		if (reg.GetStr(REGSTR_STARTUP, path, sizeof(path))) {
			RemoveSameLink(path, IPMSG_EXENAME);
			MakePathU8(buf, path, IPMSG_SHORTCUT_NAME);
			if (IsDlgButtonChecked(STARTUP_CHECK) || isSilent) {
				ShellLink(setupPath, buf);
			}
			else {
			//	DeleteLink(buf);
			}
		}
	}
	if (desktopLink) {
		if (reg.GetStr(REGSTR_DESKTOP, path, sizeof(path))) {
			MakePathU8(buf, path, IPMSG_SHORTCUT_NAME);
			if (IsDlgButtonChecked(DESKTOP_CHECK) || isSilent) {
				ShellLink(setupPath, buf);
			}
			else {
			//	DeleteLink(buf);
			}
		}
	}

	if (programLink) {
		if (reg.GetStr(REGSTR_PROGRAMS, path, sizeof(path))) {
			MakePathU8(buf, path, IPMSG_SHORTCUT_NAME);	// 旧登録の削除
			DeleteLink(buf);

			MakePathU8(buf, path, IPMSG_FULLNAME);
			CreateDirectoryU8(buf, NULL);

			MakePathU8(path, buf, IPMSG_SHORTCUT_NAME);
			ShellLink(setupPath, path, IPMSG_FULLNAME, IPMSG_APPID);

			MakePathU8(path, buf, UNINST_SHORTCUT_NAME);

			char	uninstPath[MAX_PATH_U8];
			MakePathU8(uninstPath, setupDir, UNINST_EXENAME);
			ShellLink(uninstPath, path);
		}
	}
}

void TInstDlg::RegisterAppInfo(void)
{
	if (!isAppReg) return;

	TRegistry	reg(HKEY_CURRENT_USER);
	char		setupPath[MAX_PATH_U8];

	MakePathU8(setupPath, setupDir, IPMSG_EXENAME);

	// パス情報を登録
	if (reg.ChangeApp(HSTOOLS_STR, IP_MSG)) {
		reg.SetStr(REGSTR_PATH, setupDir);
		reg.CloseKey();
	}

	// レジストリにアプリケーション情報を登録
	if (reg.OpenKey(REGSTR_PATH_APPPATHS)) {
		if (reg.CreateKey(IPMSG_EXENAME)) {
			reg.SetStr(NULL, setupPath);
			reg.SetStr(REGSTR_PATH, setupDir);
			reg.CloseKey();
		}
		reg.CloseKey();
	}

	// レジストリにアンインストール情報を登録
	if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
		if (reg.CreateKey(IPMSG_NAME)) {
			char path[MAX_PATH_U8];
			reg.SetStr(REGSTR_VAL_UNINSTALLER_DISPLAYNAME, IPMSG_FULLNAME);

			MakePath(path, setupDir, IPMSG_EXENAME);
			reg.SetStr(REGSTR_VAL_UNINSTALLER_DISPLAYICON, path);

			MakePath(path, setupDir, UNINST_EXENAME);
			reg.SetStr(REGSTR_VAL_UNINSTALLER_COMMANDLINE, path);

			reg.SetStr(REGSTR_VAL_UNINSTALLER_DISPLAYVER, GetVersionStr());

			reg.SetStr(REGSTR_VAL_UNINSTALLER_PUBLISHER, "H.Shirouzu & Asahi Net, Inc.");
			reg.SetInt(REGSTR_VAL_UNINSTALLER_ESTIMATESIZE, 3300); // KB
			reg.SetStr(REGSTR_VAL_UNINSTALLER_HELPLINK, LoadStrU8(IDS_IPMSGHELPURL));
			reg.SetStr(REGSTR_VAL_UNINSTALLER_URLUPDATEINFO, LoadStrU8(IDS_IPMSGURL));
			reg.SetStr(REGSTR_VAL_UNINSTALLER_URLINFOABOUT, LoadStrU8(IDS_SUPPORTBBS));
			reg.SetStr(REGSTR_VAL_UNINSTALLER_COMMENTS, "shirouzu@ipmsg.org");

			reg.CloseKey();
		}
		reg.CloseKey();
	}
}

BOOL TInstDlg::SetStat(Stat _stat)
{
	stat = _stat;

	int	chk_sw = (stat == INST_INIT || stat == INST_RETRY) ? SW_SHOW : SW_HIDE;
	int	msg_sw = (stat == INST_INIT || stat == INST_RETRY) ? SW_HIDE : SW_SHOW;

	::ShowWindow(GetDlgItem(STARTUP_CHECK), chk_sw);
	::ShowWindow(GetDlgItem(DESKTOP_CHECK), chk_sw);
	::ShowWindow(GetDlgItem(RUNAS_BUTTON), ::IsUserAnAdmin() ? SW_HIDE : chk_sw);
	::ShowWindow(GetDlgItem(INST_STATIC),   msg_sw);

	UINT id =	(stat == INST_INIT)  ? IDS_LABEL_START :
				(stat == INST_RETRY) ? IDS_LABEL_RETRY :
				(stat == INST_RUN)   ? IDS_LABEL_CANCEL :
				(stat == INST_END)   ? IDS_LABEL_COMPLETE : IDS_LABEL_START;
	::SetWindowText(GetDlgItem(IDOK), LoadStr(id));

	if (stat == INST_INIT || stat == INST_RETRY) {
		progBar.SendMessage(PBM_SETPOS, 0, 0);
	}
	::ShowWindow(GetDlgItem(NOFW_CHK),
		(fwCheckMode && (stat == INST_INIT || stat == INST_RETRY)) ? SW_SHOW : SW_HIDE);

	return	TRUE;
}

BOOL TInstDlg::CheckFw(BOOL *third_fw)
{
	BOOL	is_virtual = TIsVirtualizedDirW(U8toWs(setupDir));
	BOOL	need_admin = TIsEnableUAC() && !::IsUserAnAdmin();

	if (is_virtual && need_admin) {
		if (isSilent || MessageBox(LoadStr(IDS_REQUIREADMIN), INSTALL_STR,
				MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
			return	FALSE;
		}
		return	RunAsAdmin(hWnd, TRUE), FALSE;
	}

	TRegistry	reg(HKEY_CURRENT_USER);
	reg.ChangeApp(HSTOOLS_STR, IP_MSG);
	reg.SetInt(FWCHECKMODE_STR, 1);

	if (!IsDlgButtonChecked(NOFW_CHK)) {
		BOOL		is_domain  = IsDomainEnviron();
		BOOL		is_tout = FALSE;
		FwStatus	fs;
		char		setupPath[MAX_PATH_U8];

		MakePathU8(setupPath, setupDir, IPMSG_EXENAME);

		*third_fw = Is3rdPartyFwEnabled(TRUE, 5000, &is_tout);

		GetFwStatusEx(U8toWs(setupPath), &fs);
		// Is3rdParty../GetFwStat..で固まると次回は1に
		reg.SetInt(FWCHECKMODE_STR, is_tout);

		if (need_admin && !*third_fw && fs.fwEnable && !isSilent && // domain環境は拒否エントリ
			(fs.IsBlocked() || !(is_domain || fs.IsAllowed()))) {  // がすでに存在する場合のみ
			if (MessageBox(LoadStr(IDS_REQUIREADMIN_FW), INSTALL_STR,
					MB_OKCANCEL|MB_ICONINFORMATION) == IDOK) {
				return	RunAsAdmin(hWnd, TRUE), FALSE;
			}
			if (MessageBox(LoadStr(IDS_NOADMIN_CONTINUE), INSTALL_STR,
					MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
				return	FALSE;
			}
		}
	}
	return	TRUE;
}

BOOL TInstDlg::MakeExtractDir(char *extractDir)
{
	if (isSilent) {
		if (*extractDir == 0) {
			GetCurrentDirectoryU8(sizeof(extractDir), extractDir);
		}
	}
	else {
		::SHGetSpecialFolderPathU8(0, extractDir, CSIDL_DESKTOPDIRECTORY, FALSE);

		SetDlgItemTextU8(EXTRACT_EDIT, extractDir);

		if (!BrowseDirDlg(this, EXTRACT_EDIT, LoadStrU8(IDS_EXTRACTDIR), &isExt64)) {
			return FALSE;
		}
		if (!GetDlgItemTextU8(EXTRACT_EDIT, extractDir, MAX_PATH_U8)) {
			return FALSE;
		}
	}

	if (isSubDir) {
		auto	p = scope_raii(strdup(GetVersionStr()), [&](auto p) { free(p); });
		auto	major = strtok(p, ".");
		auto	minor = strtok(0, "");
		AddPathU8(extractDir, Fmt("ipmsg%s%s%s%s", major, minor,
			gEnableHook ? "" : "-n", isExt64 ? "_x64" : ""));
	}
	MakeDirAllU8(extractDir);
	return	TRUE;
}

void GetDictName(const char *fname, char *dname, BOOL is_x64)
{
	strcpy(dname, fname);

	if (is_x64 && find_if(begin(DualFiles), end(DualFiles),
		[&](auto v) { return !strcmp(v, fname); }) != end(DualFiles)) {
		strcat(dname, ".x64");
	}
}

BOOL TInstDlg::ExtractAll(const char *dir)
{
	BOOL	need_prog = (isSilent || isExtract) ? FALSE : TRUE;

	if (need_prog && !Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	CreateDirectoryU8(dir, NULL);

	DWORD	attr = GetFileAttributesU8(dir);

	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		SetExtractErr(CREATE_DIR_ERR, dir);
		return	FALSE;
	}

	int		max_retry = isSilent ? 10 : 1;

	if (need_prog && !Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

// ファイル生成
	for (auto &fname: SetupFiles) {
		BOOL	is_rotate = FALSE;
		char	path[MAX_PATH_U8];
		char	dname[MAX_PATH_U8];

		MakePath(path, dir, fname);
		GetDictName(fname, dname, isExt64);

		if (!IPDictCopy(&ipDict, dname, path, &is_rotate) &&
			!IsSameFileDict(&ipDict, dname, path)) {
			return	FALSE;
		}
		if (need_prog && !Progress()) {
			SetExtractErr(USER_CANCEL, dir);
			return	FALSE;
		}
	}

	return	TRUE;
}

// ファイル生成
BOOL TInstDlg::ExtractCore(char *dir)
{
	if (!ExtractAll(dir)) {
		const char *msg = "";
		const char *err_msg = NULL;
		ExtractErr err = GetExtractErr(&msg);

		switch (err) {
		case CREATE_DIR_ERR:
			msg = LoadStrU8(IDS_NOTCREATEDIR);
			break;

		case CREATE_FILE_ERR:
			msg = Fmt("%s\r\n%s", LoadStrU8(IDS_NOTCREATEFILE), err_msg);
			break;

		case DATA_BROKEN:
			msg = LoadStrU8(IDS_BROKENARCHIVE);
			break;

		case USER_CANCEL:
			msg = LoadStrU8(IDS_USERCANCEL);
			break;
		}
		if (isInternal) {	// 自動アップデート失敗の場合は、起動は行う
			AppKick(INTERNAL_ERR);
		}
		else {
			//msgStatic.SetWindowTextU8(msg);
			if (isSilent) {
				OutW(U8toWs(msg));
			} else {
				MessageBoxU8(msg, INSTALL_STR);
			}
		}
		SetStat(INST_RETRY);
		return	FALSE;
	}
	SetStat(INST_END);
	return	TRUE;
}

// 展開のみ
BOOL TInstDlg::Extract(void)
{
	if (!MakeExtractDir(extractDir)) {
		return FALSE;
	}

	if (!ExtractCore(extractDir)) {
		return	FALSE;
	}
	if (isSilent) {
		OutW(L"succeeded\n");
	} else {
		ShellExecuteW(NULL, NULL, U8toWs(extractDir), 0, 0, SW_SHOW);
	}
	TApp::Exit(0);
	return TRUE;
}

BOOL TInstDlg::Install(void)
{
// 現在、起動中の ipmsg を終了
	int	st = TerminateIPMsg(hWnd, LoadStr(IDS_TERMINATE), INSTALL_STR, isSilent);
	if (st == 1) {	// 手動中断
		return	FALSE;
	}

// インストールパス設定
	GetDlgItemTextU8(FILE_EDIT, setupDir, sizeof(setupDir));
	char	setupPath[MAX_PATH_U8];
	MakePathU8(setupPath, setupDir, IPMSG_EXENAME);

// FW check
	BOOL	third_fw = FALSE;
	if (IsWinVista()) {
		if (!CheckFw(&third_fw)) {
			return	FALSE;
		}
	}

	if (st == 2) {
		if (!isSilent) {
			MessageBox(LoadStr(IDS_CANTTERMINATE), INSTALL_STR);
			return	FALSE;
		}
	}
//	if (!runasImm && !isSilent &&
//		MessageBox(LoadStr(IDS_START), INSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
//		return	FALSE;
//	}
	runasImm = FALSE;

	SetStat(INST_RUN);
	msgStatic.SetWindowTextU8(LoadStrU8(IDS_MSG_INSTALL));
	progBar.EnableWindow(TRUE);

// 展開
	if (!ExtractCore(setupDir)) {
		return	FALSE;
	}

	// Firewall 登録
	if (IsUserAnAdmin()) {
		SetFwStatusEx(U8toWs(setupPath), IP_MSG_W);
	}

	// スタートアップ＆デスクトップにショートカットを登録
	CreateShortCut();
	RegisterAppInfo();

// コピーしたアプリケーションを起動
	const char *msg = LoadStr(IDS_SETUPCOMPLETE);
	int			flg = MB_OKCANCEL|MB_ICONINFORMATION;

	if (IsWinVista() && third_fw) {
		msg = Fmt("%s\r\n\r\n%s", msg, LoadStr(IDS_COMPLETE_3RDFWADD));
//		flg |= MB_DEFBUTTON2;
	}
//	TLaunchDlg	dlg(msg, isFirst, this);
	if (isExec) {
		Mode	mode = isInternal ? INTERNAL : SIMPLE;

		if (!isSilent) {
			if (isFirst) {
				mode = FIRST;
			}
			//else if (dlg.needHist) {
			//	mode = HISTORY;
			// }
			else {
				mode = INSTALLED;
			}
		}
		if (runasWnd) {
			Wstr	wbuf(setupDir);
			if (::SendDlgItemMessageW(runasWnd, FILE_EDIT, WM_SETTEXT, 0, (LPARAM)wbuf.Buf())) {
				::PostMessage(runasWnd, WM_IPMSG_QUIT, 1, mode);
				runasWnd = NULL;
			}
		}
		else {
			AppKick(mode);
		}
	}

	if (runasWnd) {
		::PostMessage(runasWnd, WM_IPMSG_QUIT, 0, 0);
		runasWnd = NULL;
	}

	if (isSilent) {
		OutW(L"succeeded\n");
	}

//	msgStatic.SetWindowTextU8(LoadStrU8(IDS_INST_SUCCESS));
//	::SetWindowText(GetDlgItem(IDOK), LoadStr(IDS_LABEL_COMPLETE));

	TApp::Exit(0);
	return	TRUE;
}

/*
	起動ダイアログ
*/
TLaunchDlg::TLaunchDlg(LPCSTR _msg, BOOL _isFirst, TWin *_win) : TDlg(LAUNCH_DIALOG, _win)
{
	msg = strdup(_msg);
	isFirst = _isFirst;
	needHist = FALSE;
}

TLaunchDlg::~TLaunchDlg()
{
	free(msg);
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TLaunchDlg::EvCreate(LPARAM lParam)
{
	SetDlgItemText(MESSAGE_STATIC, msg);
	HBITMAP	hBmp = ::LoadBitmap(TApp::hInst(), (LPCSTR)PAYPAL_BITMAP);

	if (isFirst) {
		::ShowWindow(GetDlgItem(HIST_CHECK), SW_HIDE);
	}
	else {
		CheckDlgButton(HIST_CHECK, FALSE);
	}

	Show();
	return	TRUE;
}

#define NOTIFY_SETTINGS	L"shell32.dll,Options_RunDLL 5"

BOOL TLaunchDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case IDOK:
		needHist = IsDlgButtonChecked(HIST_CHECK);
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}


/*
	一行入力
*/
TInputDlg::TInputDlg(char *_dirBuf, TWin *_win) : TDlg(INPUT_DIALOG, _win)
{
	dirBuf = _dirBuf;
}

BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextU8(INPUT_EDIT, dirBuf, MAX_PATH_U8);
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

/*
	ディレクトリダイアログ用汎用ルーチン
*/
BOOL BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title, BOOL *is_x64)
{ 
	IMalloc			*iMalloc = NULL;
	BROWSEINFOW		brInfo;
	LPITEMIDLIST	pidlBrowse;
	char			buf[MAX_PATH_U8];
	WCHAR			wbuf[MAX_PATH];
	Wstr			wtitle(title);

	parentWin->GetDlgItemTextU8(editCtl, buf, sizeof(buf));
	U8toW(buf, wbuf, wsizeof(wbuf));
	if (!SUCCEEDED(SHGetMalloc(&iMalloc))) {
		return FALSE;
	}

	TBrowseDirDlg	dirDlg(buf, sizeof(buf), is_x64);
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = wbuf;
	brInfo.lpszTitle = wtitle.Buf();
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS|BIF_SHAREABLE|BIF_BROWSEINCLUDEFILES|BIF_USENEWUI;
	brInfo.lpfn = BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)&dirDlg;
	brInfo.iImage = 0;

	BOOL	ret = FALSE;
	do {
		if ((pidlBrowse = ::SHBrowseForFolderW(&brInfo))) {
			if (::SHGetPathFromIDListW(pidlBrowse, wbuf)) {
				::SetDlgItemTextW(parentWin->hWnd, editCtl, wbuf);
				ret = TRUE;
			}
			iMalloc->Free(pidlBrowse);
			break;
		}
	} while (dirDlg.IsDirty());

	iMalloc->Release();
 	return	ret;
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK BrowseDirDlg_Proc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM data)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		((TBrowseDirDlg *)data)->AttachWnd(hWnd);
		break;

	case BFFM_SELCHANGED:
		if (((TBrowseDirDlg *)data)->hWnd)
			((TBrowseDirDlg *)data)->SetFileBuf(lParam);
		break;
	}
	return 0;
}

/*
	BrowseDlg用サブクラス生成
*/
BOOL TBrowseDirDlg::AttachWnd(HWND _hWnd)
{
	BOOL	ret = TSubClass::AttachWnd(_hWnd);
	dirtyFlg = FALSE;

// ディレクトリ設定
	DWORD	attr = GetFileAttributesU8(fileBuf);
	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		GetParentDirU8(fileBuf, fileBuf);
	}
	SendMessageW(BFFM_SETSELECTIONW, TRUE, (LPARAM)U8toWs(fileBuf));
	SetWindowText(IPMSG_FULLNAME);

// ボタン作成
	if (is_x64) {
		TPoint	pt = { 30, 30 };
		TSize	sz = { 80, 30 };

		::CreateWindow(BUTTON_CLASS, "For x86", WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|WS_GROUP,
			pt.x, pt.y, sz.cx, sz.cy, hWnd, (HMENU)X86_RADIO, TApp::GetInstance(), NULL);

		::CreateWindow(BUTTON_CLASS, "For x64", WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON,
			pt.x + sz.cx, pt.y, sz.cx, sz.cy, hWnd, (HMENU)X64_RADIO, TApp::GetInstance(), NULL);

		CheckDlgButton(*is_x64 ? X64_RADIO : X86_RADIO, TRUE);

		HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
		if (hDlgFont) {
			SendDlgItemMessage(X86_RADIO, WM_SETFONT, (LPARAM)hDlgFont, 0L);
			SendDlgItemMessage(X64_RADIO, WM_SETFONT, (LPARAM)hDlgFont, 0L);
		}
	}

	return	ret;
}

/*
	BrowseDlg用 WM_COMMAND 処理
*/
BOOL TBrowseDirDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case MKDIR_BUTTON:
		{
			char		dirBuf[MAX_PATH_U8], path[MAX_PATH_U8];
			TInputDlg	dlg(dirBuf, this);
			if (dlg.Exec() != IDOK)
				return	TRUE;
			MakePathU8(path, fileBuf, dirBuf);
			if (CreateDirectoryU8(path, NULL))
			{
				strcpy(fileBuf, path);
				dirtyFlg = TRUE;
				PostMessage(WM_CLOSE, 0, 0);
			}
		}
		return	TRUE;

	case RMDIR_BUTTON:
		if (RemoveDirectoryU8(fileBuf))
		{
			GetParentDirU8(fileBuf, fileBuf);
			dirtyFlg = TRUE;
			PostMessage(WM_CLOSE, 0, 0);
		}
		return	TRUE;

	case X86_RADIO:
		*is_x64 = FALSE;
		return	TRUE;

	case X64_RADIO:
		*is_x64 = TRUE;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TBrowseDirDlg::SetFileBuf(LPARAM list)
{
	Wstr	wbuf(MAX_PATH);
	BOOL	ret = ::SHGetPathFromIDListW((LPITEMIDLIST)list, wbuf.Buf());
	if (ret) {
		WtoU8(wbuf.s(), fileBuf, fileBufSize);
	}
	return	ret;
}

