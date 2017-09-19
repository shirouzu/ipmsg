/*static char *version_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2016   version.h	Ver4.00"; */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Version
	Create					: 2010-05-23(Sun)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef VERSION_H
#define VERSION_H

struct VerInfo {
	DWORD	type;	// Win32 == 0x00010001, x64 == 0x00010002
					// v4.00a17
	WORD	major;	//  4
	WORD	minor;	//    00
	WORD	rev;	//       17
	VerInfo() {
		type	= 0;
		major	= 0;
		minor	= 0;
		rev		= 0;
	}
};

const char *GetVersionStr(BOOL is_basever=FALSE);
BOOL GetVerInfo(VerInfo *vi);
const char *GetVerHexInfo();
BOOL GetVerInfoByHex(const char *s, VerInfo *vi);
double VerStrToDouble(const char *s);

#endif

