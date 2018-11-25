/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2016   logeditdlg.h	Ver4.00 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer items
	Create					: 2015-04-10(Sat)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGEDITDLG_H
#define LOGEDITDLG_H

#include "logdb.h"

class TMsgHeadEditDlg : public TDlg {
protected:
	Cfg		*cfg;
	LogHost	*in_host;
	LogHost	*out_host;

public:
	TMsgHeadEditDlg(Cfg *_cfg, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	int	Exec(LogHost *_in_host, LogHost *_out_host);
};

class TMsgEditDlg : public TDlg {
protected:
	Cfg		*cfg;
	Wstr	*in_msg;
	Wstr	*out_msg;
	int		title_ids;

public:
	TMsgEditDlg(Cfg *_cfg, UINT _id=MSGBODY_DIALOG, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
	int	Exec(Wstr *_in_msg, Wstr *_out_msg, int title_ids=0);
};

#endif


