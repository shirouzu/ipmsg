static char *senddlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   senddlg.cpp	Ver4.60";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Send Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */
#include "resource.h"
#include "ipmsg.h"

using namespace std;

/*
	SendDialog の初期化
*/
HFONT	TSendDlg::hEditFont	= NULL;
HFONT	TSendDlg::hListFont	= NULL;
LOGFONT	TSendDlg::orgFont	= {};

TSendDlg::TSendDlg(MsgMng *_msgmng, ShareMng *_shareMng, THosts *_hosts, Cfg *_cfg,
					LogMng *_logmng, ReplyInfo *rInfo, TWin *_parent) :
	TListDlg(SEND_DIALOG, _parent),
	editSub(_cfg, this),
	hostListView(this),
	hostListHeader(&hostListView),
	imageWin(_cfg, this),
	sendBtn(this),
	captureBtn(this),
	refreshBtn(this),
	menuCheck(this),
	memCntText(this),
	repFilCheck(this),
	retryDlg(this)
{
	TRecvDlg *recvDlg = rInfo ? rInfo->recvDlg : NULL;
	recvId			= recvDlg ? recvDlg->twinId : 0;
	hRecvWnd		= recvDlg ? recvDlg->hWnd : 0;
	posMode			= rInfo ? rInfo->posMode : ReplyInfo::NONE;
	foreDuration	= rInfo ? rInfo->foreDuration : 0;
	isMultiRecv		= rInfo ? rInfo->isMultiRecv : FALSE;
	cmdHWnd			= rInfo ? rInfo->cmdHWnd : NULL;
	cmdFlags		= rInfo ? rInfo->cmdFlags : NULL;

	msgMng			= _msgmng;
	shareMng		= _shareMng;
	shareInfo		= NULL;
	shareStr		= NULL;
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
	captureMode		= FALSE;
	listOperateCnt	= 0;
	hiddenDisp		= FALSE;
	repFilDisp		= FALSE;
	*selectGroup	= 0;
	*filterStr		= 0;
	currentMidYdiff	= cfg->SendMidYdiff;
	maxItems		= 0;
	lvStateEnable	= FALSE;
	sortItem		= -1;
	sortRev			= FALSE;
	findDlg			= NULL;
//	hCurMenu		= NULL;
	listConfirm		= FALSE;
	sendRecvList	= TRUE;

	msg.Init(recvDlg ? recvDlg->GetMsgBuf() : NULL);
	if (rInfo && rInfo->body) {
		strncpyz(msg.msgBuf, rInfo->body->s(), sizeof(msg.msgBuf));
	}

	if (rInfo) {
		if (rInfo->replyList) {
			replyList = *rInfo->replyList;
		}
		else if (rInfo->recvDlg) {
			replyList = rInfo->recvDlg->GetReplyList();
		}
		if (rInfo->fileList) {
			shareInfo = shareMng->CreateShare(packetNo);
			for (auto &u: *rInfo->fileList) {
				shareMng->AddFileShare(shareInfo, u->Buf());
			}
		}
	}
	if (replyList.size() == 0 && msg.hostSub.addr.IsEnabled()) {
		replyList.push_back(msg.hostSub);
	}

	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
}

/*
	SendDialog の終了
*/
TSendDlg::~TSendDlg()
{
	if (findDlg)
		delete findDlg;

	// ListView メモリリーク暫定対策...
	hostListView.DeleteAllItems();

	delete [] sendEntry;
	delete [] shareStr;

	if (hostArray) {
		free(hostArray);
	}
}

BOOL TSendDlg::PreProcMsg(MSG *msg)
{
	if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE) {
		return TRUE;
	}
	return	TDlg::PreProcMsg(msg);
}

#ifndef AW_HOR_POSITIVE
#define AW_HOR_POSITIVE 0x00000001
#define AW_HOR_NEGATIVE 0x00000002
#define AW_VER_POSITIVE 0x00000004
#define AW_VER_NEGATIVE 0x00000008
#define AW_CENTER 0x00000010
#define AW_HIDE 0x00010000
#define AW_ACTIVATE 0x00020000
#define AW_SLIDE 0x00040000
#define AW_BLEND 0x00080000
#endif


static HICON hRepFilIcon;
static HICON hRepFilRevIcon;

/*
	SendDialog 生成時の CallBack
*/
BOOL TSendDlg::EvCreate(LPARAM lParam)
{
	AttachItemWnd();

	if (cfg->ReplyFilter) {
		if (isMultiRecv && replyList.size() <= 1) {
			memCntText.CreateTipWnd(L"No multicast Info (old version message)");
		}
		else if (replyList.size() >= 1) {
			repFilCheck.Show();
			repFilDisp = TRUE;
			SetWindowText("Send Message (for Reply)");
			SetReplyInfoTip();
		}
	}

	SetDlgIcon(hWnd);

	for (auto &h: replyList) {
		if (Host *host = cfg->priorityHosts.GetHostByName(&h)) {
			if (host->priority <= 0) {
				hiddenDisp = TRUE;
				break;
			}
		}
	}

	HMENU	hMenu = ::GetSystemMenu(hWnd, FALSE);
	AppendMenuU8(hMenu, MF_SEPARATOR, NULL, NULL);
	SetMainMenu(hMenu);

	if (cfg->AbnormalButton) {
		sendBtn.SetWindowTextU8(LoadStrU8(IDS_FIRE));
	}

	if ((!cmdHWnd && cfg->SecretCheck) || (cmdHWnd && (cmdFlags & IPMSG_SEAL_FLAG))) {
		CheckDlgButton(SECRET_CHECK, cfg->SecretCheck);
	}
	else if (cfg->PasswordUse) {
		::EnableWindow(GetDlgItem(PASSWORD_CHECK), FALSE);
	}

	InitializeHeader();
	hostListView.LetterKey(cfg->LetterKey);

	SetFont();
	SetSize();
	ReregisterEntry();

	hostListView.SetFocus();

	if (replyList.size() > 0) {
		int		total_items = 0;
		for (auto &h: replyList) {
			if (SelectHost(&h, FALSE, FALSE)) {
				total_items++;
			}
		}
		if (total_items == 0) {	// 返信先が一人もいない場合は全員表示
			PostMessage(WM_COMMAND, REPFIL_CHK, 0);
		}
	}

	PostMessage(WM_DELAYSETTEXT, 0, 0);

	SetupItemIcons();

	::DragAcceptFiles(hWnd, msgMng->IsAvailableTCP());

	CheckDisp();
	DisplayMemberCnt();

	if (foreDuration) {	// ログビューアからのダブルクリック起動対策
		SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW);
		SetTimer(IPMSG_FOREDURATION_TIMER, foreDuration);
	}

	return	TRUE;
}

void TSendDlg::AttachItemWnd()
{
	editSub.AttachWnd(GetDlgItem(SEND_EDIT));
	memCntText.AttachWnd(GetDlgItem(MEMBERCNT_TEXT));

	sendBtn.AttachWnd(GetDlgItem(IDOK));
	sendBtn.CreateTipWnd();

	DWORD	add_style = LVS_EX_HEADERDRAGDROP|LVS_EX_FULLROWSELECT;
	if (cfg->GridLineCheck) add_style |= LVS_EX_GRIDLINES;
	hostListView.AttachWnd(GetDlgItem(HOST_LIST), add_style);
	hostListHeader.AttachWnd((HWND)hostListView.SendMessage(LVM_GETHEADER, 0, 0));

	captureBtn.AttachWnd(GetDlgItem(CAPTURE_BUTTON));
	captureBtn.CreateTipWnd(LoadStrW(IDS_IMAGERECTMENU));

	refreshBtn.AttachWnd(GetDlgItem(REFRESH_BUTTON));
	refreshBtn.CreateTipWnd(LoadStrW(IDS_REFRESH));

	menuCheck.AttachWnd(GetDlgItem(MENU_CHECK));
	menuCheck.CreateTipWnd(LoadStrW(IDS_SENDMENU));

	repFilCheck.AttachWnd(GetDlgItem(REPFIL_CHK));
	repFilCheck.CreateTipWnd(LoadStrW(IDS_REPLYFILTER));
}

void TSendDlg::SetupItemIcons()
{
	if (IsWinVista()) {
		ULONG_PTR	exStyle;

		exStyle = captureBtn.GetWindowLong(GWL_EXSTYLE) & ~WS_EX_STATICEDGE;
		captureBtn.SetWindowLong(GWL_EXSTYLE, exStyle);

		exStyle = refreshBtn.GetWindowLong(GWL_EXSTYLE) & ~WS_EX_STATICEDGE;
		refreshBtn.SetWindowLong(GWL_EXSTYLE, exStyle);
	}

	static HICON hRefreshIcon;
	if (!hRefreshIcon) hRefreshIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)REFRESH_ICON);
	refreshBtn.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hRefreshIcon);

	static HICON hMenuIcon;
	if (!hMenuIcon) hMenuIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)MENU_ICON);
	menuCheck.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hMenuIcon);

	static HICON hCameraIcon;
	if (!hCameraIcon) hCameraIcon = ::LoadIcon(TApp::hInst(),
			(LPCSTR)(DWORD_PTR)(IsWinVista() ? CAMERA_ICON : CAMERAXP_ICON));
	captureBtn.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hCameraIcon);
	if ((cfg->ClipMode & CLIP_ENABLE) == 0) {
		captureBtn.EnableWindow(FALSE);
	}

	if (!hRepFilIcon) {
		hRepFilIcon    = ::LoadIcon(TApp::hInst(), (LPCSTR)MULTI_ICON);
		hRepFilRevIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)MULTIREV_ICON);
	}
	repFilCheck.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hRepFilIcon);

/*
	static HICON hSendIcon;
	if (!hSendIcon) hSendIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)SEND_ICON);
	SendDlgItemMessage(IDOK, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hSendIcon);

	static HICON hSealIcon;
	if (!hSealIcon) hSealIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)SEAL_ICON);
	SendDlgItemMessage(SECRET_CHECK, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hSealIcon);

	static HICON hKeyIcon;
	if (!hKeyIcon) hKeyIcon = ::LoadIcon(TApp::hInst(), (LPCSTR)KEY_ICON);
	SendDlgItemMessage(PASSWORD_CHECK, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hKeyIcon);
*/
}


void TSendDlg::SetReplyInfoTip()
{
#define UNAMELIST_BUF	((MAX_ULIST + 1) * (MAX_LISTBUF + 100))
	Wstr	wulist(UNAMELIST_BUF);
	WCHAR	*wu = wulist.Buf();
	int		len = 0;

	*wu = 0;
	for (auto &h: replyList) {
		char	buf[MAX_LISTBUF];
		MakeListString(cfg, &h, hosts, buf);

		len += snwprintfz(wu + len, UNAMELIST_BUF-len, L" %s\n", U8toWs(buf));
		if (len > UNAMELIST_BUF - MAX_LISTBUF) {
			break;
		}
	}

	int		wlen = UNAMELIST_BUF + 100;
	Wstr	wbuf(wlen);
	snwprintfz(wbuf.Buf(), wlen, L"%s (%zd)\n%s", LoadStrW(IDS_ORGTOLIST), replyList.size(), wulist.s());
	memCntText.CreateTipWnd(wbuf.Buf(), 500, 30000);

	snwprintfz(wbuf.Buf(), wlen, L"%s (%zd)\n%s", LoadStrW(IDS_REPLYFILTER), replyList.size(), wulist.s());
	repFilCheck.CreateTipWnd(wbuf.Buf(), 500, 30000);
}

/*
	Construct/Rebuild Column Header
*/
void TSendDlg::InitializeHeader(void)
{
	int		order[MAX_SENDWIDTH] = {};
	int		revItems[MAX_SENDWIDTH] = {};

// カラムヘッダを全削除
	while (maxItems > 0) {
		hostListView.DeleteColumn(--maxItems);
	}

	ColumnItems = cfg->ColumnItems & ~(1 << SW_ABSENCE);
	memcpy(FullOrder, cfg->SendOrder, sizeof(FullOrder));

	for (int i=0; i < MAX_SENDWIDTH; i++) {
		if (GetItem(ColumnItems, i)) {
			items[maxItems] = i;
			revItems[i] = maxItems++;
		}
	}
	int		orderCnt = 0;
	for (int i=0; i < MAX_SENDWIDTH; i++) {
		if (GetItem(ColumnItems, FullOrder[i])) {
			order[orderCnt++] = revItems[FullOrder[i]];
		}
	}

	static char	*headerStr[MAX_SENDWIDTH];
	if (!headerStr[SW_NICKNAME]) {
		headerStr[SW_NICKNAME]	= LoadStrU8(IDS_USER);
		headerStr[SW_PRIORITY]	= LoadStrU8(IDS_PRIORITY);
		headerStr[SW_ABSENCE]	= LoadStrU8(IDS_ABSENCE);
		headerStr[SW_GROUP]		= LoadStrU8(IDS_GROUP);
		headerStr[SW_HOST]		= LoadStrU8(IDS_HOST);
		headerStr[SW_USER]		= LoadStrU8(IDS_LOGON);
		headerStr[SW_IPADDR]	= LoadStrU8(IDS_IPADDR);
	}

	for (int i=0; i < maxItems; i++) {
		hostListView.InsertColumn(i, headerStr[items[i]], cfg->SendWidth[items[i]]);
	}
	hostListView.SetColumnOrder(order, maxItems);
}

void TSendDlg::GetOrder(void)
{
	int		order[MAX_SENDWIDTH], orderCnt=0;

	if (!hostListView.GetColumnOrder(order, maxItems)) {
		MessageBoxU8(LoadStrU8(IDS_COMCTL), LoadStrU8(IDS_CANTGETORDER));
		return;
	}
	for (int i=0; i < MAX_SENDWIDTH; i++) {
		if (GetItem(ColumnItems, FullOrder[i]))
			FullOrder[i] = items[order[orderCnt++]];
	}
	memcpy(cfg->SendOrder, FullOrder, sizeof(FullOrder));
}

BOOL TSendDlg::IsReplyListConsist()
{
	int cnt = hostListView.GetSelectedItemCount();

	if (replyList.size() == 0) {
		return TRUE;
	}
	if (cnt != replyList.size()) {
		return	FALSE;
	}

	for (int i=0; i < memberCnt && cnt > 0; i++) {
		if (hostListView.IsSelected(i)) {
			BOOL	is_same = FALSE;
			for (auto &h: replyList) {
				if (IsSameHost(&h, &hostArray[i]->hostSub)) {
					is_same = TRUE;
					cnt--;
					break;
				}
			}
			if (!is_same) {
				return FALSE;
			}
		}
	}
	return	TRUE;
}

void TSendDlg::CheckDisp(void)
{
	int cnt = hostListView.GetSelectedItemCount();

	sendBtn.EnableWindow(cnt > 0 ? TRUE : FALSE);

	BOOL	isConsist = (cnt == 0 || IsReplyListConsist()) ? TRUE : FALSE;
	char	label[MAX_BUF];
	UINT	label_id = (cfg->ListConfirm && !isConsist && !listConfirm) ?
				IDS_CONFIRM : cfg->AbnormalButton ? IDS_FIRE : IDS_SEND;
	snprintfz(label, sizeof(label), cnt >= 2 || !isConsist ? " %s(%d%s)" : "%s",
				LoadStrU8(label_id), cnt, isConsist ? "" : "*");
	sendBtn.SetWindowTextU8(label);

#define SEND_DISPLABEL_MAX	30
#define DISPLABEL_BUF	((SEND_DISPLABEL_MAX * 2) * (MAX_LISTBUF + 100))
	Wstr	wbuf(DISPLABEL_BUF);
	WCHAR	*w = wbuf.Buf();
	int		len = 0;

	len += snwprintfz(w + len, DISPLABEL_BUF, L"%s (%d)\n", LoadStrW(IDS_TOLIST), cnt);
	if (!isConsist) {
		len += snwprintfz(w + len, DISPLABEL_BUF-len, L"%s\n", LoadStrW(IDS_LISTNOTCONSIST));
	}

	int	disp_cnt=0;

	for (int i=0; i < memberCnt; i++) {
		if (hostListView.IsSelected(i)) {
			char	buf[MAX_LISTBUF];
			MakeListString(cfg, hostArray[i], buf);
			len += snwprintfz(w + len, DISPLABEL_BUF-len, L" %s\n", U8toWs(buf));

			if (++disp_cnt >= SEND_DISPLABEL_MAX ||
				len > DISPLABEL_BUF - MAX_LISTBUF) {
				if (disp_cnt < cnt) {
					len += snwprintfz(w + len, DISPLABEL_BUF - len, L"...too many receivers");
				}
				break;
			}
		}
	}
	sendBtn.SetTipTextW(w, 500, 30000);
}

void TSendDlg::GetSeparateArea(RECT *sep_rc)
{
	TRect	lv_rc, file_rc;
	POINT	lv_pt, file_pt;
	int		lv_height;
	int		file_width;

	hostListView.GetWindowRect(&lv_rc);
	lv_height = lv_rc.cy();
	lv_pt.x = lv_rc.left;
	lv_pt.y = lv_rc.top;
	::ScreenToClient(hWnd, &lv_pt);

	::GetWindowRect(GetDlgItem(FILE_BUTTON), &file_rc);
	file_width = file_rc.cx();
	file_pt.x = file_rc.left;
	file_pt.y = file_rc.top;
	::ScreenToClient(hWnd, &file_pt);

	sep_rc->left   = file_pt.x;
	sep_rc->right  = file_pt.x + file_width;
	sep_rc->top    = lv_pt.y + lv_height;
	sep_rc->bottom = file_pt.y;
}

BOOL TSendDlg::IsSeparateArea(int x, int y)
{
	POINT	pt = {x, y};
	RECT	sep_rc;

	GetSeparateArea(&sep_rc);
	return	PtInRect(&sep_rc, pt);
}

BOOL TSendDlg::OpenLogView(BOOL is_dblclk)
{
	if ((int)hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0) == 1) {
		for (int i=0; i < memberCnt; i++) {
			if (hostListView.IsSelected(i)) {
				if (auto *logDb = logmng->GetLogDb()) {
					if (logDb->GetMaxNum(U8toWs(hostArray[i]->hostSub.u.userName)) > 0) {
						::SendMessage(GetMainWnd(), WM_LOGVIEW_OPEN, 1,
							(LPARAM)hostArray[i]->hostSub.u.userName); // open subviewer
					}
				}
				return	TRUE;
			}
		}
	}
	::SendMessage(GetMainWnd(), WM_COMMAND, MENU_LOGVIEWER, 0); // open main viewer
	return	TRUE;
}

/*
	WM_COMMAND CallBack
*/
BOOL TSendDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
//		extern void bo_test();
//		bo_test();
//		*(int *)0 = 0;

		if (IsSending())
			return	TRUE;
//		if (findDlg && findDlg->hWnd) {
//			return	findDlg->SetForegroundWindow();
//		}
		if (cfg->AbsenceCheck && (cfg->Debug & 0x2) == 2) {
			if (MessageBoxU8(LoadStrU8(IDS_ABSENCEQUERY), IP_MSG, MB_OKCANCEL) != IDOK)
				return	TRUE;
			::SendMessage(GetMainWnd(), WM_COMMAND, MENU_ABSENCE, 0);
		}
		if (cfg->ListConfirm && !IsReplyListConsist() && !listConfirm) {
			listConfirm = TRUE;
			CheckDisp();
			return TRUE;
		}

		SendMsg();
		if (shareInfo && shareInfo->fileCnt == 0) {
			shareMng->DestroyShare(shareInfo);
			shareInfo = NULL;
		}
		return	TRUE;

	case IDCANCEL:
//		if (findDlg && findDlg->hWnd)
//			return	findDlg->Destroy(), TRUE;

		if (!modalCount) {
			if (shareInfo) {
				shareMng->DestroyShare(shareInfo);
				shareInfo = NULL;
			}
			Finished();
		}
		return	TRUE;

	case REFRESH_BUTTON:
		::PostMessage(GetMainWnd(), WM_REFRESH_HOST, (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? FALSE : TRUE, 0);
		return	TRUE;

	case REPFIL_CHK:
		repFilDisp = !repFilDisp;
		repFilCheck.SendMessage(BM_SETIMAGE, IMAGE_ICON,
			(LPARAM)(repFilDisp ? hRepFilIcon : hRepFilRevIcon));
		FilterHost(filterStr);
		DisplayMemberCnt();
		return	TRUE;

	case SECRET_CHECK:
		if (cfg->PasswordUse) {
			if (IsDlgButtonChecked(SECRET_CHECK)) {
				::EnableWindow(GetDlgItem(PASSWORD_CHECK), TRUE);
			}
			else {
				CheckDlgButton(PASSWORD_CHECK, 0);
				::EnableWindow(GetDlgItem(PASSWORD_CHECK), FALSE);
			}
		}
		break;

	case MENU_FILEADD:
		{
			char	buf[MAX_PATH_U8] = "";
			if (ShareFileAddDlg(this, this, shareMng, shareInfo ?
				shareInfo : (shareInfo = shareMng->CreateShare(packetNo)), cfg))
			{
				RestrictShare();
				SetFileButton(this, FILE_BUTTON, shareInfo);
				EvSize(SIZE_RESTORED, 0, 0);
			}
		}
		break;

	case MENU_FOLDERADD:
		{
			for (int i=0; i < 5; i++)
			{
				if (*cfg->lastOpenDir && GetFileAttributesU8(cfg->lastOpenDir) == 0xffffffff)
					if (GetParentDirU8(cfg->lastOpenDir, cfg->lastOpenDir) == FALSE)
						break;
			}
			if (BrowseDirDlg(this, LoadStrU8(IDS_FOLDERATTACH), cfg->lastOpenDir,
				cfg->lastOpenDir, sizeof(cfg->lastOpenDir)
				)) {
				if (!shareInfo) {
					shareInfo = shareMng->CreateShare(packetNo);
				}
				shareMng->AddFileShare(shareInfo, cfg->lastOpenDir);
				RestrictShare();
				SetFileButton(this, FILE_BUTTON, shareInfo);
				EvSize(SIZE_RESTORED, 0, 0);
			}
		}
		break;

	case FILE_BUTTON:
		TShareDlg(shareMng, shareInfo ? shareInfo : (shareInfo = shareMng->CreateShare(packetNo)), cfg, this).Exec();
		RestrictShare();
		SetFileButton(this, FILE_BUTTON, shareInfo);
		EvSize(SIZE_RESTORED, 0, 0);
		break;

	case MENU_IMAGEPASTE:
		editSub.SendMessage(WM_COMMAND, WM_PASTE_IMAGE, 0);
		break;

	case MENU_LOGVIEWER:
		OpenLogView();
		break;

	case MENU_SETUP:
		::PostMessage(GetMainWnd(), WM_IPMSG_SETUPDLG, SENDRECV_SHEET, 0);
		return TRUE;

	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case ALLSELECT_ACCEL:
		editSub.SendMessage(EM_SETSEL, 0, (LPARAM)-1);
		return	TRUE;

	case MENU_FINDDLG:
		if (!findDlg) {
			findDlg = new TFindDlg(cfg, this);
		}
		if (!findDlg->hWnd) {
			findDlg->Create();
		}
		else {
			findDlg->SetForegroundWindow();
		}
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
//		cfg->WriteRegistry(CFG_WINSIZE);
//		return	TRUE;
//	case MENU_SAVECOLUMN:
		GetOrder();
		for (int i=0; i < maxItems; i++) {
			cfg->SendWidth[items[i]] = (int)hostListView.SendMessage(LVM_GETCOLUMNWIDTH, i, 0);
		}
		cfg->WriteRegistry(CFG_WINSIZE);
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
			cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_SHOWHELP/* | CF_EFFECTS*/;
			cf.nFontType	= SCREEN_FONTTYPE;
			if (::ChooseFont(&cf))
			{
				*targetFont = tmpFont;
				SetFont(TRUE);
				::InvalidateRgn(hWnd, NULL, TRUE);
				cfg->WriteRegistry(CFG_FONT);
				::PostMessage(GetMainWnd(), WM_SENDDLG_FONTCHANGED, 0, 0);
			}
		}
		return	TRUE;

	case MENU_DEFAULTFONT:
		cfg->SendEditFont = cfg->SendListFont = orgFont;
		{
			char	*ex_font = LoadStrA(IDS_PROPORTIONALFONT);
			if (ex_font && *ex_font)
				strcpy(cfg->SendListFont.lfFaceName, ex_font);
		}
		SetFont(TRUE);
		::InvalidateRgn(hWnd, NULL, TRUE);
		cfg->WriteRegistry(CFG_FONT);
		return	TRUE;

	case MENU_NORMALSIZE:
		GetWindowRect(&rect);
		currentMidYdiff = 0;
		MoveWindow(rect.left, rect.top, orgRect.right - orgRect.left, orgRect.bottom - orgRect.top, TRUE);
		return	TRUE;

	case MENU_MEMBERDISP:
		if (TSortDlg(cfg, this).Exec() == IDOK) {
			GetOrder();
			cfg->WriteRegistry(CFG_WINSIZE);
			DelAllHost();
			InitializeHeader();
			for (int i=0; i < hosts->HostCnt(); i++) {
				AddHost(hosts->GetHost(i));
			}
		}
		return	TRUE;

	case MENU_LISTINCLUDE:
		sendRecvList = !sendRecvList;
		return	TRUE;

	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_PASTE_REV:
	case WM_PASTE_IMAGE:
	case WM_SAVE_IMAGE:
	case WM_INSERT_IMAGE:
	case WM_CLEAR:
	case EM_SETSEL:
		editSub.SendMessage(WM_COMMAND, wID, 0);
		return	TRUE;

	case SEND_EDIT:
//		if (wNotifyCode == EN_CHANGE) {
//			editSub.SetHyperLink();
//		}
		return	TRUE;

	case WM_EDIT_IMAGE:
		imageWin.Create(&editSub);
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

	case CAPTURE_BUTTON:
	case MENU_IMAGERECT:
		{
			BOOL	is_shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
			if (is_shift ^ cfg->CaptureMinimize) {
				Show(SW_MINIMIZE);
				SetTimer(IPMSG_IMAGERECT_TIMER, cfg->CaptureDelayEx);
			}
			else {
				SetTimer(IPMSG_IMAGERECT_TIMER, cfg->CaptureDelay);
			}
			return	TRUE;
		}

	default:
		if (wID >= MENU_PRIORITY_RESET && wID < MENU_LRUUSER)
		{
			if (wID == MENU_PRIORITY_RESET)
			{
				if (MessageBoxU8(LoadStrU8(IDS_DEFAULTSET), IP_MSG, MB_OKCANCEL) != IDOK) {
					return	TRUE;
				}
				while (cfg->priorityHosts.HostCnt() > 0) {
					Host	*host = cfg->priorityHosts.GetHost(0);
					cfg->priorityHosts.DelHost(host);
					host->SafeRelease();
				}
				for (int i=0; i < hosts->HostCnt(); i++) {
					hosts->GetHost(i)->priority = DEFAULT_PRIORITY;
				}
			}
			else if (wID == MENU_PRIORITY_HIDDEN)
			{
				hiddenDisp = !hiddenDisp;
			}
			else if (wID >= MENU_PRIORITY_START && wID < MENU_GROUP_START)
			{
				int	priority = wID - MENU_PRIORITY_START;

				for (int i=0; i < memberCnt; i++)
				{
					if (hostArray[i]->priority == priority || !hostListView.IsSelected(i)) {
						continue;
					}
					if (!cfg->priorityHosts.GetHostByName(&hostArray[i]->hostSub)) {
						cfg->priorityHosts.AddHost(hostArray[i]);
					}
					hostArray[i]->priority = priority;
				}
			}
			DelAllHost();
			for (int i=0; i < hosts->HostCnt(); i++) {
				AddHost(hosts->GetHost(i));
			}
			if (wID != MENU_PRIORITY_HIDDEN) {
				cfg->WriteRegistry(CFG_HOSTINFO|CFG_DELHOST|CFG_DELCHLDHOST);
			}
		}
		else if (wID >= MENU_LRUUSER && wID < MENU_LRUUSER + (UINT)cfg->lruUserMax) {
			int		idx = wID - MENU_LRUUSER;
			UsersObj *obj = cfg->lruUserList.EndObj();
			for (int i=0; i < idx && obj; i++) {
				obj = cfg->lruUserList.PrevObj(obj);
			}
			if (obj) {
				UserObj *user = (UserObj *)obj->users.EndObj();
				for (int j=0; user; j++) {
					SelectHost(&user->hostSub, j == 0 ? TRUE : FALSE, FALSE);
					user = (UserObj *)obj->users.PrevObj(user);
				}
			}
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
				if (strcmp(selectGroup, hostArray[lvi.iItem]->groupName) == 0 &&
						!IsSameHost(&hostArray[lvi.iItem]->hostSub, msgMng->GetLocalHost())) {
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
			CheckDisp();
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
	if (uCmdType >= MENU_SETUP && uCmdType < 20000) { // メニュー範囲(10000-20000)
		return	EvCommand(0, (WORD)uCmdType, 0);
	}
	return	FALSE;
}

BOOL TSendDlg::EventMenuLoop(UINT uMsg, BOOL fIsTrackPopupMenu)
{
//	if (uMsg == WM_EXITMENULOOP) {
//		hCurMenu = NULL;
//		KillTimer(IPMSG_KEYCHECK_TIMER);
//	}
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
			ModifyMenuU8(hMenu, MENU_SAVEPOS, MF_BYCOMMAND|(cfg->SendSavePos ? MF_CHECKED :  0),
						MENU_SAVEPOS, LoadStrU8(IDS_SAVEPOS));
		}
//		hCurMenu = hMenu;
//		SetTimer(IPMSG_KEYCHECK_TIMER, 200);
//		return	TRUE;
	}
	return	FALSE;
}

/*
	Color Event CallBack
*/
BOOL TSendDlg::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
#if 0
	COLORREF	bkref	= 0x000000;
	COLORREF	foreref	= 0x00ff00;
	COLORREF	dlgref	= 0xeeeeee;
	COLORREF	statref	= 0xffff00;

	switch (uMsg) {
	case WM_CTLCOLORDLG:	// dlg 地
		{
			static HBRUSH hb;
			if (!hb) hb = ::CreateSolidBrush(dlgref);
			*result = hb;
		}
		SetTextColor(hDcCtl, foreref);
		SetBkColor(hDcCtl, dlgref);
		break;
	case WM_CTLCOLOREDIT:	// edit 地
/*		{
			static HBRUSH hb;
			if (!hb) hb = ::CreateSolidBrush(bkref);
			*result = hb;
		}
		SetTextColor(hDcCtl, foreref);
		SetBkColor(hDcCtl, bkref);
*/		break;
	case WM_CTLCOLORSTATIC:	// static control & check box 地
		{
			static HBRUSH hb;
			if (!hb) hb = ::CreateSolidBrush(dlgref);
			*result = hb;
		}
//		SetTextColor(hDcCtl, statref);
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


BOOL TSendDlg::AppendDropFilesAsText(const char *path)
{
	DWORD	start = 0;
	DWORD	end = 0;
	Wstr	wpath(path);

	editSub.GetCurSelLast(&start, &end);

	if (end > 0) {
		DWORD	s = 0, e = 0;

		editSub.SendMessageW(EM_SETSEL, (WPARAM)end-1, (LPARAM)end);
		editSub.SendMessageW(EM_GETSEL, (WPARAM)&s, (LPARAM)&e);

		if (e - s == 1) {	// 1文字選択を確認
			WCHAR	w[2] = {};
			editSub.SendMessageW(EM_GETSELTEXT, 0, (LPARAM)w);
			editSub.SendMessageW(EM_SETSEL, (WPARAM)end, (LPARAM)end);
			if (w[0] != '\r' && w[0] != '\n') {
				editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)L"\r\n");
				end += 1;
			}
		}
	}

	editSub.SendMessageW(EM_SETSEL, (WPARAM)end, (LPARAM)end);

	editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)wpath.s());
	editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)L"\r\n");
	end += (DWORD)wcslen(wpath.s()) + 1;
	editSub.SendMessageW(EM_SETSEL, (WPARAM)end, (LPARAM)end);
	editSub.SetCurSelLast(end, end);

	return	TRUE;
}

BOOL TSendDlg::RestrictShare()
{
	if (cfg->NoFileTrans > 0 && shareInfo) {
		for (int i=shareInfo->fileCnt - 1; i >= 0; i--) {
			auto finfo = shareInfo->fileInfo[i];

			if (finfo->MemData()) {	// clip data
				continue;
			}
			if (IsNetVolume(finfo->Fname())) {
				AppendDropFilesAsText(finfo->Fname());
				shareInfo->RemoveFileInfo(i);
			}
		}
	}
	return	TRUE;
}

/*
	DropFiles Event CallBack
*/
BOOL TSendDlg::EvDropFiles(HDROP hDrop)
{
	char	buf[MAX_PATH_U8];
	int		max = DragQueryFileU8(hDrop, ~0UL, 0, 0);

	if (!shareInfo) {
		shareInfo = shareMng->CreateShare(packetNo);
	}

	for (int i=0; i < max; i++) {
		if (DragQueryFileU8(hDrop, i, buf, sizeof(buf)) <= 0) {
			break;
		}
		shareMng->AddFileShare(shareInfo, buf);
	}
	::DragFinish(hDrop);

	if (shareInfo->fileCnt == 0) {
		return	FALSE;
	}

	RestrictShare();
	SetFileButton(this, FILE_BUTTON, shareInfo);
	EvSize(SIZE_RESTORED, 0, 0);

	return	TRUE;
}

void TSendDlg::GetListItemStr(Host *host, int item, char *buf)
{
	switch (items[item]) {
	case SW_NICKNAME:
		strcpy(buf, *host->nickName ? host->nickName : host->hostSub.u.userName);
		break;
	case SW_USER:
		strcpy(buf, host->hostSub.u.userName);
		break;
	case SW_ABSENCE:
		strcpy(buf, (host->hostStatus & IPMSG_ABSENCEOPT) ? "*" : "");
		break;
	case SW_PRIORITY:
		if (host->priority == DEFAULT_PRIORITY) strcpy(buf, "-");
		else if (host->priority <= 0) strcpy(buf, "X");
		else sprintf(buf, "%d", cfg->PriorityMax - (host->priority - DEFAULT_PRIORITY) / PRIORITY_OFFSET);
		break;
	case SW_GROUP:
		strcpy(buf, host->groupName);
		break;
	case SW_HOST:
		strcpy(buf, host->hostSub.u.hostName);
		break;
	case SW_IPADDR:
		host->hostSub.addr.S(buf);
		break;
	default:
		*buf = 0;
		break;
	}
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
			ReregisterEntry(TRUE);
		}
		return	TRUE;

	case LVN_ODSTATECHANGED:
	case LVN_ITEMCHANGED:
		if (listOperateCnt == 0) {
			listConfirm = FALSE;
			CheckDisp();
			DisplayMemberCnt();
		}
		return	TRUE;

	case LVN_ENDSCROLL:	// XP の対策
		::InvalidateRect(editSub.hWnd, NULL, TRUE);
		::UpdateWindow(editSub.hWnd);
		return	TRUE;

	case NM_DBLCLK:
		if (NMITEMACTIVATE *nact = (NMITEMACTIVATE *)pNmHdr) {
			OpenLogView(TRUE);
		}
		return	TRUE;

	case EN_LINK:
		{
			ENLINK	*el = (ENLINK *)pNmHdr;
			switch (el->msg) {
			case WM_LBUTTONUP:
				editSub.LinkSel(el);
				editSub.JumpLink(el, FALSE);
				break;

			case WM_LBUTTONDBLCLK:
				editSub.JumpLink(el, TRUE);
				break;

			case WM_RBUTTONUP:
				editSub.PostMessage(WM_CONTEXTMENU, 0, GetMessagePos());
				break;
			}
		}
		return	FALSE;

//	case EN_CHANGE:
//		editSub.SetHyperLink();
//		return	FALSE;
	}

//	Debug("code=%d info=%d infow=%d clk=%d\n", pNmHdr->code, LVN_GETDISPINFO, LVN_GETINFOTIPW, LVN_COLUMNCLICK);

	return	FALSE;
}

/*
	WM_MOUSEMOVE CallBack
*/
BOOL TSendDlg::EvMouseMove(UINT fwKeys, POINTS pos)
{

	if ((fwKeys & MK_LBUTTON) && captureMode) {
		if (lastYPos == (int)pos.y)
			return	TRUE;
		lastYPos = (int)pos.y;

		RECT	tmpRect;
		int		min_y = (5 * (item[refresh_item].y + item[refresh_item].cy) - 3 * (item[host_item].y + item[host_item].cy)) / 2;

		if (pos.y < min_y) {
			pos.y = min_y;
		}

		currentMidYdiff += (int)(short)(pos.y - dividYPos);
		EvSize(SIZE_RESTORED, 0, 0);
		GetWindowRect(&tmpRect);
		MoveWindow(tmpRect.left, tmpRect.top, tmpRect.right - tmpRect.left, tmpRect.bottom - tmpRect.top, TRUE);

/*
		static HBRUSH hBrush;
		static RECT	old_rc;
		RECT sep_rc = {};
		RECT sub_rc = {};

		if (!hBrush) hBrush = (HBRUSH)::GetStockObject(DKGRAY_BRUSH);
		HDC hDc = ::GetDC(hWnd);
		GetSeparateArea(&sep_rc);
		SubtractRect(&sub_rc, &old_rc, &sep_rc);
		InvalidateRect(&sub_rc, TRUE);
		::FillRect(hDc, &sep_rc, hBrush);
		::ReleaseDC(hWnd, hDc);
		old_rc = sep_rc;
*/
		dividYPos = (int)pos.y;
		return	TRUE;
	}
	return	FALSE;
}

BOOL TSendDlg::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg)
	{
	case WM_LBUTTONDOWN:
		if (!captureMode && IsSeparateArea(pos.x, pos.y)) {
			POINT	pt;
			captureMode = TRUE;
			::SetCapture(hWnd);
			::GetCursorPos(&pt);
			::ScreenToClient(hWnd, &pt);
			dividYPos = pt.y;
			lastYPos = 0;
		}
		return	TRUE;

	case WM_LBUTTONUP:
		if (captureMode) {
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
	UINT	dwFlg = SWP_SHOWWINDOW | SWP_NOZORDER;
	UINT	dwHideFlg = SWP_HIDEWINDOW | SWP_NOZORDER;
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
	if (!(hdwp = ::DeferWindowPos(hdwp, refreshBtn.hWnd, NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff * 2 / 3), wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[menu_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, menuCheck.hWnd, NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff * 2 / 3), wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[repfil_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, repFilCheck.hWnd, NULL, wpos->x + xdiff, wpos->y + (currentMidYdiff >= 0 ? 0 : currentMidYdiff * 2 / 3), wpos->cx, wpos->cy, (cfg->ReplyFilter && ((!isMultiRecv && replyList.size() >= 1) || replyList.size() >= 2)) ? dwFlg : dwHideFlg)))
		return	FALSE;

	wpos = &item[file_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(FILE_BUTTON), NULL, wpos->x, wpos->y + currentMidYdiff, wpos->cx + xdiff, wpos->cy, isFileBtn ? dwFlg : (SWP_HIDEWINDOW|SWP_NOZORDER))))
		return	FALSE;

	wpos = &item[edit_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, editSub.hWnd, NULL, wpos->x, (isFileBtn ? wpos->y : item[file_item].y) + currentMidYdiff, wpos->cx + xdiff, wpos->cy + ydiff - currentMidYdiff + (isFileBtn ? 0 : wpos->y - item[file_item].y), dwFlg)))
		return	FALSE;

	wpos = &item[capture_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, captureBtn.hWnd, NULL, wpos->x, wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[ok_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, sendBtn.hWnd, NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 6 / 7), wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	wpos = &item[passwd_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(PASSWORD_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, cfg->PasswordUse ? dwFlg : dwHideFlg)))
		return	FALSE;

	wpos = &item[secret_item];
	if (!(hdwp = ::DeferWindowPos(hdwp, GetDlgItem(SECRET_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, dwFlg)))
		return	FALSE;

	EndDeferWindowPos(hdwp);

	if (cfg->PasswordUse) {
		::InvalidateRgn(GetDlgItem(PASSWORD_CHECK), NULL, TRUE);
	}
	::InvalidateRgn(GetDlgItem(SECRET_CHECK), NULL, TRUE);
	::InvalidateRgn(sendBtn.hWnd, NULL, TRUE);

	if (parent) {
		::InvalidateRgn(editSub.hWnd, NULL, TRUE);
	}

	return	TRUE;
}

/*
	最大/最小 Size 設定
*/
BOOL TSendDlg::EvGetMinMaxInfo(MINMAXINFO *info)
{
	info->ptMinTrackSize.x = (orgRect.right - orgRect.left) * 2 / 3;
	info->ptMinTrackSize.y = (item[host_item].y + item[host_item].cy + currentMidYdiff) + (shareInfo && shareInfo->fileCnt ? 130 : 95);
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

BOOL TSendDlg::EvSetCursor(HWND cursorWnd, WORD nHitTest, WORD wMouseMsg)
{
	static HCURSOR hResizeCursor;
	BOOL	need_set = captureMode;

	if (!need_set) {
		POINT	pt;
		::GetCursorPos(&pt);
		::ScreenToClient(hWnd, &pt);
		if (IsSeparateArea(pt.x, pt.y)) need_set = TRUE;
	}

	if (need_set) {
		if (!hResizeCursor) hResizeCursor = ::LoadCursor(TApp::hInst(), (LPCSTR)SEP_CUR);
		::SetCursor(hResizeCursor);
		return	TRUE;
	}

	return FALSE;
}

BOOL TSendDlg::EvNcHitTest(POINTS pos, LRESULT *result)
{
	return	FALSE;
}

/*
	App定義 Event CallBack
*/
BOOL TSendDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DELAYSETTEXT:
		if (cmdHWnd) {
			Wstr	w(msg.msgBuf);
			editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)w.s());
			SendMsg();
		}
		else {
			SetQuoteStr(msg.msgBuf, cfg->QuoteStr);
			editSub.SetFocus();
			if (*msg.msgBuf && (cfg->QuoteCheck & 0x10) == 0) {
				editSub.SendMessageW(EM_SETSEL, (WPARAM)0, (LPARAM)0);
				editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)L"\r\n\r\n");
				editSub.SendMessageW(EM_SETSEL, (WPARAM)0, (LPARAM)0);
			}
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
	if (_timerID == IPMSG_IMAGERECT_TIMER) {
		KillTimer(IPMSG_IMAGERECT_TIMER);
		imageWin.Create();
		return	TRUE;
	}
	if (_timerID == IPMSG_FOREDURATION_TIMER) {
		KillTimer(IPMSG_FOREDURATION_TIMER);
		SetWindowPos(HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW);
		return	TRUE;
	}

	if (IsSendFinish()) {
		KillTimer(IPMSG_SEND_TIMER);
		if (timerID == IPMSG_DUMMY_TIMER)	// 再入よけ
			return	FALSE;
		timerID = IPMSG_DUMMY_TIMER;
		Finished();
		return	TRUE;
	}
	if (retryCnt++ <= cfg->RetryMax) {
		SendMsgCore();
		return	TRUE;
	}

	KillTimer(IPMSG_SEND_TIMER);
	char *buf = new char [MAX_UDPBUF];
	*buf = 0;

	for (int i=0; i < sendEntryNum; i++) {
		SendEntry	*ent = sendEntry + i;
		if (ent->Status() != ST_DONE) {
			MakeListString(cfg, ent->Host(), buf + strlen(buf));
			strcat(buf, "\r\n");
		}
	}
	strcat(buf, LoadStrU8(IDS_RETRYSEND));
	retryDlg.SetFlags(TMsgBox::RETRY | TMsgBox::BIGX | TMsgBox::CENTER);
	int ret = cmdHWnd ? IDCANCEL : retryDlg.Exec(buf, IP_MSG);
	delete [] buf;

	if (ret == IDRETRY && !IsSendFinish()) {
		retryCnt = 0;
		SendMsgCore();
		timerID = IPMSG_SEND_TIMER;
		SetTimer(IPMSG_SEND_TIMER, cfg->RetryMSec);
	}
	else {
		if (cmdHWnd) {
		}
		Finished();
	}

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
	DynBuf	dbuf(MAX_UDPBUF * 3);
	WCHAR	*s = wbuf.Buf();
	WCHAR	*d = (WCHAR *)dbuf.Buf();
	BOOL	is_quote = TRUE;
	int		quote_len = wquote.Len();

	for (int i=0; i < MAX_UDPBUF && *s; i++) {
		if (is_quote) {
			wcscpy(d, wquote.s());
			d += quote_len;
			is_quote = FALSE;
		}
		if ((*d++ = *s++) == '\n') is_quote = TRUE;
	}
	if ((WCHAR *)dbuf.Buf() != d) {
		if (*(d - 1) != '\n') {
			*d++ = '\r';
			*d++ = '\n';
		}
	}

	*d = 0;

	editSub.SendMessageW(EM_REPLACESEL, 0, (LPARAM)dbuf.Buf());
}

inline char *strtoupper(char *buf, const char *org, int max_size)
{
	char	*buf_sv  = buf;
	char	*buf_end = buf + max_size -1;

	while (buf < buf_end && *org) {
		*buf++ = toupper(*org++);
	}
	*buf = 0;

	return	buf_sv;
}

BOOL TSendDlg::IsFilterHost(Host *host)
{
	char	buf[MAX_NAMEBUF];
	char	uname[MAX_NAMEBUF];
	char	*p;

	if (repFilDisp) {
		BOOL	is_match = FALSE;
		for (auto &h: replyList) {
			if (IsSameHost(&h, &host->hostSub)) {
				is_match = TRUE;
				break;
			}
		}
		if (!is_match) {
			return	FALSE;
		}
	}

	if (!*filterStr) return TRUE;

	strtoupper(uname, host->hostSub.u.userName, sizeof(uname));
	if ((p = (char *)GetUserNameDigestField(uname))) *p = 0;

	if (strstr(*host->nickName ? strtoupper(buf, host->nickName, sizeof(buf)) : uname, filterStr)
		|| cfg->FindAll && (strstr(strtoupper(buf, host->groupName, sizeof(buf)), filterStr)
					|| strstr(strtoupper(buf, host->hostSub.u.hostName, sizeof(buf)), filterStr)
					|| strstr(uname, filterStr))) {
		return	TRUE;
	}
	return	FALSE;
}

/*
	HostEntryの追加
*/
void TSendDlg::AddHost(Host *host, BOOL is_sel, BOOL disp_upd)
{
	if (IsSending() || host->priority <= 0 && !hiddenDisp || !IsFilterHost(host))
		return;

	char		buf[MAX_BUF];
	LV_ITEMW	lvi;

	lvi.mask = LVIF_TEXT|LVIF_PARAM|(lvStateEnable ? LVIF_STATE : LVIF_IMAGE);
	lvi.iItem = GetInsertIndexPoint(host);
	lvi.iSubItem = 0;

	int		state = 0;
	if (host->hostStatus & IPMSG_CLIPBOARDOPT) {
		if (GetUserNameDigestField(host->hostSub.u.userName)) {
			state = STI_SIGN_CLIP;
		}
		else if ((host->hostStatus & IPMSG_ENCRYPTOPT)) {
			state = STI_ENC_CLIP;
		}
		else {
			state = STI_CLIP;
		}
	}
	else if (host->hostStatus & IPMSG_FILEATTACHOPT) {
		if (GetUserNameDigestField(host->hostSub.u.userName)) {
			state = STI_SIGN_FILE;
		}
		else if (host->hostStatus & IPMSG_ENCRYPTOPT) {
			state = STI_ENC_FILE;
		}
		else {
			state = STI_FILE;
		}
	}
	else {
		if (GetUserNameDigestField(host->hostSub.u.userName)) {
			state = STI_SIGN;
		}
		else if (host->hostStatus & IPMSG_ENCRYPTOPT) {
			state = STI_ENC;
		}
		else {
			state = STI_NONE;
		}
	}

	int		absence = (host->hostStatus & IPMSG_ABSENCEOPT) ? 1 : 0;
	lvi.state = INDEXTOSTATEIMAGEMASK(state + 1) | INDEXTOOVERLAYMASK(absence);
	lvi.stateMask = LVIS_STATEIMAGEMASK | LVIS_OVERLAYMASK;
	if (is_sel) {
		lvi.state     |= LVIS_SELECTED;
		lvi.stateMask |= LVIS_SELECTED;
	}

	GetListItemStr(host, 0, buf);
	lvi.pszText = U8toWs(buf);
	lvi.cchTextMax = 0;
	lvi.iImage = 0;
	lvi.lParam = (LPARAM)host;

	listOperateCnt++;
	int		index;
	if ((index = (int)hostListView.SendMessage(LVM_INSERTITEMW, 0, (LPARAM)&lvi)) >= 0)
	{
#define BIG_ALLOC	1000
		if ((memberCnt % BIG_ALLOC) == 0) {
			hostArray = (Host **)realloc(hostArray, (memberCnt + BIG_ALLOC) * sizeof(Host *));
		}
		memmove(hostArray + index +1, hostArray + index, (memberCnt++ - index) * sizeof(Host *));
		hostArray[index] = host;

		for (int i=1; i < maxItems; i++) {	// i==0があると、LVN_ITEMCHANGED が発生する…
			GetListItemStr(host, i, buf);
			hostListView.SetSubItem(index, i, buf);
		}

		if (disp_upd) {
			DisplayMemberCnt();
		}
	}
	listOperateCnt--;
}

int TSendDlg::GetHostIdx(Host *host, BOOL *is_sel)
{
	for (int i=0; i < memberCnt; i++) {
		if (hostArray[i] == host) {
			if (is_sel) {
				*is_sel = hostListView.IsSelected(i);
			}
			return	i;
		}
	}
	return	-1;
}

/*
	HostEntryの修正
*/
void TSendDlg::ModifyHost(Host *host, BOOL disp_upd)
{
	BOOL	is_sel = FALSE;
	DelHost(host, &is_sel, FALSE);
	AddHost(host, is_sel, disp_upd);
}

/*
	HostEntryの削除
*/
void TSendDlg::DelHost(Host *host, BOOL *_is_sel, BOOL disp_upd)
{
	if (IsSending())
		return;

	listOperateCnt++;

	BOOL	is_sel_tmp = FALSE;
	BOOL	&is_sel = _is_sel ? *_is_sel : is_sel_tmp;
	int		idx = GetHostIdx(host, &is_sel);

	if (is_sel) {
		listConfirm = FALSE;
	}

	if (idx != -1 && hostListView.DeleteItem(idx))
	{
		memmove(hostArray + idx, hostArray + idx +1, (memberCnt - idx -1) * sizeof(Host *));
		memberCnt--;
		if (disp_upd) {
			DisplayMemberCnt();
			CheckDisp();
		}
	}
	listOperateCnt--;
}

void TSendDlg::DispUpdate(void)
{
	DisplayMemberCnt();
	CheckDisp();
}

void TSendDlg::DelAllHost(void)
{
	if (IsSending())
		return;

	listConfirm = FALSE;
	hostListView.DeleteAllItems();
	free(hostArray);
	hostArray = NULL;
	memberCnt = 0;
	DisplayMemberCnt();
	CheckDisp();
}

void TSendDlg::ReregisterEntry(BOOL keep_select)
{
	vector<HostSub> sel_host;

	if (keep_select && hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0) > 0) {
		for (int i=0; i < memberCnt; i++) {
			if (!hostListView.IsSelected(i)) {
				continue;
			}
			sel_host.push_back(hostArray[i]->hostSub);
		}
	}

	DelAllHost();
	for (int i=0; i < hosts->HostCnt(); i++) {
		AddHost(hosts->GetHost(i));
	}

	if (keep_select && sel_host.size() > 0) {
		for (auto itr=sel_host.rbegin(); itr != sel_host.rend(); itr++) {
			SelectHost(&(*itr));
		}
	}
}

/*
	HostEntryへの挿入index位置を返す
*/
UINT TSendDlg::GetInsertIndexPoint(Host *host)
{
	int	min = 0;
	int	max = memberCnt -1;

	while (min <= max)
	{
		int	idx = (min + max) / 2;
		int	ret;

		if ((ret = CompareHosts(host, hostArray[idx])) > 0) {
			min = idx +1;
		}
		else if (ret < 0) {
			max = idx -1;
		}
		else {	// 無い筈
			min = idx;
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

	 // serverは常に最後尾に
	if ((host1->hostStatus & IPMSG_SERVEROPT) ^ (host2->hostStatus & IPMSG_SERVEROPT)) {
		return	(host1->hostStatus & IPMSG_SERVEROPT) ? 1 : -1;
	}

	if (sortItem != -1) {
		switch (sortItem) {
		case SW_NICKNAME:
			ret = strcmp(*host1->nickName ? host1->nickName : host1->hostSub.u.userName, *host2->nickName ? host2->nickName : host2->hostSub.u.userName); break;
		case SW_ABSENCE:	// 今のところ、通らず
			ret = (host1->hostStatus & IPMSG_ABSENCEOPT) > (host2->hostStatus & IPMSG_ABSENCEOPT) ? 1 : (host1->hostStatus & IPMSG_ABSENCEOPT) < (host2->hostStatus & IPMSG_ABSENCEOPT) ? -1 : 0; break;
		case SW_GROUP:
			ret = strcmp(*host1->groupName ? host1->groupName : "\xff", *host2->groupName ? host2->groupName : "\xff"); break;
		case SW_HOST:
			ret = strcmp(host1->hostSub.u.hostName, host2->hostSub.u.hostName); break;
		case SW_IPADDR:
			ret = host1->hostSub.addr > host2->hostSub.addr ? 1 : -1;
			break;
		case SW_USER:
			ret = strcmp(host1->hostSub.u.userName, host2->hostSub.u.userName); break;
		case SW_PRIORITY:
			ret = host1->priority > host2->priority ? 1 : host1->priority < host2->priority ? -1 : 0; break;
		}
		if (ret) {
			return	sortRev ? -ret : ret;
		}
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

		name1 = *host1->nickName ? host1->nickName : host1->hostSub.u.userName;
		name2 = *host2->nickName ? host2->nickName : host2->hostSub.u.userName;
		if (!(cfg->Sort & IPMSG_NOKANJISORTOPT))
		{
			if (IS_KANJI(*name1) && !IS_KANJI(*name2))
				ret = -1;
			if (!IS_KANJI(*name1) && IS_KANJI(*name2))
				ret = 1;
		}
		if (ret == 0)
			if ((ret = StrCmp(name1, name2)) == 0)
				if ((ret = StrCmp(host1->hostSub.u.hostName, host2->hostSub.u.hostName)) == 0)
					ret = StrCmp(host1->hostSub.u.userName, host2->hostSub.u.userName);
		break;

	case IPMSG_HOSTSORT:
		if (!(cfg->Sort & IPMSG_NOKANJISORTOPT))
		{
			if (IS_KANJI(*host1->hostSub.u.hostName) && !IS_KANJI(*host2->hostSub.u.hostName))
				ret = 1;
			if (!IS_KANJI(*host1->hostSub.u.hostName) && IS_KANJI(*host2->hostSub.u.hostName))
				ret = -1;
		}
		if (ret == 0)
			ret = StrCmp(host1->hostSub.u.hostName, host2->hostSub.u.hostName);
		if (ret)
			break;

		// host名で比較がつかないときは IPADDRSORTに従う
		// このまま 下に落ちる

	case IPMSG_IPADDRSORT:
		if (host1->hostSub.addr > host2->hostSub.addr)
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
BOOL TSendDlg::SelectHost(HostSub *hostSub, BOOL force, BOOL byAddr)
{
	BOOL	ret = FALSE;
	int		i;
	LV_ITEM	lvi;
	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_STATE;
	lvi.stateMask = LVIS_FOCUSED|LVIS_SELECTED;

	for (i=0; i < memberCnt; i++) {
		if ((byAddr && hostSub->addr == hostArray[i]->hostSub.addr &&
			 hostSub->portNo == hostArray[i]->hostSub.portNo) ||
			(!byAddr && IsSameHost(hostSub, &hostArray[i]->hostSub))) {
			lvi.state = LVIS_FOCUSED|LVIS_SELECTED;
			hostListView.SendMessage(LVM_SETITEMSTATE, i, (LPARAM)&lvi);
			hostListView.SendMessage(LVM_ENSUREVISIBLE, i, 0);
			hostListView.SendMessage(LVM_SETSELECTIONMARK, 0, i);
			hostListView.SetFocusIndex(i);
			ret = TRUE;
			if (!force) {
				break;
			}
		}
		else if (force) {
			lvi.state = 0;
			hostListView.SendMessage(LVM_SETITEMSTATE, i, (LPARAM)&lvi);
		}
	}
	CheckDisp();
	return	ret;
}

/*
	検索
*/
BOOL TSendDlg::SelectFilterHost()
{
	int		startNo = hostListView.GetFocusIndex() + 1;

	if (!*filterStr) {
		return	FALSE;
	}

	for (int i=0; i < memberCnt; i++) {
		Host	*host = hostArray[(i + startNo) % memberCnt];

		if (IsFilterHost(host)) {
			SelectHost(&host->hostSub, TRUE);
			return	TRUE;
		}
	}
	return	FALSE;
}

/*
	Filter
*/
int TSendDlg::FilterHost(char *_filterStr)
{
	Host	*selected_host = NULL;

	strtoupper(filterStr, _filterStr, sizeof(filterStr));

	for (int i=0; i < hosts->HostCnt(); i++) {
		Host	*host = hosts->GetHost(i);

		if (IsFilterHost(host)) {
			int	idx = GetInsertIndexPoint(host);
			if (idx < memberCnt && host == hostArray[idx]) {
				if (!*filterStr && !selected_host && hostListView.IsSelected(idx)) {
					selected_host = host;
				}
			}
			else AddHost(host);
		}
		else {
			DelHost(host, 0, FALSE);
		}
	}
	DispUpdate();

	// 選択位置を表示
	if (selected_host) {
		SelectHost(&selected_host->hostSub);
	}

	return	memberCnt;
}

/*
	Member数を表示
*/
void TSendDlg::DisplayMemberCnt(void)
{
	if (repFilDisp) {
		static char	replys[MAX_LISTBUF];
		static char	*replys_p = replys + strcpyz(replys, LoadStrU8(IDS_REPLYNUM));
		int		sel_num = (int)hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0);

		sprintf(replys_p, replyList.size() == sel_num ? "%d" : "%d/%d", sel_num, replyList.size());
		memCntText.SetWindowTextU8(replys);
	}
	else {
		static char	normal[MAX_LISTBUF];
		static char	*normal_p = normal + strcpyz(normal, LoadStrU8(IDS_USERNUM));

		sprintf(normal_p, "%d", memberCnt);
		memCntText.SetWindowTextU8(normal);
	}
}

void TSendDlg::AddLruUsers(void)
{
	UsersObj	*obj;

	if (cfg->lruUserMax <= 0) goto END;

// 既存エントリの検査
	obj = cfg->lruUserList.TopObj();
	for (int i=0; obj; i++) {
		if (sendEntryNum == obj->users.Num()) {
			UserObj *user = obj->users.TopObj();
			int	j = 0;
			for ( ; j < sendEntryNum; j++) {
				SendEntry	*ent = sendEntry + j;
				if (!user || !IsSameHost(&user->hostSub, &ent->Host()->hostSub)) break;
				user = obj->users.NextObj(user);
			}
			if (j == sendEntryNum) {
				cfg->lruUserList.DelObj(obj);
				cfg->lruUserList.AddObj(obj); // update lru
				goto END;
			}
		}
		obj = cfg->lruUserList.NextObj(obj);
	}

	obj = new UsersObj();
	cfg->lruUserList.AddObj(obj);

	for (int i=0; i < sendEntryNum; i++) {
		SendEntry	*ent = sendEntry + i;
		UserObj *user = new UserObj();
		user->hostSub = ent->Host()->hostSub;
		obj->users.AddObj(user);
	}

END:
	while (cfg->lruUserMax < cfg->lruUserList.Num()) {
		obj = cfg->lruUserList.TopObj();
		cfg->lruUserList.DelObj(obj);
		delete obj;
	}
}

void TSendDlg::MakeUlistCore(int self_idx, std::vector<User> *uvec)
{
	uvec->push_back(sendEntry[self_idx].Host()->hostSub.u);

	for (int i=0; i < sendEntryNum; i++) {
		if (i != self_idx) {
			SendEntry	*ent = sendEntry + i;
			uvec->push_back(ent->Host()->hostSub.u);
		}
	}
}

void TSendDlg::MakeUlistStr(int self_idx, char *ulist)
{
	vector<User>	uvec;

	MakeUlistCore(self_idx, &uvec);

	msgMng->UserToUList(uvec, ulist);
}

void TSendDlg::MakeUlistDict(int self_idx, IPDict *dict)
{
	vector<User>	uvec;

	MakeUlistCore(self_idx, &uvec);

	msgMng->UserToUListDict(uvec, dict);
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
	BOOL	is_clip_enable = FALSE;

	if (ctl_on && shift_on)
		command = rbutton_on ? IPMSG_GETINFO : IPMSG_GETABSENCEINFO;
	else if (ctl_on || rbutton_on)
		return	FALSE;
	else if (!shareInfo && editSub.GetWindowTextLengthW() <= 0) {
		if (cmdHWnd || MessageBoxU8(LoadStrU8(IDS_EMPTYMSG), IP_MSG,
						 MB_OKCANCEL|MB_ICONQUESTION) == IDCANCEL) return FALSE;
	}

	if (listOperateCnt && !cmdHWnd) {
		MessageBoxU8(LoadStrU8(IDS_BUSYMSG));
		return	FALSE;
	}

	if ((sendEntryNum = (int)hostListView.SendMessage(LVM_GETSELECTEDCOUNT, 0, 0)) <= 0)
		return FALSE;

	if (!(sendEntry = new SendEntry [sendEntryNum])) {
		return	FALSE;
	}

	int		storeCnt = 0;
	for (int i=0; i < memberCnt && storeCnt < sendEntryNum; i++) {
		if (!hostListView.IsSelected(i))
			continue;
		if ((cfg->ClipMode & CLIP_ENABLE) && (hostArray[i]->hostStatus & IPMSG_CLIPBOARDOPT)) {
			is_clip_enable = TRUE;
		}
		sendEntry[storeCnt++].SetHost(hostArray[i]);
	}
	if ((sendEntryNum = storeCnt) == 0) {
		delete [] sendEntry;
		sendEntry = NULL;
		return	FALSE;
	}

	timerID = IPMSG_DUMMY_TIMER;	// この時点で送信中にする
	msg.timestamp = time(NULL);

	if (findDlg && findDlg->hWnd) findDlg->Destroy();
	TWin::Show(SW_HIDE);

//	DetachParent();

	if (shift_on) recvId = 0; // 返信Windowを閉じさせない
	::PostMessage(GetMainWnd(), WM_SENDDLG_HIDE, 0, twinId);

	if (is_clip_enable && msgMng->IsAvailableTCP()) {
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		ULONG	file_base = msgMng->MakePacketNo();

		for (int i=0; i < cfg->ClipMax; i++) {
			int		pos = 0;
			VBuf	*vbuf = editSub.GetPngByte(i, &pos);
			char	fname[MAX_PATH_U8];

			if (!vbuf) {
				break;
			}
			if (!shareInfo) {
				shareInfo = shareMng->CreateShare(packetNo);
			}
			MakeClipFileName(file_base, pos, 1, fname);
			if (shareMng->AddMemShare(shareInfo, fname, vbuf->Buf(), (DWORD)vbuf->Size(), pos)) {
				if (cfg->LogCheck /* && (cfg->ClipMode & CLIP_SAVE) */) {
					FileInfo *fileInfo = shareInfo->fileInfo[shareInfo->fileCnt -1];
					SaveImageFile(cfg, fileInfo->Fname(), vbuf);
				}
			}
			delete vbuf;
		}
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	}

	if (IsDlgButtonChecked(SECRET_CHECK) != 0) {
		command |= IPMSG_SECRETEXOPT;
	}
	if (cfg->PasswordUse && IsDlgButtonChecked(PASSWORD_CHECK) != 0) {
		command |= IPMSG_PASSWORDOPT;
	}
	if (shareInfo && shareInfo->fileCnt) {
		command |= IPMSG_FILEATTACHOPT;
		for (int i=0; i < shareInfo->fileCnt; i++) {
			FileInfo *fileInfo = shareInfo->fileInfo[i];
			if (GET_MODE(fileInfo->Attr()) == IPMSG_FILE_CLIPBOARD) {
				continue;
			}
			if ((cfg->downloadLink & 0x1) == 0 && cfg->LogCheck && *cfg->LogFile) {
				char	fname[MAX_PATH_U8];
				ForcePathToFname(fileInfo->Fname(), fname);
				CreateDownloadLinkU8(cfg, fname, fileInfo->Fname(), msg.timestamp, FALSE);
			}
		}
	}

//	editSub.GetWindowTextU8(msg.msgBuf, MAX_UDPBUF);
//	editSub.ExGetText(msg.msgBuf, MAX_UDPBUF);

	editSub.GetTextUTF8(msg.msgBuf, sizeof(msg.msgBuf), TRUE);

//	WCHAR	wbuf[MAX_UDPBUF];
//	memset(wbuf, 0, sizeof(wbuf));
//	editSub.GetStreamText(wbuf, MAX_UDPBUF, SF_UNICODE|SF_TEXT);
//	WtoU8(wbuf, msg.msgBuf, MAX_UDPBUF);

	logmng->WriteSendStart();

	// Addtional Option ... IPMSG_ENCRYPTOPT & IPMSG_UTF8OPT
	int		cryptCapa = cfg->GetCapa();
	ULONG	opt = IPMSG_CAPUTF8OPT | IPMSG_UTF8OPT
					| ((command & IPMSG_FILEATTACHOPT) ? IPMSG_ENCEXTMSGOPT : 0)
					| (cryptCapa ? IPMSG_ENCRYPTOPT : 0);
	ULONG	opt_sum = 0;
	BOOL	use_sign = FALSE;
	BOOL	is_multi = FALSE;

	if (sendEntryNum >= 2) {
		command |= IPMSG_MULTICASTOPT;
		if (sendEntryNum <= MAX_ULIST && cryptCapa && sendRecvList) {
			command |= IPMSG_ENCEXTMSGOPT;
			is_multi = TRUE;
		}
	}

	for (int i=0; i < sendEntryNum; i++) {
		SendEntry	*ent = sendEntry + i;
		Host		*host = ent->Host();
		ULONG		opt_target = opt & host->hostStatus;
		ULONG		cmd_target = command;

		if (opt_target & IPMSG_CAPUTF8OPT) {
			opt_target |= IPMSG_UTF8OPT;
		}
		opt_sum |= opt_target;
		if ((shareInfo && shareInfo->fileCnt == shareInfo->clipCnt) &&
			(host->hostStatus & IPMSG_CLIPBOARDOPT) == 0) {
			cmd_target &= ~(IPMSG_FILEATTACHOPT);
		}
		if ((host->hostStatus & IPMSG_ENCEXTMSGOPT) == 0) {
			cmd_target &= ~IPMSG_ENCEXTMSGOPT;
		}
		if ((cmd_target & IPMSG_ENCEXTMSGOPT) && is_multi) {
			ent->SetUseUlist(TRUE);
		}
		logmng->WriteSendHead(host);

		if (GetUserNameDigestField(host->hostSub.u.userName)) {
			use_sign = TRUE;
		}

		ent->SetStatus((opt_target & IPMSG_ENCRYPTOPT) == 0 ? ST_MAKEMSG :
					host->pubKey.Key() ? ST_MAKECRYPTMSG : ST_GETCRYPT);
		ent->SetCommand(cmd_target | opt_target);

		if (shareInfo && shareInfo->fileCnt > 0 && (cmd_target & IPMSG_FILEATTACHOPT)) {
			if (shareInfo->fileCnt > shareInfo->clipCnt) {
				ent->SetUseFile(TRUE);
			}
			if (shareInfo->clipCnt > 0) {
				ent->SetUseClip(TRUE);
			}
		}
	}
	command |= opt_sum;

	int	share_len = 0;
	if (shareInfo && shareInfo->fileCnt)		// ...\0no:fname:size:mtime:
	{
		DynBuf	buf(MAX_UDPBUF * 2 / 3);
		shareInfo->EncodeMsg(buf, (int)buf.Size(), &shareDictList, &share_len);
		shareStr = strdupNew(buf);

		shareMng->AddHostShare(shareInfo, sendEntry, sendEntryNum);
	}


	// メッセージ切りつめ
	int ulist_len = 0;
	if (is_multi) {
		char *tmp_ulist = msg.exBuf;
		*tmp_ulist = 0;
		MakeUlistStr(0, tmp_ulist);
		ulist_len = (int)strlen(tmp_ulist) + 1;
	}
	int	max_len = msgMng->GetEncryptMaxMsgLen() - (share_len + ulist_len);
	TruncateMsg(msg.msgBuf, FALSE, max_len);

	logmng->WriteSendMsg(packetNo, msg.msgBuf, command,
				(opt_sum & IPMSG_ENCRYPTOPT) == 0 ? 0 :
				IsUserNameExt(cfg) && use_sign ?
					((cryptCapa & IPMSG_SIGN_SHA256) ? LOG_SIGN2_OK : LOG_SIGN_OK) :
					(cryptCapa & IPMSG_RSA_2048) ? LOG_ENC2 :
					(cryptCapa & IPMSG_RSA_1024) ? LOG_ENC1 : LOG_ENC0,
			msg.timestamp, shareInfo);

	AddLruUsers();
	if (GET_MODE(command) == IPMSG_GETINFO) {
		::PostMessage(GetMainWnd(), WM_HISTDLG_OPEN, 1, 0);
	}

	SendMsgCore();

	timerID = IPMSG_SEND_TIMER;
	SetTimer(IPMSG_SEND_TIMER, cfg->RetryMSec);

	return	TRUE;
}

//BOOL TSendDlg::DetachParent(HWND hTarget)
//{
//	if (parent && parent->hWnd && (!hTarget || hTarget == parent->hWnd)) {
//		::SetParent(hWnd, 0);
//		parent = NULL;
//		return	TRUE;
//	}
//	return	FALSE;
//}

/*
	メッセージの暗号化
*/
BOOL TSendDlg::MakeMsgPacket(SendEntry *entry)
{
	DynBuf	buf(MAX_UDPBUF);
	DynBuf	pktBuf(MAX_UDPBUF);
	BOOL	ret = FALSE;

	if ((entry->Host()->hostStatus & IPMSG_CAPIPDICTOPT) && cfg->IPDictEnabled()) {
		IPDict	dict;

		msgMng->InitIPDict(&dict, GET_MODE(entry->Command()), GET_OPT(entry->Command()), packetNo);
		if (entry->UseUlist()) {
			MakeUlistDict(int(entry - sendEntry), &dict);
		}
		if (shareDictList.size() > 0) {
			msgMng->AddFileShareDict(&dict, shareDictList);
		}
		LocalNewLineToUnix(msg.msgBuf, buf, MAX_UDPBUF);
		msgMng->AddBodyDict(&dict, buf);
		msgMng->EncIPDict(&dict, &entry->Host()->pubKey, &pktBuf);
	}
	else {
		if (entry->Status() == ST_MAKECRYPTMSG) {
			char *ulist = NULL;
			if (entry->UseUlist()) {
				ulist = pktBuf;
				MakeUlistStr(int(entry - sendEntry), ulist);
			}
			ret = msgMng->MakeEncryptMsg(entry->Host(), packetNo, msg.msgBuf,
				shareStr || ulist ? true : false, shareStr, ulist, buf);
		}
		if (!ret) {
			entry->SetCommand(entry->Command() & ~IPMSG_ENCRYPTOPT);
			LocalNewLineToUnix(msg.msgBuf, buf, MAX_UDPBUF);
		}

		int		msg_len = 0;
		msgMng->MakeMsg(pktBuf, packetNo, entry->Command(), buf,
						(entry->Command() & IPMSG_ENCEXTMSGOPT)  != 0 ||
						(entry->Command() & IPMSG_FILEATTACHOPT) == 0 ? NULL : shareStr,
						NULL, &msg_len);
		pktBuf.SetUsedSize(msg_len);
	}

	entry->SetMsg(pktBuf, (int)pktBuf.UsedSize());
	entry->SetStatus(ST_SENDMSG);

	return	TRUE;
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
		msgMng->GetPubKey(&entry->Host()->hostSub, FALSE);
	}
	if (entry->Status() == ST_MAKECRYPTMSG || entry->Status() == ST_MAKEMSG) {
		MakeMsgPacket(entry);		// ST_MAKECRYPTMSG/ST_MAKEMSG -> ST_SENDMSG
	}
	if (entry->Status() == ST_SENDMSG) {
		msgMng->UdpSend(entry->Host()->hostSub.addr, entry->Host()->hostSub.portNo,
						entry->Msg(), entry->MsgLen(retryCnt >= 3 ? true : false));
	}
	return	TRUE;
}

/*
	送信終了通知
	packet_noが、この SendDialogの送った送信packetであれば、TRUE
*/
BOOL TSendDlg::SendFinishNotify(HostSub *hostSub, ULONG packet_no)
{
	for (int i=0; i < sendEntryNum; i++) {
		SendEntry	*ent = sendEntry + i;
		if (ent->Status() == ST_SENDMSG &&
			ent->Host()->hostSub.addr == hostSub->addr &&
			ent->Host()->hostSub.portNo == hostSub->portNo &&
			(packet_no == packetNo || packet_no == 0))
		{
			ent->SetStatus(ST_DONE);
			if (GET_MODE(ent->Command()) == IPMSG_SENDMSG &&
				(ent->Command() & IPMSG_SECRETOPT)) {
				U8str	histMsg;
				if (ent->Command() & IPMSG_FILEATTACHOPT) {
					int	id = ent->UseClip() ? ent->UseFile() ? IDS_FILEWITHCLIP :
						IDS_WITHCLIP : IDS_FILEATTACH;
					histMsg += LoadStrU8(id);
					histMsg += " ";
				}
				histMsg += msg.msgBuf;
				HistNotify hn = { &ent->Host()->hostSub, packetNo, histMsg.s() };
				::SendMessage(GetMainWnd(), WM_HISTDLG_NOTIFY,  (WPARAM)&hn, 0);
			}
			if (IsSendFinish() && hWnd)		//再送MessageBoxU8を消す
			{
				if (retryDlg.hWnd) {
					Debug("done=%p", retryDlg.hWnd);
					retryDlg.Destroy();
				}
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
		SendEntry	*ent = sendEntry + i;

		if (ent->Status() == ST_GETCRYPT
			&& IsSameHostEx(&ent->Host()->hostSub, hostSub))
		{
			if (!ent->Host()->pubKey.Key()) {
				ent->Host()->pubKey.Set(pubkey, len, e, capa);
			}
			ent->SetStatus(ST_MAKECRYPTMSG);
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
		SendEntry	*ent = sendEntry + i;

		if (ent->Status() != ST_DONE)
		{
			finish = FALSE;
			break;
		}
	}

	return	finish;
}

HBITMAP CreateStatusBmp(int cx, int cy, int status)
{
	VBuf	bmpVbuf;
	int		aligned_line_size;

	if (!PrepareBmp(cx, cy, &aligned_line_size, &bmpVbuf)) return NULL;

	int		lines  = cy / 3;
	int		offset = (cy - lines * 3) / 2;

	DWORD	color =  0;

	switch (status) {
	case STI_NONE:		color = rRGB(128,128,128);	break; // Win v1.00-v1.47
	case STI_FILE:		color = rRGB(180,180,240);	break; // MacOSX
	case STI_CLIP:		color = rRGB(200,200,255);	break;
	case STI_ENC:		color = rRGB(200,160,160);	break;
	case STI_SIGN:		color = rRGB(240,180,180);	break;
	case STI_ENC_FILE:	color = rRGB(230,230,230);	break; // Win v2.00-v2.11
	case STI_ENC_CLIP:	color = rRGB(230,230,255);	break; // Win v3.00- in Win2K
	case STI_SIGN_FILE:	color = rRGB(255,200,200);	break; // Win v3.00 disable Clip settings
	case STI_SIGN_CLIP:	color = rRGB(255,255,255);	break; // Win v3.00-
	}

	for (int i=0; i < cy; i++) {
		BYTE	*data = bmpVbuf.UsedEnd() + aligned_line_size * i;
		for (int j=0; j < cx; j++) {
			memcpy(data, &color, 3);
			data += 3;
		}
	}
	return	FinishBmp(&bmpVbuf);
}

HBITMAP CreateAbsenceBmp(int cx, int cy)
{
	VBuf	bmpVbuf;
	int		aligned_line_size;
	int		i, j;

	if (!PrepareBmp(cx, cy, &aligned_line_size, &bmpVbuf)) return NULL;

	DWORD	on  = rRGB( 64, 64, 64);
	DWORD	mid = rRGB(100,100,100);
	DWORD	off = rRGB(255,255,255);
	DWORD	rgb, last_rgb = off;

	for (i=0; i < cy; i++) {
		BYTE	*data = bmpVbuf.UsedEnd() + aligned_line_size * i;
		float	yratio = (float)i / cy;
		rgb = yratio >= 0.10 && yratio < 0.24 || yratio >= 0.33 && yratio < 0.83 ? on : off;
//		if (last_rgb != mid && rgb != last_rgb) rgb = mid;
		last_rgb = rgb;
		for (j=0; j < cx; j++) {
			float	xratio = (float)j / cx;
			DWORD	*target = xratio >= 0.25 && xratio < 0.65 ? &rgb : &off;
			memcpy(data, target, 3);
			data += 3;
		}
	}
	return	FinishBmp(&bmpVbuf);
}


/*
	Font 設定
*/
void TSendDlg::SetFont(BOOL force_reset)
{
	if (!*orgFont.lfFaceName) {
		HFONT	hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0);

		if (!hFont || ::GetObject(hFont, sizeof(LOGFONT), (void *)&orgFont) == 0) return;
		if (*cfg->SendEditFont.lfFaceName == 0) cfg->SendEditFont = orgFont;
		if (*cfg->SendListFont.lfFaceName == 0) {
			cfg->SendListFont = orgFont;
			char	*ex_font = LoadStrA(IDS_PROPORTIONALFONT);
			if (ex_font && *ex_font) {
				strcpy(cfg->SendListFont.lfFaceName, ex_font);
			}
		}
	}

	if (!hListFont || !hEditFont || force_reset) {
		if (hListFont) ::DeleteObject(hListFont);
		if (hEditFont) ::DeleteObject(hEditFont);

		if (*cfg->SendListFont.lfFaceName) {
			hListFont = ::CreateFontIndirect(&cfg->SendListFont);
		}
		if (*cfg->SendEditFont.lfFaceName) {
			hEditFont = ::CreateFontIndirect(&cfg->SendEditFont);
		}
	}

	hostListView.SendMessage(WM_SETFONT, (WPARAM)hListFont, 0);
	hostListHeader.ChangeFontNotify();
	if (hostListView.GetItemCount() > 0) {	// EvCreate時は不要
		ReregisterEntry(TRUE);
	}
	editSub.SetFont(&cfg->SendEditFont);

	static HIMAGELIST	himlState;
	static int			lfHeight;

	if (!himlState || lfHeight != cfg->SendListFont.lfHeight) {
		int		cx = abs(cfg->SendListFont.lfHeight) / 3;
		int		cy = abs(cfg->SendListFont.lfHeight);
		HBITMAP	hbmpItem;

		if (himlState)	ImageList_Destroy(himlState);
		lfHeight = cfg->SendListFont.lfHeight;
		himlState = ImageList_Create(cx, cy, ILC_COLOR24|ILC_MASK, STI_MAX, 0);

		for (int i=0; i < STI_MAX; i++) {
			hbmpItem = CreateStatusBmp(cx, cy, i);
			ImageList_Add(himlState, hbmpItem, NULL);
			::DeleteObject(hbmpItem);
		}
		hbmpItem = CreateAbsenceBmp(cx, cy);
		ImageList_AddMasked(himlState, hbmpItem, RGB(255,255,255));
		::DeleteObject(hbmpItem);
		ImageList_SetOverlayImage(himlState, STI_MAX, 1);
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

	::GetWindowPlacement(refreshBtn.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[refresh_item]);

	::GetWindowPlacement(menuCheck.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[menu_item]);

	::GetWindowPlacement(editSub.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[edit_item]);

	::GetWindowPlacement(captureBtn.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[capture_item]);

	::GetWindowPlacement(sendBtn.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[ok_item]);

	::GetWindowPlacement(repFilCheck.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[repfil_item]);

	::GetWindowPlacement(GetDlgItem(FILE_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[file_item]);

	::GetWindowPlacement(GetDlgItem(SECRET_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[secret_item]);

	::GetWindowPlacement(GetDlgItem(PASSWORD_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[passwd_item]);

	GetWindowRect(&rect);
	orgRect = rect;

	TRect	scRect;
	GetCurrentScreenSize(&scRect, hRecvWnd);

	int	cx = scRect.cx();
	int	cy = scRect.cy();
	int	xsize = rect.cx() + cfg->SendXdiff;
	int	ysize = rect.cy() + cfg->SendYdiff;

	POINT	pt = {};
	LONG	&x = pt.x;
	LONG	&y = pt.y;

	if (cfg->SendSavePos) {
		x = cfg->SendXpos;
		y = cfg->SendYpos;
	}
	else {
		if (posMode == ReplyInfo::NONE) {
			x = (cx - xsize)/2 + (rand() % (cx/4)) - cx/8;
			y = (cy - ysize)/2 + (rand() % (cy/4)) - cy/8;
		}
		else {
			POINT	pt = {};
			GetCursorPos(&pt);

			switch (posMode) {
			case ReplyInfo::POS_MID:
				x = pt.x - (xsize / 3);
				y = pt.y - (ysize / 4);
				break;

			case ReplyInfo::POS_MIDDOWN:
				x = pt.x - (xsize / 3);
				y = pt.y + 10;
				break;

			default:
				x = pt.x + 10;
				y = pt.y + 10;
				break;
			}
		}
	}
	if (cfg->SendSavePos == 0 || !MonitorFromPoint(pt, MONITOR_DEFAULTTONULL)) {
		if (x + xsize > cx) {
			x = cx - xsize;
		}
		if (y + ysize > cy) {
			y = cy - ysize;
		}
		x = max(x, scRect.left);
		y = max(y, scRect.top);
	}

	EvSize(SIZE_RESTORED, 0, 0);
	MoveWindow(x, y, xsize, ysize, TRUE);
}

void TSendDlg::PopupContextMenu(POINTS pos)
{
	HMENU	hMenu = ::CreatePopupMenu();
	HMENU	hPriorityMenu = ::CreateMenu();
	HMENU	hGroupMenu = ::CreateMenu();
	char	buf[MAX_BUF];
	int		selectNum = hostListView.GetSelectedItemCount();
//	char	*appendStr = selectNum > 0 ? LoadStrU8(IDS_MOVETO) : LoadStrU8(IDS_SELECT);
	char	*appendStr = selectNum > 0 ? LoadStrU8(IDS_MOVETO) : LoadStrU8(IDS_MOVETO);
	u_int	flag = selectNum <= 0 ? MF_GRAYED : 0;
	BOOL	isJapanese = IsLang(LANG_JAPANESE);

	if (!hMenu || !hPriorityMenu || !hGroupMenu) return;

	SetMainMenu(hMenu);

// group select
	int	rowMax = ::GetSystemMetrics(SM_CYSCREEN) / ::GetSystemMetrics(SM_CYMENU) -1;

	for (int i=0; i < memberCnt; i++) {
		int	menuMax = ::GetMenuItemCount(hGroupMenu);
		int	cnt2;

		for (cnt2=0; cnt2 < menuMax; cnt2++) {
			GetMenuStringU8(hGroupMenu, cnt2, buf, sizeof(buf), MF_BYPOSITION);
			if (strcmp(buf, hostArray[i]->groupName) == 0)
				break;
		}
		if (cnt2 == menuMax && *hostArray[i]->groupName)
			AppendMenuU8(hGroupMenu, MF_STRING|((menuMax % rowMax || !menuMax) ? 0 : MF_MENUBREAK),
						MENU_GROUP_START + menuMax, hostArray[i]->groupName);
	}
	InsertMenuU8(hMenu, 1 + (cfg->lruUserMax > 0 ? 1 : 0),
				MF_POPUP|(::GetMenuItemCount(hGroupMenu) ? 0 : MF_GRAYED)|MF_BYPOSITION,
				(UINT_PTR)hGroupMenu, LoadStrU8(IDS_GROUPSELECT));

	if (selectNum >= 2) {
		AppendMenuU8(hMenu, MF_STRING|(sendRecvList ? MF_CHECKED : 0),
			MENU_LISTINCLUDE, LoadStrU8(IDS_LISTINCLUDE));
	}

// priority menu
	for (int i=cfg->PriorityMax; i >= 0; i--) {
		int	len = 0;

		if (!isJapanese)
			len += snprintfz(buf + len, sizeof(buf)-len, "%s ", appendStr);

		if (i == 0) {
			len += snprintfz(buf + len, sizeof(buf)-len, LoadStrU8(IDS_NODISP));
		}
		else if (i == 1) {
			len += snprintfz(buf + len, sizeof(buf)-len, LoadStrU8(IDS_DEFAULTDISP));
		}
		else {
			len += snprintfz(buf + len, sizeof(buf)-len, LoadStrU8(IDS_DISPPRIORITY),
					cfg->PriorityMax - i + 1);
		}

		if (isJapanese) {
			len += snprintfz(buf + len, sizeof(buf)-len, " %s ", appendStr);
		}

		snprintfz(buf + len, sizeof(buf)-len,
				i == 1 ? LoadStrU8(IDS_MEMBERCOUNTDEF) : LoadStrU8(IDS_MEMBERCOUNT),
				hosts->PriorityHostCnt(i * PRIORITY_OFFSET, PRIORITY_OFFSET),
				cfg->priorityHosts.PriorityHostCnt(i * PRIORITY_OFFSET, PRIORITY_OFFSET));
		AppendMenuU8(hPriorityMenu, MF_STRING|flag, MENU_PRIORITY_START + i * PRIORITY_OFFSET,
					buf);
	}

	AppendMenuU8(hPriorityMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hPriorityMenu, MF_STRING|(hiddenDisp ? MF_CHECKED : 0), MENU_PRIORITY_HIDDEN,
				LoadStrU8(IDS_TMPNODISPDISP));
	AppendMenuU8(hPriorityMenu, MF_STRING, MENU_PRIORITY_RESET, LoadStrU8(IDS_RESETPRIORITY));
	InsertMenuU8(hMenu, 2 + (cfg->lruUserMax > 0 ? 1 : 0),
				MF_POPUP|MF_BYPOSITION, (UINT_PTR)hPriorityMenu, LoadStrU8(IDS_SORTFILTER));

	::TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);
}

void TSendDlg::SetMainMenu(HMENU hMenu)
{
	if (cfg->lruUserMax > 0) {
		HMENU	hLruMenu = ::CreateMenu();
		int		i=0;
		for (auto obj=cfg->lruUserList.EndObj(); obj && i < cfg->lruUserMax;
				obj=cfg->lruUserList.PrevObj(obj)) {
			char	buf[MAX_BUF];
			int		len = 0;
			BOOL	total_enabled = FALSE;
			for (auto user=obj->users.TopObj(); user; user=obj->users.NextObj(user)) {
				Host *host = hosts->GetHostByName(&user->hostSub);
				BOOL enabled = FALSE;
				if (host) total_enabled = enabled = TRUE;
				if (!host) host = cfg->priorityHosts.GetHostByName(&user->hostSub);
				len += snprintfz(buf + len, sizeof(buf)-len, "%s%s",
					len ? "  " :  "", enabled ? "" : "(");
				if (host && *host->nickName) {
					len += snprintfz(buf + len, sizeof(buf)-len, "%s", host->nickName);
				}
				else {
					len += snprintfz(buf + len, sizeof(buf)-len, "%s/%s",
									user->hostSub.u.userName, user->hostSub.u.hostName);
				}
				if (!enabled) len += snprintfz(buf + len, sizeof(buf)-len, ")");
				if (len > 120) {
					len += snprintfz(buf + len, sizeof(buf)-len, " ...(%d)", obj->users.Num());
					break;
				}
			}
			AppendMenuU8(hLruMenu, MF_STRING|(total_enabled ? 0 : MF_DISABLED|MF_GRAYED),
							MENU_LRUUSER + i++, buf);
		}
		i = min(cfg->lruUserList.Num(), cfg->lruUserMax);
		AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)hLruMenu, Fmt(LoadStrU8(IDS_LRUUSER), i));
	}
	AppendMenuU8(hMenu, MF_STRING, MENU_FINDDLG, LoadStrU8(IDS_FINDDLG));
	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);

	BOOL file_flg = (msgMng->IsAvailableTCP() && cfg->NoFileTrans != 1) ? 0 : MF_DISABLED|MF_GRAYED;
	AppendMenuU8(hMenu, MF_STRING|file_flg, MENU_FILEADD, LoadStrU8(IDS_FILEATTACHMENU));
	AppendMenuU8(hMenu, MF_STRING|file_flg, MENU_FOLDERADD, LoadStrU8(IDS_FOLDERATTACHMENU));

	HMENU	hImgMenu = ::CreateMenu();
	AppendMenuU8(hImgMenu, MF_STRING|((cfg->ClipMode & CLIP_ENABLE) ? 0 : MF_DISABLED|MF_GRAYED),
						MENU_IMAGERECT, LoadStrU8(IDS_IMAGERECTMENU));
	AppendMenuU8(hImgMenu, MF_STRING|
						(!(cfg->ClipMode & CLIP_ENABLE) || !IsImageInClipboard(hWnd) ?
							MF_DISABLED|MF_GRAYED : 0),
						MENU_IMAGEPASTE, LoadStrU8(IDS_IMAGEPASTEMENU));
	AppendMenuU8(hImgMenu, MF_STRING|((cfg->ClipMode & CLIP_ENABLE) ? 0 : MF_DISABLED|MF_GRAYED),
						WM_INSERT_IMAGE, LoadStrU8(IDS_INSERT_IMAGE));

	AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)hImgMenu, LoadStrU8(IDS_IMAGEMENU));

	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);

//	AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)::LoadMenu(TApp::hInst(), (LPCSTR)SENDFONT_MENU),
//						LoadStrU8(IDS_FONTSET));
	AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)::LoadMenu(TApp::hInst(), (LPCSTR)SENDSIZE_MENU),
						LoadStrU8(IDS_SIZESET));
//	AppendMenuU8(hMenu, MF_STRING, MENU_SAVECOLUMN, LoadStrU8(IDS_SAVECOLUMN));
//	AppendMenuU8(hMenu, MF_STRING, MENU_SAVEPOS, LoadStrU8(IDS_SAVEPOS));
//	AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
	AppendMenuU8(hMenu, MF_STRING, MENU_MEMBERDISP, LoadStrU8(IDS_MEMBERDISP));
	AppendMenuU8(hMenu, MF_STRING, MENU_SETUP, LoadStrU8(IDS_SETUP));

	if (cfg->LogCheck && *cfg->LogFile) {
		AppendMenuU8(hMenu, MF_SEPARATOR, 0, 0);
		AppendMenuU8(hMenu, MF_STRING, MENU_LOGVIEWER, LoadStrU8(IDS_OPENLOGVIEW));
	}
}

void TSendDlg::Finished()
{
	if (cmdHWnd && cfg->IPDictEnabled()) {
		IPIpc			ipc;
		IPDict			out;
		IPDictStrList	tl;
		int				num = 0;

		for (int i=0; i < sendEntryNum; i++) {
			SendEntry	*ent = sendEntry + i;
			auto	u = make_shared<U8str>(
				Fmt("%s : %s", (ent->Status()==ST_DONE) ? "OK" : "NG", ent->Host()->S()));
			tl.push_back(u);
			if (ent->Status() == ST_DONE) {
				num++;
			}
		}
		out.put_str_list(IPMSG_TOLIST_KEY, tl);
		out.put_int(IPMSG_STAT_KEY, num);

		ipc.SaveDictToMap(cmdHWnd, FALSE, out);
		::SendMessage(cmdHWnd, WM_IPMSG_CMDRES, 0, 0);
	}

	if (!modalCount) {
		::PostMessage(GetMainWnd(), WM_SENDDLG_EXIT, 0, twinId);
	}
}


