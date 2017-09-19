static char *ipipc_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		ipipc.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger Inter-Process Communication
	Create					: 2017-05-24(Wed)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	======================================================================== */

#include "ipmsg.h"

IPIpc::IPIpc()
{
	hMap = NULL;
}

IPIpc::~IPIpc()
{
	UnInit();
}

void IPIpc::UnInit()
{
	if (hMap) {
		::CloseHandle(hMap);
		hMap = NULL;
	}
}

void GenMapName(char *buf, HWND hWnd, BOOL is_in)
{
	sprintf(buf, "ipmsg_%s_%zd", is_in ? "in" : "out", (DWORD_PTR)hWnd);
}

BOOL IPIpc::LoadDictFromMap(HWND hWnd, BOOL is_in, IPDict *dict)
{
	char	map_name[MAX_PATH];
	GenMapName(map_name, hWnd, is_in);

	BOOL	ret = FALSE;

	if (HANDLE hTmpMap = ::OpenFileMapping(FILE_MAP_READ, FALSE, map_name)) {
		if (BYTE *data = (BYTE *)::MapViewOfFile(hTmpMap, FILE_MAP_READ, 0, 0, 0)) {
			MEMORY_BASIC_INFORMATION	mbi = {};
			if (::VirtualQueryEx(::GetCurrentProcess(), data, &mbi, sizeof(mbi))) {
				if (dict->unpack(data, mbi.RegionSize) > 0) {
					ret = TRUE;
				}
			}
			::UnmapViewOfFile(data);
		}
		::CloseHandle(hTmpMap);
	}
	return	ret;
}

BOOL IPIpc::SaveDictToMap(HWND hWnd, BOOL is_in, const IPDict &dict)
{
	char	map_name[MAX_PATH];
	GenMapName(map_name, hWnd, is_in);

	size_t	size = dict.pack_size();
	hMap = ::CreateFileMapping((HANDLE)~0, 0, PAGE_READWRITE, 0, (DWORD)size, map_name);
	if (!hMap) {
		return	FALSE;
	}

	BOOL	ret = FALSE;
	if (BYTE *data = (BYTE *)::MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0)) {
		dict.pack(data, size);
		::UnmapViewOfFile(data);
		ret = TRUE;
	}

	return	ret;
}

