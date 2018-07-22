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

#include "../instdata/instcmn.h"
#include "../instdata/instcore.h"
#include "../ipmsgdef.h"

#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "winmm.lib")

int ExecInTempDir();

#define TEMPDIR_OPT	L"/TEMPDIR"
#define RUNAS_OPT	L"/runas="
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
	isInternal = FALSE;
	stat = INST_INIT;
	*setupDir = 0;

	fwCheckMode = 0;
	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.ChangeApp(HSTOOLS_STR, IP_MSG)) {
		reg.GetInt(FWCHECKMODE_STR, &fwCheckMode);
	}
}

TInstDlg::~TInstDlg()
{
}

BOOL TInstDlg::ParseCmdLine()
{
#define USAGE_STR	\
		"IPMsg_installer usage\r\n"					\
		"  /SILENT ... silent install mode\r\n"		\
		"  /INSTDIR=\"path\" ... default path\r\n"		\
		"  /HELP ... show help(this message)"

	U8str	cmdlineU8(::GetCommandLineW());
	char	*cmdu8 = cmdlineU8.Buf();

	if (strstr(cmdu8, "/HELP")) {
		goto ERR;
	}

	if (strstr(cmdu8, "/SILENT")) {
		isSilent = TRUE;
	}

	if (strstr(cmdu8, "/INTERNAL")) {
		isInternal = TRUE;
		isSilent = TRUE;
	}

	if (char *p = strstr(cmdu8, "/INSTDIR=")) {
		char	*top = p + 9;
		size_t	len = strlen(top);

		if (*top == '"') {
			top++;
			if (char *end = strchr(top, '"')) {
				len = end - top;
			} else {
				goto ERR;
			}
		}
		else if (char *end = strchr(top, ' ')) {
			len = end - top;
		}
		if (len + 1 >= MAX_PATH_U8) {
			goto ERR;
		}
		memcpy(setupDir, top, len);
		setupDir[len] = 0;
		Debug("exp path=<%s> len=%d\n", setupDir, len);
	}

	return	TRUE;

ERR:
	MessageBox(USAGE_STR);
	return	FALSE;
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TInstDlg::EvCreate(LPARAM lParam)
{
	if (!ParseCmdLine()) {
		TApp::Exit(0);
		return	FALSE;
	}

	HICON hBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
		32, 32, LR_DEFAULTCOLOR);
	HICON hSmallIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON, IMAGE_ICON,
		16, 16, LR_DEFAULTCOLOR);

	SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hBigIcon);
	SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

	if (IsWinVista() && !::IsUserAnAdmin() && TIsEnableUAC()) {
		runasBtn.AttachWnd(GetDlgItem(RUNAS_BUTTON));
		runasBtn.CreateTipWnd(LoadStrW(IDS_LABLE_RUNAS));
		runasBtn.ShowWindow(SW_SHOW);
		runasBtn.SendMessage(BCM_SETSHIELD, 0, 1);
	}

	SetStat(INST_INIT);

	char	title[256];
	char	title2[256];

	GetWindowText(title, sizeof(title));
	sprintf(title2, "%s ver%s", title, GetVersionStr());
	SetWindowText(title2);

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

	InitDir();

	SetDlgItemTextU8(FILE_EDIT, setupDir);
	CheckDlgButton(STARTUP_CHECK, 1);
	CheckDlgButton(DESKTOP_CHECK, 1);
	CheckDlgButton(EXTRACT_CHECK, 0);
	if (fwCheckMode) {
		CheckDlgButton(NOFW_CHK, 1);
	}

	char	*p = strstr(GetCommandLine(), "runas=");
	if (p) {
		runasWnd = (HWND)strtoull(p+6, 0, 16);
		if (!runasWnd || !IsWindow(runasWnd)) {
			TApp::Exit(0);
			return	FALSE;
		}
		if ((p = strstr(p, ",imm="))) runasImm = atoi(p+5);

		CheckDlgButton(EXTRACT_CHECK, ::IsDlgButtonChecked(runasWnd, EXTRACT_CHECK));
		CheckDlgButton(STARTUP_CHECK, ::IsDlgButtonChecked(runasWnd, STARTUP_CHECK));
		CheckDlgButton(DESKTOP_CHECK, ::IsDlgButtonChecked(runasWnd, DESKTOP_CHECK));
		CheckDlgButton(NOFW_CHK,      ::IsDlgButtonChecked(runasWnd, NOFW_CHK));

		WCHAR	wbuf[MAX_PATH] = L"";
		::SendDlgItemMessageW(runasWnd, FILE_EDIT, WM_GETTEXT, MAX_PATH, (LPARAM)wbuf);
		SetDlgItemTextU8(FILE_EDIT, WtoU8s(wbuf));

		::SendMessage(runasWnd, WM_IPMSG_HIDE, 0, 0);
		if (runasImm) {
			PostMessage(WM_IPMSG_INSTALL, 0, 0);
		}
	}

	msgStatic.AttachWnd(GetDlgItem(INST_STATIC));

	progBar.AttachWnd(GetDlgItem(INST_PROG));
	progBar.SendMessage(PBM_SETRANGE, 0, MAKELPARAM(0, 9));
	progBar.SendMessage(PBM_SETSTEP, 1, 0);
	progBar.EnableWindow(FALSE);

	if (isSilent) {
		PostMessage(WM_IPMSG_INSTALL, 0, 0);
	}

	return	TRUE;
}

BOOL TInstDlg::InitDir()
{
// 現在ディレクトリ設定
	char	buf[MAX_PATH_U8] = "";
	char	dir[MAX_PATH_U8] = "";

	if (isInternal) {
		GetModuleFileNameU8(NULL, buf, sizeof(buf));
		GetParentDirU8(buf, setupDir);
		return	TRUE;
	}

	SHGetSpecialFolderPathU8(NULL, buf, CSIDL_LOCAL_APPDATA);
	MakePathU8(dir, buf, IP_MSG);

// 既にセットアップされている場合は、セットアップディレクトリを読み出す
	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.ChangeApp(HSTOOLS_STR, IP_MSG)) {
		if (reg.GetStr(REGSTR_PATH, buf, sizeof(buf)) && *buf) {
			strcpy(dir, buf);
		}
		else {
			isFirst = TRUE;
		}
		reg.CloseKey();
	}

	if (!*setupDir) {
		strcpy(setupDir, dir);
	}

	return	TRUE;
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
		switch (stat) {
		case INST_INIT:
		case INST_RETRY:
			CheckDlgButton(EXTRACT_CHECK, 0);
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
		CheckDlgButton(EXTRACT_CHECK, 1);
		Install();
		return	TRUE;

	case FILE_BUTTON:
		BrowseDirDlg(this, FILE_EDIT, "Select Install Directory");
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
		Install();
		if (isSilent) {
			TApp::Exit(0);
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
	this->Sleep(200);
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
	if (!isInternal && reg.GetStr(REGSTR_STARTUP, path, sizeof(path))) {
		RemoveSameLink(path, IPMSG_EXENAME);
		MakePathU8(buf, path, IPMSG_SHORTCUT_NAME);
		if (IsDlgButtonChecked(STARTUP_CHECK) || isSilent) {
			ShellLink(setupPath, buf);
		}
		else {
			DeleteLink(buf);
		}
	}
	if (!isInternal && reg.GetStr(REGSTR_DESKTOP, path, sizeof(path))) {
		MakePathU8(buf, path, IPMSG_SHORTCUT_NAME);
		if (IsDlgButtonChecked(DESKTOP_CHECK) || isSilent) {
			ShellLink(setupPath, buf);
		}
		else {
			DeleteLink(buf);
		}
	}
	if (/*!isInternal &&*/ reg.GetStr(REGSTR_PROGRAMS, path, sizeof(path))) {
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

BOOL TInstDlg::SetStat(Stat _stat)
{
	stat = _stat;

	int	chk_sw = (stat == INST_INIT || stat == INST_RETRY) ? SW_SHOW : SW_HIDE;
	int	msg_sw = (stat == INST_INIT || stat == INST_RETRY) ? SW_HIDE : SW_SHOW;

	::ShowWindow(GetDlgItem(STARTUP_CHECK), chk_sw);
	::ShowWindow(GetDlgItem(DESKTOP_CHECK), chk_sw);
//	::ShowWindow(GetDlgItem(EXTRACT_CHECK), chk_sw);
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

BOOL TInstDlg::Install(void)
{
	BOOL	extract_only = IsDlgButtonChecked(EXTRACT_CHECK);

// 現在、起動中の ipmsg を終了
	int		st = extract_only ? 0 :
		TerminateIPMsg(hWnd, LoadStr(IDS_TERMINATE), INSTALL_STR, isSilent);
	if (st == 1) {	// 手動中断
		return	FALSE;
	}

// インストールパス設定
	GetDlgItemTextU8(FILE_EDIT, setupDir, sizeof(setupDir));
	char	setupPath[MAX_PATH_U8];
	MakePathU8(setupPath, setupDir, IPMSG_EXENAME);

	BOOL	third_fw = FALSE;

	if (IsWinVista()) {
		BOOL	is_virtual = TIsVirtualizedDirW(U8toWs(setupDir));
		BOOL	need_admin = TIsEnableUAC() && !::IsUserAnAdmin();

		if (is_virtual && need_admin) {
			if (isSilent || MessageBox(LoadStr(IDS_REQUIREADMIN), INSTALL_STR,
					MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
				return	FALSE;
			}
			return	RunAsAdmin(hWnd, TRUE);
		}

		TRegistry	reg(HKEY_CURRENT_USER);
		reg.ChangeApp(HSTOOLS_STR, IP_MSG);
		reg.SetInt(FWCHECKMODE_STR, 1);

		if (!IsDlgButtonChecked(NOFW_CHK)) {
			BOOL		is_domain  = IsDomainEnviron();
			BOOL		is_tout = FALSE;
			FwStatus	fs;

			third_fw = Is3rdPartyFwEnabled(TRUE, 5000, &is_tout);

			GetFwStatusEx(U8toWs(setupPath), &fs);
			// Is3rdParty../GetFwStat..で固まると次回は1に
			reg.SetInt(FWCHECKMODE_STR, is_tout);

			if (need_admin && !third_fw && fs.fwEnable && !isSilent &&	 // domain環境は拒否エントリが
				(fs.IsBlocked() || !(is_domain || fs.IsAllowed()))) { // すでに存在する場合のみ
				if (MessageBox(LoadStr(IDS_REQUIREADMIN_FW), INSTALL_STR,
						MB_OKCANCEL|MB_ICONINFORMATION) == IDOK) {
					return	RunAsAdmin(hWnd, TRUE);
				}
				if (MessageBox(LoadStr(IDS_NOADMIN_CONTINUE), INSTALL_STR,
						MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
					return	FALSE;
				}
			}
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

// ファイル生成
	if (!ExtractAll(setupDir, isSilent, isSilent ? NULL : this)) {
		const char *msg = "";
		const char *err_msg = NULL;
		ExtractErr err = GetExtractErr(&msg);

		switch (err) {
		case CREATE_DIR_ERR:
			if (!isSilent) {
				msg = LoadStrU8(IDS_NOTCREATEDIR);
			}
			break;

		case CREATE_FILE_ERR:
			if (!isSilent) {
				msg = Fmt("%s\r\n%s", LoadStrU8(IDS_NOTCREATEFILE), err_msg);
			}
			break;

		case DATA_BROKEN:
			if (!isSilent) {
				msg = LoadStrU8(IDS_BROKENARCHIVE);
			}
			break;

		case USER_CANCEL:
			if (!isSilent) {
				msg = LoadStrU8(IDS_USERCANCEL);
			}
			break;
		}
		if (isInternal) {	// 自動アップデート失敗の場合は、起動は行う
			AppKick(INTERNAL_ERR);
		}
		else {
			//msgStatic.SetWindowTextU8(msg);
			MessageBoxU8(msg, INSTALL_STR);
		}
		SetStat(INST_RETRY);
		return	FALSE;
	}
	SetStat(INST_END);

// 展開のみ
	if (extract_only) {
		ShellExecuteW(NULL, NULL, U8toWs(setupDir), 0, 0, SW_SHOW);
		return TRUE;
	}

	// Firewall 登録
	if (IsUserAnAdmin()) {
		SetFwStatusEx(U8toWs(setupPath), IP_MSG_W);
	}
	if (!isInternal) {
	// スタートアップ＆デスクトップにショートカットを登録
		CreateShortCut();

	// パス情報を登録
		TRegistry	reg(HKEY_CURRENT_USER);
		if (reg.ChangeApp(HSTOOLS_STR, IP_MSG)) {
			reg.SetStr(REGSTR_PATH, setupDir);
			reg.CloseKey();
		}
	}

	// レジストリにアプリケーション情報を登録
	TRegistry	reg(HKEY_CURRENT_USER);
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


// コピーしたアプリケーションを起動
	const char *msg = LoadStr(IDS_SETUPCOMPLETE);
	int			flg = MB_OKCANCEL|MB_ICONINFORMATION;

	if (IsWinVista() && third_fw) {
		msg = Fmt("%s\r\n\r\n%s", msg, LoadStr(IDS_COMPLETE_3RDFWADD));
//		flg |= MB_DEFBUTTON2;
	}
	TLaunchDlg	dlg(msg, isFirst, this);
	if (isSilent /*|| dlg.Exec() == IDOK*/ || TRUE) {
		Mode	mode = isInternal ? INTERNAL : SIMPLE;

		if (!isSilent) {
			if (isFirst) {
				mode = FIRST;
			} else if (dlg.needHist) {
				mode = HISTORY;
			} else {
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

	msgStatic.SetWindowTextU8(LoadStrU8(IDS_INST_SUCCESS));

	::SetWindowText(GetDlgItem(IDOK), LoadStr(IDS_LABEL_COMPLETE));

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
void BrowseDirDlg(TWin *parentWin, UINT editCtl, char *title)
{ 
	IMalloc			*iMalloc = NULL;
	BROWSEINFOW		brInfo;
	LPITEMIDLIST	pidlBrowse;
	char			buf[MAX_PATH_U8];
	WCHAR			wbuf[MAX_PATH];
	Wstr			wtitle(title);

	parentWin->GetDlgItemTextU8(editCtl, buf, sizeof(buf));
	U8toW(buf, wbuf, wsizeof(wbuf));
	if (!SUCCEEDED(SHGetMalloc(&iMalloc)))
		return;

	TBrowseDirDlg	dirDlg(buf, sizeof(buf));
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = wbuf;
	brInfo.lpszTitle = wtitle.Buf();
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS|BIF_SHAREABLE|BIF_BROWSEINCLUDEFILES|BIF_USENEWUI;
	brInfo.lpfn = BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)&dirDlg;
	brInfo.iImage = 0;

	do {
		if ((pidlBrowse = ::SHBrowseForFolderW(&brInfo))) {
			if (::SHGetPathFromIDListW(pidlBrowse, wbuf))
				::SetDlgItemTextW(parentWin->hWnd, editCtl, wbuf);
			iMalloc->Free(pidlBrowse);
			break;
		}
	} while (dirDlg.IsDirty());

	iMalloc->Release();
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
#if 0
	RECT	tmp_rect;
	::GetWindowRect(GetDlgItem(IDOK), &tmp_rect);
	POINT	pt = { tmp_rect.left, tmp_rect.top };
	::ScreenToClient(hWnd, &pt);
	int		cx = (pt.x - 30) / 2, cy = tmp_rect.bottom - tmp_rect.top;

	::CreateWindowU8(BUTTON_CLASS, LoadStrU8(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
		10, pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::hInst(), NULL);
	::CreateWindowU8(BUTTON_CLASS, LoadStrU8(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
		18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::hInst(), NULL);

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont) {
		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (WPARAM)hDlgFont, 0L);
		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (WPARAM)hDlgFont, 0L);
	}
#endif

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

