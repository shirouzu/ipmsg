/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2017   logchilddef.h	Ver4.60 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer Child Define
	Create					: 2017-07-15(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGCHILDDEF_H
#define LOGCHILDDEF_H

#define FILE_CX			60
#define FILEMIN_CX		(30 + ITEMW_GAP)
#define USR_MINCX		30
#define FROMTO_CX		13
#define FROMTO_CY		16
#define REPLYBMP_CX		18
#define REPLYBMP_CY		19
#define FILEBMP_CX		15
#define FILEBMP_CY		15
#define FILEMAX_CX		(FILEBMP_CX + 200)
#define FILEMID_CX		(FILEBMP_CX + 100)
#define CLIPBMP_CX		13
#define CLIPBMP_CY		15
#define CLIPMIN_CX		(CLIPBMP_CX + ITEMW_GAP)
#define FAVBMP_CX		15
#define FAVBMP_CY		17
#define CMNTBMP_CX		16
#define CMNTBMP_CY		17
#define IMENUBMP_CX		14
#define IMENUBMP_CY		20
#define FAVMIN_CX		(FAVBMP_CX + ITEMW_GAP)
#define CROPUSER_CX		10
#define CROPUSER_CY		12
#define UNOPEN_CX		10
#define UNOPEN_CY		7
#define UNOPENB_CX		52
#define UNOPENB_CY		25
#define FOLD_CX			16
#define FOLD_CY			8
#define NOCLIP_CX		15
#define NOCLIP_CY		15
#define FOCUS_WIDTH		4

#define WHITE_RGB	RGB(255,255,255)
#define BLACK_RGB	RGB(  0,  0,  0)
#define SHINE_RGB	RGB(240,240,240)
#define MIDSHINE_RGB RGB(230,230,230)
#define SHADOW_RGB	RGB(127,127,127)
#define MIDGRAY_RGB	RGB(170,170,170)
#define GRAY_RGB	RGB(210,210,210)
#define SEL_RGB		RGB(50, 140,255)
#define MARK_RGB	RGB(128,255,128)
#define CMNT_RGB	RGB(230,255,230)
#define FIND_RGB	RGB(255,128,128)
#define LINK_RGB	RGB(  0,  0,255)
#define TRANS_RGB	RGB(253,253,253)
#define TRANS2_RGB	RGB(192,192,192)

#define SENDMSG_FOREDURATION	1000
#define FIND_CHAR_MAX	1000

#define SCR_HIRES_MAX	50
#define SCR_HIRES_MIN	3
#define FOLD_MARGIN		2

#endif

