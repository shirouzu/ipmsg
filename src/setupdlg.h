/*	@(#)Copyright (C) H.Shirouzu 2013-2017   setupdlg.h	Ver4.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Setup Dialog
	Create					: 2013-03-03(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef SETUPDLG_H
#define SETUPDLG_H

#include "tipwnd.h"

class TSetupDlg;

class TSetupSheet : public TDlg {
protected:
	Cfg			*cfg;
	THosts		*hosts;
//	TListEx<UrlObj>	tmpUrlList;	// for sheet6
//	UrlObj		*curUrl;	// for sheet
	TSetupDlg	*parentDlg;
	BOOL		updateReady;

public:
	TSetupSheet() {
		cfg = NULL;
		hosts = NULL;
		parentDlg = NULL;
		/*curUrl = NULL;*/
	}
	BOOL Create(int resid, Cfg *_cfg, THosts *hosts, TSetupDlg *_parent);
	BOOL SetData();
	BOOL GetData();
	BOOL CheckData();
	void ReflectDisp();
	void ShowHelp();
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl);
	virtual BOOL	EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#define MAIN_SHEET     SETUP_SHEET1
#define DETAIL_SHEET   SETUP_SHEET2
#define SENDRECV_SHEET SETUP_SHEET3
#define IMAGE_SHEET    SETUP_SHEET4
#define LINK_SHEET     SETUP_SHEET5
#define LOG_SHEET      SETUP_SHEET6
#define AUTOSAVE_SHEET SETUP_SHEET7
#define REMOTE_SHEET   SETUP_SHEET8
#define BACKUP_SHEET   SETUP_SHEET9
#define SERVER_SHEET   SETUP_SHEET10
#define DIRMODE_SHEET  SETUP_SHEET11
#define LABTEST_SHEET  SETUP_SHEET12
#define UPDATE_SHEET   SETUP_SHEET13
#define PASSWORD_SHEET SETUP_SHEET18
#define URL_SHEET      SETUP_SHEET19

class TSetupDlg : public TDlg {
	Cfg			*cfg;
	THosts		*hosts;
	int			curIdx;
	TSubClass	setupList;
	TSetupSheet	*sheet;
	std::vector<int>	idVec;	// idx to sheet_id
	std::map<int, int>	idxMap;	// sheet_id to idx;
	BOOL		isFirstMode;
	TTipWnd		tipWnd;
//	BOOL		FirstMode();
	void		SetSheetIdx(int idx=-1);

public:
	TSetupDlg(Cfg *_cfg, THosts *hosts, BOOL is_first, TWin *_parent = NULL);
	virtual ~TSetupDlg();
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
	virtual BOOL	EvCreate(LPARAM lParam);
	void		SetSheet(int sheet_id);
	TSetupSheet	*GetSheet(int sheet_id);
};

#endif

