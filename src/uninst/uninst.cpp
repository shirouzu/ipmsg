static char *uninst_id = 
	"@(#)Copyright (C) H.Shirouzu 1998-2017   uninst.cpp	Ver4.61";
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
#include "uninst.h"
#include "../version.h"
#include "../ipmsgdef.h"

#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "winmm.lib")

#define MAIN_EXEC
#include "../install/instcmn.h"

/*
	WinMain
*/
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	::SetDllDirectory("");
	::SetCurrentDirectoryW(TGetExeDirW());

	if (!TSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
		TLoadLibraryExW(L"iertutil.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cryptbase.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"urlmon.dll", TLT_SYSDIR);
		TLoadLibraryExW(L"cscapi.dll", TLT_SYSDIR);
	}

	TUninstApp	app(hI, cmdLine, nCmdShow);
	return	app.Run();
}

/*
	インストールアプリケーションクラス
*/
TUninstApp::TUninstApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow) : TApp(_hI, _cmdLine, _nCmdShow)
{
}

TUninstApp::~TUninstApp()
{
}

void TUninstApp::InitWindow(void)
{
	InitCommonControls();
	TDlg *maindlg = new TUninstDlg(cmdLine);
	mainWnd = maindlg;
	maindlg->Create();
}


#define SILENT_OPT	L"/SILENT"
#define HELP_OPT	L"/h"
#define RUNAS_OPT	L"/runas="

#define USAGE_STR	L"\r\n \
USAGE: \r\n \
	/SILENT ... silent uninstall\r\n \
"

/*
	メインダイアログクラス
*/
TUninstDlg::TUninstDlg(char *cmdLine) :
	TDlg(UNINSTALL_DIALOG),
	runasBtn(this)
{
	isSilent = FALSE;
	runasWnd = NULL;
	OpenDebugConsole(ODC_PARENT);

	int		orgArgc;
	auto	orgArgv = CommandLineToArgvExW(::GetCommandLineW(), &orgArgc);

	for (int i=1; orgArgv[i] /*&& orgArgv[i][0] == '/'*/; i++) {
		auto	arg = orgArgv[i];

		if (wcsicmp(arg, SILENT_OPT) == 0) {
			isSilent = TRUE;
		}
		else if (wcsnicmp(arg, RUNAS_OPT, wsizeof(RUNAS_OPT)-1) == 0) {
			auto	p = arg + wsizeof(RUNAS_OPT)-1;
			runasWnd = (HWND)wcstoull(p, 0, 16);
		}
		else {
			ErrMsg(USAGE_STR, (wcsicmp(arg, HELP_OPT) == 0) ? NULL :
				FmtW(L"Unrecognized option: %s", arg));
			TApp::Exit(-1);
		}
	}
}

TUninstDlg::~TUninstDlg()
{
}

void TUninstDlg::ErrMsg(const WCHAR *body, const WCHAR *title)
{
	auto	s = title ? FmtW(L"%s: %s\n", title, body) : body;

	if (isSilent) {
		OutW(s);
		TApp::Exit(-1);
	} else {
		MessageBoxW(s, IP_MSG_W);
	}
}

BOOL IsAppRegistered()
{
	TRegistry	reg(HKEY_LOCAL_MACHINE);

	if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
		if (reg.OpenKey(IPMSG_NAME)) {
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TUninstDlg::EvCreate(LPARAM lParam)
{
	char	title[256];
	char	title2[256];

	GetWindowText(title, sizeof(title));
	sprintf(title2, "%s ver%s", title, GetVersionStr());
	SetWindowText(title2);

	if (IsWinVista() && !::IsUserAnAdmin() && TIsEnableUAC()) {
		runasBtn.AttachWnd(GetDlgItem(RUNAS_BUTTON));
		runasBtn.CreateTipWnd(LoadStrW(IDS_LABLE_RUNAS));
		runasBtn.ShowWindow(SW_SHOW);
		runasBtn.SendMessage(BCM_SETSHIELD, 0, 1);
	}

	GetWindowRect(&rect);
	int		cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int		xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;

	::SetClassLong(hWnd, GCL_HICON, LONG_RDC(::LoadIcon(TApp::hInst(), (LPCSTR)SETUP_ICON)));

	if (isSilent) {
		Show(SW_HIDE);
	}
	else {
		MoveWindow((cx - xsize)/2, (cy - ysize)/2, xsize, ysize, TRUE);
		Show();
	}

// 現在ディレクトリ設定
	char	resetupDir[MAX_PATH_U8] = "";

	GetModuleFileNameU8(NULL, resetupDir, sizeof(resetupDir));
	GetParentDirU8(resetupDir, resetupDir);

	SetDlgItemTextU8(RESETUP_EDIT, resetupDir);

	if (runasWnd) {
		::SendMessage(runasWnd, IPMSG_QUIT_MESSAGE, 0, 0);
		CheckDlgButton(DELPUBKEY_CHECK, ::IsDlgButtonChecked(runasWnd, DELPUBKEY_CHECK));
		PostMessage(WM_COMMAND, IDOK, 0);
	}

	if (isSilent) {
		PostMessage(WM_COMMAND, IDOK, 0);
	}

	return	TRUE;
}

/*
	メインダイアログ用 WM_COMMAND 処理ルーチン
*/
BOOL TUninstDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		UnInstall();
		return	TRUE;

	case IDCANCEL:
		TApp::Exit(wID);
		return	TRUE;

	case RUNAS_BUTTON:
		RunAsAdmin(hWnd);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TUninstDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == IPMSG_QUIT_MESSAGE) {
		TApp::Exit(0);
		return	TRUE;
	}
	return	FALSE;
}

BOOL DeleteKeySet(const char *csp_name, const char *cont_name, DWORD flag)
{
	HCRYPTPROV	hCsp = NULL;

	if (!::CryptAcquireContext(&hCsp, cont_name, csp_name, PROV_RSA_FULL, flag|CRYPT_DELETEKEYSET)) {
		if (::CryptAcquireContext(&hCsp, cont_name, csp_name, PROV_RSA_FULL, flag)) {
			::CryptReleaseContext(hCsp, 0);
			return	FALSE;
		}
	}
	return	TRUE;
}

BOOL IsRotateFile(const char *org, const char *target, int max_rotate)
{
	char	path[MAX_PATH_U8];
	char	*p = path + strcpyz(path, org);

	for (int i=1; i < max_rotate; i++) {
		sprintf(p, ".%d", i);
		if (strcmpi(path, target) == 0) {
			return	TRUE;
		}
	}
	return	FALSE;
}

// ユーザファイルが無いことの確認
BOOL ReadyToRmFolder(const char *dir)
{
	char	dir_aster[MAX_PATH_U8];
	WIN32_FIND_DATA_U8	fdat;

	MakePathU8(dir_aster, dir, "*");

	HANDLE	fh = FindFirstFileU8(dir_aster, &fdat);
	if (fh == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}

	BOOL ret = TRUE;

	do {
		if (strcmp(fdat.cFileName, ".") && strcmp(fdat.cFileName, "..")) {
			BOOL	found = FALSE;
			for (int i=0; UnSetupFiles[i]; i++) {
				if (strcmpi(UnSetupFiles[i], fdat.cFileName) == 0 ||
					IsRotateFile(UnSetupFiles[i], fdat.cFileName, 10)) {
					found = TRUE;
					break;
				}
			}
			if (!found) {
				ret = FALSE;
			}
		}
	} while (ret && FindNextFileU8(fh, &fdat));

	::FindClose(fh);

	return	ret;
}

void TUninstDlg::DeleteShortcut(void)
{
	TRegistry	reg(HKEY_CURRENT_USER);
	char		dir[MAX_PATH_U8];
	char		path[MAX_PATH_U8];

	if (!reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		return;
	}
	if (reg.GetStr(REGSTR_STARTUP, dir, sizeof(dir))) {
		RemoveSameLink(dir, IPMSG_EXENAME);
		MakePathU8(path, dir, IPMSG_SHORTCUT_NAME);
		DeleteLink(path);
	}
	if (reg.GetStr(REGSTR_DESKTOP, dir, sizeof(dir))) {
		MakePathU8(path, dir, IPMSG_SHORTCUT_NAME);
		DeleteLink(path);
	}
	if (reg.GetStr(REGSTR_PROGRAMS, dir, sizeof(dir))) {
		MakePathU8(path, dir, IPMSG_FULLNAME);
		DeleteLinkFolder(path);
	}
}

//------------------------------------------------------
// アプリケーション情報削除用
//------------------------------------------------------

void TUninstDlg::RemoveAppRegs(void)
{
// レジストリからアプリケーション情報を削除
	TRegistry	reg(HKEY_CURRENT_USER);

	for (int i=0; i < 2; i++) {
		if (reg.OpenKey(REGSTR_PATH_APPPATHS)) {
			reg.DeleteKey(IPMSG_EXENAME);
			reg.CloseKey();
		}

	// レジストリからアンインストール情報を削除
		if (reg.OpenKey(REGSTR_PATH_UNINSTALL)) {
			reg.DeleteKey(IPMSG_NAME);
			reg.CloseKey();
		}
		reg.ChangeTopKey(HKEY_LOCAL_MACHINE);
	}
}


BOOL TUninstDlg::DeletePubkey(void)
{
	BOOL	ret = TRUE;
	char	contName[MAX_PATH_U8];
	char	userName[MAX_PATH_U8];

	DWORD	size = sizeof(userName);
	::GetUserName(userName, &size);

	sprintf(contName, "ipmsg.rsa2048.%s", userName);
	if (!DeleteKeySet(MS_ENHANCED_PROV, contName, CRYPT_MACHINE_KEYSET) ||
		!DeleteKeySet(MS_ENHANCED_PROV, contName, 0)) {
		ret = FALSE;
	}

	sprintf(contName, "ipmsg.rsa1024.%s", userName);
	if (!DeleteKeySet(MS_ENHANCED_PROV, contName, CRYPT_MACHINE_KEYSET) ||
		!DeleteKeySet(MS_ENHANCED_PROV, contName, 0)) {
		ret = FALSE;
	}

	sprintf(contName, "ipmsg.rsa512.%s", userName);
	if (!DeleteKeySet(MS_DEF_PROV, contName, CRYPT_MACHINE_KEYSET) ||
		!DeleteKeySet(MS_DEF_PROV, contName, 0)) {
		ret = FALSE;
	}

	return	ret;
}

BOOL TUninstDlg::UnInstall(void)
{
	char	setupDir[MAX_PATH_U8];		// セットアップディレクトリ情報を保存
	GetDlgItemTextU8(RESETUP_EDIT, setupDir, sizeof(setupDir));

	if ((IsAppRegistered() || TIsVirtualizedDirW(U8toWs(setupDir)))
		&& IsWinVista() && !::IsUserAnAdmin() && TIsEnableUAC()) {
		if (isSilent) {
			DebugU8("Require admin privilege for deleting AppInfo in Control Panel\n");
			TApp::Exit(-1);
			return	FALSE;
		}
		else if (MessageBox(LoadStr(IDS_REQUIREADMIN), "",
					MB_OKCANCEL|MB_ICONINFORMATION) == IDOK) {
			return	RunAsAdmin(hWnd);
		}
	}

// 現在、起動中の ipmsg を終了
	int		st = TerminateIPMsg(hWnd, LoadStr(IDS_TERMINATE), UNINSTALL_STR, isSilent);

	if (st == 1) return FALSE;
	if (st == 2) {
		if (isSilent) {
			Debug("Can't terminate ipmsg\n");
			TApp::Exit(-1);
			return FALSE;
		}
		if (!IsWinVista() || ::IsUserAnAdmin() || !TIsEnableUAC()) {
			MessageBox(LoadStr(IDS_CANTTERMINATE), UNINSTALL_STR);
			return FALSE;
		}
		if (MessageBox(LoadStr(IDS_REQUIREADMIN_TERM), "",
						MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) return FALSE;
		return	RunAsAdmin(hWnd);
	}

	if (!runasWnd && !isSilent &&
		MessageBox(LoadStr(IDS_START), UNINSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
		return	FALSE;
	}

// 公開鍵削除
	if (!isSilent && IsDlgButtonChecked(DELPUBKEY_CHECK)) {
		if (!DeletePubkey()) {
			if (IsWinVista() && !::IsUserAnAdmin() && TIsEnableUAC()) {
				if (MessageBox(LoadStr(IDS_REQUIREADMIN_PUBKEY), "",
					MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) return FALSE;
				return	RunAsAdmin(hWnd);
			}
		}
	}

// スタートアップ＆デスクトップから削除
	DeleteShortcut();

// インストール情報削除
	char	logDir[MAX_PATH_U8] = {};
	RemoveAppRegs();

// レジストリからユーザー設定情報を削除
	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.ChangeApp(HS_TOOLS)) {
		const char *ipmsg_reg = []() { return
			LoadStr(IDS_REGIPMSG);
		}();

		if (reg.OpenKey(ipmsg_reg)) {
			char	path[MAX_PATH_U8];
			if (reg.GetStr("LogFile", path, sizeof(path))) {
				GetParentDirU8(path, logDir);
				DWORD	attr = GetFileAttributesU8(logDir);
				if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
					*logDir = 0;
				}
			}
			reg.CloseKey();
		}
		reg.DeleteChildTree(ipmsg_reg);
	}

	for (int i=0; UnSetupFiles[i]; i++) {
		char	path[MAX_PATH_U8];

		MakePathU8(path, setupDir, UnSetupFiles[i]);
		DeleteFileU8(path);
	}

// 終了メッセージ
	BOOL	can_rm = ReadyToRmFolder(setupDir);
	char	*msg = LoadStrU8(can_rm ? IDS_UNINSTCOMPLETE : IDS_UNINSTCOMPLETEEX);
	if (!isSilent) {
		MessageBoxU8(msg);
	}

	char	path[MAX_PATH_U8];

	GetParentDirU8(setupDir, path);
	SetCurrentDirectoryU8(path);

	if (ReadyToRmFolder(setupDir)) {
		WCHAR cmd[MAX_PATH];
		WCHAR dir[MAX_PATH];

		U8toW(setupDir, dir, MAX_PATH);
		MakePathW(cmd, dir, UNINST_BAT_W);

		if (FILE *fp = _wfopen(cmd, L"w")) {
			for (int i=0; i < 10; i++) {
				fwprintf(fp, L"timeout 1 /NOBREAK\n");
				fwprintf(fp, L"rd /s /q \"%s\"\n", dir);
			}
			fclose(fp);

			STARTUPINFOW		sui = { sizeof(sui) };
			PROCESS_INFORMATION	pi = {};

			WCHAR opt[MAX_PATH];
			swprintf(opt, L"cmd.exe /c \"%s\"", cmd);
			if (::CreateProcessW(NULL, opt, 0, 0, 0, CREATE_NO_WINDOW, 0, 0, &sui, &pi)) {
				::CloseHandle(pi.hThread);
				::CloseHandle(pi.hProcess);
			}
		}
	}
	else {
		if (isSilent) {
			OutW(L"%s can't remove\n", U8toWs(setupDir));
		}
		else {
			ShellExecuteU8(NULL, NULL, setupDir, 0, 0, SW_SHOW);
		}
	}

	if (*logDir && !isSilent) {
		ShellExecuteU8(NULL, NULL, logDir, 0, 0, SW_SHOW);
	}

	TApp::Exit(0);
	return	TRUE;
}

