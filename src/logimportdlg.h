/*	@(#)Copyright (C) H.Shirouzu and Asahi Net, Inc. 2015-2016   logimportdlg.h	Ver4.00 */
/* ========================================================================
	Project  NameF			: IP Messenger for Win32
	Module Name				: LogViewer items
	Create					: 2015-04-10(Sat)
	Update					: 2016-08-17(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef LOGIMPORTDLG_H
#define LOGIMPORTDLG_H

#include "logdb.h"
#include "logdbconv.h"
#include "logchildview.h"

class TLogImportDlg : public TDlg {
protected:
	Cfg				*cfg;
	TChildView		*view;
	TListViewEx		logListView;
	LogDb			*logDb;
	VBVec<LogVec>	logVec;

	enum { LLV_NO, LLV_DATE, LLV_FNAME, LLV_SIZE, LLV_NUM, LLV_MAX };

	BOOL	Init();
	BOOL	LoadData();
	BOOL	AddLog();
	BOOL	DelLog();

public:
	TLogImportDlg(Cfg *_cfg, LogDb *_logDb, TChildView *_view);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy();
	virtual BOOL	EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight);
};

#endif


