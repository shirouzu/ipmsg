/*	@(#)Copyright (C) H.Shirouzu 2013-2015   setupdlg.h	Ver3.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Setup Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SETUPDLG_H
#define SETUPDLG_H

class TSetupSheet : public TDlg {
protected:
	Cfg				*cfg;
	THosts			*hosts;
	TListEx<UrlObj>	tmpUrlList;	// for sheet6
	UrlObj			*curUrl;	// for sheet

public:
	TSetupSheet() { cfg = NULL; hosts = NULL; }
	BOOL Create(int resid, Cfg *_cfg, THosts *hosts, TWin *_parent);
	BOOL SetData();
	BOOL GetData();
	BOOL CheckData();
	void ReflectDisp();
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
};

#define MAIN_SHEET     SETUP_SHEET1
#define DETAIL_SHEET   SETUP_SHEET2
#define SENDRECV_SHEET SETUP_SHEET3
#define IMAGE_SHEET    SETUP_SHEET4
#define LOG_SHEET      SETUP_SHEET5
#define AUTOSAVE_SHEET SETUP_SHEET6
#define PASSWORD_SHEET SETUP_SHEET7
#define URL_SHEET      SETUP_SHEET8
#define REMOTE_SHEET   SETUP_SHEET9
#define BACKUP_SHEET   SETUP_SHEET10
#define MAX_SETUP_SHEET	(SETUP_SHEET10 - SETUP_SHEET1 + 1)

class TSetupDlg : public TDlg {
	Cfg			*cfg;
	THosts		*hosts;
	int			curIdx;
	TSubClass	setupList;
	TSetupSheet	sheet[MAX_SETUP_SHEET];

public:
	TSetupDlg(Cfg *_cfg, THosts *hosts, TWin *_parent = NULL);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	void	SetSheet(int idx=-1);
};

#endif

