static char *version_id = 
	"@(#)Copyright (C) H.Shirouzu 2010-2018   version.cpp	Ver4.83";

#include "ipmsg.h"

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
	Update					: 2017-08-21(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char base_ver[128];
static char full_ver[128];
static VerInfo verInfo;

/*
	Version文字列
*/
const char *GetVersionStr(BOOL is_base)
{
	if (*base_ver == 0) {
		if (const char *p = strstr(version_id, "Ver")) {
			strcpy(base_ver, p + 3);
		}
		strcpy(full_ver, base_ver);
		strcat(full_ver, additional_ver);
	}

	return	is_base ? base_ver : full_ver;
}

BOOL GetVerInfo(VerInfo *vi_in)
{
	if (verInfo.type == 0) {
		verInfo.type = IPMSG_VER_WIN_TYPE;	// once

		if (const char *ver = GetVersionStr(TRUE)) {
			verInfo.major = atoi(ver);

			if ((ver = strchr(ver, '.'))) {
				ver += 1;
				verInfo.minor = atoi(ver);

				if (strlen(ver) <= 2) {
					verInfo.rev = 10000;	// over beta && under rel
				}
				else {
					verInfo.rev = int(atof(ver+3) * 10);

					switch (ver[2]) {
					case 'a':	// alpha N
						// verInfo.rev += 0;
						break;
					case 'b':	// beta N
						verInfo.rev +=  1000;
						break;
					case 'r':	// rel N
						verInfo.rev += 10000;
						break;
					default:
						break;
					}
				}
			}
		}
	}

	if (vi_in) {
		*vi_in = verInfo;
	}
	return	TRUE;
}

const char *GetVerHexInfo()
{
	static char verHexStr[128];

	if (verInfo.type == 0) {
		GetVerInfo(NULL);
	}

	if (*verHexStr == 0) {
		sprintf(verHexStr, "%08X:%d:%d:%d",
			verInfo.type, verInfo.major, verInfo.minor, verInfo.rev);
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

double VerStrToDouble(const char *s)
{
	char *opt = NULL;

	double	ver = strtod(s, &opt);
	double	sub = 0.0001;

	if (opt && *opt) {
		switch (tolower(*opt)) {
		case 'a':
			sub = strtod(opt+1, NULL) * 0.00000001;
			break;
		case 'b':
			sub = strtod(opt+1, NULL) * 0.000001;
			break;
		case 'r':
			sub = strtod(opt+1, NULL) * 0.0001;
			break;
		default:
			Debug("VerStrToDouble: unknown %s opt=%s\n", s, opt);
			break;
		}
	}

	return	ver + sub;
}

