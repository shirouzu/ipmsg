/*	@(#)Copyright (C) H.Shirouzu 2016-2017   environ.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Environment header
	Create					: 2016-02-16(Tue)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSG_ENVIRON_H
#define IPMSG_ENVIRON_H

#ifndef _DEBUG
//#define IPMSG_PRO
//#define OFFICIAL_SVR
#endif

//#define DISALBE_HOOK

#ifdef IPMSG_PRO
#define ENV_DEF
#include "ipmsgext.dat"
#undef  ENV_DEF
#endif

#endif
