static char *install_id = 
	"@(#)Copyright (C) H.Shirouzu 1998-2011   install.cpp	Ver3.21";
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Installer Application Class
	Create					: 1998-06-14(Sun)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include "resource.h"
#include "install.h"
#include "../version.h"
#include "../../external/zlib/zlib.h"

char	*SetupFiles [] = { IPMSG_EXENAME, SETUP_EXENAME, IPMSGHELP_NAME, NULL };

/*
	WinMain
*/
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmdLine, int nCmdShow)
{
	if (!IsWin2K()) {
		MessageBox(0, "Please use old version (v2.06 or earlier)", "Win95/98/Me/NT4.0 is not support", MB_OK);
		::ExitProcess(0xffffffff);
		return	0;
	}

	TInstApp	app(hI, cmdLine, nCmdShow);

	return	app.Run();
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
TInstDlg::TInstDlg(char *cmdLine) : TDlg(INSTALL_DIALOG)
{
	runasImm = FALSE;
	runasWnd = NULL;
}

TInstDlg::~TInstDlg()
{
}

/*
	メインダイアログ用 WM_INITDIALOG 処理ルーチン
*/
BOOL TInstDlg::EvCreate(LPARAM lParam)
{
	char	title[256], title2[256];
	GetWindowText(title, sizeof(title));
	::wsprintf(title2, "%s ver%s", title, GetVersionStr());
	SetWindowText(title2);

	GetWindowRect(&rect);
	int		cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int		xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;

	::SetClassLong(hWnd, GCL_HICON, (LONG)::LoadIcon(TApp::GetInstance(), (LPCSTR)SETUP_ICON));
	MoveWindow((cx - xsize)/2, (cy - ysize)/2, xsize, ysize, TRUE);
	Show();

	if (IsWinVista() && !TIsUserAnAdmin() && TIsEnableUAC()) {
		HWND	hRunas = GetDlgItem(RUNAS_BUTTON);
		::SetWindowLong(hRunas, GWL_STYLE, ::GetWindowLong(hRunas, GWL_STYLE)|WS_VISIBLE);
		::SendMessage(hRunas, BCM_SETSHIELD, 0, 1);
	}

// 現在ディレクトリ設定
	char	buf[MAX_PATH_U8], setupDir[MAX_PATH_U8];
#ifdef _WIN64
	char	x86dir[MAX_PATH_U8] = "";
#endif

// Program Filesのパス取り出し
	TRegistry	reg(HKEY_LOCAL_MACHINE);
	if (reg.OpenKey(REGSTR_PATH_SETUP)) {
		if (reg.GetStr(REGSTR_PROGRAMFILES, buf, sizeof(buf))) {
			MakePath(setupDir, buf, IPMSG_STR);
		}
#ifdef _WIN64
		if (reg.GetStr(REGSTR_PROGRAMFILESX86, buf, sizeof(buf)))
			MakePath(x86dir, buf, IPMSG_STR);
#endif
		reg.CloseKey();
	}

// 既にセットアップされている場合は、セットアップディレクトリを読み出す
	if (reg.OpenKey(REGSTR_PATH_APPPATHS)) {
		if (reg.OpenKey(IPMSG_EXENAME)) {
			if (reg.GetStr(REGSTR_PATH, buf, sizeof(buf))) {
#ifdef _WIN64
				if (strcmp(buf, x86dir))
#endif
				strcpy(setupDir, buf);
			}
			reg.CloseKey();
		}
		reg.CloseKey();
	}

	SetDlgItemTextU8(FILE_EDIT, setupDir);
	CheckDlgButton(STARTUP_CHECK, 1);
	CheckDlgButton(PROGRAM_CHECK, 1);
	CheckDlgButton(DESKTOP_CHECK, 1);
	CheckDlgButton(EXTRACT_CHECK, 0);

	char	*p = strstr(GetCommandLine(), "runas=");
	if (p) {
		runasWnd = (HWND)strtoul(p+6, 0, 16);
		if (!runasWnd || !IsWindow(runasWnd)) PostQuitMessage(0);
		if ((p = strstr(p, ",imm="))) runasImm = atoi(p+5);

		CheckDlgButton(EXTRACT_CHECK, ::IsDlgButtonChecked(runasWnd, EXTRACT_CHECK));
		CheckDlgButton(STARTUP_CHECK, ::IsDlgButtonChecked(runasWnd, STARTUP_CHECK));
		CheckDlgButton(PROGRAM_CHECK, ::IsDlgButtonChecked(runasWnd, PROGRAM_CHECK));
		CheckDlgButton(DESKTOP_CHECK, ::IsDlgButtonChecked(runasWnd, DESKTOP_CHECK));

		WCHAR	wbuf[MAX_PATH] = L"";
		::SendDlgItemMessageW(runasWnd, FILE_EDIT, WM_GETTEXT, MAX_PATH, (LPARAM)wbuf);
		SetDlgItemTextU8(FILE_EDIT, WtoU8(wbuf));

		::SendMessage(runasWnd, WM_IPMSG_HIDE, 0, 0);
		if (runasImm) {
			PostMessage(WM_IPMSG_INSTALL, 0, 0);
		}
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

BOOL RunAsAdmin(HWND hWnd, BOOL imm)
{
	WCHAR	path[MAX_PATH], buf[MAX_BUF];

	::GetModuleFileNameW(::GetModuleHandle(NULL), path, sizeof(path));
	swprintf(buf, L"/runas=%x,imm=%d", hWnd, imm);
	ShellExecuteW(hWnd, L"runas", path, buf, NULL, SW_SHOW);

	return TRUE;
}

/*
	メインダイアログ用 WM_COMMAND 処理ルーチン
*/
BOOL TInstDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		CheckDlgButton(EXTRACT_CHECK, 0);
		Install();
		return	TRUE;

	case IDCANCEL:
		::PostQuitMessage(0);
		return	TRUE;

	case RUNAS_BUTTON:
		RunAsAdmin(hWnd, FALSE);
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

BOOL TInstDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_IPMSG_HIDE:
		Show(SW_HIDE);
		return	TRUE;

	case WM_IPMSG_QUIT:
		if (wParam == 1) {
			AppKick();
		}
		PostQuitMessage(0);
		return	TRUE;

	case WM_IPMSG_INSTALL:
		Install();
		return	TRUE;
	}
	return	FALSE;
}

BOOL TInstDlg::AppKick()
{
	char	setupDir[MAX_PATH_U8], setupPath[MAX_PATH_U8];

	GetDlgItemTextU8(FILE_EDIT, setupDir, sizeof(setupDir));
	SetCurrentDirectoryU8(setupDir);

	MakePath(setupPath, setupDir, IPMSG_EXENAME);
	return	(int)ShellExecuteU8(NULL, NULL, setupPath, "/SHOW_HISTORY", 0, SW_SHOW) > 32;
}

BOOL TInstDlg::Install(void)
{
	char	buf[MAX_PATH_U8], setupDir[MAX_PATH_U8], setupPath[MAX_PATH_U8];
	char	installPath[MAX_PATH_U8];
	BOOL	extract_only = IsDlgButtonChecked(EXTRACT_CHECK);

// 現在、起動中の ipmsg を終了
	int		st = extract_only ? 0 : TerminateIPMsg();
	if (st == 1) return	FALSE;

// インストールパス設定
	GetDlgItemTextU8(FILE_EDIT, setupDir, sizeof(setupDir));

	if (IsWinVista() && !TIsUserAnAdmin() && TIsEnableUAC()
			&& TIsVirtualizedDirV(U8toW(setupDir))) {
		if (MessageBox(GetLoadStr(IDS_REQUIREADMIN), INSTALL_STR,
				MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) return	FALSE;
		return	RunAsAdmin(hWnd, TRUE);
	}

	CreateDirectoryU8(setupDir, NULL);
	DWORD	attr = GetFileAttributesU8(setupDir);
	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return	MessageBox(GetLoadStr(IDS_NOTCREATEDIR), INSTALL_STR), FALSE;
	MakePath(setupPath, setupDir, IPMSG_EXENAME);

	if (st == 2) {
		MessageBox(GetLoadStr(IDS_CANTTERMINATE), INSTALL_STR);
		return	FALSE;
	}
	if (!runasImm &&
		MessageBox(GetLoadStr(IDS_START), INSTALL_STR, MB_OKCANCEL|MB_ICONINFORMATION) != IDOK) {
		return	FALSE;
	}
	runasImm = FALSE;

// ファイル生成
	for (int i=0; SetupFiles[i]; i++) {
		MakePath(installPath, setupDir, SetupFiles[i]);
		CreateStatus cs = CreateFileBySelf(installPath, SetupFiles[i]);
		if (cs == CS_BROKEN) {
			MessageBox(GetLoadStr(IDS_BROKENARCHIVE), INSTALL_STR);
			return	FALSE;
		}
		else if (cs == CS_ACCESS) {
			const char *msg = FmtStr("%s\r\n%s", GetLoadStrU8(IDS_NOTCREATEFILE), installPath);
			MessageBoxU8(msg, INSTALL_STR);
			return	FALSE;
		}
	}

// 展開のみ
	if (extract_only) {
		ShellExecuteU8(NULL, NULL, setupDir, 0, 0, SW_SHOW);
		return TRUE;
	}

// スタートアップ＆デスクトップに登録
	TRegistry	reg(HKEY_CURRENT_USER);
	if (reg.OpenKey(REGSTR_SHELLFOLDERS)) {
		char	*regStr[] = { REGSTR_STARTUP, REGSTR_PROGRAMS, REGSTR_DESKTOP, NULL };
		BOOL	resId[]   = { STARTUP_CHECK,  PROGRAM_CHECK,   DESKTOP_CHECK,  NULL };

		for (int i=0; regStr[i]; i++) {
			if (reg.GetStr(regStr[i], buf, sizeof(buf))) {
				if (i != 0 || !RemoveSameLink(buf, buf))
					::wsprintf(buf + strlen(buf), "\\%s", IPMSG_SHORTCUT_NAME);
				if (IsDlgButtonChecked(resId[i]))
					SymLink(setupPath, buf);
				else
					DeleteLink(buf);
			}
		}
		reg.CloseKey();
	}

// レジストリにアプリケーション情報を登録
	reg.ChangeTopKey(HKEY_LOCAL_MACHINE);
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
			MakePath(buf, setupDir, SETUP_EXENAME);
			strcat(buf, " /r");
			reg.SetStr(REGSTR_VAL_UNINSTALLER_DISPLAYNAME, IPMSG_FULLNAME);
			reg.SetStr(REGSTR_VAL_UNINSTALLER_COMMANDLINE, buf);
			reg.CloseKey();
		}
		reg.CloseKey();
	}

// コピーしたアプリケーションを起動
	const char *msg = GetLoadStr(IDS_SETUPCOMPLETE);
	int			flg = MB_OKCANCEL|MB_ICONINFORMATION;

//	if (IsWinVista() && TIsUserAnAdmin() && TIsEnableUAC()) {
//		msg = FmtStr("%s%s", msg, GetLoadStr(IDS_COMPLETE_UACADD));
//		flg |= MB_DEFBUTTON2;
//	}
	if (IsWin7()) {
		msg = FmtStr("%s%s", msg, GetLoadStr(IDS_COMPLETE_WIN7));
	}
	if (MessageBox(msg, INSTALL_STR, flg) == IDOK) {
		if (runasWnd) {
			Wstr	wbuf(setupDir);
			if (::SendDlgItemMessageW(runasWnd, FILE_EDIT, WM_SETTEXT, 0, (LPARAM)wbuf.Buf())) {
				::PostMessage(runasWnd, WM_IPMSG_QUIT, 1, 0);
				runasWnd = NULL;
			}
		}
		else {
			AppKick();
		}
	}
	else {
		HWND	hHelp = ShowHelpU8(0, setupDir, GetLoadStrU8(IDS_IPMSGHELP), "#history");
		if (hHelp) {
			Show(SW_HIDE);
			while (::IsWindow(hHelp)) {
				this->Sleep(100);
			}
		}
	}

	if (runasWnd) {
		::PostMessage(runasWnd, WM_IPMSG_QUIT, 0, 0);
		runasWnd = NULL;
	}

//	ShellExecuteU8(NULL, NULL, setupDir, 0, 0, SW_SHOW);
	::PostQuitMessage(0);
	return	TRUE;
}

/*
	同じ内容を持つショートカットを削除（スタートアップへの重複登録よけ）
*/
BOOL RemoveSameLink(const char *dir, char *remove_path)
{
	char				path[MAX_PATH_U8], dest[MAX_PATH_U8], arg[MAX_PATH_U8];
	HANDLE				fh;
	WIN32_FIND_DATA_U8	data;
	BOOL				ret = FALSE;

	::wsprintf(path, "%s\\*.*", dir);
	if ((fh = FindFirstFileU8(path, &data)) == INVALID_HANDLE_VALUE)
		return	FALSE;

	do {
		::wsprintf(path, "%s\\%s", dir, data.cFileName);
		if (ReadLinkU8(path, dest, arg) && *arg == 0) {
			int		dest_len = strlen(dest), ipmsg_len = strlen(IPMSG_EXENAME);
			if (dest_len > ipmsg_len &&
					strncmpi(dest + dest_len - ipmsg_len, IPMSG_EXENAME, ipmsg_len) == 0) {
				ret = DeleteFileU8(path);
				if (remove_path)
					strcpy(remove_path, path);
			}
		}

	} while (FindNextFileU8(fh, &data));

	::FindClose(fh);
	return	ret;
}


/*
	立ち上がっている IPMSG を終了
*/
int TInstDlg::TerminateIPMsg()
{
	BOOL	existFlg = FALSE;

	::EnumWindows(TerminateIPMsgProc, (LPARAM)&existFlg);
	if (existFlg) {
		if (MessageBox(GetLoadStr(IDS_TERMINATE), INSTALL_STR, MB_OKCANCEL) == IDCANCEL)
			return	1;
		::EnumWindows(TerminateIPMsgProc, NULL);
	}
	existFlg = FALSE;
	::EnumWindows(TerminateIPMsgProc, (LPARAM)&existFlg);

	return	!existFlg ? 0 : 2;
}

/*
	lParam == NULL ...	全 IPMSG を終了
	lParam != NULL ...	lParam を BOOL * とみなし、IPMSG proccess が存在する
						場合は、そこにTRUE を代入する。
*/
BOOL CALLBACK TerminateIPMsgProc(HWND hWnd, LPARAM lParam)
{
	char	buf[MAX_BUF];

	if (::GetClassName(hWnd, buf, sizeof(buf)) != 0) {
		if (strncmpi(IPMSG_CLASS, buf, strlen(IPMSG_CLASS)) == 0) {
			if (lParam)
				*(BOOL *)lParam = TRUE;		// existFlg;
			else {
				::SendMessage(hWnd, WM_CLOSE, 0, 0);
				for (int i=0; i < 10; i++) {
					Sleep(300);
					if (!IsWindow(hWnd)) break;
				}
			}
		}
	}
	return	TRUE;
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
	U8toW(buf, wbuf, MAX_PATH);
	if (!SUCCEEDED(SHGetMalloc(&iMalloc)))
		return;

	TBrowseDirDlg	dirDlg(buf);
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = wbuf;
	brInfo.lpszTitle = wtitle.Buf();
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	brInfo.lpfn = BrowseDirDlg_Proc;
	brInfo.lParam = (LPARAM)&dirDlg;
	brInfo.iImage = 0;

	do {
		if ((pidlBrowse = ::SHBrowseForFolderV((BROWSEINFO *)&brInfo))) {
			if (::SHGetPathFromIDListV(pidlBrowse, wbuf))
				::SetDlgItemTextV(parentWin->hWnd, editCtl, wbuf);
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
	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
		GetParentDirU8(fileBuf, fileBuf);
	SendMessageW(BFFM_SETSELECTIONW, TRUE, (LPARAM)U8toW(fileBuf));
	SetWindowText(IPMSG_FULLNAME);

// ボタン作成
	RECT	tmp_rect;
	::GetWindowRect(GetDlgItem(IDOK), &tmp_rect);
	POINT	pt = { tmp_rect.left, tmp_rect.top };
	::ScreenToClient(hWnd, &pt);
	int		cx = (pt.x - 30) / 2, cy = tmp_rect.bottom - tmp_rect.top;

	::CreateWindowU8(BUTTON_CLASS, GetLoadStr(IDS_MKDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 10, pt.y, cx, cy, hWnd, (HMENU)MKDIR_BUTTON, TApp::GetInstance(), NULL);
	::CreateWindowU8(BUTTON_CLASS, GetLoadStr(IDS_RMDIR), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, 18 + cx, pt.y, cx, cy, hWnd, (HMENU)RMDIR_BUTTON, TApp::GetInstance(), NULL);

	HFONT	hDlgFont = (HFONT)SendDlgItemMessage(IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont) {
		SendDlgItemMessage(MKDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(RMDIR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
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
			if (!dlg.Exec())
				return	TRUE;
			MakePath(path, fileBuf, dirBuf);
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
	BOOL	ret = ::SHGetPathFromIDListV((LPITEMIDLIST)list, wbuf.Buf());
	if (ret) {
		WtoU8(wbuf, fileBuf, MAX_PATH_U8);
	}
	return	ret;
}

/*
	一行入力
*/
BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextU8(INPUT_EDIT, dirBuf, MAX_PATH_U8);
		EndDialog(TRUE);
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
		return	TRUE;
	}
	return	FALSE;
}

/*
	親ディレクトリ取得（必ずフルパスであること。UNC対応）
*/
BOOL GetParentDirU8(const char *srcfile, char *dir)
{
	char	path[MAX_BUF], *fname=NULL;

	if (GetFullPathNameU8(srcfile, sizeof(path), path, &fname) == 0 || fname == NULL)
		return	strcpy(dir, srcfile), FALSE;

	if (fname - path > 3 || path[1] != ':')
		*(fname - 1) = 0;
	else
		*fname = 0;		// C:\ の場合

	strcpy(dir, path);
	return	TRUE;
}

/*
	リンク
	あらかじめ、CoInitialize(NULL); を実行しておくこと
*/
BOOL SymLink(LPCSTR src, LPSTR dest, LPCSTR arg)
{
	IShellLink		*shellLink;
	IPersistFile	*persistFile;
	Wstr			wsrc(src), wdest(dest), warg(arg);
	BOOL			ret = FALSE;
	char			buf[MAX_PATH_U8];

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&shellLink))) {
		shellLink->SetPath((char *)wsrc.Buf());
		shellLink->SetArguments((char *)warg.Buf());
		GetParentDirU8(src, buf);
		shellLink->SetWorkingDirectory((char *)U8toW(buf));

		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Save(wdest, TRUE))) {
				ret = TRUE;
				GetParentDirU8(WtoU8(wdest), buf);
				::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH|SHCNF_FLUSH, U8toA(buf), NULL);
			}
			persistFile->Release();
		}
		shellLink->Release();
	}
	return	ret;
}

/*
	リンクファイル削除
*/
BOOL DeleteLink(LPCSTR path)
{
	char	dir[MAX_PATH_U8];

	if (!DeleteFileU8(path))
		return	FALSE;

	GetParentDirU8(path, dir);
	::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH|SHCNF_FLUSH, U8toA(dir), NULL);

	return	TRUE;
}

BOOL DeflateData(BYTE *buf, DWORD size, VBuf *vbuf)
{
	BOOL	ret = FALSE;
	z_stream z = {0};

	if (inflateInit(&z) != Z_OK) return FALSE;
	z.next_out = vbuf->Buf();
	z.avail_out = vbuf->Size();
	z.next_in = buf;
	z.avail_in = size;
	if (inflate(&z, Z_NO_FLUSH) == Z_STREAM_END) {
		vbuf->SetUsedSize(vbuf->Size() - z.avail_out);
		ret = TRUE;
	}
	inflateEnd(&z);
	return	ret;
}

BYTE *FindSeparatedData(BYTE *buf, DWORD buf_size, char *fname, DWORD *size)
{
	char	*sep = "\n======================================================================\n";
	int		sep_len = strlen(sep);
	int		sep_end = sep_len -1;
	BYTE	*search_end = buf + buf_size - 1000;
	BYTE	*p = buf;
	int		fname_len = strlen(fname);

	while (p < search_end) {
		if (p[sep_end] != '\n' && p[sep_end] != '=') {
			p += sep_len;
			continue;
		}
		if (memcmp(p, sep, sep_len) == 0) {
			p += sep_len;
			if ((*size = atoi((char *)p)) > 0) {
				char *f = (char *)memchr(p, ' ', 20);
				if (f) {
					f++;
					if (memcmp(f, fname, fname_len) == 0 && f[fname_len] == '\n') {
						return	(BYTE *)f + fname_len + 1;
					}
				}
			}
		}
		else p++;
	}
	return	NULL;
}

#define FILESIZE_MAX	(1024 * 1024)
CreateStatus CreateFileBySelf(char *path, char *fname)
{
	HANDLE	hSelfFile = INVALID_HANDLE_VALUE;
	HANDLE	hMap = NULL;
	BYTE	*data = NULL;
	BYTE	*target = NULL;
	char	self_name[MAX_PATH] = "";
	DWORD	selfSize = 0;
	DWORD	size = 0;
	CreateStatus ret = CS_BROKEN;

	GetModuleFileName(::GetModuleHandle(NULL), self_name, sizeof(self_name));
	hSelfFile = ::CreateFile(self_name, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
							OPEN_EXISTING, 0, 0);
	hMap = ::CreateFileMapping(hSelfFile, 0, PAGE_READONLY, 0, 0, 0);
	data = (BYTE *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	if (!data) goto END;

	selfSize = GetFileSize(hSelfFile, 0);
	if ((target = FindSeparatedData(data, selfSize, fname, &size))) {
		VBuf	vbuf(FILESIZE_MAX);
		if (DeflateData(target, size, &vbuf)) {
			ret = CS_ACCESS;
			HANDLE	hDestFile = ::CreateFile(path, GENERIC_WRITE, 0, 0,
									CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
			if (hDestFile != INVALID_HANDLE_VALUE) {
				if (::WriteFile(hDestFile, vbuf.Buf(), vbuf.UsedSize(), &size, 0)) {
					ret = CS_OK;
				}
				::CloseHandle(hDestFile);
			}
		}
	}

END:
	::UnmapViewOfFile(data);
	::CloseHandle(hMap);
	::CloseHandle(hSelfFile);
	return	ret;
}

