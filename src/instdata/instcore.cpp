static char *install_id = 
	"@(#)Copyright (C) H.Shirouzu 1998-2017   instcore.cpp	Ver4.61";
/* ========================================================================
	Project  Name			: Installer for IPMSG32
	Module Name				: Installer Application Class
	Create					: 1998-06-14(Sun)
	Update					: 2017-07-31(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "../tlib/tlib.h"
#include "../version.h"
#include "../../external/zlib/zlib.h"

#include "../install/install.h"
#include "../instdata/instcmn.h"
#include "../instdata/instcore.h"

// files_data(64).dat は
// ipmsg.exe / uninst.exe / ipmsg.chm / iptoast.dll / ipmsg.png / ipexc.png
// の zlib圧縮済みデータが BYTE配列で定義されたもの
#ifdef _WIN64
#include "files_data64.dat"
#else
#include "files_data.dat"
#endif

static ExtractErr ExtractErrNo = NO_EXTRACT_ERR;
static char Extractmsg[MAX_PATH_U8];

void SetExtractErr(ExtractErr err, const char *msg)
{
	ExtractErrNo = err;
	strcpy(Extractmsg, msg);
}

ExtractErr GetExtractErr(const char **msg)
{
	if (msg) {
		*msg = Extractmsg;
	}
	return	ExtractErrNo;
}

BOOL DeflateData(BYTE *buf, DWORD size, VBuf *vbuf)
{
	BOOL	ret = FALSE;
	z_stream z = {};

	if (inflateInit(&z) != Z_OK) {
		return FALSE;
	}
	z.next_out = vbuf->Buf();
	z.avail_out = (uInt)vbuf->Size();
	z.next_in = buf;
	z.avail_in = size;
	if (inflate(&z, Z_NO_FLUSH) == Z_STREAM_END) {
		vbuf->SetUsedSize(vbuf->Size() - z.avail_out);
		ret = TRUE;
	}
	inflateEnd(&z);

	return	ret;
}

BOOL RotateFileW(const WCHAR *path, int max_cnt, BOOL with_delete)
{
	BOOL	ret = TRUE;

	for (int i=max_cnt-1; i >= 0; i--) {
		WCHAR	src[MAX_PATH];
		WCHAR	dst[MAX_PATH];

		snwprintfz(src, wsizeof(src), (i == 0) ? L"%s" : L"%s.%d", path, i);
		snwprintfz(dst, wsizeof(dst), L"%s.%d", path, i+1);

		if (::GetFileAttributesW(src) == 0xffffffff) {
			continue;
		}
		if (::MoveFileExW(src, dst, MOVEFILE_REPLACE_EXISTING)) {
			if (with_delete) {
				::MoveFileExW(dst, NULL, MOVEFILE_DELAY_UNTIL_REBOOT); // require admin priv...
			}
		}
		else {
			ret = FALSE;
		}
	}
	return	ret;
}

BOOL MiniCopyW(const WCHAR *dst, BYTE *data, DWORD data_size, FILETIME mtime, int wait_retry=0,
	BOOL *is_rotate=NULL)
{
	HANDLE		hDst;
	BOOL		ret = FALSE;
	DWORD		dstSize;
	BOOL		isRotate = FALSE;

	do {
		hDst = ::CreateFileW(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (hDst != INVALID_HANDLE_VALUE) {
			break;
		}
		Sleep(500);
	} while (--wait_retry >= 0);

	if (hDst == INVALID_HANDLE_VALUE) {
		RotateFileW(dst, 10, TRUE);
		if (is_rotate) {
			*is_rotate = TRUE;
		}
		hDst = ::CreateFileW(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	}

	if (hDst != INVALID_HANDLE_VALUE) {
		if (::WriteFile(hDst, data, data_size, &dstSize, 0) && data_size == dstSize) {
			ret = TRUE;
		}
		::SetFileTime(hDst, NULL, NULL, &mtime);
		::CloseHandle(hDst);
	}
	else {
		DebugW(L"dst(%s) open err(%x)\n", dst, GetLastError());
		return	FALSE;
	}

	return	ret;
}

#define FILESIZE_MAX (200 * 1024 * 1024)
BOOL CreateFileByData(const char *fname, const char *dir, BYTE *data, DWORD size,
	FILETIME mtime, int retry_num)
{
	BOOL	ret = FALSE;
	char	installPath[MAX_PATH_U8];
	VBuf	vbuf(FILESIZE_MAX);

	MakePathU8(installPath, dir, fname);

	if (!DeflateData(data, size, &vbuf)) {
		SetExtractErr(DATA_BROKEN);
		return	FALSE;
	}

	Wstr	wpath(installPath);
	ret = MiniCopyW(wpath.s(), vbuf.Buf(), (DWORD)vbuf.UsedSize(), mtime, retry_num);

	if (!ret) {
		SetExtractErr(CREATE_FILE_ERR, installPath);
	}

	return	ret;
}

BOOL VerifyCompressedData(BYTE *data, DWORD size)
{
	VBuf	vbuf(FILESIZE_MAX);

	return	DeflateData(data, size, &vbuf);
}

BOOL ExtractAll(const char *dir, BOOL is_silent, TInstDlg *dlg)
{
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	CreateDirectoryU8(dir, NULL);

	DWORD	attr = GetFileAttributesU8(dir);

	if (attr == 0xffffffff || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		SetExtractErr(CREATE_DIR_ERR, dir);
		return	FALSE;
	}

	int		max_retry = is_silent ? 10 : 1;

	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

// ファイル生成
	if (!CreateFileByData(IPMSGHELP_NAME, dir, ipmsg_chm,
		(DWORD)sizeof(ipmsg_chm), ipmsg_chm_mtime, max_retry)) {
		return	FALSE;	// 一番最後に定義しているデータから展開
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	if (!CreateFileByData(UNINST_EXENAME, dir, uninst_exe,
		 (DWORD)sizeof(uninst_exe), uninst_exe_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

//	SetExtractErr(CREATE_FILE_ERR, dir);
//	return	FALSE;

	if (!CreateFileByData(IPMSGPNG_NAME, dir, ipmsg_png,
		(DWORD)sizeof(ipmsg_png), ipmsg_png_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	if (!CreateFileByData(IPEXCPNG_NAME, dir, ipexc_png,
		(DWORD)sizeof(ipexc_png), ipexc_png_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	if (!CreateFileByData(IPTOAST_NAME, dir, iptoast_dll,
		(DWORD)sizeof(iptoast_dll), iptoast_dll_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	if (!CreateFileByData(IPCMD_EXENAME, dir, ipcmd_exe,
		(DWORD)sizeof(ipcmd_exe), ipcmd_exe_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	if (!CreateFileByData(IPMSG_EXENAME, dir, ipmsg_exe,
		(DWORD)sizeof(ipmsg_exe), ipmsg_exe_mtime, max_retry)) {
		return	FALSE;
	}
	if (dlg && !dlg->Progress()) {
		SetExtractErr(USER_CANCEL, dir);
		return	FALSE;
	}

	return	TRUE;
}

BOOL VerifyAllCompressedData()
{
	if (!VerifyCompressedData(ipmsg_chm, (DWORD)sizeof(ipmsg_chm))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(uninst_exe, (DWORD)sizeof(uninst_exe))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(ipmsg_png, (DWORD)sizeof(ipmsg_png))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(ipexc_png, (DWORD)sizeof(ipexc_png))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(iptoast_dll, (DWORD)sizeof(iptoast_dll))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(ipcmd_exe, (DWORD)sizeof(ipcmd_exe))) {
		return	FALSE;
	}
	if (!VerifyCompressedData(ipmsg_exe, (DWORD)sizeof(ipmsg_exe))) {
		return	FALSE;
	}
	return	TRUE;
}


