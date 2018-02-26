static char *recvdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2017   recvdlg.cpp	Ver4.60";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Receive Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2017-07-16(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "ipmsg.h"
#include <process.h>
#include <map>

int		TRecvDlg::createCnt	= 0;
HBITMAP	TRecvDlg::hDummyBmp	= 0;
HFONT	TRecvDlg::hHeadFont	= NULL;
HFONT	TRecvDlg::hEditFont	= NULL;
LOGFONT	TRecvDlg::orgFont	= {};


TRecvDlg::TRecvDlg(MsgMng *_msgMng, THosts *_hosts, Cfg *_cfg, LogMng *_logmng, TWin *_parent)
	: TListDlg(RECEIVE_DIALOG, _parent), editSub(_cfg, this), replyBtn(this),
		logViewBtn(this), recvHead(this)
{
	msgMng			= _msgMng;
	cfg				= _cfg;
	openFlg			= FALSE;
	logmng			= _logmng;
	hosts			= _hosts;
	fileObj			= NULL;
	shareInfo		= NULL;
	recvEndDlg		= NULL;
	timerID			= 0;
	retryCnt		= 0;
	cryptCapa		= 0;
	logOpt			= 0;
	useClipBuf		= 0;
	useDummyBmp		= 0;
	isRep			= FALSE;
	isAutoSave		= FALSE;
	*autoSaves		= 0;
	hookCheck		= FALSE;
	createCnt++;

	if (!hDummyBmp) {
		hDummyBmp = LoadBitmap(TApp::hInst(), (LPCSTR)DUMMYPIC_BITMAP);
	}
}

TRecvDlg::~TRecvDlg()
{
	if (shareInfo) {
		if (shareInfo->fileCnt > 0 &&
			(!cfg->IsSavedPacket(&msg) || !cfg->ReproMsg)) {
			msgMng->Send(&msg.hostSub, IPMSG_RELEASEFILES, msg.packetNo);
			if (IsSameHost(&msg.hostSub, msgMng->GetLocalHost())) {
				cfg->DeleteShare(msg.packetNo);
			}
		}
		// あとで受信終了通知のコードを入れる
		delete shareInfo;
		shareInfo = NULL;
	}
	if (fileObj) {
		delete fileObj->conInfo;
		delete fileObj;
	}

	ClipBuf *clipBuf;
	while ((clipBuf = clipList.TopObj())) {
		clipList.DelObj(clipBuf);
		delete clipBuf;
	}
	CleanupAutoSaveSize();

	createCnt--;
}

TRecvDlg::SelfStatus TRecvDlg::Init(MsgBuf *msgOrg, const char *rep_head, ULONG clipBase,
	const char *auto_saved)
{
	msg.Init(msgOrg);

	if (rep_head) {
		isRep = TRUE;	// reproduction mode
		strcpy(head, rep_head);
	} else {
		MakeListString(cfg, &msg.hostSub, hosts, head);
	}

	status = INIT;
	if (msg.decMode == MsgBuf::DEC_OK) {
		logOpt |= LOG_ENC2;
		logOpt |= (msg.signMode == MsgBuf::SIGN_OK) ? LOG_SIGN2_OK : LOG_SIGN_ERR;
	}
	else if ((msg.command & IPMSG_ENCRYPTOPT)) {
		status = msgMng->DecryptMsg(&msg, &cryptCapa, &logOpt) ? INIT : ERR;
		if (status == ERR) {
			if (!isRep) {
				msgMng->Send(&msg.hostSub,
					IPMSG_SENDMSG|IPMSG_NOLOGOPT|IPMSG_AUTORETOPT, LoadStr(IDS_DECRYPTERR));
			}
		}
		else if (logOpt & LOG_SIGN_NOKEY) { // 署名検証用の公開鍵を送ってもらう
			Host	*host = hosts->GetHostByName(&msg.hostSub);
			if (!host) host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
			if (!host || !host->pubKey.Key()) {
				if (!host && GetMain()) {
					GetMain()->BroadcastEntrySub(&msg.hostSub, IPMSG_BR_ENTRY);
				}
				msgMng->GetPubKey(&msg.hostSub);
			}
		}
	}
//	if (cfg->masterPub.KeyLen() > 0) {
//		//GenUlData();
//	}

	DWORD	share_flg = ((cfg->ClipMode & CLIP_ENABLE) ? ShareInfo::SI_CLIP : 0) |
						(cfg->NoFileTrans == 1 ? 0 : ShareInfo::SI_FILE);

	if (msg.ipdict.has_key(IPMSG_FILE_KEY)) {
		shareInfo = new ShareInfo(msg.ipdict, share_flg);
	}
	else if (msg.command & IPMSG_FILEATTACHOPT) {
		shareInfo = new ShareInfo(msg.exBuf, share_flg);
	}
	if (shareInfo) {
		if (shareInfo->fileCnt > 0) {
			fileObj = new RecvFileObj;
		}
		else {
			delete shareInfo;
			shareInfo = NULL;
		}
	}

	if (shareInfo && (cfg->ClipMode & CLIP_ENABLE)) {
		InitCliped(&clipBase);
	}

	if (shareInfo && fileObj && auto_saved && *auto_saved) {
		InitAutoSaved(auto_saved);
	}

	replyList.push_back(msg.hostSub);

	if (msg.ipdict.has_key(IPMSG_TOLIST_KEY)) {
		msgMng->UListToHostSubDict(&msg, &recvList);
	}
	else if ((msg.command & IPMSG_ENCEXTMSGOPT) && *msg.uList) {
		msgMng->UListToHostSub(&msg, &recvList);
	}
	for (auto &h: recvList) {
		replyList.push_back(h);
	}

	if (!VerifyUserNameExtension(cfg, &msg)) {
		logOpt |= LOG_UNAUTH;
	}

	if (status != ERR) {
		if ((msg.command & IPMSG_PASSWORDOPT) == 0 || !cfg->PasswordUse || !cfg->PasswdLogCheck) {
			if (!isRep) {
				logmng->WriteRecvMsg(&msg, logOpt, hosts, shareInfo, &recvList, &msg.msgId);
				msgOrg->msgId = msg.msgId; // for cfg->SavePacket
			}
		}
		hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
		CheckSpecialCommand();
	}

	if (!isRep && status != ERR && status != REMOTE) {
		if (cfg->ReproMsg) {
			cfg->SavePacket(msgOrg, head, clipBase); // encoded message
		}
	}

	return	status;
}

BOOL TRecvDlg::GetLogStr(U8str *u)
{
	return logmng->GetRecvMsg(&msg, logOpt, hosts, shareInfo, &recvList, u);
}

BOOL TRecvDlg::InitCliped(ULONG *_clipBase)
{
	int		clip_num = 0;
	int		noclip_num = 0;
	ULONG	&clipBase = *_clipBase;

	if (clipBase == 0) {
		clipBase = msgMng->MakePacketNo();
	}

	for (int i=0; i < shareInfo->fileCnt; i++) {
		if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
			const char *ext = strrchr(shareInfo->fileInfo[i]->Fname(), '.');
			if (ext && strcmpi(ext, ".png") == 0 && clip_num++ < cfg->ClipMax) {
				int		cur_pos = shareInfo->fileInfo[i]->Pos();
				char	buf[MAX_PATH_U8];

				for (int j=0; j < i; ) {	// pos が被った場合、次のposに
					if (cur_pos == shareInfo->fileInfo[j]->Pos()) {
						cur_pos++;
						j = 0;	// 最初から確認しなおし
					} else j++;
				}
				shareInfo->fileInfo[i]->SetPos(cur_pos);
				MakeClipFileName(clipBase, cur_pos, 0, buf);
				shareInfo->fileInfo[i]->SetFname(buf);
				shareInfo->fileInfo[i]->SetSelected(TRUE);
			}
			else {
				shareInfo->RemoveFileInfo(i);
				i--;
			}
		}
		else {
			noclip_num++;
		}
	}
	if (clip_num) {
		useClipBuf = noclip_num ? 2 : 1;
	}
	return	TRUE;
}

WCHAR **AutoSaveToWArray(const char *auto_saved, int *_num)
{
	char	*saved = strdup(auto_saved);
	WCHAR	**ret = NULL;
	char	*p = NULL;
	int		&num = *_num;

	num = 0;

	for (char *tok=sep_tok(saved, ':', &p); tok; tok=sep_tok(NULL, ':', &p)) {
		if (char *equal = strchr(tok, '=')) {
			if (!(ret = (WCHAR **)realloc(ret, sizeof(WCHAR *) * ++num))) {
				num = 0;
				break;
			}
			ret[num-1] = U8toW(equal + 1);
		}
	}
	free(saved);
	return	ret;
}

void FreeWArray(WCHAR **array, int num)
{
	if (array) {
		for (int i=0; i < num; i++) {
			delete [] array[i];
		}
		free(array);
	}
}

BOOL TRecvDlg::InitAutoSaved(const char *auto_saved)
{
	strcpy(autoSaves, auto_saved);
	std::map<int, char *> fmap;

	for (char *tok=strtok(autoSaves, ":"); tok; tok=strtok(NULL, ":")) {
		char	*equal = strchr(tok, '=');
		if (!equal) continue;
		*equal = 0;
		fmap[strtol(tok, 0, 16)] = equal + 1;
	}

	for (int i=shareInfo->fileCnt-1; i >= 0; i--) {
		if (GET_MODE(shareInfo->fileInfo[i]->Attr()) != IPMSG_FILE_CLIPBOARD) {
			auto itr = fmap.find(shareInfo->fileInfo[i]->Id());
			if (itr != fmap.end()) {
				shareInfo->RemoveFileInfo(i);
			}
		}
	}
	isAutoSave = TRUE;

	strcpy(autoSaves, auto_saved);
	return	TRUE;
}

BOOL TRecvDlg::PreProcMsg(MSG *msg)
{
	if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE) {
		return TRUE;
	}
	return	TDlg::PreProcMsg(msg);
}

BOOL TRecvDlg::EvCreate(LPARAM lParam)
{
	editSub.AttachWnd(GetDlgItem(RECV_EDIT));
	editSub.SendMessage(EM_SETBKGNDCOLOR, FALSE, ::GetSysColor(COLOR_3DFACE));

	replyBtn.AttachWnd(GetDlgItem(IDOK));
	replyBtn.CreateTipWnd();

	logViewBtn.AttachWnd(GetDlgItem(LOGVIEW_BUTTON));
	logViewBtn.CreateTipWnd(LoadStrW(IDS_OPENLOGVIEW));

	recvHead.AttachWnd(GetDlgItem(RECV_HEAD));
	recvHead.CreateTipWnd(U8toWs(head));

	SetDlgIcon(hWnd);
	recvHead.SetWindowTextU8(head);

	UINT	rtitle_id = IDS_UNICAST;
	if (msg.command & IPMSG_AUTORETOPT) {
		rtitle_id = IDS_UNIABSENCE;
	} else if (msg.command & IPMSG_BROADCASTOPT) {
		rtitle_id = IDS_BROADCAST;
	} else if (msg.command & IPMSG_MULTICASTOPT) {
		rtitle_id = IDS_MULTICAST;
	}
	SetDlgItemTextU8(RECV_TITLE, LoadStrU8(rtitle_id));

	char	buf[MAX_LISTBUF];
	char	title[MAX_LISTBUF];
	GetWindowTextU8(buf, sizeof(buf));

	snprintfz(title, sizeof(title), "%s%s", isRep ? LoadStrU8(IDS_RESTORE) : "", buf);

	if (msg.command & IPMSG_ENCRYPTOPT) {
		strcat(title,
			(logOpt & LOG_SIGN2_OK) ? " ++++" :
			(logOpt & LOG_SIGN_OK) ? " +++" :
			(logOpt & LOG_ENC2)    ? " ++"  :
			(logOpt & LOG_ENC1)    ? " +" : " -");
		if (logOpt & LOG_SIGN_ERR) strcat(title, LoadStrU8(IDS_SIGN_ERROR_TITLE));
	}
	else if (logOpt & LOG_UNAUTH) {
		strcpy(title, LoadStrU8(IDS_UNAUTHORIZED_TITLE));
	}

	SetWindowTextU8(title);

	char	head2[MAX_NAMEBUF];
	sprintf(head2, "at %s", Ctime(&msg.timestamp));
	SetDlgItemTextU8(RECV_HEAD2, head2);

	editSub.ExSetText(msg.msgBuf);
//	editSub.SetWindowTextU8(msg.msgBuf);

	if (msg.command & IPMSG_SECRETOPT) {
		editSub.ShowWindow(SW_HIDE);
		::ShowWindow(GetDlgItem(QUOTE_CHECK), SW_HIDE);
	}
	else {
		::ShowWindow(GetDlgItem(OPEN_BUTTON), SW_HIDE);
		openFlg = TRUE;
		if (shareInfo) {
			SetFileButton(this, FILE_BUTTON, shareInfo, autoSaves);
		}
	}

	if (msg.command & IPMSG_FILEATTACHOPT) {
		int	id = (useClipBuf == 2) ? IDS_FILEWITHCLIP :
				 (useClipBuf == 1) ? IDS_WITHCLIP     : IDS_FILEATTACH;
		GetDlgItemTextU8(OPEN_BUTTON, buf, sizeof(buf));
		strcat(buf, " ");
		strcat(buf, LoadStrU8(id));
		SetDlgItemTextU8(OPEN_BUTTON, buf);
	}

	if (cfg->PasswordUse && (msg.command & IPMSG_PASSWORDOPT)) {
		GetDlgItemTextU8(OPEN_BUTTON, buf, sizeof(buf));
		strcat(buf, LoadStrU8(IDS_KEYOPEN));
		SetDlgItemTextU8(OPEN_BUTTON, buf);
	}

	if (cfg->QuoteCheck & 0x1) {
		CheckDlgButton(QUOTE_CHECK, cfg->QuoteCheck);
	}

	//SetDlgItemTextU8(IDOK, LoadStrU8(IDS_INTERCEPT));
	char	label[MAX_BUF];
	snprintfz(label, sizeof(label), replyList.size() >= 2 ? " %s(%d)" : "%s",
		LoadStrU8(cfg->AbnormalButton ? IDS_INTERCEPT : IDS_REPLY), replyList.size());
	replyBtn.SetWindowTextU8(label);

	SetFont();
	SetSize();

	HMENU	hMenu = ::GetSystemMenu(hWnd, FALSE);
	AppendMenuU8(hMenu, MF_SEPARATOR, NULL, NULL);
	SetMainMenu(hMenu);

	if (msg.command & IPMSG_NOADDLISTOPT) {
		replyBtn.EnableWindow(FALSE);
	}
	EvSize(SIZE_RESTORED, 0, 0);

	if (shareInfo && (cfg->ClipMode & CLIP_ENABLE)) {
		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				editSub.InsertBitmapByHandle(hDummyBmp, shareInfo->fileInfo[i]->Pos());
				useDummyBmp++;
			}
		}
		if (isRep) {
			LoadClipFromFile();
		}
		else {
			SaveFile();	// for clipboard
		}
	}

	if (cfg->TaskbarUI) { // これがないと、なぜか開封にフォーカスが当たらない？
		SetTimer(IPMSG_DELAYFOCUS_TIMER, IPMSG_DELAYFOCUS_TIME);
	}

	if (shareInfo && (cfg->autoSaveFlags & AUTOSAVE_ENABLED)) {
		BOOL	auto_save = TRUE;
		if (cfg->autoSaveLevel > 0) {
			Host	*host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
			if (!host ||
			  ((cfg->PriorityMax - (host->priority/PRIORITY_OFFSET)) + 1) > cfg->autoSaveLevel) {
				auto_save = FALSE;
			}
		}
		if (auto_save) {
			SetTimer(IPMSG_AUTOSAVE_TIMER, cfg->autoSaveTout * 1000);
		}
	}

	if (cfg->LogCheck && *cfg->LogFile) {
		static HICON hViewIcon = (HICON)::LoadImage(TApp::hInst(),
			(LPCSTR)LOGVIEWMONO_ICON, IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
		logViewBtn.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hViewIcon);
		logViewBtn.Show();
	}

	SetReplyInfo();

	return	TRUE;
}

void TRecvDlg::SetReplyInfo()
{
#define UNAMELIST_BUF	((MAX_ULIST + 1) * (MAX_LISTBUF + 100))
	Wstr	wbuf(UNAMELIST_BUF);
	WCHAR	*w = wbuf.Buf();
	int		len = 0;

	len += snwprintfz(w + len, UNAMELIST_BUF, L"%s\n", LoadStrW(IDS_TOLIST));
	for (auto &h: replyList) {
		char	buf[MAX_LISTBUF];
		MakeListString(cfg, &h, hosts, buf);

		len += snwprintfz(w + len, UNAMELIST_BUF-len,  L" %s\n", U8toWs(buf));
		if (len > UNAMELIST_BUF - MAX_LISTBUF) {
			break;
		}
	}

	replyBtn.SetTipTextW(wbuf.Buf(), 500, 30000);
}

BOOL IsShowDirectImage(Cfg *cfg, HostSub *hostSub)
{
	if ((cfg->ClipMode & CLIP_CONFIRM_ALL) == 0) return	TRUE;
	if ((cfg->ClipMode & CLIP_CONFIRM_STRICT))   return FALSE;

	Host *host = cfg->priorityHosts.GetHostByName(hostSub);
	return	host && host->priority > DEFAULT_PRIORITY ? TRUE : FALSE;
}

BOOL TRecvDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		*msg.msgBuf = 0;
		if (openFlg && IsDlgButtonChecked(QUOTE_CHECK)) {
			DWORD	start = 0;
			DWORD	end = 0;
			editSub.GetCurSel(&start, &end);

			U8str	u(MAX_UDPBUF);
			editSub.GetTextUTF8(u.Buf(), MAX_UDPBUF);

			if ((cfg->QuoteCheck & 0x2) == 0 && (start == end)) {
				TruncTailQuote(cfg, u.s(), msg.msgBuf, sizeof(msg.msgBuf));
			}
			else {
				strncpyz(msg.msgBuf, u.s(), sizeof(msg.msgBuf));
			}
		}
		::PostMessage(GetMainWnd(), WM_SENDDLG_OPEN, twinId, 0);
		return	TRUE;

	case IDCANCEL:
		if (fileObj && fileObj->conInfo) {
			return	EvCommand(0, FILE_BUTTON, 0), TRUE;	// 転送中断を促す
		}
		if (shareInfo && shareInfo->fileCnt > 0) {
			if (MessageBoxU8(LoadStrU8(IDS_SHAREDESTROY), IP_MSG, MB_OKCANCEL) != IDOK) {
				return TRUE;
			}
		}
		else if (clipList.TopObj()) {
			ClipBuf *clipBuf = clipList.TopObj();
			if (clipBuf->finished) {
				if (MessageBoxU8(LoadStrU8(IDS_CLIPDESTROY), IP_MSG, MB_OKCANCEL) != IDOK) {
					return TRUE;
				}
			}
		}
		else if (!openFlg) {
			if (MessageBoxU8(LoadStrU8(IDS_OPENDESTROY), IP_MSG, MB_OKCANCEL) != IDOK) {
				return TRUE;
			}
			cfg->DeletePacket(&msg);
		}

		if (timerID == 0) {
			if (!modalCount) {
				::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, twinId);
			}
		}
		else {
			TWin::Show(SW_HIDE);
		}
		return	TRUE;

	case IMAGE_BUTTON:
		InsertImages();
		return	TRUE;

	case LOGVIEW_BUTTON:
		::SendMessage(GetMainWnd(), WM_LOGVIEW_OPEN, 1, (LPARAM)msg.hostSub.u.userName);
		return	TRUE;

	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case ALLSELECT_ACCEL:
		editSub.SendMessage(EM_SETSEL, 0, (LPARAM)-1);
		return	TRUE;

	case OPEN_BUTTON:
		if (openFlg) {
			return	TRUE;
		}

		if (cfg->PasswordUse && (msg.command & IPMSG_PASSWORDOPT)) {
			TPasswordDlg	dlg(cfg, this);

			if (dlg.Exec() != IDOK) {
				return	TRUE;
			}
			if (cfg->PasswdLogCheck) {
				if (!isRep) {
					logmng->WriteRecvMsg(&msg, logOpt, hosts, shareInfo, &recvList, &msg.msgId);
				}
			}
		}
		openFlg = TRUE;
		cfg->DeletePacket(&msg);

		editSub.ShowWindow(SW_SHOW);
		::ShowWindow(GetDlgItem(QUOTE_CHECK), SW_SHOW);
		::ShowWindow(GetDlgItem(OPEN_BUTTON), SW_HIDE);
		::EnableWindow(GetDlgItem(OPEN_BUTTON), FALSE);
		if (cfg->LogCheck && *cfg->LogFile) {
			logViewBtn.Show();
		}
	
		if (shareInfo) {
			SetFileButton(this, FILE_BUTTON, shareInfo, autoSaves);
			EvSize(SIZE_RESTORED, 0, 0);
		}
		if (IsShowDirectImage(cfg, &msg.hostSub)) {
			InsertImages();
		}

		if (msg.command & IPMSG_SECRETOPT) {
			char	buf[MAX_LISTBUF];
			int		cmd = IPMSG_READMSG | (msg.command & IPMSG_READCHECKOPT);

			sprintf(buf, "%ld", msg.packetNo);
			packetNo = msgMng->MakeMsg(readMsgBuf, cmd, buf);
			msgMng->UdpSend(msg.hostSub.addr, msg.hostSub.portNo, readMsgBuf);

			if (msg.command & IPMSG_READCHECKOPT) {
				timerID = SetTimer(IPMSG_RECV_TIMER, cfg->RetryMSec);
			}
			MsgIdent	*mi = new MsgIdent;
			mi->Init(msg.hostSub.u.userName, msg.hostSub.u.hostName, msg.packetNo, msg.msgId);
			::PostMessage(GetMainWnd(), WM_RECVDLG_READCHECK, 1, (LPARAM)mi);
		}
		return	TRUE;

	case RECV_EDIT:
//		if (wNotifyCode == EN_CHANGE) {
//			editSub.SetHyperLink();
//		}
		return	TRUE;

	case RECV_HEAD:
		if (wNotifyCode == EN_SETFOCUS) {
			::HideCaret(recvHead.hWnd);
		}
		return	TRUE;

	case RECV_HEAD2:
		if (wNotifyCode == EN_SETFOCUS) {
			::HideCaret(GetDlgItem(RECV_HEAD2));
		}
		return	TRUE;

	case FILE_BUTTON:
		CleanupAutoSaveSize();
		if (fileObj && fileObj->conInfo) {
			if (fileObj->hThread) {
				::SuspendThread(fileObj->hThread);
			}
			int ret = MessageBoxU8(LoadStrU8(IDS_TRANSSTOP), IP_MSG, MB_OKCANCEL);
			if (fileObj->hThread) {
				::ResumeThread(fileObj->hThread);
			}
			if (ret == IDOK && fileObj->conInfo) {
				EndRecvFile(TRUE);
			}
		}
		else if (fileObj) {
			if (*autoSaves && (!shareInfo || shareInfo->fileCnt == 0)) {
				MakeAutoSaveDir(cfg, fileObj->saveDir);

				BOOL	ret = FALSE;
				int		num = 0;
				WCHAR	**array = AutoSaveToWArray(autoSaves, &num);
				if (array) {
					ret = TOpenExplorerSelW(U8toWs(fileObj->saveDir), array, num);
					FreeWArray(array, num);
				}
				if (!ret) {
					ShellExecuteU8(NULL, NULL, fileObj->saveDir, 0, 0, SW_SHOW);
				}
				isAutoSave = FALSE;
				*autoSaves = 0;
				SetFileButton(this, FILE_BUTTON, shareInfo);
				EvSize(SIZE_RESTORED, 0, 0);
			}
			else {
				Host	*host = hosts->GetHostByName(&msg.hostSub);
				if (!host) host = cfg->priorityHosts.GetHostByName(&msg.hostSub);

				TSaveCommonDlg	dlg(shareInfo, cfg, host ? host->hostStatus : 0, this);
				if (dlg.Exec()) {
					fileObj->Init();
					strncpyz(fileObj->saveDir, cfg->lastSaveDir, MAX_PATH_U8);
					SaveFile();
				}
			}
		}
		break;

	case MENU_SAVEPOS:
		if ((cfg->RecvSavePos = !cfg->RecvSavePos) != 0) {
			GetWindowRect(&rect);
			cfg->RecvXpos = rect.left;
			cfg->RecvYpos = rect.top;
		}
		cfg->WriteRegistry(CFG_WINSIZE);
		return	TRUE;

	case MENU_SAVESIZE:
		GetWindowRect(&rect);
		cfg->RecvXdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
		cfg->RecvYdiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);
		cfg->WriteRegistry(CFG_WINSIZE);
		return	TRUE;

	case MENU_NORMALSIZE:
		GetWindowRect(&rect);
		MoveWindow(rect.left, rect.top, orgRect.right - orgRect.left, orgRect.bottom - orgRect.top, TRUE);
		return	TRUE;

	case MENU_EDITFONT: case MENU_HEADFONT:
		CHOOSEFONT	cf;
		LOGFONT		tmpFont, *targetFont;

		targetFont = wID == MENU_EDITFONT ? &cfg->RecvEditFont : &cfg->RecvHeadFont;
		memset(&cf, 0, sizeof(cf));
		cf.lStructSize	= sizeof(cf);
		cf.hwndOwner	= hWnd;
		cf.lpLogFont	= &(tmpFont = *targetFont);
		cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_SHOWHELP;
		cf.nFontType	= SCREEN_FONTTYPE;
		if (::ChooseFont(&cf)) {
			*targetFont = tmpFont;
			SetFont(TRUE);
			::InvalidateRgn(hWnd, NULL, TRUE);
		}
		cfg->WriteRegistry(CFG_FONT);
		return	TRUE;

	case MENU_DEFAULTFONT:
		cfg->RecvHeadFont = orgFont;
		cfg->RecvEditFont = orgFont;
		SetFont(TRUE);
		::InvalidateRgn(hWnd, NULL, TRUE);
		cfg->WriteRegistry(CFG_FONT);
		return	TRUE;

	case WM_UNDO:
	case WM_CUT:
	case WM_COPY:
	case WM_PASTE:
	case WM_PASTE_REV:
	case WM_PASTE_IMAGE:
	case WM_SAVE_IMAGE:
	case WM_CLEAR:
	case EM_SETSEL:
		editSub.SendMessage(WM_COMMAND, wID, 0);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TRecvDlg::LoadClipFromFile(void)
{
	for (int i=shareInfo->fileCnt-1; i >= 0; i--) {
		if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
			ClipBuf *clipBuf = new ClipBuf(0, shareInfo->fileInfo[i]->Pos());
			if (LoadImageFile(cfg, shareInfo->fileInfo[i]->Fname(), &clipBuf->vbuf)) {
				clipBuf->finished = TRUE;
				clipList.AddObj(clipBuf);
			} else {
				delete clipBuf;
			}
			shareInfo->RemoveFileInfo(i);
		}
	}
	SetFileButton(this, FILE_BUTTON, shareInfo, autoSaves);
	EvSize(SIZE_RESTORED, 0, 0);

	return	TRUE;
}

BOOL TRecvDlg::SaveFile(BOOL is_autosave, BOOL is_first)
{
	isAutoSave = is_autosave;

	int		target;
	for (target=0; target < shareInfo->fileCnt; target++) {
		if (shareInfo->fileInfo[target]->IsSelected()) {
			break;
		}
	}
	if (target == shareInfo->fileCnt) {
		return	FALSE;
	}
	if (isAutoSave) {
		if (FileInfo *finfo = shareInfo->fileInfo[target]) {
			if (GET_MODE(finfo->Attr()) == IPMSG_FILE_REGULAR) {
				if (cfg->autoSavedSize + finfo->Size() > (int64)cfg->autoSaveMax * 1000 * 1000) {
					return FALSE;
				}
			}
		}
	}

	memset(fileObj, 0, (char *)&fileObj->totalTrans - (char *)fileObj);
	fileObj->conInfo = new ConnectInfo;
	fileObj->fileInfo = shareInfo->fileInfo[target];
	fileObj->isDir = GET_MODE(fileObj->fileInfo->Attr()) == IPMSG_FILE_DIR ? TRUE : FALSE;
	fileObj->isClip = GET_MODE(fileObj->fileInfo->Attr()) == IPMSG_FILE_CLIPBOARD ? TRUE : FALSE;
	fileObj->status = fileObj->isDir ? FS_DIRFILESTART : FS_TRANSFILE;
	fileObj->hFile = INVALID_HANDLE_VALUE;

	if (fileObj->isDir) {
		strncpyz(fileObj->path, fileObj->saveDir, MAX_PATH_U8);
	}

	fileObj->conInfo->startTick = fileObj->conInfo->lastTick = ::GetTick();
	fileObj->curFiles = 0;
	fileObj->curTrans = 0;

	if (is_first) {
		fileObj->startTick = fileObj->lastTick = fileObj->conInfo->startTick;
		fileObj->totalTrans = 0;
		fileObj->totalFiles = 0;
	}

	if (ConnectRecvFile()) {
		SetDlgItemTextU8(FILE_BUTTON, LoadStrU8(IDS_PREPARETRANS));
	} else {
		delete fileObj->conInfo;
		fileObj->conInfo = NULL;
	}

	return	TRUE;
}

BOOL TRecvDlg::EvSysCommand(WPARAM uCmdType, POINTS pos)
{
	switch (uCmdType)
	{
	case MENU_SAVEPOS:
	case MENU_SAVESIZE:
	case MENU_NORMALSIZE:
	case MENU_EDITFONT: case MENU_HEADFONT:
	case MENU_DEFAULTFONT:
		return	EvCommand(0, (WORD)uCmdType, 0);
	}

	return	FALSE;
}

/*
	App定義 Event CallBack
*/
BOOL TRecvDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_TCPEVENT:
		TcpEvent(wParam, lParam);
		return	TRUE;

	case WM_RECVDLG_FILEBUTTON:
		SetTransferButtonText();
		return	TRUE;
	}
	return	FALSE;
}

BOOL TRecvDlg::AutoSaveCheck()
{
	if ((status != INIT && (status != SHOW || openFlg))
		|| !shareInfo || !shareInfo->fileCnt || !fileObj) {
		return FALSE;
	}
	for (int i=0; i < shareInfo->fileCnt; i++) {	// clip board download waiting...
		if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
			SetTimer(IPMSG_AUTOSAVE_TIMER, 5000);
			return	TRUE;
		}
	}

	fileObj->Init();
	if (!MakeAutoSaveDir(cfg, fileObj->saveDir)) return FALSE;

	BOOL	is_save = FALSE;
	int64	auto_saved = cfg->autoSavedSize;

	for (int i=0; i < shareInfo->fileCnt; i++) {	// すでにクリップボードはDL済み
		if (auto_saved + shareInfo->fileInfo[i]->Size() >
			(int64)cfg->autoSaveMax * 1000 * 1000
			|| (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_DIR &&
			((cfg->autoSaveFlags & AUTOSAVE_INCDIR) == 0))) {
			continue;
		}
		char	path[MAX_PATH_U8];
		strcpy(path, shareInfo->fileInfo[i]->Fname());
		if (MakeNonExistFileName(fileObj->saveDir, path)) {
			shareInfo->fileInfo[i]->SetFname(path);
			shareInfo->fileInfo[i]->SetSelected(TRUE);
			is_save = TRUE;
		}
		auto_saved += shareInfo->fileInfo[i]->Size();
	}
	if (is_save) {
		MakeDirAllU8(fileObj->saveDir);
		SaveFile(TRUE);
	}

	return	TRUE;
}

/*
	WM_TIMER event call back
	送信確認/再送用
*/
BOOL TRecvDlg::EvTimer(WPARAM _timerID, TIMERPROC proc)
{
	if (_timerID == IPMSG_DELAYFOCUS_TIMER) {
		KillTimer(IPMSG_DELAYFOCUS_TIMER);
		SetForegroundWindow();
		::SetFocus(GetDlgItem(OPEN_BUTTON));
		return TRUE;
	}
	if (_timerID == IPMSG_AUTOSAVE_TIMER) {
		KillTimer(IPMSG_AUTOSAVE_TIMER);
		AutoSaveCheck();
		return TRUE;
	}

	if (retryCnt++ < cfg->RetryMax * 2) {
		msgMng->UdpSend(msg.hostSub.addr, msg.hostSub.portNo, readMsgBuf);
		return	TRUE;
	}

	KillTimer(IPMSG_RECV_TIMER);
	if (timerID == 0) {	// 再入よけ
		return	FALSE;
	}
	timerID = 0;

	if (!::IsWindowVisible(hWnd)) {
		if (!modalCount) {
			::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, twinId);
		}
	}

	return	TRUE;
}

//BOOL GetNoWrapString(HWND hWnd, int cx, char *str, char *buf, int maxlen)
//{
//	Wstr	wstr(str);
//	int		len = (int)wcslen(wstr.s());
//	int		fit_len = 0;
//	SIZE    size;
//	HDC		hDc;
//
//	if ((hDc = ::GetDC(hWnd))) {
//		if (::GetTextExtentExPointW(hDc, wstr.s(), len, cx, &fit_len, NULL, &size)) {
//			if (fit_len < len) {
//				wstr[fit_len] = 0;
//			}
//		}
//		::ReleaseDC(hWnd, hDc);
//	}
//
//	WtoU8(wstr.s(), buf, maxlen);
//	return	TRUE;
//}

BOOL TRecvDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType != SIZE_RESTORED && fwSizeType != SIZE_MAXIMIZED) {
		return	FALSE;
	}

	GetWindowRect(&rect);
	int	xdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	int ydiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

//	char	buf[MAX_BUF];
//	GetNoWrapString(recvHead.hWnd, headRect.right + xdiff - 40, head, buf, sizeof(buf));
//	recvHead.SetWindowTextU8(buf);

	HDWP	hdwp = ::BeginDeferWindowPos(max_recvitem);	// MAX item number
	WINPOS	*wpos;
	BOOL	isFileBtn = IsWindowEnabled(GetDlgItem(FILE_BUTTON));
	UINT	dwFlg = SWP_SHOWWINDOW | SWP_NOZORDER;
	UINT	dwHideFlg = SWP_HIDEWINDOW | SWP_NOZORDER;

	if (hdwp == NULL) return FALSE;

// サイズが小さくなる場合の調整値は、Try and Error(^^;
	wpos = &item[title_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(RECV_TITLE), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[head_item];
	if ((hdwp = ::DeferWindowPos(hdwp, recvHead.hWnd, NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[head2_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(RECV_HEAD2), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[file_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(FILE_BUTTON), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, (isFileBtn && openFlg) ? dwFlg : dwHideFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[open_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(OPEN_BUTTON), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy + ydiff, openFlg ? dwHideFlg : dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[edit_item];
	if ((hdwp = ::DeferWindowPos(hdwp, editSub.hWnd, NULL, wpos->x, (isFileBtn ? wpos->y : item[file_item].y), wpos->cx + xdiff, wpos->cy + ydiff + (isFileBtn ? 0 : wpos->y - item[file_item].y), openFlg ? dwFlg : dwHideFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[image_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IMAGE_BUTTON), NULL, wpos->x, wpos->y + ydiff, wpos->cx, wpos->cy, (openFlg && useClipBuf && clipList.TopObj()) ? dwFlg : dwHideFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[logview_item];
//	if ((hdwp = ::DeferWindowPos(hdwp, logViewBtn.hWnd, NULL, wpos->x, wpos->y + ydiff, wpos->cx, wpos->cy, openFlg ? dwFlg : dwHideFlg)) == NULL) {
	if ((hdwp = ::DeferWindowPos(hdwp, logViewBtn.hWnd, NULL, wpos->x, wpos->y + ydiff, wpos->cx, wpos->cy, (cfg->LogCheck && *cfg->LogFile) ? dwFlg : dwHideFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[ok_item];
	if ((hdwp = ::DeferWindowPos(hdwp, replyBtn.hWnd, NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 3 / 4), wpos->y + ydiff, wpos->cx + (xdiff >= 0 ? 0 : xdiff / 4), wpos->cy, dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[cancel_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IDCANCEL), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 3 / 8), wpos->y + ydiff, wpos->cx + (xdiff >= 0 ? 0 : xdiff * 1 / 4), wpos->cy, dwFlg)) == NULL) {
		return	FALSE;
	}
	wpos = &item[quote_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(QUOTE_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, openFlg ? dwFlg : dwHideFlg)) == NULL) {
		return	FALSE;
	}
	EndDeferWindowPos(hdwp);

	if (parent) {
		::InvalidateRgn(editSub.hWnd, NULL, TRUE);
	}

	return	TRUE;
}


BOOL TRecvDlg::EvGetMinMaxInfo(MINMAXINFO *info)
{
	info->ptMinTrackSize.x = (orgRect.right - orgRect.left) * 2 / 3;
	info->ptMinTrackSize.y = (orgRect.bottom - orgRect.top) * 2 / 3;

	return	TRUE;
}

/*
	Context Menu event call back
*/
BOOL TRecvDlg::EvContextMenu(HWND childWnd, POINTS pos)
{
	PopupContextMenu(pos);
	return	TRUE;
}

/*
	Notify Event CallBack
*/
BOOL TRecvDlg::EvNotify(UINT ctlID, NMHDR *pNmHdr)
{
	switch (pNmHdr->code) {
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
	}

	return	FALSE;
}


BOOL TRecvDlg::IsSamePacket(MsgBuf *test_msg)
{
	if (test_msg->packetNo == msg.packetNo && test_msg->hostSub.addr == msg.hostSub.addr && test_msg->hostSub.portNo == msg.hostSub.portNo) {
		return	TRUE;
	}
	else {
		return	FALSE;
	}
}

void TRecvDlg::SetFont(BOOL force_reset)
{
	if (!*orgFont.lfFaceName) {
		HFONT	hFont = (HFONT)SendMessage(WM_GETFONT, 0, 0);;

		if (!hFont || ::GetObject(hFont, sizeof(LOGFONT), (LPSTR)&orgFont) == NULL) return;
		if (*cfg->RecvHeadFont.lfFaceName == 0) cfg->RecvHeadFont = orgFont;
		if (*cfg->RecvEditFont.lfFaceName == 0) cfg->RecvEditFont = orgFont;
	}

	if (!hHeadFont || !hEditFont || force_reset) {
		if (hHeadFont) ::DeleteObject(hHeadFont);
		if (hEditFont) ::DeleteObject(hEditFont);

		if (*cfg->RecvHeadFont.lfFaceName) hHeadFont = ::CreateFontIndirect(&cfg->RecvHeadFont);
		if (*cfg->RecvEditFont.lfFaceName) hEditFont = ::CreateFontIndirect(&cfg->RecvEditFont);
	}

	recvHead.SendMessage(WM_SETFONT, (WPARAM)hHeadFont, 0L);
	SendDlgItemMessage(RECV_HEAD2, WM_SETFONT, (WPARAM)hHeadFont, 0L);

	editSub.SetFont(&cfg->RecvEditFont);
	editSub.ExSetText(msg.msgBuf);
	editSub.SetWindowTextU8(msg.msgBuf);
}

void TRecvDlg::SetSize(void)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);

	::GetWindowPlacement(GetDlgItem(RECV_TITLE), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[title_item]);

	::GetWindowPlacement(recvHead.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[head_item]);

	::GetWindowPlacement(GetDlgItem(RECV_HEAD2), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[head2_item]);

	::GetWindowPlacement(GetDlgItem(FILE_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[file_item]);

	::GetWindowPlacement(GetDlgItem(OPEN_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[open_item]);

	::GetWindowPlacement(editSub.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[edit_item]);

	::GetWindowPlacement(GetDlgItem(IMAGE_BUTTON), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[image_item]);

	::GetWindowPlacement(logViewBtn.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[logview_item]);

	::GetWindowPlacement(replyBtn.hWnd, &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[ok_item]);

	::GetWindowPlacement(GetDlgItem(IDCANCEL), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[cancel_item]);

	::GetWindowPlacement(GetDlgItem(QUOTE_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[quote_item]);

	GetWindowRect(&rect);
	orgRect = rect;

	recvHead.GetClientRect(&headRect);

	TRect	scRect;
	GetCurrentScreenSize(&scRect);

	int	cx = scRect.cx();
	int	cy = scRect.cy();
	int	xsize = rect.cx() + cfg->RecvXdiff;
	int	ysize = rect.cy() + cfg->RecvYdiff;

	POINT	pt = {};
	LONG	&x = pt.x;
	LONG	&y = pt.y;

	if (cfg->RecvSavePos) {
		x = cfg->RecvXpos;
		y = cfg->RecvYpos;
	}
	else {
		x = (cx - xsize)/2 + (rand() % (cx/4)) - cx/8;
		y = (cy - ysize)/2 + (rand() % (cy/4)) - cy/8;
	}

	if (cfg->RecvSavePos == 0 || !MonitorFromPoint(pt, MONITOR_DEFAULTTONULL)) {
		if (x + xsize > cx) {
			x = cx - xsize;
		}
		if (y + ysize > cy) {
			y = cy - ysize;
		}
		x = max(x, scRect.left);
		y = max(y, scRect.top);
	}

	MoveWindow(x, y, xsize, ysize, TRUE);
}

/*
	Menu Event CallBack
*/
BOOL TRecvDlg::EventInitMenu(UINT uMsg, HMENU hMenu, UINT uPos, BOOL fSystemMenu)
{
	switch (uMsg)
	{
	case WM_INITMENU:
		{
			ModifyMenuU8(hMenu, MENU_SAVEPOS, MF_BYCOMMAND|(cfg->RecvSavePos ? MF_CHECKED :  0), MENU_SAVEPOS, LoadStrU8(IDS_SAVEPOS));
		}
		return	TRUE;
	}
	return	FALSE;
}

/*
	Color Event CallBack
*/
BOOL TRecvDlg::EventCtlColor(UINT uMsg, HDC hDcCtl, HWND hWndCtl, HBRUSH *result)
{
#if 0
	switch (uMsg) {
	case WM_CTLCOLORDLG:
		{ static HBRUSH hb; if (hb == NULL) hb = ::CreateSolidBrush(0x222222); *result = hb; }
		break;
	case WM_CTLCOLOREDIT:
		{ static HBRUSH hb; if (hb == NULL) hb = ::CreateSolidBrush(0x222222); *result = hb; }
		SetTextColor(hDcCtl, 0xeeeeee);
		SetBkColor(hDcCtl, 0x222222);
		break;
	case WM_CTLCOLORSTATIC:
		{ static HBRUSH hb; if (hb == NULL) hb = ::CreateSolidBrush(0x222222); *result = hb; }
		SetTextColor(hDcCtl, 0xeeeeee);
		SetBkColor(hDcCtl, 0x222222);
		break;
	}
	return	TRUE;
#else
	return	FALSE;
#endif
}

BOOL TRecvDlg::EventButton(UINT uMsg, int nHitTest, POINTS pos)
{
	switch (uMsg) {
	case WM_RBUTTONUP:
		return	TRUE;
	}
	return	FALSE;
}

void TRecvDlg::PopupContextMenu(POINTS pos)
{
	HMENU	hMenu = ::CreatePopupMenu();

	SetMainMenu(hMenu);
	::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
	::DestroyMenu(hMenu);
}

void TRecvDlg::SetMainMenu(HMENU hMenu)
{
//	AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)::LoadMenu(TApp::hInst(), (LPCSTR)RECVFONT_MENU), LoadStrU8(IDS_FONTSET));
	AppendMenuU8(hMenu, MF_POPUP, (UINT_PTR)::LoadMenu(TApp::hInst(), (LPCSTR)RECVSIZE_MENU), LoadStrU8(IDS_SIZESET));
//	AppendMenuU8(hMenu, MF_STRING, MENU_SAVEPOS, LoadStrU8(IDS_SAVEPOS));
}

/*
	送信終了通知
*/
BOOL TRecvDlg::SendFinishNotify(HostSub *hostSub, ULONG packet_no)
{
	if (msg.hostSub.addr == hostSub->addr && msg.hostSub.portNo == hostSub->portNo && packet_no == packetNo) {
		if (timerID == IPMSG_RECV_TIMER) {
			::KillTimer(hWnd, IPMSG_RECV_TIMER);
			timerID = 0;
			if (!::IsWindowVisible(hWnd)) {
				if (!modalCount) {
					::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, twinId);
				}
			}
		}
		return	TRUE;
	}
	return	FALSE;
}

/*
	送信中は、Showのvisibleをはじく
*/
void TRecvDlg::Show(int mode)
{
	if (status != SHOW) return;

	EnableWindow(TRUE);

	if (timerID == 0 && hWnd) {
		if (mode != SW_HIDE && openFlg && IsShowDirectImage(cfg, &msg.hostSub)) {
			InsertImages(); // display and remove
		}
		TWin::Show(mode);
	}
}

BOOL TRecvDlg::InsertImages(void)
{
	BOOL	ret = TRUE;

	for (auto clipBuf=clipList.TopObj(); clipBuf; ) {
		auto next = clipList.NextObj(clipBuf);
		if (clipBuf->finished) {
			int	pos = clipBuf->pos;
			if (useDummyBmp) {
				useDummyBmp--;
				editSub.SendMessageW(EM_SETSEL, pos, pos + 1);
				pos = -1;
			}
			if (!editSub.InsertPng(&clipBuf->vbuf, pos)) {
				ret = FALSE;
			}
			clipList.DelObj(clipBuf);
			delete clipBuf;
		}
		clipBuf = next;
	}
	editSub.SendMessageW(EM_SCROLL, SB_PAGEUP, 0);
	editSub.SendMessageW(EM_SETSEL, 0, 0);
	editSub.SendMessageW(EM_SCROLLCARET, 0, 0);

	if (clipList.TopObj()) {
		::EnableWindow(GetDlgItem(IMAGE_BUTTON), TRUE);
	}
	else {
		::ShowWindow(GetDlgItem(IMAGE_BUTTON), SW_HIDE);
	}

	if (!ret) {
		MessageBox("Can't decode images.");
	}

	return	ret;
}


BOOL TRecvDlg::TcpEvent(SOCKET sd, LPARAM lParam)
{
	if (fileObj == NULL || fileObj->conInfo == NULL)
		return	FALSE;

	switch (LOWORD(lParam)) {
	case FD_CONNECT:	// connect done
		if (!fileObj->conInfo->complete) {
			fileObj->conInfo->complete = TRUE;
			StartRecvFile();
		}
		break;

	case FD_CLOSE:
		EndRecvFile();
		break;
	}
	return	TRUE;
}

BOOL TRecvDlg::ConnectRecvFile(void)
{
	fileObj->conInfo->Init();
	fileObj->conInfo->addr = msg.hostSub.addr;
	fileObj->conInfo->port = msg.hostSub.portNo;

	if (!msgMng->Connect(hWnd, fileObj->conInfo)) {
		return	FALSE;
	}
	if (fileObj->conInfo->complete) {
		StartRecvFile();
	}

	return	TRUE;
}

BOOL TRecvDlg::StartRecvFile(void)
{
	char	cmdbuf[MAX_PATH_U8];
	char	midbuf[MAX_BUF];
	char	tcpbuf[MAX_BUF];
	char	*targ_buf = cmdbuf;
	int		cmd = fileObj->isDir ? IPMSG_GETDIRFILES : IPMSG_GETFILEDATA;
	int		pkt_no = msgMng->MakePacketNo();
	int		pkt_len = 0;

	sprintf(cmdbuf, "%x:%x:", msg.packetNo, fileObj->fileInfo->Id());

	Host	*host = hosts->GetHostByName(&msg.hostSub);
	if (!host) {
		host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
	}

	if (host && GetUserNameDigestField(host->hostSub.u.userName) && host->pubKey.Key()
		&& (host->hostStatus & IPMSG_CAPFILEENCOPT)) {
		if (!fileObj->isClip) {
			cmd = IPMSG_GETDIRFILES; // ファイルでもGETDIRFILES（メモリファイルは除く）
			fileObj->status = FS_TRANSINFO;
		}
		if (cmd == IPMSG_GETFILEDATA) {
			strcat(cmdbuf, "0:"); // append file-offset
		}
		int	capa = (host->pubKey.Capa() & cfg->GetCapa());
		cmd |= IPMSG_ENCRYPTOPT;

		if ((cfg->EncTransCheck || (capa & IPMSG_NOENC_FILEBODY)) &&
			(capa & IPMSG_AES_256) && (capa & IPMSG_PACKETNO_IV) &&
			(capa & (IPMSG_SIGN_SHA1|IPMSG_SIGN_SHA256))) {
			char	*cmd_p = cmdbuf + strlen(cmdbuf);
			if (cfg->EncTransCheck) {
				BYTE	key[256/8];
				BYTE	iv[AES_BLOCK_SIZE]={};
				TGenRandom(key, sizeof(key));
				cmd_p += sprintf(cmd_p, "%x:", IPMSG_AES_256|IPMSG_PACKETNO_IV);
				cmd_p += bin2hexstr(key, sizeof(key), cmd_p);
				cmd_p += sprintf(cmd_p, ":%x", pkt_no);

				sprintf((char *)iv, "%d", pkt_no); // 先頭10byteがpkt_no_str、下位6byteがcounter
				fileObj->aes.Init(key, sizeof(key), iv);
			}
			else {
				cmd_p += sprintf(cmd_p, "%x:%x", IPMSG_NOENC_FILEBODY, pkt_no);
			}
			cmd |= IPMSG_ENCFILEOPT;
		}
		if (!msgMng->MakeEncryptMsg(host, pkt_no, cmdbuf, false, NULL, NULL, midbuf)) {
			return FALSE;
		}
		targ_buf = midbuf;
	}
	else if (cmd == IPMSG_GETFILEDATA) {
		strcat(cmdbuf,"0:"); // append file-offset
	}

	if (msg.command & IPMSG_UTF8OPT) cmd |= IPMSG_UTF8OPT;

	msgMng->MakeMsg(tcpbuf, pkt_no, cmd, targ_buf, NULL, NULL, &pkt_len);
	msgMng->ConnectDone(hWnd, fileObj->conInfo);

//fileObj->offset = fileObj->woffset = OFFSET;

#if 0
	IPDict	dict;
	DynBuf	dbuf;
	msgMng->InitIPDict(&dict, IPMSG_SENDMSG);
	dict.put_str(IPMSG_BODY_KEY, "test");
	msgMng->EncIPDict(&dict, &host->pubKey, &dbuf);

	::send(fileObj->conInfo->sd, dbuf.s(), dbuf.UsedSize(), 0);
	return FALSE;
#endif

	if (::send(fileObj->conInfo->sd, tcpbuf, pkt_len, 0) < pkt_len) {
		return	EndRecvFile(), FALSE;
	}

	if (!fileObj->isDir) {
		fileObj->curFileInfo = *fileObj->fileInfo;
	}

#define MAX_CLIPBOARD (10 * 1024 * 1024) // 10MB
	if (fileObj->isClip) {
		int64	size = fileObj->curFileInfo.Size();
		ClipBuf	*clipBuf = NULL;

		if (size <= MAX_CLIPBOARD && size > 0) {
			clipBuf = new ClipBuf((int)size, fileObj->curFileInfo.Pos());
			if (clipBuf && clipBuf->vbuf) {
				clipList.AddObj(clipBuf);
			} else if (clipBuf) {
				delete clipBuf;
				clipBuf = NULL;
			}
		}
		if (!clipBuf) return EndRecvFile(), FALSE;

		fileObj->recvBuf = (char *)clipBuf->vbuf.Buf();
	}
	else {
		fileObj->recvBuf = new char [cfg->IoBufMax];
	}

	// 0byte file だけは、特例
	if (!fileObj->isDir && fileObj->curFileInfo.Size() == 0) {
		if (OpenRecvFile()) {
			CloseRecvFile(TRUE);
			fileObj->status = FS_COMPLETE;
		}
		else fileObj->status = FS_ERROR;

		PostMessage(WM_TCPEVENT, fileObj->conInfo->sd, FD_CLOSE);
		return	TRUE;
	}

	UINT	id;	// 使わず（95系で error になるのを防ぐだけ）
	fileObj->hThread = (HANDLE)~0;	// 微妙な領域を避ける
	if (!(fileObj->hThread = (HANDLE)_beginthreadex(NULL, 0, RecvFileThread, this, 0, &id))) {
		EndRecvFile();
		return	FALSE;
	}

	return	TRUE;
}

UINT WINAPI TRecvDlg::RecvFileThread(void *_recvDlg)
{
	TRecvDlg	*recvDlg = (TRecvDlg *)_recvDlg;
	RecvFileObj	*fileObj = recvDlg->fileObj;
	fd_set		rfd;
	BOOL		(TRecvDlg::*RecvFileFunc)(void) =
				(fileObj->isDir || fileObj->status == FS_TRANSINFO)
						 ? &TRecvDlg::RecvDirFile : &TRecvDlg::RecvFile;

	FD_ZERO(&rfd);
	FD_SET(fileObj->conInfo->sd, &rfd);

	for (int waitCnt=0; waitCnt < 120 && fileObj->hThread; waitCnt++) {
		timeval	tv =  { 1, 0 };
		int		sock_ret;
		if ((sock_ret = ::select((int)fileObj->conInfo->sd + 1, &rfd, NULL, NULL, &tv)) > 0) {
			waitCnt = 0;
			if (!(recvDlg->*RecvFileFunc)()) {
				break;
			}
			if (fileObj->status == FS_COMPLETE) {
				break;
			}
		}
		else if (sock_ret == 0) {
			FD_ZERO(&rfd);
			FD_SET(fileObj->conInfo->sd, &rfd);
			fileObj->conInfo->lastTick = ::GetTick();
			recvDlg->PostMessage(WM_RECVDLG_FILEBUTTON, 0, 0);
		}
		else if (sock_ret == SOCKET_ERROR) {
			break;
		}
	}
	recvDlg->CloseRecvFile(fileObj->status == FS_COMPLETE ? TRUE : FALSE);
	if (fileObj->status != FS_COMPLETE) {
		fileObj->status = FS_ERROR;
	}

	recvDlg->PostMessage(WM_TCPEVENT, fileObj->conInfo->sd, FD_CLOSE);
	_endthreadex(0);
	return	0;
}

BOOL TRecvDlg::CloseRecvFile(BOOL setAttr)
{
	if (fileObj->isClip) {
		;
	}
	else if (fileObj->hFile != INVALID_HANDLE_VALUE) {
		if (setAttr) {
			FILETIME	ft;
			UnixTime2FileTime(fileObj->curFileInfo.Mtime(), &ft);
#if 1	// 暫定処置（protocol format 変更の可能性）
			if (fileObj->isDir || (fileObj->curFileInfo.Mtime() & 0xffffff00))
#endif
				::SetFileTime(fileObj->hFile, NULL, &ft, &ft);
		}
		fileObj->totalTrans += fileObj->offset;
		fileObj->curTrans += fileObj->offset;
		fileObj->totalFiles++;
		fileObj->curFiles++;
		fileObj->offset = fileObj->woffset = 0;

		if (isAutoSave) {
			cfg->autoSavedSize += fileObj->offset;
			autoSavedSize += fileObj->offset;
		}

		::CloseHandle(fileObj->hFile);
		fileObj->hFile = INVALID_HANDLE_VALUE;
	}
	return	TRUE;
}

BOOL TRecvDlg::DecodeDirEntry(char *buf, FileInfo *info, char *u8fname)
{
	char	*tok, *ptr, *p;

	ConvertShareMsgEscape(buf);	// "::" -> ';'

	if ((tok = sep_tok(buf, ':', &p)) == NULL) {		// header size
		return	FALSE;
	}
	if ((tok = sep_tok(NULL, ':', &p)) == NULL) {	// fname
		return	FALSE;
	}

	if (u8fname) {
		if (msg.command & IPMSG_UTF8OPT) {
			strncpyz(u8fname, tok, MAX_PATH_U8);
		}
		else {
			WCHAR	wbuf[MAX_PATH];
			AtoW(tok, wbuf, wsizeof(wbuf));
			WtoU8(wbuf, u8fname, MAX_PATH_U8);
		}
		info->SetFname(u8fname);
	}

	while ((ptr = strchr(tok, '?'))) { // UNICODE までの暫定
		*ptr = '_';
	}
	if (strlen(tok) >= MAX_PATH_U8) {
		return	FALSE;
	}

	if ((tok = sep_tok(NULL, ':', &p)) == NULL) { // size
		return	FALSE;
	}
	info->SetSize(hex2ll(tok));

	if ((tok = sep_tok(NULL, ':', &p)) == NULL) { // attr
		return	FALSE;
	}
	info->SetAttr(strtoul(tok, 0, 16));

	while ((tok = sep_tok(NULL, ':', &p))) { // exattr
		if ((ptr = strchr(tok, '=')) == NULL)
			continue;
		*ptr = 0;

		UINT	exattr = strtoul(tok, 0, 16);
		UINT	val   = strtoul(ptr + 1, 0, 16);

		switch (exattr) {
		case IPMSG_FILE_MTIME:		info->SetMtime(val);  break;
		case IPMSG_FILE_CREATETIME:	info->SetCrtime(val); break;
		case IPMSG_FILE_ATIME:		info->SetAtime(val);  break;
		default: break;
		}
	}
	return	TRUE;
}

BOOL TRecvDlg::RecvDirFile(void)
{
#define PEEK_SIZE	8

	if (fileObj->status == FS_DIRFILESTART || fileObj->status == FS_TRANSINFO) {
		int		size;
		if (fileObj->infoLen == 0) {
			if ((size = ::recv(fileObj->conInfo->sd, fileObj->info + (int)fileObj->offset,
								PEEK_SIZE - (int)fileObj->offset, 0)) <= 0) {
				return	FALSE;
			}
			if (fileObj->aes.IsKeySet()) {
				fileObj->aes.DecryptCTR((const BYTE *)fileObj->info + (int)fileObj->offset,
										  (BYTE *)fileObj->info + (int)fileObj->offset, size);
			}
			if ((fileObj->offset += size) < PEEK_SIZE) {
				return	TRUE;
			}
			fileObj->info[fileObj->offset] = 0;
			fileObj->infoLen = strtoul(fileObj->info, 0, 16);
			if (fileObj->infoLen >= sizeof(fileObj->info) -1 || fileObj->infoLen <= 0) {
				return	FALSE;	// too big or small
			}
		}
		if (fileObj->offset < fileObj->infoLen) {
			if ((size = ::recv(fileObj->conInfo->sd, fileObj->info + (int)fileObj->offset,
								fileObj->infoLen - (int)fileObj->offset, 0)) <= 0) {
				return	FALSE;
			}
			if (fileObj->aes.IsKeySet()) {
				fileObj->aes.DecryptCTR((const BYTE *)fileObj->info + (int)fileObj->offset,
										  (BYTE *)fileObj->info + (int)fileObj->offset, size);
			}
			fileObj->offset += size;
		}
		if (fileObj->offset == fileObj->infoLen) {
			fileObj->info[fileObj->infoLen] = 0;
			if (!DecodeDirEntry(fileObj->info, &fileObj->curFileInfo, 
								fileObj->isDir ? fileObj->u8fname : NULL)) {
				return	FALSE;	// Illegal entry
			}
			if (isAutoSave) {
				if (cfg->autoSavedSize + fileObj->curFileInfo.Size() >
					(int64)cfg->autoSaveMax * 1000 * 1000) {
					return FALSE;
				}
			}

			fileObj->offset = fileObj->infoLen = 0;	// 初期化
			DWORD	attr = fileObj->curFileInfo.Attr();

			if (GET_MODE(attr) == IPMSG_FILE_DIR) {
				char	buf[MAX_BUF];
				const char *fname = (fileObj->dirCnt == 0) ?
										fileObj->fileInfo->Fname() : fileObj->curFileInfo.Fname();

				if (!fileObj->isDir) return FALSE;

				if (MakePathU8(buf, fileObj->path, fname) >= MAX_PATH_U8) {
					MessageBoxU8(buf, LoadStrU8(IDS_PATHTOOLONG));
					return	FALSE;
				}
				if (!IsSafePath(buf, fname)) {
					return	FALSE;
				}
				if (!CreateDirectoryU8(buf, NULL)) {
					return	FALSE;
				}
				strncpyz(fileObj->path, buf, MAX_PATH_U8);
				fileObj->dirCnt++;
			}
			else if (GET_MODE(attr) == IPMSG_FILE_RETPARENT) {
				if (!fileObj->isDir) return FALSE;

				if (fileObj->curFileInfo.Mtime()) { // directory の time stamp をあわせる(NT系のみ)
					FILETIME	ft;
					HANDLE		hFile;
					UnixTime2FileTime(fileObj->curFileInfo.Mtime(), &ft);
					if ((hFile = CreateFileU8(fileObj->path, GENERIC_WRITE, FILE_SHARE_READ, 0,
						OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0)) != INVALID_HANDLE_VALUE) {
						::SetFileTime(hFile, NULL, NULL, &ft);
						::CloseHandle(hFile);
					}
				}
				if (fileObj->curFileInfo.Attr() & IPMSG_FILE_RONLYOPT) {
					SetFileAttributesU8(fileObj->path, FILE_ATTRIBUTE_READONLY);
				}
				if (--fileObj->dirCnt <= 0) {
					fileObj->status = FS_COMPLETE;
					return	TRUE;
				}
				if (!GetParentDirU8(fileObj->path, fileObj->path)) {
					return	FALSE;
				}
			}
			else {
				if (fileObj->isDir && fileObj->dirCnt == 0) {
					return	FALSE;
				}
				if (fileObj->curFileInfo.Size() == 0) {	// 0byte file
					if (OpenRecvFile()) {	// 0byteの場合は作成失敗を無視
						CloseRecvFile(TRUE);
					}
					if (!fileObj->isDir) {
						fileObj->status = FS_COMPLETE;
						return TRUE;
					}
				}
				fileObj->status = fileObj->curFileInfo.Size() ? FS_TRANSFILE : FS_TRANSINFO;
			}
			return	TRUE;
		}
	}

	if (fileObj->status == FS_TRANSFILE) {
		if (!RecvFile()) {
			CloseRecvFile();
			return	FALSE;
		}
		if (fileObj->status == FS_ENDFILE || fileObj->status == FS_COMPLETE) {
			CloseRecvFile(TRUE);
			if (!fileObj->isDir) return TRUE;
			fileObj->status = FS_TRANSINFO;
		}
	}

	return	TRUE;
}

BOOL TRecvDlg::OpenRecvFile(void)
{
	char	path[MAX_BUF];

	if (MakePathU8(path, fileObj->isDir ? fileObj->path : fileObj->saveDir,
				fileObj->curFileInfo.Fname()) >= MAX_PATH_U8) {
		MessageBoxU8(path, LoadStrU8(IDS_PATHTOOLONG));
		return FALSE;
	}
	if (!IsSafePath(path, fileObj->curFileInfo.Fname())) {
		MessageBoxU8(path, LoadStrU8(IDS_NOTSAFEPATH));
		return	FALSE;
	}

	if ((fileObj->hFile = CreateFileU8(path, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
			0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE) {
		if (!fileObj->isDir) {
			MessageBoxU8(LoadStrU8(IDS_CREATEFAIL), path);
		}
		return	FALSE;
	}

	if (fileObj->curFileInfo.Attr() & IPMSG_FILE_RONLYOPT) {
		SetFileAttributesU8(path, FILE_ATTRIBUTE_READONLY);
	}

//::SetFilePointer(fileObj->hFile, OFFSET, 0, FILE_BEGIN);
//::SetEndOfFile(fileObj->hFile);

	return	TRUE;
}

BOOL TRecvDlg::RecvFile(void)
{
	int		wresid = (int)(fileObj->offset - fileObj->woffset);
	int64	remain = fileObj->curFileInfo.Size() - fileObj->offset;
	int		size = 0;

	if (!fileObj->isClip) {
		if (remain > cfg->IoBufMax - wresid)
			remain = cfg->IoBufMax - wresid;
	}

	if ((size = ::recv(fileObj->conInfo->sd, fileObj->recvBuf + wresid, (int)remain, 0)) <= 0) {
		return	FALSE;
	}
	if (fileObj->aes.IsKeySet()) {
		fileObj->aes.DecryptCTR((const BYTE *)fileObj->recvBuf + wresid,
								(BYTE *)fileObj->recvBuf + wresid, size);
	}

	if (!fileObj->isClip && fileObj->hFile == INVALID_HANDLE_VALUE) {
		if (!OpenRecvFile()) {
			return	FALSE;
		}
	}

	wresid += size;
	if (!fileObj->isClip &&
		(fileObj->offset + size >= fileObj->curFileInfo.Size() || cfg->IoBufMax == wresid)) {
		DWORD	wsize;
		if (!::WriteFile(fileObj->hFile, fileObj->recvBuf, wresid, &wsize, 0)
			|| wresid != (int)wsize) {
			MessageBoxU8(LoadStrU8(IDS_WRITEFAIL));
			return	FALSE;
		}
		fileObj->woffset += wresid;
	}
	fileObj->offset += size;

	DWORD	nowTick = ::GetTick();

	if (nowTick - fileObj->conInfo->lastTick >= 1000) {
		fileObj->conInfo->lastTick = nowTick;
		PostMessage(WM_RECVDLG_FILEBUTTON, 0, 0);
	}

	if (fileObj->offset >= fileObj->curFileInfo.Size()) {
		fileObj->status = fileObj->isDir ? FS_ENDFILE : FS_COMPLETE;
	}

	return	TRUE;
}

int MakeTransRateStr(char *buf, DWORD ticks, int64 cur_size, int64 total_size)
{
	int len = 0;
	buf[len++] = ' ';
	len += MakeSizeString(buf + len, cur_size) -2;	// "MB"部分を抜く
	buf[len++] = '/';
	len += MakeSizeString(buf + len, total_size);
	buf[len++] = ' ';
	len += MakeSizeString(buf + len, cur_size * 1000 / (ticks ? ticks : 10));
	return len + sprintf(buf + len, "/s (%d%%)",
							(int)(cur_size * 100 / (total_size ? total_size : 1)));
}

int MakeDirTransRateStr(char *buf, DWORD ticks, int64 cur_size, int files)
{
	int len = 0;
	buf[len++] = ' ';
	len += sprintf(buf + len, "Total ");
	len += MakeSizeString(buf + len, cur_size);
	len += sprintf(buf + len, "/%dfiles (", files);
	len += MakeSizeString(buf + len, cur_size * 1000 / (ticks ? ticks : 1));
	return	len + sprintf(buf + len, "/s)" );
}

BOOL RecvTransEndDlg::EvCreate(LPARAM lParam)
{
	char	buf[MAX_BUF];
	DWORD	difftick = fileObj->lastTick - fileObj->startTick;
	int		len = 0;

	len += sprintf(buf + len, "Total: ");
	len += MakeSizeString(buf + len, fileObj->totalTrans);
	len += sprintf(buf + len, " (");
	len += MakeSizeString(buf + len, fileObj->totalTrans * 1000 / (difftick ? difftick : 1));
	len += sprintf(buf + len, "/s)\r\n%d", difftick/1000);

	if (difftick/1000 < 20) {
		len += sprintf(buf + len, ".%02d", (difftick%1000) / 10);
	}
	len += sprintf(buf + len, " sec   ");

	if (fileObj->totalFiles > 1 || fileObj->isDir) {
		len += sprintf(buf + len, "%d files", fileObj->totalFiles);
	}
	else {
		len += sprintf(buf + len, "%s", fileObj->fileInfo->Fname());
	}

	if (fileObj->status == FS_COMPLETE) {
		if (fileObj->totalFiles > 1 || fileObj->isDir) {
			::EnableWindow(GetDlgItem(EXEC_BUTTON), FALSE);
		}
	}

	SetDlgItemTextU8(FILE_STATIC, buf);

	GetWindowRect(&rect);
	MoveWindow(rect.left +30, rect.top + 30, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	return	TRUE;
}

BOOL RecvTransEndDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	if (fileObj->status == FS_COMPLETE && wID == IDCANCEL) {
		wID = IDOK;
	}

	switch (wID) {
	case FOLDER_BUTTON: case EXEC_BUTTON: case IDCANCEL: case IDOK: case IDRETRY:
		EndDialog(wID);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TRecvDlg::EndRecvFile(BOOL manual_suspend)
{
	if (fileObj->hThread) {
		HANDLE	hThread = fileObj->hThread;
		fileObj->hThread = 0;	// 中断の合図
		WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
	}

	fileObj->lastTick = fileObj->conInfo->lastTick = ::GetTick();
	SetTransferButtonText();

	int			target = ShareMng::GetFileInfoNo(shareInfo, fileObj->fileInfo);
	BOOL		isSingleTrans = fileObj->startTick == fileObj->conInfo->startTick;
	ClipBuf		*clipBuf = NULL;
	BOOL		isInsertImage = FALSE;

	::closesocket(fileObj->conInfo->sd);

	if (fileObj->isClip) {
		for (clipBuf=clipList.TopObj(); clipBuf; clipBuf=clipList.NextObj(clipBuf)) {
			if (fileObj->recvBuf == (char *)clipBuf->vbuf.Buf()) {
				break;
			}
		}
	}
	else {
		delete [] fileObj->recvBuf;
	}
	fileObj->recvBuf = NULL;
	delete fileObj->conInfo;
	fileObj->conInfo = NULL;
	FileInfo	*fileInfo = fileObj->fileInfo;

	if (fileObj->status == FS_COMPLETE) {
		if (fileObj->isClip && clipBuf) {
			clipBuf->finished = TRUE;
			if (cfg->LogCheck /*&& (cfg->ClipMode & CLIP_SAVE)*/) {
				SaveImageFile(cfg, fileInfo->Fname(), &clipBuf->vbuf);
				::PostMessage(GetMainWnd(), WM_LOGVIEW_UPDATE, 0, 0);
			}
		}
		if (!fileObj->isClip) {
			if (isAutoSave) {
				int	len = (int)strlen(autoSaves);
				snprintfz(autoSaves + len, sizeof(autoSaves) - len - 1, "%x=%s:",
					fileInfo->Id(), fileInfo->Fname());
			}
			if ((cfg->downloadLink & 0x1) == 0 && cfg->LogCheck && *cfg->LogFile) {
				char	targ[MAX_PATH_U8];
				MakePathU8(targ, fileObj->saveDir, fileInfo->Fname(), MAX_PATH_U8);
				CreateDownloadLinkU8(cfg, fileInfo->FnameOrg(), targ, msg.timestamp, TRUE);
			}
		}
		BOOL	is_clip_finish = TRUE;
		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (shareInfo->fileInfo[i] != fileInfo) {
				if (shareInfo->fileInfo[i]->IsSelected()) {
					shareInfo->RemoveFileInfo(target);
					return	SaveFile(isAutoSave, FALSE);	// 過去のstateに従う
				}
				if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
					is_clip_finish = FALSE;
				}
			}
		}
		if (fileObj->isClip && is_clip_finish && status == SHOW && openFlg) {
			if (IsShowDirectImage(cfg, &msg.hostSub)) {
				isInsertImage = TRUE;
			}
		}
	}
	if (cfg->ReproMsg && *autoSaves) {
		cfg->UpdatePacket(&msg, autoSaves);
	}

	int ret;

	if (manual_suspend) {
		ret = IDCANCEL;
	} else if (fileObj->isClip || isAutoSave) {
		ret = IDOK;
	} else {
		recvEndDlg = new RecvTransEndDlg(fileObj, this);
		ret = recvEndDlg->Exec();
		delete recvEndDlg;
		recvEndDlg = NULL;
		if (!shareInfo) {
			MessageBoxU8("Illegal status(recvEndDlg4)", IP_MSG, MB_OK);
			return TRUE;
		}
	}

	if (ret == EXEC_BUTTON || ret == FOLDER_BUTTON && fileObj->isDir && isSingleTrans) {
		char	buf[MAX_BUF];
		MakePathU8(buf, fileObj->saveDir, fileInfo->Fname());
		ShellExecuteU8(NULL, NULL, buf, 0, 0, SW_SHOW);
	}
	else if (ret == FOLDER_BUTTON) {
		ShellExecuteU8(NULL, NULL, fileObj->saveDir, 0, 0, SW_SHOW);
	}

	if (ret == IDOK || ret == FOLDER_BUTTON || ret == EXEC_BUTTON) {
		if (shareInfo && (!isAutoSave || fileObj->status == FS_COMPLETE)) {
			shareInfo->RemoveFileInfo(target);
		}
	}

	SetFileButton(this, FILE_BUTTON, shareInfo, autoSaves);
	EvSize(SIZE_RESTORED, 0, 0);

	if (isInsertImage) {
		InsertImages(); // display and remove
	}

	if (ret == IDRETRY) {
		PostMessage(WM_COMMAND, FILE_BUTTON, 0);
	}

	return	TRUE;
}

void TRecvDlg::SetStatus(SelfStatus _status)
{
	if (_status == SHOW && status != SHOW && openFlg) {
		cfg->DeletePacket(&msg);
	}
	status = _status;
}

void TRecvDlg::SetTransferButtonText(void)
{
	char	buf[MAX_LISTBUF];

	if (fileObj->conInfo == NULL) return;

	if (fileObj->isDir) {
		MakeDirTransRateStr(buf, fileObj->conInfo->lastTick - fileObj->conInfo->startTick,
							fileObj->curTrans + fileObj->offset, fileObj->curFiles);
	}
	else {
		MakeTransRateStr(buf, fileObj->conInfo->lastTick - fileObj->conInfo->startTick,
				fileObj->status < FS_COMPLETE ? fileObj->offset : fileObj->curFileInfo.Size(),
				fileObj->curFileInfo.Size());
	}
	SetDlgItemTextU8(FILE_BUTTON, buf);
}


BOOL TRecvDlg::CheckSpecialCommand()
{
	if (memcmp(msg.msgBuf, REMOTE_CMD, REMOTE_HEADERLEN) != 0) return FALSE;

	TRemoteDlg::Mode mode = TRemoteDlg::INIT;

	if (cfg->RemoteRebootMode) {
		if (!memcmp(msg.msgBuf + REMOTE_HEADERLEN, cfg->RemoteReboot, REMOTE_KEYSTRLEN)) {
			mode = TRemoteDlg::REBOOT;
		}
	}

	if (cfg->RemoteExitMode) {
		if (!memcmp(msg.msgBuf + REMOTE_HEADERLEN, cfg->RemoteExit, REMOTE_KEYSTRLEN)) {
			mode = TRemoteDlg::EXIT;

#define STANDBY_STR		"standby"
#define HIBERNATE_STR	"hibernate"
			if (msg.msgBuf[REMOTE_HEADERLEN + REMOTE_KEYSTRLEN] == ':') {
				const char *p = msg.msgBuf + REMOTE_HEADERLEN + REMOTE_KEYSTRLEN + 1;

				if (!strnicmp(p, STANDBY_STR, sizeof(STANDBY_STR) -1)) {
					mode = TRemoteDlg::STANDBY;
				}
				else if (!strnicmp(p, HIBERNATE_STR, sizeof(HIBERNATE_STR) -1)) {
					mode = TRemoteDlg::HIBERNATE;
				}
			}
		}
	}

	if (cfg->RemoteIPMsgExitMode) {
		if (!memcmp(msg.msgBuf + REMOTE_HEADERLEN, cfg->RemoteIPMsgExit, REMOTE_KEYSTRLEN)) {
			mode = TRemoteDlg::IPMSGEXIT;
		}
	}

	if (mode != TRemoteDlg::INIT) {
		cfg->DeletePacket(&msg);
		::PostMessage(GetMainWnd(), WM_IPMSG_REMOTE, mode, 0);
		status = REMOTE;
	}

	return TRUE;
}

void TRecvDlg::CleanupAutoSaveSize()
{
	if (autoSavedSize > 0) {
		if ((cfg->autoSavedSize -= autoSavedSize) < 0) {
			cfg->autoSavedSize = 0;
		}
		autoSavedSize = 0;
	}
}


