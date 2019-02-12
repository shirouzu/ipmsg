#include "ipmsg.h"
#define VER_STR    "4.99r3"
static char *version_id =
	"@(#)Copyright (C) H.Shirouzu and FastCopy Lab, LLC. 2010-2019	version.cpp	Ver" VER_STR
#ifdef _DEBUG
	"d"
#endif
;

// readme readme-eng / ipmsg.rc toast.rc / hhc htm / (redmine/wiki)

static char *additional_ver = ""
#ifdef _WIN64
"(x64)"
#endif
#ifdef IPMSG_PRO
#define VER_DEF
#include "ipmsgext.dat"
#undef  VER_DEF
#endif
;
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Version
	Create					: 2010-05-23(Sun)
	Update					: 2019-02-12(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
	Version文字列
*/
const char *GetVersionStr(BOOL is_base)
{
	static char base_ver[128];
	static char full_ver[128];

	if (*base_ver == 0) {
		strcpy(base_ver, VER_STR);
		strcpy(full_ver, base_ver);
		strcat(full_ver, additional_ver);
	}

	return	is_base ? base_ver : full_ver;
}

BOOL GetVerStrToInfo(const char *ver_str, VerInfo *vi)
{
	vi->type = IPMSG_VER_WIN_TYPE;
	vi->major = atoi(ver_str);

	if (auto minor = strchr(ver_str, '.')) {
		minor += 1;
		vi->minor = atoi(minor);

		auto rv = minor + 1;
		while (isdigit(*rv)) rv++;
		auto sign = *rv;
		if (sign) rv++;

		vi->rev = atoi(rv);

		switch (sign) {
		case 'a':	// alpha N
			vi->rev += 0;
			break;
		case 'b':	// beta N
			vi->rev += 50;
			break;
		case 'r':	// rel N
			vi->rev += 100;
			break;
		default:
			vi->rev += 100;
			break;
		}
	}
	return	TRUE;
}

const VerInfo *GetVerInfoCore()
{
	static VerInfo verInfo;

	if (verInfo.type == 0) {
		GetVerStrToInfo(GetVersionStr(), &verInfo);
	}
	return	&verInfo;
}

BOOL GetVerInfo(VerInfo *vi_in)
{
	if (vi_in) {
		*vi_in = *GetVerInfoCore();
	}
	return	TRUE;
}

BOOL IsVerGreater(VerInfo *vi)
{
	auto verInfo = GetVerInfoCore();

	if (vi->major > verInfo->major) return TRUE;
	if (vi->major < verInfo->major) return FALSE;
	if (vi->minor > verInfo->minor) return TRUE;
	if (vi->minor < verInfo->minor) return FALSE;
	if (vi->rev   > verInfo->rev)   return TRUE;

	return FALSE;
}

const char *GetVerHexInfoCore(const VerInfo *vi, char *s)
{
	sprintf(s, "%08X:%d:%d:%d", vi->type, vi->major, vi->minor, vi->rev);

	return	s;
}

const char *GetVerHexInfo()
{
	static char verHexStr[128];

	auto verInfo = GetVerInfoCore();

	if (*verHexStr == 0) {
		GetVerHexInfoCore(verInfo, verHexStr);
	}
	return	verHexStr;
}

BOOL GetVerInfoByHex(const char *s, VerInfo *vi)
{
	const char	*type	= s;
	const char	*major = strchr(type, ':');
	if (!major++) {
		return	FALSE;
	}

	const char *minor = strchr(major, ':');
	if (!minor++) {
		return	FALSE;
	}

	const char *rev	= strchr(minor, ':');
	if (!rev++) {
		return	FALSE;
	}

	vi->type  = strtoul(type, 0, 16);
	vi->major = atoi(major);
	vi->minor = atoi(minor);
	vi->rev   = atoi(rev);
	return	TRUE;
}


