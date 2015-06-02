/*	@(#)Copyright (C) H.Shirouzu 2005   ipmsg.h	Ver2.50 */
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Main Header
	Create					: 2005-05-29(Sun)
	Update					: 2005-05-29(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#ifndef PLUGIN_H
#define PLUGIN_H

class Plugin : public TListObj {
public:
	Plugin(char *dllname);
	HINSTANCE EntryDll(char *dllName);
	BOOL ExitDll(HINSTANCE dll);
};

class PluginMng {
protected:
	TList	plugin;

public:
	PluginMng(void);
	~PluginMng();
};


class TPluginDlg : public TDlg {
protected:
	Cfg		*cfg;
	BOOL	SetData();
	BOOL	GetData();

public:
	TPluginDlg(Cfg *_cfg, TWin *_parent = NULL);
	virtual ~TPluginDlg();

	virtual BOOL	EvCreate(LPARAM lParam);
//	virtual BOOL	EvNcDestroy(void);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
//	virtual BOOL	EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif

