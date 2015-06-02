static char *senddlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   senddlg.cpp	Ver3.00";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2011-04-20(Wed)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include "resource.h"
#include "ipmsg.h"
#include "aes.h"
#include "blowfish.h"

/*
	SendDialog の初期化
*/
TSendDlg::TSendDlg(MsgMng *_msgmng, ShareMng *_shareMng, THosts *_hosts, Cfg *_cfg, LogMng *_logmng, HWND _hRecvWnd, MsgBuf *_msg)
	: TListDlg(SEND_DIALOG), editSub(_cfg, this), separateSub(this), hostListView(this), hostListHeader(&hostListView)
{
	hRecvWnd		= _hRecvWnd;
	msgMng			= _msgmng;
	shareMng		= _shareMng;
	shareInfo		= NULL;
	shareStr		= NULL;
	shareExStr		= NULL;
	hosts			= _hosts;
	hostArray		= NULL;
	cfg				= _cfg;
	logmng			= _logmng;
	memberCnt		= 0;
	sendEntry		= NULL;
	sendEntryNum	= 0;
	packetNo		= msgMng->MakePacketNo();
	retryCnt		= 0;
	timerID			= 0;
	hEditFont		= NULL;
	hListFont		= NULL;
	captureMode		= FALSE;
	listOperateCnt	= 0;
	hiddenDisp		= FALSE;
	*selectGroup	= 0;
	currentMidYdiff	= cfg->SendMidYdiff;
	memset(&orgFont, 0, sizeof(orgFont));
	maxItems = 0;
	lvStateEnable	= FALSE;
	sortItem = -1;
	sortRev = FALSE;
	findDlg = NULL;

	msg.Init(_msg);

	hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
}

/*
	SendDialog の終了
*/
TSendDlg::~TSendDlg()
{
	if (findDlg)
		delete findDlg;

	// ListView メモリリーク暫定対策...
	hostListView.SendMessage(LVM_DELETEALLITEMS, 0, 0);

	if (hListFont)
		::DeleteObject(hListFont);
	if (hEditFont)
		::DeleteObject(hEditFont);

	delete [] sendEntry;
	delete [] shareExStr;
	delete [] shareStr;
	if (hostArray)
		free(hostArray);
}

#define AW_HOR_POSITIVE 0x00000001
#define AW_HOR_NEGATIVE 0x00000002
#define AW_VER_POSITIVE 0x00000004
#define AW_VER_NEGATIVE 0x00000008
#define AW_CENTER 0x00000010
#define AW_HIDE 0x00010000
#define AW_ACTIVATE 0x00020000
#define AW_SLIDE 0x00040000
#define AW_BLEND 0x00080000

BOOL (WINAPI *pAnimateWindow)(
  HWND hwnd,     // ウィンドウのハンドル
  DWORD dwTime,  // アニメーションの時間(ミリ秒)
  DWORD dwFlags  // アニメーションの種類
);

/*
	SendDialog 生成時の CallBack
*/
BOOL TSendDlg::EvCreate(LPARAM lParam)
{

//	if (!pAnimateWindow) {
//		pAnimateWindow = (BOOL (WINAPI *)(HWND, DWORD, DWORD))::GetProcAddress(::GetModuleHandle("user32.dll"), "AnimateWindow");
//	}

	editSub.AttachWnd(GetDlgItem(SEND_EDIT));
	editSub.SendMessage(EM_AUTOURLDETECT, 1, 0);
	editSub.SendMessage(EM_SETTARGETDEVICE, 0, 0);		// 折り返し

	separateSub.AttachWnd(GetDlgItem(SEPARATE_STATIC));
	hostListView.AttachWnd(GetDlgItem(HOST_LIST));
	hostListHeader.AttachWnd((HWND)hostListView.SendMessage(LVM_GETHEADER, 0, 0));

	SetDlgIcon(hWnd);

	if (msg.hostSub.addr) {
		Host	*host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
		if (host && host->priority <= 0)
			hiddenDisp = TRUE;
	}

	HMENU	hMenu = ::GetSystemMenu(hWnd, FALSE);
	AppendMenuU8(hMenu, MF_SEPARATOR, NULL, NULL);
	SetMainMenu(hMenu);

	if (cfg->AbnormalButton)
		SetDlgItemTextU8(IDOK, GetLoadStrU8(IDS_FIRE));

	if (cfg->SecretCheck)
		SendDlgItemMessage(SECRET_CHECK, BM_SETCHECK, cfg->SecretCheck, 0);
	else
		::EnableWindow(GetDlgItem(PASSWORD_CHECK), FALSE);

	DisplayMemberCnt();

	if (!IsNewShell())
	{
		ULONG	style;
		style = GetWindowLong(GWL_STYLE);
		style &= 0xffffff0f;
		style |= 0x00000080;
		SetWindowLong(GWL_STYLE, style);
		style = separateSub.GetWindowLong(GWL_STYLE);
		style &= 0xffffff00;
		style |= 0x00000007;
		separateSub.SetWindowLong(GWL_STYLE, style);
	}
//	SetForegroundWindow();

#if 0
	hostListView.SendMessage(LVM_SETTEXTBKCOLOR, 0, 0x222222);
	hostListView.SendMessage(LVM_SETTEXTCOLOR, 0, 0xeeeeee);
	hostListView.SendMessage(LVM_SETBKCOLOR, 0, 0x222222);
#endif

	InitializeHeader();

	SetFont();
	SetSize();
	ReregisterEntry();

	::SetFocus(hostListView.hWnd);
	if (msg.hostSub.addr)
		SelectHost(&msg.hostSub);

//	if (pAnimateWindow) {
//		pAnimateWindow(hWnd, 1000, AW_HOR_POSITIVE|AW_BLEND|AW_ACTIVATE|AW_SLIDE);
//	}

	PostMessage(WM_DELAYSETTEXT, 0, 0);

	static HICON hMenuIcon;
	if (!hMenuIcon) hMenuIcon = ::LoadIcon(TApp::GetInstance(), (LPCSTR)MENU_ICON);
	SendDlgItemMessage(MENU_CHECK, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hMenuIcon);

	return	TRUE;
}

/*
	Construct/Rebuild Column Header
*/
void TSendDlg::InitializeHeader(void)
{
	int		order[MAX_SENDWIDTH];
	int		revItems[MAX_SENDWIDTH];

// カラムヘッダを全削除
	while (maxItems > 0)
		hostListView.SendMessage(LVM_DELETECOLUMN, --maxItems, 0);

	ColumnItems = cfg->ColumnItems & ~(1 << SW_ABSENCE);
	memcpy(FullOrder, cfg->SendOrder, sizeof(FullOrder));

	int cnt;
	for (cnt=0; cnt < MAX_SENDWIDTH; cnt++) {
		if (GetItem(ColumnItems, cnt)) {
			items[maxItems] = cnt;
			revItems[cnt] = maxItems++;
		}
	}
	int		orderCnt = 0;
	for (cnt=0; cnt < MAX_SENDWIDTH; cnt++) {
		if (GetItem(ColumnItems, FullOrder[cnt]))
			order[orderCnt++] = revItems[FullOrder[cnt]];
	}

// 常に選択
	DWORD dw = hostListView.GetWindowLong(GWL_STYLE) | LVS_SHOWSELALWAYS;
	hostListView.SetWindowLong(GWL_STYLE, dw);

// １行全カラム選択など
	DWORD style = hostListView.SendMessage(LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0) | LVS_EX_FULLROWSELECT | LVS_EX_HEADERDRAGDROP;
	if (cfg->GlidLineCheck)
		style |= LVS_EX_GRIDLINES;
	else
		style &= ~LVS_EX_GRIDLINES;
	hostListView.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, style);

	static WCHAR	*headerStr[MAX_SENDWIDTH];
	if (!headerStr[SW_NICKNAME]) {
		headerStr[SW_NICKNAME]	= GetLoadStrW(IDS_USER);
		headerStr[SW_PRIORITY]	= GetLoadStrW(IDS_PRIORITY);
		headerStr[SW_ABSENCE]	= GetLoadStrW(IDS_ABSENCE);
		headerStr[SW_GROUP]		= GetLoadStrW(IDS_GROUP);
		headerStr[SW_HOST]		= GetLoadStrW(IDS_HOST);
		headerStr[SW_USER]		= GetLoadStrW(IDS_LOGON);
		headerStr[SW_IPADDR]	= GetLoadStrW(IDS_IPADDR);
	}

	LV_COLUMNW	lvC;
	memset(&lvC, 0, sizeof(lvC));
	lvC.fmt = LVCFMT_LEFT;
	lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	for (cnt = 0; cnt < maxItems; cnt++) {
		lvC.pszText = headerStr[items[cnt]];
		lvC.cx = cfg->SendWidth[items[cnt]];
		hostListView.SendMessage(LVM_INSERTCOLUMNW, lvC.iSubItem=cnt, (LPARAM)&lvC);
	}
	hostListView.SendMessage(LVM_SETCOLUMNORDERARRAY, maxItems, (LPARAM)order);
}

void TSendDlg::GetOrder(void)
{
	int		order[MAX_SENDWIDTH], orderCnt=0;

	if (hostListView.SendMessage(LVM_GETCOLUMNORDERARRAY, maxItems, (LPARAM)order) == FALSE) {
		MessageBoxU8(GetLoadStrU8(IDS_COMCTL), GetLoadStrU8(IDS_CANTGETORDER));
		return;
	}
	for (int cnt=0; cnt < MAX_SENDWIDTH; cnt++) {
		if (GetItem(ColumnItems, FullOrder[cnt]))
			FullOrder[cnt] = items[order[orderCnt++]];
	}
	memcpy(cfg->SendOrder, FullOrder, sizeof(FullOrder));
}

/*
	WM_COMMAND CallBack
*/
BOOL TSendDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		if (IsSending())
			return	TRUE;
		if (findDlg && findDlg->hWnd)
			return	findDlg->SetForegroundWindow();
		if (cfg->AbsenceCheck && cfg->Debug < 2)
		{
			if (MessageBoxU8(GetLoadStrU8(IDS_ABSENCEQUERY), MSG_STR, MB_OKCANCEL) != IDOK)
				return	TRUE;
			::SendMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		}
		SendMsg();
		if (shareInfo && shareInfo->fileCnt == 0)
			shareMng->DestroyShare(shareInfo), shareInfo = NULL;
		return	TRUE;

	case IDCANCEL:
		if (findDlg && findDlg->hWnd)
			return	findDlg->Destroy(), TRUE;

		if (shareInfo)
			shareMng->DestroyShare(shareInfo), shareInfo = NULL;
		::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, (LPARAM)this);
		return	TRUE;

	case REFRESH_BUTTON:
		::PostMessage(GetMainWnd(), WM_REFRESH_HOST, (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE, 0);
		return	TRUE;

	case SECRET_CHECK:
		if (::IsDlgButtonChecked(hWnd, SECRET_CHECK) != 0)
			::EnableWindow(GetDlgItem(PASSWORD_CHECK), TRUE);
		else {
			SendDlgItemMessage(PASSWORD_CHECK, BM_SETCHECK, 0, 0);
			::EnableWindow(GetDlgItem(PASSWORD_CHECK), FALSE);
		}
		break;

	case MENU_FILEADD:
		{
			char	buf[MAX_PATH_U8] = "";
			if (TShareDlg::FileAddDlg(this, shareMng, shareInfo ? shareInfo : (shareInfo = shareMng->CreateShare(packetNo)), cfg))
			{
				SetFileButton(this, FILE_BUTTON, shareInfo);
				EvSize(SIZE_RESTORED, 0, 0);
			}
		}
		break;

	case MENU_FOLDERADD:
		{
			for (int cnt=0; cnt < 5; cnt++)
			{
				if (*cfg->lastOpenDir && GetFileAttributesU8(cfg->lastOpenDir) == 0xffffffff)
					if (PathToDir(cfg->lastOpenDir, cfg->lastOpenDir) == FALSE)
						break;
			}
			if (BrowseDirDlg(this, GetLoadStrU8(IDS_FOLDERATTACH), cfg->lastOpenDir, cfg->lastOpenDir))
			{
				shareMng->AddFileShare(shareInfo ? shareInfo : (shareInfo = shareMng->CreateShare(packetNo)), cfg->lastOpenDir);
				SetFileButton(this, FILE_BUTTON, shareInfo);
				EvSize(SIZE_RESTORED, 0, 0);
			}
		}
		break;

	case FILE_BUTTON:
		TShareDlg(shareMng, shareInfo ? shareInfo : (shareInfo = shareMng->CreateShare(packetNo)), cfg, this).Exec();
		SetFileButton(this, FILE_BUTTON, shareInfo);
		EvSize(SIZE_RESTORED, 0, 0);
		break;

	case MENU_IMAGEPASTE:
		editSub.SendMessage(WM_COMMAND, WM_PASTE_IMAGE, 0);
		break;

	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case ALLSELECT_ACCEL:
		editSub.SendMessage(EM_SETSEL, 0, (LPARAM)-1);
		return	TRUE;

	case MENU_FINDDLG:
		if (!findDlg)
			findDlg = new TFindDlg(cfg, this);
		if (!findDlg->hWnd)
			findDlg->Create();
		return	TRUE;

	case SEPARATE_STATIC:
		return	TRUE;

	case MENU_SAVEPOS:
		if ((cfg->SendSavePos = !cfg->SendSavePos) != 0)
		{
			GetWindowRect(&rect);
			cfg->SendXpos = rect.left;
			cfg->SendYpos = rect.top;
		}
		cfg->WriteRegistry(CFG_WINSIZE);
		return	TRUE;

	case MENU_SAVESIZE:
		GetWindowRect(&rect);
		cfg->SendXdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
		cfg->SendYdiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);
		cfg->SendMidYdiff = currentMidYdiff;
		cfg->WriteRegistry(CFG_WINSIZE);
		return	TRUE;

	case MENU_SAVECOLUMN:
		{
			GetOrder();
			for (int cnt=0; cnt < maxItems; cnt++)
				cfg->SendWidth[items[cnt]] = hostListView.SendMessage(LVM_GETCOLUMNWIDTH, cnt, 0);
			cfg->WriteRegistry(CFG_WINSIZE);
		}
		return	TRUE;

	case MENU_EDITFONT: case MENU_LISTFONT:
		{
			CHOOSEFONT	cf;
			LOGFONT		tmpFont, *targetFont;

			targetFont = wID == MENU_EDITFONT ? &cfg->SendEditFont : &cfg->SendListFont;
			memset(&cf, 0, sizeof(cf));
			cf.lStructSize	= sizeof(cf);
			cf.hwndOwner	= hWnd;
			cf.lpLogFont	= &(tmpFont = *targetFont);
			cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_SHOWHELP | CF_EFFECTS;
			cf.nFontType	= SCREEN_FONTTYPE;
			if (::ChooseFont(&cf))
			{
				*targetFont = tmpFont;
				SetFont();
				::InvalidateRgn(hWnd, NULL, TRUE);
				cfg->WriteRegistry(CFG_FONT);
				::PostMessage(GetMainWnd(), WM_SENDDLG_FONTCHANGED, 0, 0);
			}
		}
		return	TRUE;

	case MENU_DEFAULTFONT:
		cfg->SendEditFont = cfg->SendListFont = orgFont;
		{
			char	*ex_font = GetLoadStrA(IDS_PROPORTIONALFONT);
			if (ex_font && *ex_font)
				strcpy(cfg->SendListFont.lfFaceName, ex_font);
		}
		SetFont();
		::InvalidateRgn(hWnd, NULL, TRUE);
		cfg->WriteRegistry(CFG_FONT);
		return	TRUE;

	case MENU_NORMALSIZE:
		GetWindowRect(&rect);
		currentMidYdiff = 0;
		MoveWindow(rect.left, rect.top, orgRect.right - orgRect.left, orgRect.bottom - orgRect.top, TRUE);
		return	TRUE;

	case MENU_MEMBERDISP:
		if (TSortDlg(cfg, this).Exec()) {
			GetOrder();
			cfg->WriteRegistry(CFG_WINSIZE);
			DelAllHost();
			InitializeHeader();
			for (int cnt=0; cnt < hosts->HostCnt(); cnt++)
				AddHost(hosts->GetHost(cnt));
		}
		return	TRUE;

	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_PASTE_REV:
	case WM_PASTE_IMAGE:
	case WM_CLEAR:
	case EM_SETSEL:
		editSub.SendMessage(WM_COMMAND, wID, 0);
		return	TRUE;

	case MENU_CHECK:
		{
			POINT	pt;
			DWORD	val;
			::GetCursorPos(&pt);
			val = POINTTOPOINTS(pt);
			PopupContextMenu(MAKEPOINTS(val));
			CheckDlgButton(MENU_CHECK, 0);
		}
		return	TRUE;

	default:
		if (wID >= MENU_PRIORITY_RESET && wID < MENU_GROUP_START)
		{
			if (wID == MENU_PRIORITY_RESET)
			{
				if (MessageBoxU8(GetLoadStrU8(IDS_DEFAULTSET), MSG_STR, MB_OKCANCEL) != IDOK)
					return	TRUE;
				while (cfg->priorityHosts.HostCnt() > 0)
				{
					Host	*host = cfg->priorityHosts.GetHost(0);
					cfg->priorityHosts.DelHost(host);
					if (host->RefCnt() == 0)
						delete host;
				}
				for (int cnt=0; cnt < hosts->HostCnt(); cnt++)
					hosts->GetHost(cnt)->priority = DEFAULT_PRIORITY;
			}
			else if (wID == MENU_PRIORITY_HIDDEN)
			{
				hiddenDisp = !hiddenDisp;
			}
			else if (wID >= MENU_PRIORITY_START && wID < MENU_GROUP_START)
			{
				int	priority = wID - MENU_PRIORITY_START;

				for (int cnt=0; cnt < memberCnt; cnt++)
				{
					if (hostArray[cnt]->priority == priority || (hostListView.SendMessage(LVM_GETITEMSTATE, cnt, LVIS_SELECTED) & LVIS_SELECTED) == 0)
						continue;
					if (hostArray[cnt]->priority == DEFAULT_PRIORITY)
						cfg->priorityHosts.AddHost(hostArray[cnt]);
					else if (priority == DEFAULT_PRIORITY)
						cfg->priorityHosts.DelHost(hostArray[cnt]);
					hostArray[cnt]->priority = priority;
				}
			}
			DelAllHost();
			for (int cnt=0; cnt < hosts->HostCnt(); cnt++)
				AddHost(hosts->GetHost(cnt));
			if (wID != MENU_PRIORITY_HIDDEN)
				cfg->WriteRegistry(CFG_HOSTINFO|CFG_DELHOST|CFG_DELCHLDHOST);
		}
		else if (wID >= MENU_GROUP_START && wID < MENU_GROUP_START + (UINT)memberCnt)
		{
			BOOL	ctl_on = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
			BOOL	ensure = FALSE;
			LV_ITEM	lvi;
			memset(&lvi, 0, sizeof(lvi));
			lvi.mask = LVIF_STATE;

			for (lvi.iItem=0; lvi.iItem < memberCnt; lvi.iItem++)
			{
				if (strcmp(selectGroup, hostArray[lvi.iItem]->groupName) == 0 && !IsSameHost(&hostArray[lvi.iItem]->hostSub, msgMng->GetLocalHost())) {
					lvi.stateMask = lvi.state = LVIS_FOCUSED|LVIS_SELECTED;
					hostListView.SendMessage(LVM_SETITEMSTATE, lvi.iItem, (LPARAM)&lvi);
					if (ensure == FALSE) {
						ensure = TRUE;
						hostListView.SendMessage(LVM_ENSUREVISIBLE, lvi.iItem, 0);
						hostListView.SendMessage(LVM_SETSELECTIONMARK, 0, lvi.iItem);
					}
				}
				else if (ctl_on == FALSE) {
					lvi.stateMask = LVIS_SELECTED;
					lvi.state = 0;
					hostListView.SendMessage(LVM_SETITEMSTATE, lvi.iItem, (LPARAM)&lvi);
				}
			}
		}
		return	TRUE;
	}

	return	FALSE;
}

/*
	WM_SYSCOMMAND CallBack
*/
BOOL TSendDlg::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	switch (uCmdType)
	{
	case MENU_SAVEPOS:
	case MENU_SAVESIZE:
	case MENU_SAVECOLUMN:
	case MENU_FINDDLG:
	case MENU_EDITFONT: case MENU_LISTFONT:
	case MENU_DEFAULTFONT:
	case MENU_NORMALSIZE:
	case MENU_MEMBERDISP:
	case MENU_FILEADD:
	case MENU_FOLDERADD:
	case MENU_IMAGEPASTE:
		return	EvCommand(0, uCmdType, 0);
	}

	return	FALSE;
}

/*
	MenuInit Event CallBack
*/
BOOL TSendDlg::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	switch (uMsg)
	{
	case WM_INITMENU:
		{
			ModifyMenuU8(hMenu, MENU_SAVEPOS, MF_BYCOMMAND|(cfg->SendSavePos ? MF_CHECKED :  0), MENU_SAVEPOS, GetLoadStrU8(IDS_SAVEPOS));
		}
		return	TRUE;
	}
	return	FALSE;
}

/*
	Color Event CallBack
*/
BOOL TSendDlg::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
#if 0
	COLORREF	bkref	= 0x0000ff;
	COLORREF	foreref	= 0x00ff00;
	COLORREF	dlgref	= 0xff0000;
	COLORREF	statref	= 0xffff00;

	switch (uMsg) {
	case WM_CTLCOLORDLG:	// dlg 地
		{ static HBRUSH hb; if (!hb) hb = ::CreateSolidBrush(dlgref); *result = hb; }
//		SetTextColor(hDcCtl, foreref);
//		SetBkColor(hDcCtl, dlgref);
		break;
	case WM_CTLCOLOREDIT:	// edit 地
		{ static HBRUSH hb; if (!hb) hb = ::CreateSolidBrush(bkref); *result = hb; }
		SetTextColor(hDcCtl, foreref);
		SetBkColor(hDcCtl, bkref);
		break;
	case WM_CTLCOLORSTATIC:	// static control & check box 地
		if (separateSub.hWnd == hWndCtl) { static HBRUSH hb; if (!hb) hb = ::CreateSolidBrush(bkref); *result = hb; } else { static HBRUSH hb; if (!hb) hb = ::CreateSolidBrush(dlgref); *result = hb; }
		SetTextColor(hDcCtl, statref);
		SetBkColor(hDcCtl, dlgref);
		break;
	}
	return	TRUE;
#else
	return	FALSE;
#endif
}

/*
	MenuSelect Event CallBack
*/
BOOL TSendDlg::EvMenuSelect(UINT uItem, UINT fuFlag, HMENU hMenu)
{
	if (uItem >= MENU_GROUP_START && uItem < MENU_GROUP_START + (UINT)memberCnt) {
		GetMenuStringU8(hMenu, uItem, selectGroup, sizeof(selectGroup), MF_BYCOMMAND);
	}
	return	FALSE;
}

/*
	DropFiles Event CallBack
*/
BOOL TSendDlg::EvDropFiles(HDROP hDrop)
{
	char	buf[MAX_BUF * 2];
	int		max = DragQueryFileU8(hDrop, ~0UL, 0, 0), cnt;

	if (!shareInfo)
		shareInfo = shareMng->CreateShare(packetNo);

	for (cnt=0; cnt < max; cnt++)
	{
		if (DragQueryFileU8(hDrop, cnt, buf, sizeof(buf)) <= 0)
			break;
		shareMng->AddFileShare(shareInfo, buf);
	}
	::DragFinish(hDrop);

	if (shareInfo->fileCnt == 0)
		return	FALSE;

	SetFileButton(this, FILE_BUTTON, shareInfo);
	EvSize(SIZE_RESTORED, 0, 0);

	return	TRUE;
}

WCHAR *TSendDlg::GetListItemStrW(Host *host, int item)
{
	static WCHAR	wbuf[MAX_LISTBUF];
	char	*s, buf[MAX_LISTBUF];

	switch (items[item]) {
	case SW_NICKNAME:
		s = *host->nickName ? host->nickName : host->hostSub.userName;
		break;
	case SW_USER:
		s = host->hostSub.userName;
		break;
	case SW_ABSENCE:
		s = (host->hostStatus & IPMSG_ABSENCEOPT) ? "*" : "";
		break;
	case SW_PRIORITY:
		s = buf;
		if (host->priority == DEFAULT_PRIORITY) s[0] = '-', s[1] = 0;
		else if (host->priority <= 0) s[0] = 'X', s[1] = 0;
		else wsprintf(s, "%d", cfg->PriorityMax - (host->priority - DEFAULT_PRIORITY) / PRIORITY_OFFSET);
		break;
	case SW_GROUP:
		s = host->groupName;
		break;
	case SW_HOST:
		s = host->hostSub.hostName;
		break;
	case SW_IPADDR:
		s = inet_ntoa(*(LPIN_ADDR)&host->hostSub.addr);
		break;
	default:
		return	NULL;
	}
	U8toW(s, wbuf, MAX_LISTBUF);
	return	wbuf;
}


/*
	Notify Event CallBack
*/
BOOL TSendDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	switch (pNmHdr->code) {
	case LVN_COLUMNCLICK:
		{
			NM_LISTVIEW	*nmLv = (NM_LISTVIEW *)pNmHdr;
			if (sortItem == items[nmLv->iSubItem]) {
				if (!(sortRev = !sortRev) && sortItem == 0)
					sortItem = -1;
			}
			else {
				sortItem = items[nmLv->iSubItem];
				sortRev = FALSE;
			}
			ReregisterEntry();
		}
		return	TRUE;

	case LVN_GETDISPINFOW:
		{
			LV_DISPINFOW	*dispInfo = (LV_DISPINFOW *)pNmHdr;
			Host	*host = (Host *)dispInfo->item.lParam;

			dispInfo->item.pszText = GetListItemStrW(host, dispInfo->item.iSubItem);
		}
		return	TRUE;

	case LVN_ENDSCROLL:	// XP の対策
		::InvalidateRect(editSub.hWnd, NULL, TRUE);
		::UpdateWindow(editSub.hWnd);
		return	TRUE;

	case EN_LINK:
		{
			ENLINK	*el = (ENLINK *)pNmHdr;
			if (el->msg == WM_LBUTTONDOWN) {
//				Debug("EN_LINK (%d %d)\n", el->chrg.cpMin, el->chrg.cpMax);
				editSub.SendMessageV(EM_EXSETSEL, 0, (LPARAM)&el->chrg);
//				editSub.EventUser(WM_EDIT_DBLCLK, 0, 0);
			}
		}
		return	TRUE;
	}

//	Debug("code=%d info=%d infow=%d clk=%d\n", pNmHdr->code, LVN_GETDISPINFO, LVN_GETINFOTIPW, LVN_COLUMNCLICK);

	return	FALSE;
}

/*
	WM_MOUSEMOVE CallBack
*/
BOOL TSendDlg::EvMouseMove(UINT fwKeys, POINTS pos)
{

	if ((fwKeys & MK_LBUTTON) && captureMode)
	{
		if (lastYPos == (int)pos.y)
			return	TRUE;
		lastYPos = (int)pos.y;

		RECT	tmpRect;
		int		min_y = (5 * (item[refresh_item].y + item[refresh_item].cy) - 3 * item[separate_item].y) / 2;

		if (pos.y < min_y)
			pos.y = min_y;

		currentMidYdiff += (int)(short)(pos.y - dividYPos);
		EvSize(SIZE_RESTORED, 0, 0);
		GetWindowRect(&tmpRect);
		MoveWindow(tmpRect.left, tmpRect.top, tmpRect.right - tmpRect.left, tmpRect.bottom - tmpRect.top, TRUE);
		dividYPos = (int)pos.y;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TSendDlg::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_LBUTTONUP:
		if (captureMode)
		{
			captureMode = FALSE;
			::ReleaseCapture();
			return	TRUE;
		}
		break;
	}
	return	FALSE;
}

/*
	Size 変更
*/
BOOL TSendDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType != SIZE_RESTORED && fwSizeType != SIZE_MAXIMIZED)
		return	FALSE;

	GetWindowRect(&rect);
	int	xdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	int ydiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

	HDWP	hdwp = ::BeginDeferWindowPos(max_senditem);
	WINPOS	*wpos;
	BOOL	isFileBtn = shareInfo && shareInfo->fileCnt > 0 ? TRUE : FALSE;
	UINT	dwFlg = (IsNewShell() ? SWP_SHOWWINDOW : SWP_NOREDRAW) | SWP_NOZORDER;
	if (!hdwp)
		return	FALSE;

// サイズが小さくなる場合の調整値は、Try and Error(^^;
	wpos = &item[host_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, hostListView.hWnd, NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy + currentMidYdiff, dwFlg)))
		return	FALSE;

	wpos = &item[member_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(MEMBERCNT_TEXT), NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff / 2), wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[refresh_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(REFRESH_BUTTON), NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff * 2 / 3), wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[menu_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(MENU_CHECK), NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff * 2 / 3), wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[file_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(FILE_BUTTON), NULL, wpos->x, wpos->y + currentMidYdiff, wpos->cx + xdiff, wpos->cy, isFileBtn ? dwFlg : (SWP_HIDEWINDOW|SWP_NOZORDER))))
		return	FALSE;

	wpos = &item[edit_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, editSub.hWnd, NULL, wpos->x, (isFileBtn ? wpos->y : item[file_item].y) + currentMidYdiff, wpos->cx + xdiff, wpos->cy + ydiff - currentMidYdiff + (isFileBtn ? 0 : wpos->y - item[file_item].y), dwFlg)))
		return	FALSE;

	wpos = &item[ok_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IDOK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 6 / 7), wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[passwd_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(PASSWORD_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[secret_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(SECRET_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[separate_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, separateSub.hWnd, NULL, wpos->x, wpos->y + currentMidYdiff, wpos->cx + xdiff, wpos->cy, dwFlg)))
		return	FALSE;

	EndDeferWindowPos(hdwp);

	if (!IsNewShell())
		::InvalidateRgn(hWnd, NULL, TRUE);
	else if (captureMode)
	{
		::InvalidateRgn(GetDlgItem(PASSWORD_CHECK), NULL, TRUE);
		::InvalidateRgn(GetDlgItem(SECRET_CHECK), NULL, TRUE);
		::InvalidateRgn(GetDlgItem(IDOK), NULL, TRUE);
	}

	return	TRUE;
}

/*
	最大/最小 Size 設定
*/
BOOL TSendDlg::EvGetMinMaxInfo(MINMAXINFO *info)
{
	info->ptMinTrackSize.x = (orgRect.right - orgRect.left) * 2 / 3;
	info->ptMinTrackSize.y = (item[separate_item].y + item[separate_item].cy + currentMidYdiff) + (shareInfo && shareInfo->fileCnt ? 130 : 95);
	info->ptMaxTrackSize.y = 10000;		//y方向の制限を外す

	return	TRUE;
}

/*
	Context Menu event call back
*/
BOOL TSendDlg::EvContextMenu(HWND childWnd, POINTS pos)
{
	PopupContextMenu(pos);
	return	TRUE;
}

BOOL TSendDlg::EvMeasureItem(UINT ctlID, MEASUREITEMSTRUCT *lpMis)
{
	return	FALSE;
}

BOOL TSendDlg::EvDrawItem(UINT ctlID, DRAWITEMSTRUCT *lpDis)
{
	return	FALSE;
}

/*
	User定義 Event CallBack
*/
BOOL TSendDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DELAYSETTEXT:
		SetQuoteStr(msg.msgBuf, cfg->QuoteStr);
		::SetFocus(editSub.hWnd);
		return	TRUE;

	case WM_SENDDLG_RESIZE:
		if (!captureMode)
		{
			POINT	pt;
			captureMode = TRUE;
			::SetCapture(hWnd);
			::GetCursorPos(&pt);
			::ScreenToClient(hWnd, &pt);
			dividYPos = pt.y;
			lastYPos = 0;
		}
		return	TRUE;
	}
	return	FALSE;
}

/*
	WM_TIMER event call back
	送信確認/再送用
*/
BOOL TSendDlg::EvTimer(WPARAM _timerID, TIMERPROC proc)
{
	if (IsSendFinish())
	{
		::KillTimer(hWnd, IPMSG_SEND_TIMER);
		if (timerID == IPMSG_DUMMY_TIMER)	// 再入よけ
			return	FALSE;
		timerID = IPMSG_DUMMY_TIMER;
		::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, (LPARAM)this);
		return	TRUE;
	}
	if (retryCnt++ <= cfg->RetryMax)
	{
		SendMsgCore();
		return	TRUE;
	}

	::KillTimer(hWnd, IPMSG_SEND_TIMER);
	char *buf = new char [MAX_UDPBUF];
	*buf = 0;

	for (int cnt=0; cnt < sendEntryNum; cnt++)
	{
		if (sendEntry[cnt].Status() != ST_DONE)
		{
			MakeListString(cfg, sendEntry[cnt].Host(), buf + strlen(buf));
			strcat(buf, "\r\n");
		}
	}
	strcat(buf, GetLoadStrU8(IDS_RETRYSEND));
	int ret = MessageBoxU8(buf, MSG_STR, MB_RETRYCANCEL|MB_ICONINFORMATION);
	delete [] buf;

	if (ret == IDRETRY && !IsSendFinish())
	{
		retryCnt = 0;
		SendMsgCore();
		timerID = IPMSG_SEND_TIMER;
		if (::SetTimer(hWnd, IPMSG_SEND_TIMER, cfg->RetryMSec, NULL) == 0)
			::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, (LPARAM)this);
	}
	else
		::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, (LPARAM)this);

	return	TRUE;
}


/*
	送信中は、Showのvisibleをはじく
*/
void TSendDlg::Show(int mode)
{
	if (timerID == 0 && hWnd)
		TWin::Show(mode);
}


/*
	引用mark をつけて、EditControlに張り付け
*/
void TSendDlg::SetQuoteStr(LPSTR str, LPCSTR quoteStr)
{
	Wstr	wbuf(str);
	Wstr	wquote(quoteStr);
	VBuf	vbuf(MAX_UDPBUF * 3);
	WCHAR	*s = wbuf.Buf();
	WCHAR	*d = (WCHAR *)vbuf.Buf();
	BOOL	is_quote = TRUE;
	int		quote_len = wcslen(wquote);

	for (int i=0; i < MAX_UDPBUF && *s; i++) {
		if (is_quote) {
			wcscpy(d, wquote);
			d += quote_len;
			is_quote = FALSE;
		}
		if ((*d++ = *s++) == '\n') is_quote = TRUE;
	}
	*d = 0;

	editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)vbuf.Buf());
}

/*
	HostEntryの追加
*/
#define MAX_ICON	5
void TSendDlg::AddHost(Host *host)
{
	if (IsSending() || host->priority <= 0 && !hiddenDisp)
		return;

	LV_ITEM	lvi;

	lvi.mask = LVIF_TEXT|LVIF_PARAM|(lvStateEnable ? LVIF_STATE : LVIF_IMAGE);
	lvi.iItem = GetInsertIndexPoint(host);
	lvi.iSubItem = 0;
	int	state = ((host->hostStatus & IPMSG_ABSENCEOPT) ? 0 : 1) | ((host->hostStatus & IPMSG_ENCRYPTOPT) ? 4 : ((host->hostStatus & IPMSG_FILEATTACHOPT) ? 2 : 0));
	lvi.state = INDEXTOSTATEIMAGEMASK((state + 1) % (MAX_ICON + 1));
	lvi.stateMask = LVIS_STATEIMAGEMASK;
	lvi.pszText = (char *)GetListItemStrW(host, 0);
	lvi.cchTextMax = 0;
	lvi.iImage = 0;
	lvi.lParam = (LPARAM)host;

	listOperateCnt++;
	int		index;
	if ((index = (int)hostListView.SendMessage(LVM_INSERTITEMW, 0, (LPARAM)&lvi)) >= 0)
	{
#define BIG_ALLOC	100
		if ((memberCnt % BIG_ALLOC) == 0)
			hostArray = (Host **)realloc(hostArray, (memberCnt + BIG_ALLOC) * sizeof(Host *));
		memmove(hostArray + index +1, hostArray + index, (memberCnt++ - index) * sizeof(Host *));
		hostArray[index] = host;
		DisplayMemberCnt();
	}
	listOperateCnt--;
}

/*
	HostEntryの修正
*/
void TSendDlg::ModifyHost(Host *host)
{
	DelHost(host);
	AddHost(host);		//将来、選択を解除せずにしたい...
}

/*
	HostEntryの削除
*/
void TSendDlg::DelHost(Host *host)
{
	if (IsSending())
		return;

	listOperateCnt++;

	int		index;

	for (index=0; index < memberCnt; index++)
		if (hostArray[index] == host)
			break;

	if (index < memberCnt && hostListView.SendMessage(LVM_DELETEITEM, index, 0) != LB_ERR)
	{
		memmove(hostArray + index, hostArray + index +1, (memberCnt - index -1) * sizeof(Host *));
		memberCnt--;
		DisplayMemberCnt();
	}
	listOperateCnt--;
}

void TSendDlg::DelAllHost(void)
{
	if (IsSending())
		return;
	hostListView.SendMessage(LVM_DELETEALLITEMS, 0, 0);
	free(hostArray);
	hostArray = NULL;
	memberCnt = 0;
	DisplayMemberCnt();
}

void TSendDlg::ReregisterEntry()
{
	DelAllHost();
	for (int cnt=0; cnt < hosts->HostCnt(); cnt++)
		AddHost(hosts->GetHost(cnt));
}

/*
	HostEntryへの挿入index位置を返す
*/
UINT TSendDlg::GetInsertIndexPoint(Host *host)
{
	int		index, min = 0, max = memberCnt -1, ret;

	while (min <= max)
	{
		index = (min + max) / 2;

		if ((ret = CompareHosts(host, hostArray[index])) > 0)
			min = index +1;
		else if (ret < 0)
			max = index -1;
		else {	// 無い筈
			min = index;
			break;
		}
	}

	return	min;
}

/*
	二つのHostの比較 ... binary search用
*/
#define IS_KANJI(x) ((x) & 0x80)		//なんと手抜きマクロ(^^;

int TSendDlg::CompareHosts(Host *host1, Host *host2)
{
	int		ret = 0;

	if (host1->hostStatus & IPMSG_SERVEROPT)	// serverは常に最後尾に
		return	1;
	if (host2->hostStatus & IPMSG_SERVEROPT)	// ちょっと手抜き判断(^^;
		return	-1;

	if (sortItem != -1) {
		switch (sortItem) {
		case SW_NICKNAME:
			ret = strcmp(*host1->nickName ? host1->nickName : host1->hostSub.userName, *host2->nickName ? host2->nickName : host2->hostSub.userName); break;
		case SW_ABSENCE:	// 今のところ、通らず
			ret = (host1->hostStatus & IPMSG_ABSENCEOPT) > (host2->hostStatus & IPMSG_ABSENCEOPT) ? 1 : (host1->hostStatus & IPMSG_ABSENCEOPT) < (host2->hostStatus & IPMSG_ABSENCEOPT) ? -1 : 0; break;
		case SW_GROUP:
			ret = strcmp(*host1->groupName ? host1->groupName : "\xff", *host2->groupName ? host2->groupName : "\xff"); break;
		case SW_HOST:
			ret = strcmp(host1->hostSub.hostName, host2->hostSub.hostName); break;
		case SW_IPADDR:
			ret = ntohl(host1->hostSub.addr) > ntohl(host2->hostSub.addr) ? 1 : -1;
			break;
		case SW_USER:
			ret = strcmp(host1->hostSub.userName, host2->hostSub.userName); break;
		case SW_PRIORITY:
			ret = host1->priority > host2->priority ? 1 : host1->priority < host2->priority ? -1 : 0; break;
		}
		if (ret)
			return	sortRev ? -ret : ret;
	}

	if (host1->priority < host2->priority)
		ret = 1;
	else if (host1->priority > host2->priority)
		ret = -1;
	else if ((ret = GroupCompare(host1, host2)) == 0)
		ret = SubCompare(host1, host2);

	return	(cfg->Sort & IPMSG_ALLREVSORTOPT) || sortRev ? -ret : ret;
}

/*
	二つのHostの比較 Sub routine
*/
int TSendDlg::GroupCompare(Host *host1, Host *host2)
{
	int	ret = 0;

	if (!(cfg->Sort & IPMSG_NOGROUPSORTOPT) && *cfg->GroupNameStr)
	{
		if ((ret = strcmp(host1->groupName, host2->groupName)) != 0)
		{
			if (strcmp(host1->groupName, cfg->GroupNameStr) == 0)
				ret = -1;
			else if (strcmp(host2->groupName, cfg->GroupNameStr) == 0)
				ret = 1;
			else
			{
				if (*host1->groupName == 0)
					ret = 1;
				else if (*host2->groupName == 0)
					ret = -1;
				else if (!(cfg->Sort & IPMSG_NOKANJISORTOPT))
				{
					if (IS_KANJI(*host1->groupName) && !IS_KANJI(*host2->groupName))
						ret = -1;
					else if (!IS_KANJI(*host1->groupName) && IS_KANJI(*host2->groupName))
						ret = 1;
				}
				ret = (cfg->Sort & IPMSG_GROUPREVSORTOPT) ? -ret : ret;
			}
		}
	}
	return	ret;
}

int TSendDlg::SubCompare(Host *host1, Host *host2)
{
	int ret = 0, (*StrCmp)(const char*, const char*) = (cfg->Sort & IPMSG_ICMPSORTOPT) ? stricmp : strcmp;

	switch (GET_MODE(cfg->Sort))
	{
	case IPMSG_NAMESORT: default:
		char	*name1, *name2;

		name1 = *host1->nickName ? host1->nickName : host1->hostSub.userName;
		name2 = *host2->nickName ? host2->nickName : host2->hostSub.userName;
		if (!(cfg->Sort & IPMSG_NOKANJISORTOPT))
		{
			if (IS_KANJI(*name1) && !IS_KANJI(*name2))
				ret = -1;
			if (!IS_KANJI(*name1) && IS_KANJI(*name2))
				ret = 1;
		}
		if (ret == 0)
			if ((ret = StrCmp(name1, name2)) == 0)
				if ((ret = StrCmp(host1->hostSub.hostName, host2->hostSub.hostName)) == 0)
					ret = StrCmp(host1->hostSub.userName, host2->hostSub.userName);
		break;

	case IPMSG_HOSTSORT:
		if (!(cfg->Sort & IPMSG_NOKANJISORTOPT))
		{
			if (IS_KANJI(*host1->hostSub.hostName) && !IS_KANJI(*host2->hostSub.hostName))
				ret = 1;
			if (!IS_KANJI(*host1->hostSub.hostName) && IS_KANJI(*host2->hostSub.hostName))
				ret = -1;
		}
		if (ret == 0)
			ret = StrCmp(host1->hostSub.hostName, host2->hostSub.hostName);
		if (ret)
			break;

		// host名で比較がつかないときは IPADDRSORTに従う
		// このまま 下に落ちる

	case IPMSG_IPADDRSORT:
		if (ntohl(host1->hostSub.addr) > ntohl(host2->hostSub.addr))
			ret = 1;
		else
			ret = -1;
		break;
	}
	return	(cfg->Sort & IPMSG_SUBREVSORTOPT) ? -ret : ret;
}

/*
	ListBox内の指定hostを選択
	force = TRUE の場合、既選択項目がクリアされる
*/
void TSendDlg::SelectHost(HostSub *hostSub, BOOL force)
{
	int		index;
	LV_ITEM	lvi;
	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED|LVIS_SELECTED;

	for (index=0; index < memberCnt; index++) {
		if (hostSub->addr == hostArray[index]->hostSub.addr && hostSub->portNo == hostArray[index]->hostSub.portNo) {
			lvi.state = LVIS_FOCUSED|LVIS_SELECTED;
			hostListView.SendMessage(LVM_SETITEMSTATE, index, (LPARAM)&lvi);
			hostListView.SendMessage(LVM_ENSUREVISIBLE, index, 0);
			hostListView.SendMessage(LVM_SETSELECTIONMARK, 0, index);
			hostListView.SetFocusIndex(index);
			if (!force)
				return;
		}
		else if (force) {
			lvi.state = 0;
			hostListView.SendMessage(LVM_SETITEMSTATE, index, (LPARAM)&lvi);
		}
	}
}

/*
	Member数を表示
*/
void TSendDlg::DisplayMemberCnt(void)
{
	static char	buf[MAX_LISTBUF];
	static char	*append_point;

	if (*buf == 0) {
		strcpy(buf, GetLoadStrU8(IDS_USERNUM));
	}

	if (!append_point)
		append_point = buf + strlen(buf);

	wsprintf(append_point, "%d", memberCnt);
	SetDlgItemTextU8(MEMBERCNT_TEXT, buf);
}

/*
	通常送信
*/
BOOL TSendDlg::SendMsg(void)
{
	ULONG	command = IPMSG_SENDMSG|IPMSG_SENDCHECKOPT;

	BOOL	ctl_on = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
	BOOL	shift_on = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
	BOOL	rbutton_on = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? TRUE : FALSE;
	BOOL	is_bmp_only = FALSE, is_clip_enable = FALSE;
	int		i, storeCnt;

	if (ctl_on && shift_on)
		command = rbutton_on ? IPMSG_GETINFO : IPMSG_GETABSENCEINFO;
	else if (ctl_on || shift_on || rbutton_on)
		return	FALSE;

	if (listOperateCnt) {
		MessageBoxU8(GetLoadStrU8(IDS_BUSYMSG));
		return	FALSE;
	}

	if ((sendEntryNum = (int)hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0)) <= 0 || !(sendEntry = new SendEntry [sendEntryNum]))
		return	FALSE;

	for (i=0, storeCnt=0; i < memberCnt && storeCnt < sendEntryNum; i++) {
		if ((hostListView.SendMessage(LVM_GETITEMSTATE, i, LVIS_SELECTED) & LVIS_SELECTED) == 0)
			continue;
		if ((cfg->ClipMode & CLIP_ENABLE) && (hostArray[i]->hostStatus & IPMSG_CLIPBOARDOPT))
			is_clip_enable = TRUE;
		sendEntry[storeCnt++].SetHost(hostArray[i]);
	}
	if (storeCnt == 0) {
		delete [] sendEntry;
		return	FALSE;
	}

	timerID = IPMSG_DUMMY_TIMER;	// この時点で送信中にする

	TWin::Show(SW_HIDE);
	::SendMessage(GetMainWnd(), WM_SENDDLG_HIDE, 0, (LPARAM)this);

	if (is_clip_enable) {
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

		for (i=0; i < cfg->ClipMax; i++) {
			int		pos = 0;
			VBuf	*vbuf = editSub.GetPngByte(i, &pos);
			if (vbuf) {
				if (!shareInfo) {
					shareInfo = shareMng->CreateShare(packetNo);
					is_bmp_only = TRUE;
				}
				char	fname[MAX_PATH];
				MakeClipFileName(msgMng->MakePacketNo(), TRUE, fname);
				if (shareMng->AddMemShare(shareInfo, fname, vbuf->Buf(), vbuf->Size(), pos)) {
					if (cfg->LogCheck && (cfg->ClipMode & CLIP_SAVE)) {
						FileInfo *fileInfo = shareInfo->fileInfo[shareInfo->fileCnt -1];
						SaveImageFile(cfg, fileInfo->Fname(), vbuf);
					}
				}
				delete vbuf;
			}
		}
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	}

	if (::IsDlgButtonChecked(hWnd, SECRET_CHECK) != 0)
		command |= IPMSG_SECRETEXOPT;
	if (::IsDlgButtonChecked(hWnd, PASSWORD_CHECK) != 0)
		command |= IPMSG_PASSWORDOPT;
	if (shareInfo && shareInfo->fileCnt)
		command |= IPMSG_FILEATTACHOPT;

//	editSub.GetWindowTextU8(msg.msgBuf, MAX_UDPBUF);
//	editSub.ExGetText(msg.msgBuf, MAX_UDPBUF);
	editSub.GetTextUTF8(msg.msgBuf, sizeof(msg.msgBuf), TRUE);

//	WCHAR	wbuf[MAX_UDPBUF];
//	memset(wbuf, 0, sizeof(wbuf));
//	editSub.GetStreamText(wbuf, MAX_UDPBUF, SF_UNICODE|SF_TEXT);
//	WtoU8(wbuf, msg.msgBuf, MAX_UDPBUF);

	logmng->WriteSendStart();

	if (sendEntryNum > 1)
		command |= IPMSG_MULTICASTOPT;

	// Addtional Option ... IPMSG_ENCRYPTOPT & IPMSG_UTF8OPT
	int		cryptCapa = GetLocalCapa(cfg);
	ULONG	opt = IPMSG_CAPUTF8OPT | IPMSG_UTF8OPT
					| ((command & IPMSG_FILEATTACHOPT) ? IPMSG_ENCEXTMSGOPT : 0)
					| (cryptCapa ? IPMSG_ENCRYPTOPT : 0);
	ULONG	opt_sum = 0;
	BOOL	use_sign = FALSE;

	for (i=0; i < storeCnt; i++) {
		char		hostStr[MAX_LISTBUF];
		Host		*host = sendEntry[i].Host();
		ULONG		opt_target = opt & host->hostStatus;

		if (opt_target & IPMSG_CAPUTF8OPT) opt_target |= IPMSG_UTF8OPT;
		opt_sum |= opt_target;
		if (GetUserNameDigestField(host->hostSub.userName)) use_sign = TRUE;
		MakeListString(cfg, host, hostStr);
		logmng->WriteSendHead(hostStr);

		sendEntry[i].SetStatus((opt_target & IPMSG_ENCRYPTOPT) == 0 ? ST_MAKEMSG :
					host->pubKey.Key() && host->pubKeyUpdated ? ST_MAKECRYPTMSG : ST_GETCRYPT);
		sendEntry[i].SetCommand(command | opt_target);
	}
	command |= opt_sum;

	logmng->WriteSendMsg(msg.msgBuf, command,
				(opt_sum & IPMSG_ENCRYPTOPT) == 0 ? 0 :
				IsUserNameExt(cfg) && use_sign ? LOG_SIGN_OK :
				(cryptCapa & (IPMSG_RSA_2048|IPMSG_RSA_1024)) ? LOG_ENC1 : LOG_ENC0,
			shareInfo);

	if (shareInfo && shareInfo->fileCnt)		// ...\0no:fname:size:mtime:
	{
		char	buf[MAX_UDPBUF / 2];
		EncodeShareMsg(shareInfo, buf, sizeof(buf), TRUE);
		shareStr = strdup(buf);

	//	EncodeShareMsg(shareInfo, buf, sizeof(buf), TRUE);
	//	shareExStr = strdup(buf);

		shareMng->AddHostShare(shareInfo, sendEntry, sendEntryNum);
	}

	SendMsgCore();

	timerID = IPMSG_SEND_TIMER;
	if (::SetTimer(hWnd, IPMSG_SEND_TIMER, cfg->RetryMSec, NULL) == 0)
		::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, (LPARAM)this);

	return	TRUE;
}

/*
	メッセージの暗号化
*/
BOOL TSendDlg::MakeMsgPacket(SendEntry *entry)
{
	char	*tmpbuf = new char[MAX_UDPBUF];
	char	*buf = new char[MAX_UDPBUF];
	char	*target_msg = (entry->Command() & IPMSG_UTF8OPT) ? msg.msgBuf : U8toA(msg.msgBuf);
	int		msgLen;
	BOOL	ret = FALSE;

	if (entry->Status() == ST_MAKECRYPTMSG) {
		ret = MakeEncryptMsg(entry, target_msg, tmpbuf);
	}
	if (!ret) {
		entry->SetCommand(entry->Command() & ~IPMSG_ENCRYPTOPT);
		strncpyz(tmpbuf, target_msg, MAX_UDPBUF);
	}

	msgMng->MakeMsg(buf, packetNo, entry->Command(), tmpbuf,
		(entry->Command() & IPMSG_ENCEXTMSGOPT) ? NULL : shareStr, &msgLen);
	entry->SetMsg(buf, msgLen);
	entry->SetStatus(ST_SENDMSG);

	delete [] buf;
	delete [] tmpbuf;
	return	TRUE;
}

BOOL TSendDlg::MakeEncryptMsg(SendEntry *entry, char *msgstr, char *buf)
{
	HCRYPTKEY	hExKey = 0, hKey = 0;
	Host		*host = entry->Host();
	BYTE		skey[MAX_BUF], data[MAX_UDPBUF], msg_data[MAX_UDPBUF], iv[256/8];
	int			len = 0, shareStrLen = 0;
	int			capa = host->pubKey.Capa() & GetLocalCapa(cfg);
	int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 : (capa & IPMSG_RSA_1024) ? KEY_1024 : KEY_512;
	HCRYPTPROV	target_csp = cfg->priv[kt].hCsp;
	DWORD 		msgLen, encMsgLen;
	int			max_body_size = MAX_UDPBUF - MAX_BUF;
	double		stringify_coef = 0.0;
	int			(*bin2str_revendian)(const BYTE *, int, char *) = NULL;
	int			(*bin2str)(const BYTE *, int, char *)           = NULL;

	// すべてのマトリクスはサポートしない（特定の組み合わせのみサポート）
	if ((capa & IPMSG_RSA_2048) && (capa & IPMSG_AES_256)) {
		capa &= IPMSG_RSA_2048 | IPMSG_AES_256 | IPMSG_SIGN_SHA1 | IPMSG_PACKETNO_IV | IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_1024) && (capa & IPMSG_BLOWFISH_128)) {
		capa &= IPMSG_RSA_1024 | IPMSG_BLOWFISH_128 | IPMSG_PACKETNO_IV | IPMSG_ENCODE_BASE64;
	}
	else if ((capa & IPMSG_RSA_512) && (capa & IPMSG_RC2_40)) {
		capa &= IPMSG_RSA_512 | IPMSG_RC2_40 | IPMSG_ENCODE_BASE64;
	}
	else return	FALSE;

	if (capa & IPMSG_ENCODE_BASE64) {	// base64
#pragma warning ( disable : 4056 )
		stringify_coef = 4.0 / 3.0;
		bin2str_revendian = bin2b64str_revendian;
		bin2str = bin2b64str;
	}
	else {				// hex encoding
		stringify_coef = 2.0;
		bin2str_revendian = bin2hexstr_revendian;
		bin2str = bin2hexstr;
	}

	if (shareStr) {
		shareStrLen = strlen(shareStr) + 1;
		max_body_size -= shareStrLen;
	}
	max_body_size -= (int)(host->pubKey.KeyLen() * stringify_coef * ((capa & IPMSG_SIGN_SHA1) ? 2 : 1));
	max_body_size = (int)(max_body_size / stringify_coef);

	// KeyBlob 作成
	host->pubKey.KeyBlob(data, sizeof(data), &len);

	// 送信先公開鍵の import
	if (!pCryptImportKey(target_csp, data, len, 0, 0, &hExKey)) {
		host->pubKey.UnSet();
		return GetLastErrorMsg("CryptImportKey"), FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (capa & IPMSG_PACKETNO_IV) sprintf((char *)iv, "%u", packetNo);

	// UNIX 形式の改行に変換
	msgLen = MsgMng::LocalNewLineToUnix(msgstr, (char *)msg_data, max_body_size) + 1;
	if (shareStr && (entry->Command() & IPMSG_ENCEXTMSGOPT)) {
		memcpy(msg_data + msgLen, shareStr, shareStrLen);
		msgLen += shareStrLen;
	}

	if (capa & IPMSG_AES_256) {	// AES
		// AES 用ランダム鍵作成
		if (!pCryptGenRandom(target_csp, len = 256/8, data))
			return	GetLastErrorMsg("CryptGenRandom"), FALSE;

		// AES 用共通鍵のセット
		AES		aes(data, len);

		//共通鍵の暗号化
		if (!pCryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF))
			return GetLastErrorMsg("CryptEncrypt"), FALSE;
		bin2str_revendian(data, len, (char *)skey);	// 鍵をhex文字列に

		// メッセージ暗号化
		encMsgLen = aes.Encrypt(msg_data, data, msgLen, iv);
	}
	else if (capa & IPMSG_BLOWFISH_128) {	// blowfish
		// blowfish 用ランダム鍵作成
		if (!pCryptGenRandom(target_csp, len = 128/8, data))
			return	GetLastErrorMsg("CryptGenRandom"), FALSE;

		// blowfish 用共通鍵のセット
		CBlowFish	bl(data, len);

		//共通鍵の暗号化
		if (!pCryptEncrypt(hExKey, 0, TRUE, 0, data, (DWORD *)&len, MAX_BUF))
			return GetLastErrorMsg("CryptEncrypt"), FALSE;
		bin2str_revendian(data, len, (char *)skey);	// 鍵をhex文字列に

		// メッセージ暗号化
		encMsgLen = bl.Encrypt(msg_data, data, msgLen, BF_CBC|BF_PKCS5, iv);
	}
	else {	// RC2
		if (!pCryptGenKey(target_csp, CALG_RC2, CRYPT_EXPORTABLE, &hKey))
			return	GetLastErrorMsg("CryptGenKey"), FALSE;

		pCryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, NULL, (DWORD *)&len);
		if (!pCryptExportKey(hKey, hExKey, SIMPLEBLOB, 0, data, (DWORD *)&len))
			return GetLastErrorMsg("CryptExportKey"), FALSE;

		len -= SKEY_HEADER_SIZE;
		bin2str_revendian(data + SKEY_HEADER_SIZE, len, (char *)skey);
		memcpy(data, msg_data, msgLen);

		// メッセージの暗号化
		encMsgLen = msgLen;
		if (!pCryptEncrypt(hKey, 0, TRUE, 0, data, &encMsgLen, MAX_UDPBUF))
			return GetLastErrorMsg("CryptEncrypt RC2"), FALSE;
		pCryptDestroyKey(hKey);
	}
	len =  wsprintf(buf, "%X:%s:", capa, skey);
	len += bin2str(data, (int)encMsgLen, buf + len);

	pCryptDestroyKey(hExKey);

	// 電子署名の追加
	if (capa & IPMSG_SIGN_SHA1) {
		HCRYPTHASH	hHash;
		if (pCryptCreateHash(target_csp, CALG_SHA, 0, 0, &hHash)) {
			if (pCryptHashData(hHash, msg_data, msgLen, 0)) {
				DWORD		sigLen = 0;
				pCryptSignHash(hHash, AT_KEYEXCHANGE, 0, 0, 0, &sigLen);
				if (pCryptSignHash(hHash, AT_KEYEXCHANGE, 0, 0, data, &sigLen)) {
					buf[len++] = ':';
					len += bin2str_revendian(data, (int)sigLen, buf + len);
				}
			}
			pCryptDestroyHash(hHash);
		}
	}

	return TRUE;
}

/*
	通常送信Sub routine
*/
BOOL TSendDlg::SendMsgCore(void)
{
	for (int i=0; i < sendEntryNum; i++) {
		SendMsgCoreEntry(sendEntry + i);
	}
	return	TRUE;
}

/*
	通常送信Sub routine
*/
BOOL TSendDlg::SendMsgCoreEntry(SendEntry *entry)
{
	if (entry->Status() == ST_GETCRYPT) {
		char	capa_str[MAX_BUF];
		int		capa =GetLocalCapa(cfg);

		wsprintf(capa_str, "%x", capa);
		msgMng->Send(&entry->Host()->hostSub, IPMSG_GETPUBKEY, capa_str);
	}
	if (entry->Status() == ST_MAKECRYPTMSG || entry->Status() == ST_MAKEMSG) {
		MakeMsgPacket(entry);		// ST_MAKECRYPTMSG/ST_MAKEMSG -> ST_SENDMSG
	}
	if (entry->Status() == ST_SENDMSG) {
		msgMng->UdpSend(entry->Host()->hostSub.addr, entry->Host()->hostSub.portNo, entry->Msg(), entry->MsgLen());
	}
	return	TRUE;
}

inline char *strtoupper(char *buf, const char *org)
{
	BOOL dbcsFlg = FALSE;

	for (int i=0; buf[i] = org[i]; i++) {
		if (dbcsFlg)
			dbcsFlg = FALSE;
		else if (IsDBCSLeadByte(org[i]))
			dbcsFlg = TRUE;
		else
			buf[i] = toupper(org[i]);
	}
	return	buf;
}

/*
	検索
*/
BOOL TSendDlg::FindHost(char *findStr, BOOL isAllfind)
{
	int		startNo = hostListView.GetFocusIndex() + 1;
	char	find_buf[MAX_NAMEBUF], buf[MAX_NAMEBUF];

	if (*findStr == '\0')
		return	FALSE;

	strtoupper(find_buf, findStr);

	for (int i=0; i < memberCnt; i++) {
		int		index = (i + startNo) % memberCnt;
		if (*hostArray[index]->nickName)
			strtoupper(buf, hostArray[index]->nickName);
		else
			strtoupper(buf, hostArray[index]->hostSub.userName);

		if (strstr(buf, find_buf) || isAllfind &&
			(strstr(strtoupper(buf, hostArray[index]->groupName), find_buf)
			|| strstr(strtoupper(buf, hostArray[index]->hostSub.hostName), find_buf)
			|| strstr(strtoupper(buf, hostArray[index]->hostSub.userName), find_buf))) {
			SelectHost(&hostArray[index]->hostSub, TRUE);
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	送信終了通知
	packet_noが、この SendDialogの送った送信packetであれば、TRUE
*/
BOOL TSendDlg::SendFinishNotify(HostSub *hostSub, ULONG packet_no)
{
	for (int i=0; i < sendEntryNum; i++)
	{
		if (sendEntry[i].Status() == ST_SENDMSG &&
			sendEntry[i].Host()->hostSub.addr == hostSub->addr &&
			sendEntry[i].Host()->hostSub.portNo == hostSub->portNo &&
			(packet_no == packetNo || packet_no == 0))
		{
			sendEntry[i].SetStatus(ST_DONE);
			if (GET_MODE(sendEntry[i].Command()) == IPMSG_SENDMSG &&
				(sendEntry[i].Command() & IPMSG_SECRETOPT)) {
				::SendMessage(GetMainWnd(), WM_HISTDLG_NOTIFY,
						(WPARAM)sendEntry[i].Host(), (LPARAM)packetNo);
			}
			if (IsSendFinish() && hWnd)		//再送MessageBoxU8を消す
			{
				HWND	hMessageWnd = ::GetNextWindow(hWnd, GW_HWNDPREV);
				if (hMessageWnd && ::GetWindow(hMessageWnd, GW_OWNER) == hWnd)
					::PostMessage(hMessageWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
			}
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	送信終了通知
	packet_noが、この SendDialogの送った送信packetであれば、TRUE
*/
BOOL TSendDlg::SendPubKeyNotify(HostSub *hostSub, BYTE *pubkey, int len, int e, int capa)
{
	for (int i=0; i < sendEntryNum; i++)
	{
		if (sendEntry[i].Status() == ST_GETCRYPT
			&& IsSameHostEx(&sendEntry[i].Host()->hostSub, hostSub))
		{
			if (!sendEntry[i].Host()->pubKey.Key()) {
				sendEntry[i].Host()->pubKey.Set(pubkey, len, e, capa);
				sendEntry[i].Host()->pubKeyUpdated = TRUE;
			}
			sendEntry[i].SetStatus(ST_MAKECRYPTMSG);
			SendMsgCoreEntry(sendEntry + i);
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	送信(確認)中かどうか
*/
BOOL TSendDlg::IsSending(void)
{
	return	timerID ? TRUE : FALSE;
}

/*
	送信終了したかどうか
*/
BOOL TSendDlg::IsSendFinish(void)
{
	BOOL	finish = TRUE;

	for (int i=0; i < sendEntryNum; i++)
	{
		if (sendEntry[i].Status() != ST_DONE)
		{
			finish = FALSE;
			break;
		}
	}

	return	finish;
}

/*
	Font 設定
*/
void TSendDlg::SetFont(void)
{
	HFONT	hDlgFont;

	if (!(hDlgFont = (HFONT)SendMessage(WM_GETFONT, 0, 0)))
		return;
	if (::GetObject(hDlgFont, sizeof(LOGFONT), (void *)&orgFont) == 0)
		return;

	if (*cfg->SendEditFont.lfFaceName == 0)	//初期データセット
		cfg->SendEditFont = orgFont;
	if (*cfg->SendListFont.lfFaceName == 0) {
		cfg->SendListFont = orgFont;
		char	*ex_font = GetLoadStrA(IDS_PROPORTIONALFONT);
		if (ex_font && *ex_font)
			strcpy(cfg->SendListFont.lfFaceName, ex_font);
	}

	if (*cfg->SendListFont.lfFaceName && (hDlgFont = ::CreateFontIndirect(&cfg->SendListFont)))
	{
		hostListView.SendMessage(WM_SETFONT, (WPARAM)hDlgFont, 0);
		if (hListFont)
			::DeleteObject(hListFont);
		hListFont = hDlgFont;
 	}
	hostListHeader.ChangeFontNotify();
	if (hostListView.SendMessage(LVM_GETITEMCOUNT, 0, 0) > 0) {	// EvCreate時は不要
		ReregisterEntry();
	}
	if (*cfg->SendEditFont.lfFaceName && (hDlgFont = ::CreateFontIndirect(&cfg->SendEditFont)))
	{
		editSub.SetFont(&cfg->SendEditFont);
//		editSub.SendMessage(WM_SETFONT, (WPARAM)hDlgFont, 0);
//		if (hEditFont)
//			::DeleteObject(hEditFont);
		hEditFont = hDlgFont;
	}

	static HIMAGELIST	himlState;
	static int			lfHeight;
	static HICON		hiconItem[MAX_ICON];

	if (!himlState || lfHeight != cfg->SendListFont.lfHeight) {
		if (!hiconItem[0]) {
			hiconItem[0] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)IPMSGV1ABS_ICON);
			hiconItem[1] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)IPMSGV1_ICON);
			hiconItem[2] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)FILEABS_ICON);
			hiconItem[3] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)FILE_ICON);
			hiconItem[4] = ::LoadIcon(TApp::GetInstance(), (LPCSTR)ABSENCE_ICON);
		}
		if (himlState)
			ImageList_Destroy(himlState);
		lfHeight = cfg->SendListFont.lfHeight;
		himlState = ImageList_Create(abs(cfg->SendListFont.lfHeight) / 4, abs(cfg->SendListFont.lfHeight), TRUE, 1, 1);
		for (int i=0; i < MAX_ICON; i++)
			ImageList_AddIcon(himlState, hiconItem[i]);
	}
	hostListView.SendMessage(LVM_SETIMAGELIST, LVSIL_STATE, (LPARAM)himlState);
	if (hostListView.SendMessage(LVM_GETIMAGELIST, LVSIL_STATE, 0))
		lvStateEnable = TRUE;
	else {
		hostListView.SendMessage(LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himlState);
		lvStateEnable = FALSE;
	}
}

/*
	Size 設定
*/
void TSendDlg::SetSize(void)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);

	::GetWindowPlacement(hostListView.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[host_item]);

	::GetWindowPlacement(GetDlgItem(MEMBERCNT_TEXT), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[member_item]);

	::GetWindowPlacement(GetDlgItem(REFRESH_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[refresh_item]);

	::GetWindowPlacement(GetDlgItem(MENU_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[menu_item]);

	::GetWindowPlacement(editSub.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[edit_item]);

	::GetWindowPlacement(GetDlgItem(IDOK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[ok_item]);

	::GetWindowPlacement(GetDlgItem(FILE_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[file_item]);

	::GetWindowPlacement(GetDlgItem(SECRET_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[secret_item]);

	::GetWindowPlacement(GetDlgItem(PASSWORD_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[passwd_item]);

	::GetWindowPlacement(separateSub.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[separate_item]);

	GetWindowRect(&rect);
	orgRect = rect;

	RECT	scRect;
	GetCurrentScreenSize(&scRect, hRecvWnd);

	int	cx = scRect.right - scRect.left, cy = scRect.bottom - scRect.top;
	int	xsize = rect.right - rect.left + cfg->SendXdiff, ysize = rect.bottom - rect.top + cfg->SendYdiff;
	int	x = cfg->SendXpos, y = cfg->SendYpos;

	if (cfg->SendSavePos == 0)
	{
		x = (cx - xsize)/2 + (rand() % (cx/4)) - cx/8;
		y = (cy - ysize)/2 + (rand() % (cy/4)) - cy/8;
	}
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	EvSize(SIZE_RESTORED, 0, 0);
	MoveWindow(max(x, scRect.left), max(y, scRect.top), xsize, ysize, TRUE);
}

void TSendDlg::PopupContextMenu(POINTS pos)
{
	HMENU	hMenu = ::CreatePopupMenu();
	HMENU	hPriorityMenu = ::CreateMenu();
	HMENU	hGroupMenu = ::CreateMenu();
	int		cnt;
	char	buf[MAX_BUF];
	int		selectNum = (int)hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0);
//	char	*appendStr = selectNum > 0 ? GetLoadStrU8(IDS_MOVETO) : GetLoadStrU8(IDS_SELECT);
	char	*appendStr = selectNum > 0 ? GetLoadStrU8(IDS_MOVETO) : GetLoadStrU8(IDS_MOVETO);
	u_int	flag = selectNum <= 0 ? MF_GRAYED : 0;

	if (!hMenu || !hPriorityMenu || !hGroupMenu)
		return;

	BOOL	isJapanese = IsLang(LANG_JAPANESE);

// priority menu
	for (cnt=cfg->PriorityMax; cnt >= 0; cnt--)
	{
		char	*ptr = buf;

		if (!isJapanese)
			ptr += wsprintf(ptr, "%s ", appendStr);

		if (cnt == 0)
			ptr += wsprintf(ptr, GetLoadStrU8(IDS_NODISP));
		else if (cnt == 1)
			ptr += wsprintf(ptr, GetLoadStrU8(IDS_DEFAULTDISP));
		else
			ptr += wsprintf(ptr, GetLoadStrU8(IDS_DISPPRIORITY), cfg->PriorityMax - cnt + 1);

		if (isJapanese)
			ptr += wsprintf(ptr, " %s ", appendStr);

		wsprintf(ptr, cnt == 1 ? GetLoadStrU8(IDS_MEMBERCOUNTDEF) : GetLoadStrU8(IDS_MEMBERCOUNT), hosts->PriorityHostCnt(cnt * PRIORITY_OFFSET, PRIORITY_OFFSET), cfg->priorityHosts.PriorityHostCnt(cnt * PRIORITY_OFFSET, PRIORITY_OFFSET));
		AppendMenuU8(hPriorityMenu, MF_STRING|flag, MENU_PRIORITY_START + cnt * PRIORITY_OFFSET, buf);
	}

	AppendMenuU8(hPriorityMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hPriorityMenu, MF_STRING|(hiddenDisp ? MF_CHECKED : 0), MENU_PRIORITY_HIDDEN, GetLoadStrU8(IDS_TMPNODISPDISP));
	AppendMenuU8(hPriorityMenu, MF_STRING, MENU_PRIORITY_RESET, GetLoadStrU8(IDS_RESETPRIORITY));
	AppendMenuU8(hMenu, MF_POPUP, (UINT)hPriorityMenu, GetLoadStrU8(IDS_SORTFILTER));
//	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);

// group select
	int	rowMax = ::GetSystemMetrics(SM_CYSCREEN) / ::GetSystemMetrics(SM_CYMENU) -1;

	for (cnt=0; cnt < memberCnt; cnt++)
	{
		int		menuMax = ::GetMenuItemCount(hGroupMenu), cnt2;

		for (cnt2=0; cnt2 < menuMax; cnt2++)
		{
			GetMenuStringU8(hGroupMenu, cnt2, buf, sizeof(buf), MF_BYPOSITION);
			if (strcmp(buf, hostArray[cnt]->groupName) == 0)
				break;
		}
		if (cnt2 == menuMax && *hostArray[cnt]->groupName)
			AppendMenuU8(hGroupMenu, MF_STRING|((menuMax % rowMax || menuMax == 0) ? 0 : MF_MENUBREAK), MENU_GROUP_START + menuMax, hostArray[cnt]->groupName);
	}
	AppendMenuU8(hMenu, MF_POPUP|(::GetMenuItemCount(hGroupMenu) ? 0 : MF_GRAYED), (UINT)hGroupMenu, GetLoadStrU8(IDS_GROUPSELECT));
//	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);

	SetMainMenu(hMenu);
	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);
}

BOOL IsImageInClipboard(HWND hWnd)
{
	if (!OpenClipboard(hWnd)) return FALSE;

	BOOL ret = FALSE;
	UINT fmt = 0;

	while ((fmt = EnumClipboardFormats(fmt))) {
		if (fmt == CF_BITMAP || fmt == CF_DIB || fmt == CF_DIBV5) {
			ret = TRUE;
			break;
		}
	}

	CloseClipboard();
	return	ret;
}

void TSendDlg::SetMainMenu(HMENU hMenu)
{
	AppendMenuU8(hMenu, MF_STRING, MENU_FINDDLG, GetLoadStrU8(IDS_FINDDLG));
	AppendMenuU8(hMenu, MF_STRING, MENU_FILEADD, GetLoadStrU8(IDS_FILEATTACHMENU));
	AppendMenuU8(hMenu, MF_STRING, MENU_FOLDERADD, GetLoadStrU8(IDS_FOLDERATTACHMENU));

	AppendMenuU8(hMenu, MF_STRING|(!IsImageInClipboard(hWnd) ? MF_DISABLED|MF_GRAYED : 0),
						MENU_IMAGEPASTE, GetLoadStrU8(IDS_IMAGEPASTEMENU));

	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING, MENU_SAVECOLUMN, GetLoadStrU8(IDS_SAVECOLUMN));

	AppendMenuU8(hMenu, MF_POPUP, (UINT)::LoadMenu(TApp::GetInstance(), (LPCSTR)SENDFONT_MENU), GetLoadStrU8(IDS_FONTSET));
	AppendMenuU8(hMenu, MF_POPUP, (UINT)::LoadMenu(TApp::GetInstance(), (LPCSTR)SIZE_MENU), GetLoadStrU8(IDS_SIZESET));
	AppendMenuU8(hMenu, MF_STRING, MENU_SAVEPOS, GetLoadStrU8(IDS_SAVEPOS));
	AppendMenuU8(hMenu, MF_STRING, MENU_MEMBERDISP, GetLoadStrU8(IDS_MEMBERDISP));
}

BOOL SetFileButton(TDlg *dlg, int buttonID, ShareInfo *info)
{
	char	buf[MAX_BUF] = "", fname[MAX_PATH_U8];
	int		offset = 0;
	for (int cnt=0; cnt < info->fileCnt; cnt++)
	{
		if (dlg->ResId() == SEND_DIALOG)
			ForcePathToFname(info->fileInfo[cnt]->Fname(), fname);
		else
			strncpyz(fname, info->fileInfo[cnt]->Fname(), MAX_PATH_U8);
		offset += wsprintf(buf + offset, "%s ", fname);
		if (offset + MAX_PATH_U8 >= sizeof(buf))
			break;
	}
	dlg->SetDlgItemTextU8(buttonID, buf);
	::ShowWindow(dlg->GetDlgItem(buttonID), info->fileCnt ? SW_SHOW : SW_HIDE);
	::EnableWindow(dlg->GetDlgItem(buttonID), info->fileCnt ? TRUE : FALSE);
	return	TRUE;
}

