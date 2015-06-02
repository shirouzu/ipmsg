static char *plugin_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2001   plugin.cpp	Ver1.47";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Plug-in
	Create					: 1997-09-29(Mon)
	Update					: 2001-12-06(Thu)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include "plugin.h"

#if 0
PluginMng::PluginMng(void)
{
}

PluginMng::~PluginMng(void)
{
	while (plug_cnt > 0)
		ExitDll(plug_dll[plug_cnt -1]);

	if (plug_dll)
		free(plug_dll);
}

HINSTANCE PluginMng::EntryDll(char *dllName)
{
	HINSTANCE	dll;
	BOOL		(*pPluginInit)(void);

	if ((dll = ::LoadLibrary(dllName)) == NULL)
		return	NULL;

	if ((pPluginInit = (BOOL (*)(void))::GetProcAddress(dll, "PluginInitialize")) && pPluginInit())
	{
		if ((plug_dll = (HINSTANCE *)realloc(plug_dll, sizeof(HINSTANCE) * (plug_cnt + 1))))
		{
			plug_dll[plug_cnt++] = dll;
			return	dll;
		}
	}

	::FreeLibrary(dll);
	return	NULL;
}

BOOL PluginMng::ExitDll(HINSTANCE dll)
{
	BOOL	(*pPlugTerm)(void);

	for (int cnt=0; cnt < plug_cnt; cnt++)
	{
		if (plug_dll[cnt] != dll)
			continue;
		if ((pPlugTerm = (BOOL (*)(void))::GetProcAddress(plug_dll[cnt], "PluginTermiante")))
			pPlugTerm();
		::FreeLibrary(plug_dll[cnt]);
		memmove(plug_dll + cnt, plug_dll + cnt +1, (--plug_cnt - cnt) * sizeof(HINSTANCE));
		return	TRUE;
	}
	return	FALSE;
}

#endif

TPluginDlg::TPluginDlg(Cfg *_cfg, TWin *_parent) : TDlg(PLUGIN_DIALOG, _parent)
{
	cfg = _cfg;
}

TPluginDlg::~TPluginDlg()
{
}

BOOL TPluginDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int x = rect.left + 50, y = rect.top + 50;
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	if (cfg->LogCheck)
		LogMng::StrictLogFile(cfg->LogFile);
	MoveWindow(x, y, xsize, ysize, FALSE);
	SetData();

	return	TRUE;
}

BOOL TPluginDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(TRUE);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TPluginDlg::SetData()
{
	return	TRUE;
}


BOOL TPluginDlg::GetData()
{
	return	TRUE;
}

