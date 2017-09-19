/* static char *ipipc_id = 
	"@(#)Copyright (C) 2017 H.Shirouzu		ipipc.h	Ver4.50"; */
/* ========================================================================
	Project  Name			: IP Messenger Inter-Process Communication
	Create					: 2017-05-24(Wed)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	======================================================================== */
#ifndef IPIPC_H
#define IPIPC_H

class IPIpc {
	HANDLE	hMap;

public:
	IPIpc();
	~IPIpc();

	void UnInit();

	BOOL LoadDictFromMap(HWND hWnd, BOOL is_in, IPDict *dict);
	BOOL SaveDictToMap(HWND hWnd, BOOL is_in, const IPDict &dict);
};


#endif

