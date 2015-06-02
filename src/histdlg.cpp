static char *histdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 2011-2015   histdlg.cpp	Ver3.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: History Dialog
	Create					: 2011-07-24(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"

#define MAX_HISTHASH	1001
HistHash::HistHash() : THashTbl(MAX_HISTHASH)
{
	top = end = lruTop = lruEnd = NULL;
}

HistHash::~HistHash()
{
}

BOOL HistHash::IsSameVal(THashObj *obj, const void *val)
{
	return	strcmp(((HistObj *)obj)->info, (const char *)val) == 0;
}

void HistHash::Clear()
{
	UnInit();
	Init(MAX_HISTHASH);
	top = end = lruTop = lruEnd = NULL;
}

void HistHash::Register(THashObj *_obj, u_int hash_id)
{
	HistObj	*obj = (HistObj *)_obj;
	THashTbl::Register(obj, hash_id);
	if (top) {
		obj->next = top;
		obj->prev = NULL;
		top->prev = obj;
		top = obj;
	}
	else {
		top = end = obj;
		obj->next = obj->prev = NULL;
	}
}

void HistHash::RegisterLru(HistObj *obj)
{
	if (lruTop) {
		obj->lruNext = lruTop;
		obj->lruPrev = NULL;
		lruTop->lruPrev = obj;
		lruTop = obj;
	}
	else {
		lruTop = lruEnd = obj;
		obj->lruNext = obj->lruPrev = NULL;
	}
}

void HistHash::UnRegister(THashObj *_obj)
{
	HistObj	*obj = (HistObj *)_obj;

	if (obj->next) {
		obj->next->prev = obj->prev;
	}
	if (obj == top) {
		top = obj->next;
	}
	if (obj->prev) {
		obj->prev->next = obj->next;
	}
	if (obj == end) {
		end = obj->prev;
	}
	obj->next = obj->prev = NULL;

	UnRegisterLru(obj);

	THashTbl::UnRegister(obj);
}

void HistHash::UnRegisterLru(HistObj *obj)
{
	if (obj->lruNext) {
		obj->lruNext->lruPrev = obj->lruPrev;
	}
	if (obj == lruTop) {
		lruTop = obj->lruNext;
	}
	if (obj->lruPrev) {
		obj->lruPrev->lruNext = obj->lruNext;
	}
	if (obj == lruEnd) {
		lruEnd = obj->lruPrev;
	}
	obj->lruNext = obj->lruPrev = NULL;
}


/*
	THistDlg
*/
THistDlg::THistDlg(Cfg *_cfg, THosts *_hosts, TWin *_parent) : TDlg(HISTORY_DIALOG, _parent), histListView(this), histListHeader(&histListView)
{
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	hosts = _hosts;
	hListFont = NULL;
	openedMode = FALSE;
	unOpenedNum = 0;
}

/*
	終了ルーチン
*/
THistDlg::~THistDlg()
{
	histListView.DeleteAllItems();
	if (hListFont) {
		::DeleteObject(hListFont);
		hListFont = NULL;
	}
}

/*
*/
BOOL THistDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	SetDlgItem(HISTORY_LIST, XY_FIT);
	SetDlgItem(IDOK, HMID_FIT);
	SetDlgItem(OPENED_CHECK, LEFT_FIT);
	SetDlgItem(CLEAR_BUTTON, RIGHT_FIT);

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		orgRect = rect;

		int xsize = rect.right - rect.left + cfg->HistXdiff;
		int ysize = rect.bottom - rect.top + cfg->HistYdiff;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN);
		int	cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;
		MoveWindow((x < 0) ? 0 : x, (y < 0) ? 0 : y, xsize, ysize, FALSE);
	}
	else {
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	}

	FitDlgItems();
	CheckDlgButton(OPENED_CHECK, openedMode);
	SetTitle();

	return	TRUE;
}

BOOL THistDlg::EvDestroy()
{
	if (!::IsIconic(hWnd)) GetWindowRect(&rect);

	cfg->HistXdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	cfg->HistYdiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

	SaveColumnInfo();

	cfg->WriteRegistry(CFG_WINSIZE);
	return	FALSE;
}

void THistDlg::SaveColumnInfo()
{
	int	col_num = MAX_HISTWIDTH - (openedMode ? 0 : 1);

	for (int i=0; i < col_num; i++) {
		cfg->HistWidth[i] = histListView.GetColumnWidth(i);
	}
}

/*
	WM_COMMAND CallBack
*/
BOOL THistDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case OPENED_CHECK:
		SaveColumnInfo();
		openedMode = !openedMode;
		SetAllData();
		return	TRUE;

	case CLEAR_BUTTON:
		histListView.DeleteAllItems();
		if (openedMode) {
			while (histHash.LruTop()) histHash.UnRegister(histHash.LruTop()); // !UnRegisterLRU()
		}
		else {
			for (HistObj *obj=histHash.Top(); obj; ) {
				HistObj *next = obj->next;
				if (!*obj->odate) histHash.UnRegister(obj);
				obj = next;
			}
			unOpenedNum = 0;
		}
		SetTitle();
		return	TRUE;
	}
	return	FALSE;
}

BOOL THistDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (histListView.hWnd) {	// EvCreate 中に通ると Userリソースリーク？
		if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED) {
			return	FitDlgItems();
		}
	}
	return	FALSE;;
}

/*
	Window生成ルーチン
*/
BOOL THistDlg::Create(HINSTANCE hI)
{
	if (!TDlg::Create(hI)) return	FALSE;

	histListView.AttachWnd(GetDlgItem(HISTORY_LIST));
	histListHeader.AttachWnd((HWND)histListView.SendMessage(LVM_GETHEADER, 0, 0));

	SetFont();
	SetHeader(); // dummy for reflect font
	SetAllData();

	return	TRUE;
}

void THistDlg::SetTitle()
{
	SetWindowTextU8(Fmt(GetLoadStrU8(openedMode ? IDS_OPENINFO : IDS_UNOPENINFO),
						openedMode ? (histHash.GetRegisterNum() - unOpenedNum) : unOpenedNum,
						histHash.GetRegisterNum()));
}

void THistDlg::SetHeader()
{
	int	title_id[] = { IDS_HISTUSER, IDS_HISTODATE, IDS_HISTSDATE, IDS_HISTID };
	int	i, offset = 0;
	int	col_num = MAX_HISTWIDTH - (openedMode ? 0 : 1);

	for (i=0; i < MAX_HISTWIDTH; i++) {
		histListView.DeleteColumn(0);
	}

	for (i=0; i < MAX_HISTWIDTH; i++) {
		if (!openedMode && i == HW_ODATE) {
			offset = 1;
			continue;
		}
		histListView.InsertColumn(i-offset, GetLoadStrU8(title_id[i]), cfg->HistWidth[i-offset]);
	}
}

void THistDlg::SetData(HistObj *obj)
{
	histListView.InsertItem(0, obj->user, (LPARAM)obj);
	if (openedMode) histListView.SetSubItem(0, HW_ODATE, obj->odate);
	histListView.SetSubItem(0, HW_SDATE - (openedMode ? 0 : 1), obj->sdate);
	histListView.SetSubItem(0, HW_ID    - (openedMode ? 0 : 1), obj->pktno);
}

void THistDlg::SetAllData()
{
	histListView.DeleteAllItems();
	SetHeader();
	SetTitle();

	if (openedMode) {
		for (HistObj *obj = histHash.LruEnd(); obj; obj = obj->lruPrev) {
			SetData(obj);
		}
	}
	else {
		for (HistObj *obj = histHash.End(); obj; obj = obj->prev) {
			if (!*obj->odate) SetData(obj);
		}
	}
}

int THistDlg::MakeHistInfo(HostSub *hostSub, ULONG packet_no, char *buf)
{
	return	sprintf(buf, "%s:%s:%d", hostSub->userName, hostSub->hostName, packet_no);
}

void THistDlg::SendNotify(HostSub *hostSub, ULONG packetNo)
{
#define MAX_OPENHISTORY 500
	int num = histHash.GetRegisterNum();
	if (num >= MAX_OPENHISTORY) {
		if (HistObj *obj = histHash.End()) {
			if (hWnd) histListView.DeleteItem(num-1);
			histHash.UnRegister(obj);
			if (!*obj->odate) unOpenedNum--;
			delete obj;
			num--;
		}
	}

	HistObj	*obj = new HistObj();
	int		len = MakeHistInfo(hostSub, packetNo, obj->info);

	histHash.Register(obj, MakeHash(obj->info, len, 0));

	MakeListString(cfg, hostSub, hosts, obj->user);
	SYSTEMTIME	st;
	::GetLocalTime(&st);
	sprintf(obj->sdate, "%02d/%02d %02d:%02d", st.wMonth, st.wDay, st.wHour, st.wMinute);
	sprintf(obj->pktno, "%x", packetNo);

	unOpenedNum++;

	if (hWnd) {
		if (!openedMode) {
			SetData(obj);
		}
		SetTitle();
	}
}

void THistDlg::OpenNotify(HostSub *hostSub, ULONG packetNo, char *notify)
{
	char	buf[MAX_BUF];
	int		len;
	u_int	hash_val;
	HistObj	*obj;
	int		idx;

	len = MakeHistInfo(hostSub, packetNo, buf);
	hash_val =  MakeHash(buf, len, 0);
	if (!(obj = (HistObj *)histHash.Search(buf, hash_val))) {
		SendNotify(hostSub, packetNo);
		if (!(obj = (HistObj *)histHash.Search(buf, hash_val))) {
			return;
		}
		sprintf(obj->sdate, GetLoadStrU8(IDS_UNKNOWN));
	}

	if (*obj->odate) return;

	SYSTEMTIME	st;
	::GetLocalTime(&st);
	sprintf(obj->odate, "%02d/%02d %02d:%02d", st.wMonth, st.wDay, st.wHour, st.wMinute);
	if (--unOpenedNum < 0) unOpenedNum = 0;
	histHash.RegisterLru(obj);

	if (notify) {
		strncpyz(obj->sdate, obj->odate, sizeof(obj->sdate));
		strncpyz(obj->odate, notify, sizeof(obj->odate));
	}

	if (hWnd) {
		if (openedMode) {
			SetData(obj);
		}
		else {
			if ((idx = histListView.FindItem((LPARAM)obj)) >= 0) {
				histListView.DeleteItem(idx);
			}
		}
		SetTitle();
	}
}

void THistDlg::SetFont()
{
	HFONT	hDlgFont;

	if (*cfg->SendListFont.lfFaceName && (hDlgFont = ::CreateFontIndirect(&cfg->SendListFont)))
	{
		histListView.SendMessage(WM_SETFONT, (WPARAM)hDlgFont, 0);
		if (hListFont)
			::DeleteObject(hListFont);
		hListFont = hDlgFont;
 	}
	histListHeader.ChangeFontNotify();
}

void THistDlg::Show(int mode)
{
	TDlg::Show(mode);
}

