/*	@(#)Copyright (C) H.Shirouzu 2011-2017   ipmsgcmn.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: IP Messenger Common Header
	Create					: 2011-05-03(Tue)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef IPMSGCMN_H
#define IPMSGCMN_H

//#define _USING_V110_SDK71_

#include "environ.h"
#include "tlib/tlib.h"
#include <stdio.h>
#include <time.h>
#include <richedit.h>
#include "version.h"
#include <stddef.h>
#include <richole.h>
#include <vector>
#include <map>
#include <list>
#include <atomic>

#include "resource.h"
#include "ipmsgdef.h"

#include "aes.h"
#include "ipdict.h"
#include "ipipc.h"
#include "inet.h"
#include "pubkey.h"
#include "ipmsgbase.h"
#include "msgmng.h"
#include "cfg.h"
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
	TMainWin	*ipmsgWnd;

public:
	TMsgApp(HINSTANCE _hI, LPSTR _cmdLine, int _nCmdShow);
	virtual ~TMsgApp();

	virtual void	InitWindow(void);
	virtual BOOL	PreProcMsg(MSG *msg);
};

HWND GetMainWnd(void);
TMainWin *GetMain();

#endif
