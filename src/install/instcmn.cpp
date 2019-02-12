static char *install_id = 
	"@(#)Copyright (C) H.Shirouzu 1998-2017   instcmn.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Installer Application Class
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include "../ipmsgdef.h"
#include "instcmn.h"

BOOL GenStrWithUser(const char *str, char *buf, GenStrMode mode) // 0: .%s 1: (%s)
{
	int		len = strcpyz(buf, str);
	WCHAR	user[MAX_PATH]={};
	DWORD	ulen = MAX_PATH;

	if (::GetUserNameW(user, &ulen)) {
		U8str	u(user);

		if (mode == WITH_PERIOD) {
			sprintf(buf + len, ".%s", u.s());
		}
		else if (mode == WITH_PAREN) {
			sprintf(buf + len, " (%s)", u.s());
		}
		else {
			sprintf(buf + len, "-%s", u.s());
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL RunAsAdmin(HWND hWnd, BOOL imm)
{
	WCHAR	path[MAX_PATH];
	WCHAR	buf[MAX_BUF];

	::GetModuleFileNameW(NULL, path, wsizeof(path));
	swprintf(buf, L"/runas=%p,imm=%d", hWnd, imm);
	::ShellExecuteW(hWnd, L"runas", path, buf, NULL, SW_SHOW);

	return TRUE;
}

/*
	立ち上がっている IPMSG を終了
*/
int TerminateIPMsg(HWND hWnd, const char *title, const char *msg, BOOL silent)
{
	BOOL	existFlg = FALSE;

	::EnumWindows(TerminateIPMsgProc, (LPARAM)&existFlg);
	if (existFlg) {
		if (!silent && MessageBox(hWnd, title, msg, MB_OKCANCEL) == IDCANCEL) {
			return	1;
		}
		::EnumWindows(TerminateIPMsgProc, NULL);
		Sleep(500);
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
		if (strnicmp(IPMSG_CLASS, buf, strlen(IPMSG_CLASS)) == 0) {
			if (lParam) {
				*(BOOL *)lParam = TRUE;		// existFlg;
			}
			else {
				DWORD	procId = 0;
				if (::GetWindowThreadProcessId(hWnd, &procId) && procId) {
					HANDLE	hProc = ::OpenProcess(SYNCHRONIZE, FALSE, procId);
					::PostMessage(hWnd, WM_CLOSE, 0, 0);
					::WaitForSingleObject(hProc, 10000);
					::CloseHandle(hProc);
				}
			}
		}
	}
	return	TRUE;
}

#include <ObjBase.h>
#include <propvarutil.h>
#include <propkey.h>

/*
	リンク
	あらかじめ、CoInitialize(NULL); を実行しておくこと
*/
BOOL ShellLink(LPCSTR src, LPSTR dest, LPCSTR arg, LPCSTR app_id)
{
	IShellLink		*shellLink;
	IPersistFile	*persistFile;
	Wstr			wsrc(src), wdest(dest), warg(arg);
	BOOL			ret = FALSE;

	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&shellLink))) {
		shellLink->SetPath((char *)wsrc.Buf());
		shellLink->SetArguments((char *)warg.Buf());

		char	buf[MAX_PATH_U8];
		GetParentDirU8(src, buf);
		shellLink->SetWorkingDirectory((char *)U8toWs(buf));

		if (app_id && *app_id) {
			IPropertyStore *pps;
			HRESULT hr = shellLink->QueryInterface(IID_PPV_ARGS(&pps));

			if (SUCCEEDED(hr)) {
				PROPVARIANT pv;
				hr = InitPropVariantFromString(U8toWs(app_id), &pv);
				if (SUCCEEDED(hr)) {
					pps->SetValue(PKEY_AppUserModel_ID, pv);
					pps->Commit();
					PropVariantClear(&pv);
				}
			}
			pps->Release();
		}

		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, (void **)&persistFile))) {
			if (SUCCEEDED(persistFile->Save(wdest.s(), TRUE))) {
				ret = TRUE;
				GetParentDirU8(WtoU8(wdest.s()), buf);
				::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH|SHCNF_FLUSH, U8toAs(buf), NULL);
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
	::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH|SHCNF_FLUSH, U8toAs(dir), NULL);

	return	TRUE;
}

BOOL DeleteLinkFolder(LPCSTR path)
{
	char	dir[MAX_PATH_U8];
	char	buf[MAX_PATH_U8];
	WIN32_FIND_DATA_U8	fdat;

	MakePathU8(buf, path, "*");

	HANDLE	fh = FindFirstFileU8(buf, &fdat);
	if (fh == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}
	do {
		if (strcmpi(".", fdat.cFileName) == 0 || strcmpi("..", fdat.cFileName) == 0) {
			continue;
		}
		MakePathU8(buf, path, fdat.cFileName);
		DeleteFileU8(buf);
	} while (FindNextFileU8(fh, &fdat));

	::FindClose(fh);

	RemoveDirectoryU8(path);

	GetParentDirU8(path, dir);
	::SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH|SHCNF_FLUSH, U8toAs(dir), NULL);

	return	TRUE;
}

/*
	同じ内容を持つショートカットを削除（スタートアップへの重複登録よけ）
*/
BOOL RemoveSameLink(const char *dir, const char *check_name)
{
	char				path[MAX_PATH_U8];
	char				dest[MAX_PATH_U8];
	char				arg[INFOTIPSIZE];
	HANDLE				fh;
	WIN32_FIND_DATA_U8	fdat;
	BOOL				ret = FALSE;

	sprintf(path, "%s\\*.*", dir);
	if ((fh = FindFirstFileU8(path, &fdat)) == INVALID_HANDLE_VALUE)
		return	FALSE;

	do {
		sprintf(path, "%s\\%s", dir, fdat.cFileName);
		if (ReadLinkU8(path, dest, arg) && *arg == 0) {
			int		dest_len = (int)strlen(dest);
			int		chk_len = (int)strlen(check_name);
			if (dest_len > chk_len &&
					strnicmp(dest + dest_len - chk_len, check_name, chk_len) == 0) {
				ret = DeleteFileU8(path);
			}
		}

	} while (FindNextFileU8(fh, &fdat));

	::FindClose(fh);
	return	ret;
}

/*
	ファイルの保存されているドライブ識別
*/
UINT GetDriveTypeEx(const char *file)
{
	if (file == NULL)
		return	GetDriveType(NULL);

	if (IsUncFile(file))
		return	DRIVE_REMOTE;

	char	buf[MAX_PATH_U8];
	int		len = (int)strlen(file), len2;

	strcpy(buf, file);
	do {
		len2 = len;
		GetParentDirU8(buf, buf);
		len = (int)strlen(buf);
	} while (len != len2);

	return	GetDriveTypeU8(buf);
}

