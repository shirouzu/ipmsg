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

#include "install.h"
#include "instcmn.h"
#include "instcore.h"

using namespace std;

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

BOOL DeflateData(BYTE *buf, DWORD size, DynBuf *dbuf)
{
	BOOL	ret = FALSE;
	z_stream z = {};

	if (inflateInit(&z) != Z_OK) return FALSE;
	z.next_out = dbuf->Buf();
	z.avail_out = (uInt)dbuf->Size();
	z.next_in = buf;
	z.avail_in = size;
	if (inflate(&z, Z_NO_FLUSH) == Z_STREAM_END) {
		dbuf->SetUsedSize(dbuf->Size() - z.avail_out);
		ret = TRUE;
	}
	inflateEnd(&z);
	return	ret;
}

BOOL RotateFile(const char *path, int max_cnt, BOOL with_delete)
{
	BOOL	ret = TRUE;

	for (int i=max_cnt-1; i >= 0; i--) {
		char	src[MAX_PATH_U8];
		char	dst[MAX_PATH_U8];

		snprintfz(src, sizeof(src), (i == 0) ? "%s" : "%s.%d", path, i);
		snprintfz(dst, sizeof(dst), "%s.%d", path, i+1);

		if (GetFileAttributesU8(src) == 0xffffffff) {
			continue;
		}
		if (MoveFileExU8(src, dst, MOVEFILE_REPLACE_EXISTING)) {
			if (with_delete) {
				::MoveFileExU8(dst, NULL, MOVEFILE_DELAY_UNTIL_REBOOT); // require admin priv...
			}
		}
		else {
			ret = FALSE;
		}
	}
	return	ret;
}

BOOL CleanupRotateFile(const char *path, int max_cnt)
{
	BOOL	ret = TRUE;

	for (int i=max_cnt; i > 0; i--) {
		char	targ[MAX_PATH_U8];

		snprintfz(targ, sizeof(targ), "%s.%d", path, i);

		if (GetFileAttributesU8(targ) == 0xffffffff) {
			continue;
		}
		if (!DeleteFileU8(targ)) {
			ret = FALSE;
		}
	}
	return	ret;
}

BOOL IPDictCopy(IPDict *dict, const char *fname, const char *dst, BOOL *is_rotate)
{
	HANDLE	hDst;
	BOOL	ret = FALSE;
	DWORD	dstSize = 0;
	int64	mtime;
	int64	fsize;
	IPDict	fdict;
	DynBuf	zipData;
	DynBuf	data;

	if (!dict->get_dict(fname, &fdict)) {
		return	FALSE;
	}
	if (!fdict.get_bytes(FDATA_KEY, &zipData)) {
		return	FALSE;
	}
	if (!fdict.get_int(MTIME_KEY, &mtime)) {
		return	FALSE;
	}
	if (!fdict.get_int(FSIZE_KEY, &fsize)) {
		return	FALSE;
	}
	if (!data.Alloc((size_t)fsize)) {
		return	FALSE;
	}
	if (!DeflateData(zipData.Buf(), (DWORD)zipData.UsedSize(), &data)
	|| fsize != data.UsedSize()) {
		return FALSE;
	}

#define MAX_ROTATE	10
	CleanupRotateFile(dst, MAX_ROTATE);
	hDst = CreateFileU8(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hDst == INVALID_HANDLE_VALUE) {
		RotateFile(dst, MAX_ROTATE, TRUE);
		if (is_rotate) {
			*is_rotate = TRUE;
		}
		hDst = CreateFileU8(dst, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	}

	if (hDst == INVALID_HANDLE_VALUE) {
		Debug("dst(%s) open err(%x)\n", dst, GetLastError());
		return	FALSE;
	}

	if (::WriteFile(hDst, data.Buf(), (DWORD)fsize, &dstSize, 0) && fsize == dstSize) {
		FILETIME	ct;
		FILETIME	la;
		FILETIME	ft;

		UnixTime2FileTime(mtime, &ct);
		la = ct;
		ft = ct;
		::SetFileTime(hDst, &ct, &la, &ft);
		ret = TRUE;
	}
	::CloseHandle(hDst);

	return	ret;
}


BOOL IsSameFileDict(IPDict *dict, const char *fname, const char *dst)
{
	WIN32_FIND_DATA_U8	dst_dat;
	HANDLE		hFind;
	int64		stime;
	int64		dtime;
	int64		fsize;
	IPDict		fdict;

	if (!dict->get_dict(fname, &fdict)) {
		return	FALSE;
	}
	if (!fdict.get_int(MTIME_KEY, &stime)) {
		return	FALSE;
	}
	if (!fdict.get_int(FSIZE_KEY, &fsize)) {
		return	FALSE;
	}

	if ((hFind = FindFirstFileU8(dst, &dst_dat)) == INVALID_HANDLE_VALUE) {
		return	FALSE;
	}
	::FindClose(hFind);

	if (fsize != dst_dat.nFileSizeLow) {
		return	FALSE;
	}
	dtime = FileTime2UnixTime(&dst_dat.ftLastWriteTime);

	return	(stime >> 2) == (dtime >> 2);
}


BOOL GetIPDictBySelf(IPDict *dict)
{
	auto hSelfFile = scope_raii(
			[&]() {
				char	self_name[MAX_PATH_U8] = {};
				GetModuleFileNameU8(NULL, self_name, sizeof(self_name));
				return	CreateFileU8(self_name, GENERIC_READ,
						FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
			}(),
			[&](auto hSelfFile) { ::CloseHandle(hSelfFile); });
	if (hSelfFile == INVALID_HANDLE_VALUE) return FALSE;

	auto hMap = scope_raii(
			::CreateFileMapping(hSelfFile, 0, PAGE_READONLY, 0, 0, 0),
			[&](auto hMap) { ::CloseHandle(hMap); });
	if (!hMap) return FALSE;

	auto data = scope_raii(
			(BYTE *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0),
			[&](auto data) { ::UnmapViewOfFile(data); });
	if (!data) return FALSE;

	BYTE	sep[72];
	memset(sep, '=', sizeof(sep));
	sep[0] = sep[sizeof(sep)-1] = '\n';

	auto	selfSize = (size_t)::GetFileSize(hSelfFile, 0);
	auto	max_size = selfSize - sizeof(sep);

	for (int i=0; i < max_size; ) {
		BYTE	&ch = data[i];
		BYTE	&end_ch = data[i+sizeof(sep)-1];

		if (ch == '\n') {
			if (end_ch == '\n') {
				if (memcmp(&ch, sep, sizeof(sep)) == 0 && memcmp(&end_ch+1, "IP2:", 4) == 0) {
					auto	targ = &ch + sizeof(sep);
					auto	remain = selfSize - (targ - data);
					return	(dict->unpack(targ, remain) > 0) ? TRUE : FALSE;
				}
				i += sizeof(sep)-1;
			}
			else if (end_ch == '=') {
				i++;
			}
			else {
				i += sizeof(sep)-1;
			}
		}
		else if (end_ch == '=') {
			i++;
		}
		else {
 			i += sizeof(sep)-1;
		}
	}

	return	FALSE;
}

