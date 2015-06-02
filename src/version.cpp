static char *version_id = 
	"@(#)Copyright (C) H.Shirouzu 2010-2015   version.cpp	Ver3.50"
#ifdef _WIN64
"(x64)"
#endif
;
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Version
	Create					: 2010-05-23(Sun)
	Update					: 2015-06-02(Tue)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
	Version文字列
*/
char *GetVersionStr()
{
	return	strstr(version_id, "Ver") + 3;
}


