static char *miscdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   miscdlg.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc Dialog
	Create					: 1996-12-15(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"

int TMsgDlg::createCnt = 0;

/*
	listview control の subclass化
	Focus を失ったときにも、選択色を変化させないための小細工
*/
#define INVALID_INDEX	-1
TListViewEx::TListViewEx(TWin *_parent) : TSubClassCtl(_parent)
{
	focusIndex = INVALID_INDEX;
	letterKey  = TRUE; // select item by first letter key
}

BOOL TListViewEx::AttachWnd(HWND targetWnd, DWORD addStyle)
{
	if (!TSubClass::AttachWnd(targetWnd)) return FALSE;

	LONG_PTR dw = GetWindowLong(GWL_STYLE) | LVS_SHOWSELALWAYS;
	SetWindowLong(GWL_STYLE, dw);

	LRESULT style = SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
	style |= addStyle;
	SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style);
	return TRUE;
}

BOOL TListViewEx::EventFocus(UINT uMsg, HWND hFocusWnd)
{
	LV_ITEM	lvi;

	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED;
	int	maxItem = (int)SendMessage(LVM_GETITEMCOUNT, 0, 0);
	int itemNo;

	for (itemNo=0; itemNo < maxItem; itemNo++) {
		if (SendMessage(LVM_GETITEMSTATE, itemNo, (LPARAM)LVIS_FOCUSED) & LVIS_FOCUSED)
			break;
	}

	if (uMsg == WM_SETFOCUS)
	{
		if (itemNo == maxItem) {
			lvi.state = LVIS_FOCUSED;
			if (focusIndex == INVALID_INDEX)
				focusIndex = 0;
			SendMessage(LVM_SETITEMSTATE, focusIndex, (LPARAM)&lvi);
			SendMessage(LVM_SETSELECTIONMARK, 0, focusIndex);
		}
		return	FALSE;
	}
	else {	// WM_KILLFOCUS
		if (itemNo != maxItem) {
			SendMessage(LVM_SETITEMSTATE, itemNo, (LPARAM)&lvi);
			focusIndex = itemNo;
		}
		return	TRUE;	// WM_KILLFOCUS は伝えない
	}
}

BOOL TListViewEx::EventKey(UINT uMsg, int nVirtKey, LONG lKeyData)
{
	return	FALSE;
}

BOOL TListViewEx::EvChar(WCHAR code, LPARAM keyData)
{
	if (!letterKey) {
		if (code >= 'A' && code <= 'Z' ||
			code >= 'a' && code <= 'z' ||
			code >= '0' && code <= '9') {
			if (!(GetKeyState(VK_CONTROL) & 0x8000) &&
				!(GetKeyState(VK_MENU)    & 0x8000)) {
				return TRUE;
			}
		}
	}
	return	FALSE;
}

BOOL TListViewEx::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_RBUTTONDOWN:
		return	TRUE;
	case WM_RBUTTONUP:
		::PostMessage(parent->hWnd, uMsg, nHitTest, *(LPARAM *)&pos);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListViewEx::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (focusIndex == INVALID_INDEX)
		return	FALSE;

	switch (uMsg) {
	case LVM_INSERTITEM:
		if (((LV_ITEM *)lParam)->iItem <= focusIndex)
			focusIndex++;
		break;
	case LVM_DELETEITEM:
		if ((int)wParam == focusIndex)
			focusIndex = INVALID_INDEX;
		else if ((int)wParam < focusIndex)
			focusIndex--;
		break;
	case LVM_DELETEALLITEMS:
		focusIndex = INVALID_INDEX;
		break;
	}
	return	FALSE;
}

int TListViewEx::InsertColumn(int idx, const char *title, int width, int fmt)
{
	LV_COLUMNW	lvC;
	memset(&lvC, 0, sizeof(lvC));
	lvC.fmt      = fmt;
	lvC.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.pszText  = U8toWs(title);
	lvC.cx       = width;
	lvC.iSubItem = idx;
	return	(int)SendMessage(LVM_INSERTCOLUMNW, idx, (LPARAM)&lvC);
}

BOOL TListViewEx::DeleteColumn(int idx)
{
	return	(BOOL)SendMessage(LVM_DELETECOLUMN, idx, 0);
}

int TListViewEx::GetColumnWidth(int idx)
{
	return	(int)SendMessage(LVM_GETCOLUMNWIDTH, idx, 0);
}

void TListViewEx::DeleteAllItems()
{
	SendMessage(LVM_DELETEALLITEMS, 0, 0);
}

BOOL TListViewEx::DeleteItem(int idx)
{
	return	(BOOL)SendMessage(LVM_DELETEITEM, idx, 0);
}

int TListViewEx::InsertItem(int idx, const char *str, LPARAM lParam)
{
	LV_ITEMW		lvi = {};
	lvi.mask		= LVIF_TEXT|LVIF_PARAM;
	lvi.iItem		= idx;
	lvi.pszText		= U8toWs(str);
	lvi.lParam		= lParam;
	return	(int)SendMessage(LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

int TListViewEx::SetSubItem(int idx, int subIdx, const char *str)
{
	LV_ITEMW		lvi = {};
	lvi.mask		= LVIF_TEXT;
	lvi.iItem		= idx;
	lvi.iSubItem	= subIdx;
	lvi.pszText		= U8toWs(str);
	return	(int)SendMessage(LVM_SETITEMW, 0, (LPARAM)&lvi);
}

int TListViewEx::FindItem(LPARAM lParam)
{
	LV_FINDINFO	lfi;
	lfi.flags	= LVFI_PARAM;
	lfi.lParam	= lParam;
	return	(int)SendMessage(LVM_FINDITEM, -1, (LPARAM)&lfi);
}

int TListViewEx::GetItemCount()
{
	return	(int)SendMessage(LVM_GETITEMCOUNT, 0, 0);
}

int TListViewEx::GetSelectedItemCount()
{
	return	(int)SendMessage(LVM_GETSELECTEDCOUNT, 0, 0);
}

LPARAM TListViewEx::GetItemParam(int idx)
{
	LV_ITEMW	lvi = {};
	lvi.iItem	= idx;
	lvi.mask	= LVIF_PARAM;

	if (!SendMessage(LVM_GETITEMW, 0, (LPARAM)&lvi)) return	0;
	return	lvi.lParam;
}

BOOL TListViewEx::GetColumnOrder(int *order, int num)
{
	return	(int)SendMessage(LVM_GETCOLUMNORDERARRAY, num, (LPARAM)order);
}

BOOL TListViewEx::SetColumnOrder(int *order, int num)
{
	return	(int)SendMessage(LVM_SETCOLUMNORDERARRAY, num, (LPARAM)order);
}

BOOL TListViewEx::IsSelected(int idx)
{
	return	(SendMessage(LVM_GETITEMSTATE, idx, LVIS_SELECTED) & LVIS_SELECTED) ? TRUE : FALSE;
}


TListHeader::TListHeader(TWin *_parent) : TSubClassCtl(_parent)
{
	memset(&logFont, 0, sizeof(logFont));
}

BOOL TListHeader::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == HDM_LAYOUT) {
		HD_LAYOUT *hl = (HD_LAYOUT *)lParam;
		::CallWindowProcW((WNDPROC)oldProc, hWnd, uMsg, wParam, lParam);

//		Debug("HDM_LAYOUT(USER)2 top:%d/bottom:%d diff:%d cy:%d y:%d\n", hl->prc->top, hl->prc->bottom, hl->prc->bottom - hl->prc->top, hl->pwpos->cy, hl->pwpos->y);

		if (logFont.lfHeight) {
			int	height = abs(logFont.lfHeight) + 4;
			hl->prc->top = height;
			hl->pwpos->cy = height;
		}
		return	TRUE;
	}
	return	FALSE;
}

BOOL TListHeader::ChangeFontNotify()
{
	HFONT	hFont;

	if (!(hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0)))
		return	FALSE;

	if (::GetObject(hFont, sizeof(LOGFONT), (void *)&logFont) == 0)
		return	FALSE;

//	Debug("lfHeight=%d\n", logFont.lfHeight);
	return	TRUE;
}


/*
	不在通知文設定ダイアログ
*/
extern char	*DefaultAbsence[], *DefaultAbsenceHead[];

TAbsenceDlg::TAbsenceDlg(Cfg *_cfg, TWin *_parent) : TDlg(ABSENCE_DIALOG, _parent)
{
	cfg = _cfg;
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
}

TAbsenceDlg::~TAbsenceDlg()
{
}

BOOL TAbsenceDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	if (rect.left == CW_USEDEFAULT)
	{
		DWORD	val = GetMessagePos();
		POINTS	pos = MAKEPOINTS(val);

		GetWindowRect(&rect);
		int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int x = pos.x - xsize / 2, y = pos.y - ysize;

		if (x + xsize > cx)
			x = cx - xsize;
		if (y + ysize > cy)
			y = cy - ysize;

		MoveWindow(x > 0 ? x : 0, y > 0 ? y : 0, xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	typedef char MaxBuf[MAX_PATH_U8];
	typedef char MaxHead[MAX_NAMEBUF];
	tmpAbsenceStr = new MaxBuf[cfg->AbsenceMax];
	tmpAbsenceHead = new MaxHead[cfg->AbsenceMax];

	SetData();
	return	TRUE;
}

BOOL TAbsenceDlg::EvNcDestroy(void)
{
	delete [] tmpAbsenceHead;
	delete [] tmpAbsenceStr;
	return	TRUE;
}

BOOL TAbsenceDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetData();
		cfg->WriteRegistry(CFG_ABSENCE);
		cfg->AbsenceCheck = FALSE;
		::PostMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		EndDialog(wID);
		return	TRUE;

	case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_ABSENCE);
		if (cfg->AbsenceCheck)
		{
			cfg->AbsenceCheck = FALSE;
			::PostMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		}
		EndDialog(wID);
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case ABSENCEINIT_BUTTON:
		SetDlgItemTextU8(ABSENCEHEAD_EDIT, DefaultAbsenceHead[currentChoice]);
		SetDlgItemTextU8(ABSENCE_EDIT, DefaultAbsence[currentChoice]);
		return	TRUE;

	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_HIDE_CHILDWIN, 0, 0);
		return	TRUE;

	case ABSENCE_LIST:
		if (wNotifyCode == LBN_SELCHANGE)
		{
			int		index;

			if ((index = (int)SendDlgItemMessage(ABSENCE_LIST, LB_GETCURSEL, 0, 0)) != LB_ERR)
			{
				char	oldAbsenceHead[MAX_NAMEBUF];
				strcpy(oldAbsenceHead, tmpAbsenceHead[currentChoice]);
				GetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice], MAX_NAMEBUF);
				GetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice], MAX_PATH_U8);
				if (strcmp(oldAbsenceHead, tmpAbsenceHead[currentChoice]))
				{
					SendDlgItemMessage(ABSENCE_LIST, LB_DELETESTRING, currentChoice, 0);
					Wstr	head_w(tmpAbsenceHead[currentChoice], BY_UTF8);
					SendDlgItemMessageW(ABSENCE_LIST, LB_INSERTSTRING, currentChoice, (LPARAM)head_w.Buf());
					if (currentChoice == index)
					{
						SendDlgItemMessage(ABSENCE_LIST, LB_SETCURSEL, currentChoice, 0);
						return	TRUE;
					}
				}
				currentChoice = index;
				PostMessage(WM_DELAYSETTEXT, 0, 0);
			}
		}
		else if (wNotifyCode == LBN_DBLCLK)
			PostMessage(WM_COMMAND, IDOK, 0);
		return	TRUE;
	}

	return	FALSE;
}

/*
	App定義 Event CallBack
*/
BOOL TAbsenceDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DELAYSETTEXT:
		{
			int		len = (int)strlen(tmpAbsenceStr[currentChoice]);

			SetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice]);
			SetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice]);
			SendDlgItemMessage(ABSENCE_EDIT, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		}
		return	TRUE;
	}

	return	FALSE;
}

void TAbsenceDlg::SetData(void)
{
	for (int i=0; i < cfg->AbsenceMax; i++)
	{
		strcpy(tmpAbsenceHead[i], cfg->AbsenceHead[i]);
		strcpy(tmpAbsenceStr[i], cfg->AbsenceStr[i]);
		Wstr	head_w(cfg->AbsenceHead[i], BY_UTF8);
		SendDlgItemMessageW(ABSENCE_LIST, LB_INSERTSTRING, (WPARAM)-1, (LPARAM)head_w.Buf());
	}
	currentChoice = cfg->AbsenceChoice;
	PostMessage(WM_DELAYSETTEXT, 0, 0);
	SendDlgItemMessage(ABSENCE_LIST, LB_SETCURSEL, currentChoice, 0);
}

void TAbsenceDlg::GetData(void)
{
	GetDlgItemTextU8(ABSENCEHEAD_EDIT, tmpAbsenceHead[currentChoice], MAX_NAMEBUF);
	GetDlgItemTextU8(ABSENCE_EDIT, tmpAbsenceStr[currentChoice], MAX_PATH_U8);
	for (int i=0; i < cfg->AbsenceMax; i++)
	{
		strcpy(cfg->AbsenceHead[i], tmpAbsenceHead[i]);
		strcpy(cfg->AbsenceStr[i], tmpAbsenceStr[i]);
	}
	cfg->AbsenceChoice = currentChoice;
}


/*
	ソート設定ダイアログ
*/
TSortDlg *TSortDlg::exclusiveWnd = NULL;

TSortDlg::TSortDlg(Cfg *_cfg, TWin *_parent) : TDlg(SORT_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
}

int TSortDlg::Exec(void)
{
	if (exclusiveWnd) {
		return	exclusiveWnd->SetForegroundWindow(), FALSE;
	}
	exclusiveWnd = this;
	return	TDlg::Exec();
}

BOOL TSortDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK: case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_GENERAL);
		if (wID == IDOK)
			EndDialog(wID), exclusiveWnd = NULL;
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		exclusiveWnd = NULL;
		return	TRUE;
	}

	return	FALSE;
}

BOOL TSortDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	int cx = ::GetSystemMetrics(SM_CXFULLSCREEN), cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
	int x = rect.left + 50, y = rect.top + 20;
	int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	MoveWindow(x, y, xsize, ysize, FALSE);
	SetData();

	return	TRUE;
}

void TSortDlg::SetData(void)
{
	CheckDlgButton(GROUPDISP_CHECK, GetItem(cfg->ColumnItems, SW_GROUP));
	CheckDlgButton(HOST_CHECK, GetItem(cfg->ColumnItems, SW_HOST));
	CheckDlgButton(LOGON_CHECK, GetItem(cfg->ColumnItems, SW_USER));
	CheckDlgButton(PRIORITY_CHECK, GetItem(cfg->ColumnItems, SW_PRIORITY));
	CheckDlgButton(IPADDR_CHECK, GetItem(cfg->ColumnItems, SW_IPADDR));
	CheckDlgButton(GRIDLINE_CHECK, cfg->GridLineCheck);

	CheckDlgButton(GROUP_CHECK, !(cfg->Sort & IPMSG_NOGROUPSORTOPT));
	CheckDlgButton(GROUPREV_CHECK, (cfg->Sort & IPMSG_GROUPREVSORTOPT) != 0);
	CheckDlgButton(USER_RADIO, GET_MODE(cfg->Sort) == IPMSG_NAMESORT);
	CheckDlgButton(HOST_RADIO, GET_MODE(cfg->Sort) == IPMSG_HOSTSORT);
	CheckDlgButton(IPADDR_RADIO, GET_MODE(cfg->Sort) == IPMSG_IPADDRSORT);
	CheckDlgButton(SUBREV_CHECK, (cfg->Sort & IPMSG_SUBREVSORTOPT) != 0);
	CheckDlgButton(ICMP_CHECK, (cfg->Sort & IPMSG_ICMPSORTOPT) != 0);
	CheckDlgButton(KANJI_CHECK, !(cfg->Sort & IPMSG_NOKANJISORTOPT));
}

void TSortDlg::GetData(void)
{
	SetItem(&cfg->ColumnItems, SW_GROUP, (BOOL)SendDlgItemMessage(GROUPDISP_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_HOST, (BOOL)SendDlgItemMessage(HOST_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_USER, (BOOL)SendDlgItemMessage(LOGON_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_PRIORITY, (BOOL)SendDlgItemMessage(PRIORITY_CHECK, BM_GETCHECK, 0, 0));
	SetItem(&cfg->ColumnItems, SW_IPADDR, (BOOL)SendDlgItemMessage(IPADDR_CHECK, BM_GETCHECK, 0, 0));
	cfg->GridLineCheck = (BOOL)SendDlgItemMessage(GRIDLINE_CHECK, BM_GETCHECK, 0, 0);

	cfg->Sort = 0;

	if (SendDlgItemMessage(GROUP_CHECK, BM_GETCHECK, 0, 0) == 0)
		cfg->Sort |= IPMSG_NOGROUPSORTOPT;
	if (SendDlgItemMessage(GROUPREV_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_GROUPREVSORTOPT;
	if (SendDlgItemMessage(USER_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_NAMESORT;
	if (SendDlgItemMessage(HOST_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_HOSTSORT;
	if (SendDlgItemMessage(IPADDR_RADIO, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_IPADDRSORT;
	if (SendDlgItemMessage(SUBREV_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_SUBREVSORTOPT;
	if (SendDlgItemMessage(ICMP_CHECK, BM_GETCHECK, 0, 0))
		cfg->Sort |= IPMSG_ICMPSORTOPT;
	if (SendDlgItemMessage(KANJI_CHECK, BM_GETCHECK, 0, 0) == 0)
		cfg->Sort |= IPMSG_NOKANJISORTOPT;
}

// パスワード確認用
TPasswordDlg::TPasswordDlg(Cfg *_cfg, TWin *_parent) : TDlg(PASSWORD_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	outbuf = NULL;
}

// 単なるパスワード入力用
TPasswordDlg::TPasswordDlg(char *_outbuf, TWin *_parent) : TDlg(PASSWORD_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = NULL;
	outbuf = _outbuf;
}

BOOL TPasswordDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		{
			char	_buf[MAX_NAMEBUF];
			char	*buf = outbuf ? outbuf : _buf;

			GetDlgItemTextU8(PASSWORD_EDIT, buf, MAX_NAMEBUF);
			if (cfg)
			{
				if (CheckPassword(cfg->PasswordStr, buf))
					EndDialog(wID);
				else
					SetDlgItemTextU8(PASSWORD_EDIT, ""), MessageBoxU8(LoadStrU8(IDS_CANTAUTH));
			}
			else EndDialog(wID);
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TPasswordDlg::EvCreate(LPARAM lParam)
{
	GetWindowRect(&rect);
	if (parent)
		MoveWindow(rect.left +100, rect.top +100, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	else
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	x = (::GetSystemMetrics(SM_CXFULLSCREEN) - xsize)/2;
		int y = (::GetSystemMetrics(SM_CYFULLSCREEN) - ysize)/2;
		MoveWindow(x, y, xsize, ysize, FALSE);
	}

	return	TRUE;
}


/*
	TMsgDlgは、制約の多いMessageBoxU8()、NonModal用として作成
*/
TMsgDlg::TMsgDlg(TWin *_parent) : TListDlg(MESSAGE_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
}

/*
	終了ルーチン
*/
TMsgDlg::~TMsgDlg()
{
	createCnt--;
}

/*
	Window生成ルーチン
*/
BOOL TMsgDlg::Create(const char *text, const char *title, int _showMode)
{
	showMode = _showMode;

	if (!TDlg::Create()) return	FALSE;

	SetDlgItemTextU8(MESSAGE_TEXT, text);
	SetWindowTextU8(title);
	if (showMode == SW_SHOW) {
		DWORD	key =	GetAsyncKeyState(VK_LBUTTON) |
						GetAsyncKeyState(VK_RBUTTON) |
						GetAsyncKeyState(VK_MBUTTON);
		EnableWindow(TRUE);
		ShowWindow((key & 0x8000) ? SW_SHOWNOACTIVATE : SW_SHOW);
	}
	else {
		ShowWindow(SW_SHOWMINNOACTIVE);
		EnableWindow(TRUE);
	}

	if (strstr(text, "\r\n"))
	{
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		::GetWindowPlacement(GetDlgItem(MESSAGE_TEXT), &wp);
		wp.rcNormalPosition.top -= ::GetSystemMetrics(SM_CYCAPTION) / 4;
		::SetWindowPlacement(GetDlgItem(MESSAGE_TEXT), &wp);
	}
	return	TRUE;
}

/*
	WM_COMMAND CallBack
*/
BOOL TMsgDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
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
		::PostMessage(GetMainWnd(), WM_MSGDLG_EXIT, 0, twinId);
		return	TRUE;
	}
	return	FALSE;
}

/*
	Window 生成時の CallBack。Screenの中心を起点として、すでに開いている
	TMsgDlgの枚数 * Caption Sizeを ずらして表示
*/
BOOL TMsgDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN);
		int	cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2 + createCnt * ::GetSystemMetrics(SM_CYCAPTION);
		int y = (cy - ysize)/2 + createCnt * ::GetSystemMetrics(SM_CYCAPTION);

		MoveWindow((x < 0) ? 0 : x, (y < 0) ? 0 : y, xsize, ysize,
					FALSE);
	}

	return	TRUE;
}

/*
	TMsgBoxは位置決め可能な MessageBox()
*/
TMsgBox::TMsgBox(TWin *_parent) : TDlg(MESSAGE_BOX, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);

	flags = 0;
}

/*
	Window生成ルーチン
*/
int TMsgBox::Exec(const char *_text, const char *_title, int _x, int _y)
{
	text  = _text;
	title = _title;

	x = _x;
	y = _y;

	return	TDlg::Exec();
}

int TMsgBox::Exec(const char *_text, const char *_title)
{
	text  = _text;
	title = _title;
	x = CW_USEDEFAULT;
	y = CW_USEDEFAULT;

	return	TDlg::Exec();
}

BOOL TMsgBox::Create(const char *_text, const char *_title)
{
	text  = _text;
	title = _title;
	x = CW_USEDEFAULT;
	y = CW_USEDEFAULT;

	return	TDlg::Create();
}

void TMsgBox::Setup()
{
	SetDlgItemTextU8(MESSAGE_EDIT, text);
	SetWindowTextU8(title);

	if (strstr(text, "\r\n"))
	{
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		::GetWindowPlacement(GetDlgItem(MESSAGE_EDIT), &wp);
		wp.rcNormalPosition.top -= ::GetSystemMetrics(SM_CYCAPTION) / 4;
		::SetWindowPlacement(GetDlgItem(MESSAGE_EDIT), &wp);
	}

	::ShowWindow(GetDlgItem(IDCANCEL), (flags & NOCANCEL) ? SW_HIDE : SW_SHOW);
}

/*
	WM_COMMAND CallBack
*/
BOOL TMsgBox::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;
	}
	return	FALSE;
}

/*
	Window 生成時の CallBack。Screenの中心を起点として、すでに開いている
	TMsgBoxの枚数 * Caption Sizeを ずらして表示
*/
BOOL TMsgBox::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	Setup();

	SendDlgItemMessage(MESSAGE_EDIT, EM_SETBKGNDCOLOR, FALSE, ::GetSysColor(COLOR_3DFACE));
	SendDlgItemMessage(MESSAGE_EDIT, EM_SETTARGETDEVICE, 0, 0);

	GetWindowRect(&rect);

	if (flags & RETRY) {
		SendDlgItemMessageW(IDOK, WM_SETTEXT, 0, (LPARAM)LoadStrW(IDS_RETRY));
	}

	if (flags & (DBLX|BIGX)) {
		MoveWindow(rect.x()-rect.cx(), rect.y(), int(rect.cx()*((flags & DBLX) ? 2.0 : 1.2)),
			rect.cy(), FALSE);
		GetWindowRect(&rect);
		FitMoveWindow(rect.x(), rect.y());
		GetWindowRect(&rect);
	}

	POINT	pt = { x, y };

	if ((flags & CENTER)) {
		MoveCenter(TRUE);
	}
	else {
		GetCursorPos(&pt);
		rect.Slide(pt.x - rect.x(), pt.y - rect.y() + 5);

		MoveWindow(rect.x(), rect.y(), rect.cx(), rect.cy(), TRUE);
		FitMoveWindow(rect.x(), rect.y());
	}

	SetDlgItem(MESSAGE_EDIT, XY_FIT);
	SetDlgItem(IDOK, BOTTOM_FIT|MIDX_FIT);
	if ((flags & NOCANCEL) == 0) {
		SetDlgItem(IDCANCEL, BOTTOM_FIT|RIGHT_FIT);
	}

	FitDlgItems();

	return	TRUE;
}

BOOL TMsgBox::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType == SIZE_RESTORED || fwSizeType == SIZE_MAXIMIZED) {
		return	FitDlgItems();
	}
	return	FALSE;;
}


/*
	About Dialog初期化処理
*/
TAboutDlg::TAboutDlg(Cfg *_cfg, TWin *_parent) : TDlg(ABOUT_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
}

/*
	Window 生成時の CallBack
*/
BOOL TAboutDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	char	buf[512];
	char	buf2[512];

	GetDlgItemTextU8(ABOUT_TEXT, buf, sizeof(buf));
	snprintfz(buf2, sizeof(buf2), buf, GetVersionStr());
	SetDlgItemTextU8(ABOUT_TEXT, buf2);

	if (cfg->NoTcp || cfg->NoFileTrans) {
		GetWindowTextU8(buf, sizeof(buf));
		strcat(buf, cfg->NoFileTrans == 2 ? " (No Share Transfer)" : " (No File Transfer)");
		SetWindowTextU8(buf);
	}

#ifdef IPMSG_PRO
	::ShowWindow(GetDlgItem(IPMSGUPD_BTN), SW_HIDE);
#endif

	if (rect.left == CW_USEDEFAULT)
	{
		GetWindowRect(&rect);
		int xsize = rect.right - rect.left, ysize = rect.bottom - rect.top;
		int	cx = ::GetSystemMetrics(SM_CXFULLSCREEN);
		int	cy = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int	x = (cx - xsize)/2;
		int y = (cy - ysize)/2;

		MoveWindow((x < 0) ? 0 : x, (y < 0) ? 0 : y, xsize, ysize, FALSE);
	}
	else
		MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	return	TRUE;
}

BOOL TAboutDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
	case IDCANCEL:
		EndDialog(wID);
		return	TRUE;

	case IPMSG_ICON: case IPMSGWEB_BUTTON:
		if (wID == IPMSGWEB_BUTTON || wNotifyCode == 1)
			ShellExecuteU8(NULL, NULL, LoadStrU8(IDS_IPMSGURL), NULL, NULL, SW_SHOW);
		return	TRUE;

#ifndef IPMSG_PRO
	case IPMSGUPD_BTN:
		::PostMessage(GetMainWnd(), WM_COMMAND, MENU_UPDATE, 0);
		return	TRUE;
#endif
	}
	return	FALSE;
}


TFindDlg::TFindDlg(Cfg *_cfg, TSendDlg *_parent) : TDlg(FIND_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	cfg = _cfg;
	sendDlg = _parent;
	imeStatus = FALSE;
}

BOOL TFindDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		cfg->FindAll = (BOOL)SendDlgItemMessage(ALLFIND_CHECK, BM_GETCHECK, 0, 0);

		if (sendDlg->SelectFilterHost()) {
			GetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0], MAX_NAMEBUF);
			int	i;
			for (i=1; i < cfg->FindMax; i++) {
				if (stricmp(cfg->FindStr[i], cfg->FindStr[0]) == 0) {
					break;
				}
			}
			memmove(cfg->FindStr[2], cfg->FindStr[1],
					((i >= 2 && i == cfg->FindMax) ? i-2 : i-1) * MAX_NAMEBUF);
			memcpy(cfg->FindStr[1], cfg->FindStr[0], MAX_NAMEBUF);

			DWORD	start, end;		// エディット部の選択状態の保存
			SendDlgItemMessage(FIND_COMBO, CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
			// CB_RESETCONTENT でエディット部がクリア
			// なお、DELETESTRING でも edit 同名stringの場合、同じくクリアされる
			SendDlgItemMessage(FIND_COMBO, CB_RESETCONTENT, 0, 0);
			for (int i=1; i < cfg->FindMax && cfg->FindStr[i][0]; i++) {
				Wstr	find_w(cfg->FindStr[i], BY_UTF8);
				SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, i-1, (LPARAM)find_w.Buf());
			}
			SetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0]);
			SendDlgItemMessage(FIND_COMBO, CB_SETEDITSEL, 0, MAKELPARAM(start, end));
		}
		cfg->WriteRegistry(CFG_FINDHIST);
		return	TRUE;

	case IDCANCEL: case CLOSE_BUTTON:
		sendDlg->FilterHost("");
		EndDialog(wID);
		return	TRUE;

	case FIND_COMBO:
		if (wNotifyCode == CBN_EDITCHANGE) {
			FilterHost();
		}
		else if (wNotifyCode == CBN_SELCHANGE) {
			PostMessage(WM_FINDDLG_DELAY, 0, 0);
		}
		return	TRUE;
	}

	return	FALSE;
}

int TFindDlg::FilterHost()
{
	char	buf[MAX_NAMEBUF] = "";
	GetDlgItemTextU8(FIND_COMBO, buf, sizeof(buf));
	cfg->FindAll = (BOOL)SendDlgItemMessage(ALLFIND_CHECK, BM_GETCHECK, 0, 0);
	return	sendDlg->FilterHost(buf);
}


BOOL TFindDlg::EvCreate(LPARAM lParam)
{
	CheckDlgButton(ALLFIND_CHECK, cfg->FindAll);

	for (int i=1; i < cfg->FindMax && cfg->FindStr[i][0]; i++) {
		Wstr	find_w(cfg->FindStr[i], BY_UTF8);
		SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, (WPARAM)i-1, (LPARAM)find_w.Buf());
	}
//	if (cfg->FindStr[0][0])
//		SetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0]);

	if (rect.left == CW_USEDEFAULT) {
		GetWindowRect(&rect);
		rect.left += 140;
		rect.right += 140;
		rect.top -= 20;
		rect.bottom -= 20;
	}
	MoveWindow(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	Show();
	if (cfg->ControlIME == 1) {
		imeStatus = GetImeOpenStatus(hWnd);
		if (imeStatus) SetImeOpenStatus(hWnd, FALSE);
	}

	return	TRUE;
}

BOOL TFindDlg::EvDestroy()
{
	if (imeStatus) {
		SetImeOpenStatus(hWnd, TRUE);
	}
	return	FALSE;
}

BOOL TFindDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_FINDDLG_DELAY) {
		FilterHost();
		return	TRUE;
	}
	return	FALSE;
}

/*
	BrowseDirDlg用コールバック
*/
int CALLBACK BrowseDirDlgProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM path)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		SendMessageW(hWnd, BFFM_SETSELECTIONW, (WPARAM)TRUE, path);
		break;

	case BFFM_SELCHANGED:
		break;
	}
	return 0;
}

/*
	ディレクトリダイアログ用汎用ルーチン
*/
BOOL BrowseDirDlg(TWin *parentWin, const char *title, const char *defaultDir,
	char *buf, int bufsize, DWORD flags)
{ 
	IMalloc			*iMalloc = NULL;
	BROWSEINFOW		brInfo;
	LPITEMIDLIST	pidlBrowse;
	BOOL			ret = FALSE;
	Wstr			buf_w(MAX_PATH), defaultDir_w(MAX_PATH), title_w(title);

	if (!SUCCEEDED(SHGetMalloc(&iMalloc))) return FALSE;

	U8toW(defaultDir, defaultDir_w.Buf(), MAX_PATH);
	brInfo.hwndOwner = parentWin->hWnd;
	brInfo.pidlRoot = 0;
	brInfo.pszDisplayName = buf_w.Buf();
	brInfo.lpszTitle = title_w.s();
	brInfo.ulFlags = flags;
	brInfo.lpfn = BrowseDirDlgProc;
	brInfo.lParam = (LPARAM)defaultDir_w.Buf();
	brInfo.iImage = 0;

	if ((pidlBrowse = SHBrowseForFolderW(&brInfo)))
	{
		ret = SHGetPathFromIDListW(pidlBrowse, buf_w.Buf());
		iMalloc->Free(pidlBrowse);
		if (ret) {
			WtoU8(buf_w.s(), buf, bufsize);
		}
	}

	iMalloc->Release();
	return	ret;
}

TInputDlg::TInputDlg(TWin *_parent) : TDlg(INPUT_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
}

int TInputDlg::Exec(POINT _pt, char *_buf, int _max_buf)
{
	pt		= _pt;
	buf     = _buf;
	max_buf = _max_buf;
	return	TDlg::Exec();
}

BOOL TInputDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		GetDlgItemTextU8(INPUT_EDIT, buf, max_buf);
		EndDialog(IDOK);
		return	TRUE;

	case IDCANCEL:
		EndDialog(IDCANCEL);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TInputDlg::EvCreate(LPARAM lParam)
{
	if (*buf) {
		SetDlgItemTextU8(INPUT_EDIT, buf);
	}
	MoveWindow(pt.x, pt.y, orgRect.cx(), orgRect.cy(), FALSE);
	return	TRUE;
}


/*
	FireWall Dialog
*/
TFwDlg::TFwDlg(TWin *_parent) : TDlg(FW_DIALOG, _parent)
{
}

BOOL TFwDlg::EvCreate(LPARAM lParam)
{
	::SendMessage(GetDlgItem(IDOK), BCM_SETSHIELD, 0, 1);
	BOOL ret = TDlg::EvCreate(lParam);

	static HICON hBigIcon;
	static HICON hSmallIcon;

	if (!hBigIcon) {
		hBigIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON,
			IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
		hSmallIcon = (HICON)::LoadImage(TApp::hInst(), (LPCSTR)IPMSG_ICON,
			IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	}

	MoveCenter();

	Show();
	return	ret;
}

BOOL TFwDlg::EvNcDestroy()
{
	return	TRUE;
}

BOOL TFwDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case IDOK:
		SetFirewallExcept(hWnd);
		return	TRUE;

	case IDCANCEL:
		EndDialog(IDCANCEL);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFwDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_IPMSG_SETFWRES:
		if (parent && parent->hWnd) {
			parent->PostMessage(WM_IPMSG_SETFWRES, wParam, lParam);
		}
		EndDialog(wParam ? IDOK : IDCANCEL);
		return	TRUE;
	}
	return	FALSE;
}

BOOL TFwDlg::SetFirewallExcept(HWND hTarget)
{
	char	path[MAX_PATH];
	char	arg[MAX_BUF] = "/FIREWALL";

	if (hTarget) {
		sprintf(arg + strlen(arg), "=%p", hTarget);
	}
	::GetModuleFileName(NULL, path, sizeof(path));

	return	INT_RDC(::ShellExecute(hTarget, "runas", path, arg, NULL, SW_NORMAL)) > 32;
}

