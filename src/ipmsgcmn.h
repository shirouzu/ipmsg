/*	@(#)Copyright (C) H.Shirouzu 2011-2015   ipmsgcmn.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Common Header
	Create					: 2011-05-03(Tue)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSGCMN_H
#define IPMSGCMN_H

//#define _USING_V110_SDK71_

#include "tlib/tlib.h"
#include <stdio.h>
#include <time.h>
#include <richedit.h>
#include "version.h"
#include <stddef.h>
#include <richole.h>
#include "resource.h"
#include <vector>
#include <map>

/*  IP Messenger for Windows  internal define  */
#define IPMSG_REVERSEICON			0x0100
#define IPMSG_TIMERINTERVAL			500
#define IPMSG_ENTRYMINSEC			5
#define IPMSG_GETLIST_FINISH		0
#define IPMSG_DELAYMSG_OFFSETTIME	2000
#define IPMSG_DELAYFOCUS_TIME		10

#define IPMSG_BROADCAST_TIMER		0x0101
#define IPMSG_SEND_TIMER			0x0102
#define IPMSG_LISTGET_TIMER			0x0104
#define IPMSG_LISTGETRETRY_TIMER	0x0105
#define IPMSG_ENTRY_TIMER			0x0106
#define IPMSG_DUMMY_TIMER			0x0107
#define IPMSG_RECV_TIMER			0x0108
#define IPMSG_ANS_TIMER				0x0109
#define IPMSG_CLEANUP_TIMER			0x010a
#define IPMSG_BALLOON_RECV_TIMER	0x010b
#define IPMSG_BALLOON_OPEN_TIMER	0x010c
#define IPMSG_BALLOON_DELAY_TIMER	0x010d
#define IPMSG_IMAGERECT_TIMER		0x010e
#define IPMSG_KEYCHECK_TIMER		0x010f
#define IPMSG_DELAYFOCUS_TIMER		0x0110


#define IPMSG_NICKNAME			1
#define IPMSG_FULLNAME			2

#define IPMSG_NAMESORT			0x00000000
#define IPMSG_IPADDRSORT		0x00000001
#define IPMSG_HOSTSORT			0x00000002
#define IPMSG_NOGROUPSORTOPT	0x00000100
#define IPMSG_ICMPSORTOPT		0x00000200
#define IPMSG_NOKANJISORTOPT	0x00000400
#define IPMSG_ALLREVSORTOPT		0x00000800
#define IPMSG_GROUPREVSORTOPT	0x00001000
#define IPMSG_SUBREVSORTOPT		0x00002000


#define WM_NOTIFY_TRAY			(WM_APP + 101)
#define WM_NOTIFY_RECV			(WM_APP + 102)
#define WM_NOTIFY_OPENCHECK		(WM_APP + 103)
#define WM_IPMSG_INITICON		(WM_APP + 104)
#define WM_IPMSG_CHANGE_MCAST	(WM_APP + 105)
#define WM_RECVDLG_OPEN			(WM_APP + 110)
#define WM_RECVDLG_EXIT			(WM_APP + 111)
#define WM_RECVDLG_FILEBUTTON	(WM_APP + 112)
#define WM_SENDDLG_OPEN			(WM_APP + 121)
#define WM_SENDDLG_CREATE		(WM_APP + 122)
#define WM_SENDDLG_EXIT			(WM_APP + 123)
#define WM_SENDDLG_HIDE			(WM_APP + 124)
#define WM_SENDDLG_RESIZE		(WM_APP + 125)
#define WM_SENDDLG_FONTCHANGED	(WM_APP + 126)
#define WM_UDPEVENT				(WM_APP + 130)
#define WM_TCPEVENT				(WM_APP + 131)
#define WM_REFRESH_HOST			(WM_APP + 140)
#define WM_MSGDLG_EXIT			(WM_APP + 150)
#define WM_DELMISCDLG			(WM_APP + 151)
#define WM_HIDE_CHILDWIN		(WM_APP + 160)
#define WM_EDIT_DBLCLK			(WM_APP + 170)
#define WM_DELAYSETTEXT			(WM_APP + 180)
#define WM_DELAY_BITMAP			(WM_APP + 182)
#define WM_DELAY_PASTE			(WM_APP + 183)
#define WM_PASTE_REV			(WM_APP + 184)
#define WM_PASTE_IMAGE			(WM_APP + 185)
#define WM_PASTE_TEXT			(WM_APP + 186)
#define WM_SAVE_IMAGE			(WM_APP + 187)
#define WM_EDIT_IMAGE			(WM_APP + 188)
#define WM_INSERT_IMAGE			(WM_APP + 189)
#define WM_HISTDLG_OPEN			(WM_APP + 190)
#define WM_HISTDLG_HIDE			(WM_APP + 191)
#define WM_HISTDLG_NOTIFY		(WM_APP + 192)
#define WM_FORCE_TERMINATE		(WM_APP + 193)
#define WM_FINDDLG_DELAY		(WM_APP + 194)
#define WM_IPMSG_IMECTRL		(WM_APP + 200)
#define WM_IPMSG_BRNOTIFY		(WM_APP + 201)
#define WM_IPMSG_REMOTE			(WM_APP + 202)


typedef long	Time_t;		// size of time_t is 64bit in VS2005 or later

#define SKEY_HEADER_SIZE	12

// General define
#define MAX_SOCKBUF		65536
#define MAX_UDPBUF		32768
#define MAX_BUF			1024
#define MAX_BUF_EX		(MAX_BUF * 3)
#define MAX_MULTI_PATH	(MAX_BUF * 32)
#define MAX_NAMEBUF		80
#define MAX_LISTBUF		(MAX_NAMEBUF * 4)
#define MAX_ANSLIST		100
#define MAX_FILENAME_U8	(255 * 3)

#define HS_TOOLS		"HSTools"
#define IP_MSG			"IPMsg"
#define NO_NAME			"no_name"
#define URL_STR			"://"
#define MAILTO_STR		"mailto:"
#define MSG_STR			"msg"
#define IPMSG_CLASS		"ipmsg_class"

#define REMOTE_CMD			"ipmsg-cmd:"
#define REMOTE_FMT			REMOTE_CMD "%s"
#define REMOTE_HEADERLEN	10
#define REMOTE_KEYLEN		32
#define REMOTE_KEYSTRLEN	43

#define DEFAULT_PRIORITY	10
#define PRIORITY_OFFSET		10
#define DEFAULT_PRIORITYMAX	5

#define CLIP_ENABLE			0x1
#define CLIP_SAVE			0x2
#define CLIP_CONFIRM_NORMAL	0x4
#define CLIP_CONFIRM_STRICT	0x8
#define CLIP_CONFIRM_ALL	(CLIP_CONFIRM_NORMAL|CLIP_CONFIRM_STRICT)

#include "aes.h"
#include "pubkey.h"
#include "ipmsgbase.h"
#include "cfg.h"
#include "msgmng.h"
#include "richedit.h"
#include "miscdlg.h"
#include "share.h"
#include "logmng.h"
#include "image.h"
#include "senddlg.h"
#include "recvdlg.h"
#include "setupdlg.h"
#include "histdlg.h"
#include "logview.h"
#include "remotedlg.h"
#include "miscfunc.h"
#include "mainwin.h"

class TMsgApp : public TApp {
public:
	TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TMsgApp();

	virtual void	InitWindow(void);
};

HWND GetMainWnd(void);
char *GetVersionStr(void);

#endif
