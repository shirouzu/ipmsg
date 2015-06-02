static char *miscdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2012   miscdlg.cpp	Ver3.40";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Misc Dialog
	Create					: 1996-12-15(Sun)
	Update					: 2012-04-02(Mon)
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
	focus_index = INVALID_INDEX;
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
			if (focus_index == INVALID_INDEX)
				focus_index = 0;
			SendMessage(LVM_SETITEMSTATE, focus_index, (LPARAM)&lvi);
			SendMessage(LVM_SETSELECTIONMARK, 0, focus_index);
		}
		return	FALSE;
	}
	else {	// WM_KILLFOCUS
		if (itemNo != maxItem) {
			SendMessage(LVM_SETITEMSTATE, itemNo, (LPARAM)&lvi);
			focus_index = itemNo;
		}
		return	TRUE;	// WM_KILLFOCUS は伝えない
	}
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
	if (focus_index == INVALID_INDEX)
		return	FALSE;

	switch (uMsg) {
	case LVM_INSERTITEM:
		if (((LV_ITEM *)lParam)->iItem <= focus_index)
			focus_index++;
		break;
	case LVM_DELETEITEM:
		if ((int)wParam == focus_index)
			focus_index = INVALID_INDEX;
		else if ((int)wParam < focus_index)
			focus_index--;
		break;
	case LVM_DELETEALLITEMS:
		focus_index = INVALID_INDEX;
		break;
	}
	return	FALSE;
}

int TListViewEx::InsertColumn(int idx, char *title, int width, int fmt)
{
	LV_COLUMNW	lvC;
	memset(&lvC, 0, sizeof(lvC));
	lvC.fmt      = fmt;
	lvC.mask     = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvC.pszText  = U8toW(title);
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

int TListViewEx::InsertItem(int idx, char *str, LPARAM lParam)
{
	LV_ITEMW		lvi = {0};
	lvi.mask		= LVIF_TEXT|LVIF_PARAM;
	lvi.iItem		= idx;
	lvi.pszText		= U8toW(str);
	lvi.lParam		= lParam;
	return	(int)SendMessage(LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

int TListViewEx::SetSubItem(int idx, int subIdx, char *str)
{
	LV_ITEMW		lvi = {0};
	lvi.mask		= LVIF_TEXT;
	lvi.iItem		= idx;
	lvi.iSubItem	= subIdx;
	lvi.pszText		= U8toW(str);
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
	LV_ITEMW	lvi = {0};
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
	return	(int)SendMessage(LVM_GETITEMSTATE, idx, LVIS_SELECTED) & LVIS_SELECTED ? TRUE : FALSE;
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
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
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
		EndDialog(TRUE);
		return	TRUE;

	case SET_BUTTON:
		GetData();
		cfg->WriteRegistry(CFG_ABSENCE);
		if (cfg->AbsenceCheck)
		{
			cfg->AbsenceCheck = FALSE;
			::PostMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		}
		EndDialog(FALSE);
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
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
	for (int cnt=0; cnt < cfg->AbsenceMax; cnt++)
	{
		strcpy(tmpAbsenceHead[cnt], cfg->AbsenceHead[cnt]);
		strcpy(tmpAbsenceStr[cnt], cfg->AbsenceStr[cnt]);
		Wstr	head_w(cfg->AbsenceHead[cnt], BY_UTF8);
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
	for (int cnt=0; cnt < cfg->AbsenceMax; cnt++)
	{
		strcpy(cfg->AbsenceHead[cnt], tmpAbsenceHead[cnt]);
		strcpy(cfg->AbsenceStr[cnt], tmpAbsenceStr[cnt]);
	}
	cfg->AbsenceChoice = currentChoice;
}


/*
	ソート設定ダイアログ
*/
TSortDlg *TSortDlg::exclusiveWnd = NULL;

TSortDlg::TSortDlg(Cfg *_cfg, TWin *_parent) : TDlg(SORT_DIALOG, _parent)
{
	cfg = _cfg;
}

int TSortDlg::Exec(void)
{
	if (exclusiveWnd)
		return	exclusiveWnd->SetForegroundWindow(), FALSE;

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
			EndDialog(TRUE), exclusiveWnd = NULL;
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
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
	cfg = _cfg;
	outbuf = NULL;
}

// 単なるパスワード入力用
TPasswordDlg::TPasswordDlg(char *_outbuf, TWin *_parent) : TDlg(PASSWORD_DIALOG, _parent)
{
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
					EndDialog(TRUE);
				else
					SetDlgItemTextU8(PASSWORD_EDIT, ""), MessageBoxU8(GetLoadStrU8(IDS_CANTAUTH));
			}
			else EndDialog(TRUE);
		}
		return	TRUE;

	case IDCANCEL:
		EndDialog(FALSE);
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
	TMsgDlgは、制約の多いMessageBoxU8()の代用として作成
*/
TMsgDlg::TMsgDlg(TWin *_parent) : TListDlg(MESSAGE_DIALOG, _parent)
{
	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
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
BOOL TMsgDlg::Create(char *text, char *title, int _showMode)
{
	showMode = _showMode;

	if (!TDlg::Create()) return	FALSE;

	SetDlgItemTextU8(MESSAGE_TEXT, text);
	SetWindowTextU8(title);
	if (showMode == SW_SHOW) {
		DWORD	key =	GetAsyncKeyState(VK_LBUTTON) |
						GetAsyncKeyState(VK_RBUTTON) |
						GetAsyncKeyState(VK_MBUTTON);
		::EnableWindow(hWnd, TRUE);
		::ShowWindow(hWnd, (key & 0x8000) ? SW_SHOWNOACTIVATE : SW_SHOW);
	}
	else {
		::ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
		::EnableWindow(hWnd, TRUE);
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
		EndDialog(FALSE);
		::PostMessage(GetMainWnd(), WM_MSGDLG_EXIT, (WPARAM)0, (LPARAM)this);
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

	createCnt++;
	return	TRUE;
}

/*
	About Dialog初期化処理
*/
TAboutDlg::TAboutDlg(TWin *_parent) : TDlg(ABOUT_DIALOG, _parent)
{
}

/*
	Window 生成時の CallBack
*/
BOOL TAboutDlg::EvCreate(LPARAM lParam)
{
	SetDlgIcon(hWnd);

	char	buf[512], buf2[512];
	GetDlgItemTextU8(ABOUT_TEXT, buf, sizeof(buf));
	wsprintf(buf2, buf, GetVersionStr());
	SetDlgItemTextU8(ABOUT_TEXT, buf2);

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
		EndDialog(TRUE);
		return	TRUE;

	case IPMSG_ICON: case IPMSGWEB_BUTTON:
		if (wID == IPMSGWEB_BUTTON || wNotifyCode == 1)
			ShellExecuteU8(NULL, NULL, GetLoadStrU8(IDS_IPMSGURL), NULL, NULL, SW_SHOW);
		return	TRUE;
	}
	return	FALSE;
}


TFindDlg::TFindDlg(Cfg *_cfg, TSendDlg *_parent) : TDlg(FIND_DIALOG, _parent)
{
	cfg = _cfg;
	sendDlg = _parent;
	imeStatus = FALSE;
}

BOOL TFindDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		int		cnt;
		cfg->FindAll = (BOOL)SendDlgItemMessage(ALLFIND_CHECK, BM_GETCHECK, 0, 0);

		if (sendDlg->SelectFilterHost()) {
			GetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0], MAX_NAMEBUF);
			for (cnt=1; cnt < cfg->FindMax; cnt++) {
				if (stricmp(cfg->FindStr[cnt], cfg->FindStr[0]) == 0) break;
			}
			memmove(cfg->FindStr[2], cfg->FindStr[1],
					(cnt == cfg->FindMax ? cnt-2 : cnt-1) * MAX_NAMEBUF);
			memcpy(cfg->FindStr[1], cfg->FindStr[0], MAX_NAMEBUF);
			DWORD	start, end;		// エディット部の選択状態の保存
			SendDlgItemMessage(FIND_COMBO, CB_GETEDITSEL, (WPARAM)&start, (LPARAM)&end);
			// CB_RESETCONTENT でエディット部がクリア
			// なお、DELETESTRING でも edit 同名stringの場合、同じくクリアされる
			SendDlgItemMessage(FIND_COMBO, CB_RESETCONTENT, 0, 0);
			for (cnt=1; cnt < cfg->FindMax && cfg->FindStr[cnt][0]; cnt++) {
				Wstr	find_w(cfg->FindStr[cnt], BY_UTF8);
				SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, cnt-1, (LPARAM)find_w.Buf());
			}
			SetDlgItemTextU8(FIND_COMBO, cfg->FindStr[0]);
			SendDlgItemMessage(FIND_COMBO, CB_SETEDITSEL, 0, MAKELPARAM(start, end));
		}
		cfg->WriteRegistry(CFG_FINDHIST);
		return	TRUE;

	case IDCANCEL: case CLOSE_BUTTON:
		sendDlg->FilterHost("");
		EndDialog(FALSE);
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

	for (int cnt=1; cnt < cfg->FindMax && cfg->FindStr[cnt][0]; cnt++) {
		Wstr	find_w(cfg->FindStr[cnt], BY_UTF8);
		SendDlgItemMessageW(FIND_COMBO, CB_INSERTSTRING, (WPARAM)cnt-1, (LPARAM)find_w.Buf());
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
	ファイルダイアログ用汎用ルーチン
*/
BOOL OpenFileDlg::Exec(UINT editCtl, char *title, char *filter, char *defaultDir, char *defaultExt)
{
	char buf[MAX_PATH_U8];

	if (parent == NULL)
		return FALSE;

	parent->GetDlgItemTextU8(editCtl, buf, sizeof(buf));

	if (!Exec(buf, sizeof(buf), title, filter, defaultDir, defaultExt))
		return	FALSE;

	parent->SetDlgItemTextU8(editCtl, buf);
	return	TRUE;
}

BOOL OpenFileDlg::Exec(char *target, int targ_size, char *title, char *filter, char *defaultDir,
						char *defaultExt)
{
	if (targ_size <= 1) return FALSE;

	OPENFILENAME	ofn;
	U8str			fileName(targ_size);
	U8str			dirName(targ_size);
	char			*fname = NULL;

	if (*target && GetFullPathNameU8(target, targ_size, dirName.Buf(), &fname) != 0 && fname) {
		*(fname -1) = 0;
		strncpyz(fileName.Buf(), fname, targ_size);
	}
	else if (defaultDir) {
		strncpyz(dirName.Buf(), defaultDir, targ_size);
	}

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = parent ? parent->hWnd : NULL;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = filter ? 1 : 0;
	ofn.lpstrFile = fileName.Buf();
	ofn.lpstrDefExt	 = defaultExt;
	ofn.nMaxFile = targ_size;
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir = dirName.Buf();
	ofn.lpfnHook = hook;
	ofn.Flags = OFN_HIDEREADONLY|OFN_EXPLORER|(hook ? OFN_ENABLEHOOK : 0);
	if (mode == OPEN || mode == MULTI_OPEN)
		ofn.Flags |= OFN_FILEMUSTEXIST | (mode == MULTI_OPEN ? OFN_ALLOWMULTISELECT : 0);
	else
		ofn.Flags |= (mode == NODEREF_SAVE ? OFN_NODEREFERENCELINKS : 0);
	ofn.Flags |= flags;

	U8str	dirNameBak(targ_size);
	GetCurrentDirectoryU8(targ_size, dirNameBak.Buf());

	BOOL	ret = (mode == OPEN || mode == MULTI_OPEN) ?
					GetOpenFileNameU8(&ofn) : GetSaveFileNameU8(&ofn);

	SetCurrentDirectoryU8(dirNameBak.Buf());
	if (ret) {
		if (mode == MULTI_OPEN) {
			memcpy(target, fileName.Buf(), targ_size);
		} else {
			strncpyz(target, ofn.lpstrFile, targ_size);
		}

		if (defaultDir) strncpyz(defaultDir, ofn.lpstrFile, ofn.nFileOffset);
	}

	return	ret;
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
BOOL BrowseDirDlg(TWin *parentWin, const char *title, const char *defaultDir, char *buf)
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
	brInfo.lpszTitle = title_w;
	brInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	brInfo.lpfn = BrowseDirDlgProc;
	brInfo.lParam = (LPARAM)defaultDir_w.Buf();
	brInfo.iImage = 0;

	if ((pidlBrowse = SHBrowseForFolderV((BROWSEINFO *)&brInfo)))
	{
		ret = SHGetPathFromIDListV(pidlBrowse, buf_w.Buf());
		iMalloc->Free(pidlBrowse);
		if (ret) {
			WtoU8(buf_w, buf, MAX_PATH_U8);
		}
	}

	iMalloc->Release();
	return	ret;
}

