static char *recvdlg_id = 
	"@(#)Copyright (C) H.Shirouzu 1996-2011   recvdlg.cpp	Ver3.20";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: Receive Dialog
	Create					: 1996-06-01(Sat)
	Update					: 2011-05-23(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include <time.h>
#include "resource.h"
#include "ipmsg.h"
#include "aes.h"
#include "blowfish.h"

int		TRecvDlg::createCnt = 0;
HBITMAP	TRecvDlg::hDummyBmp = 0;


TRecvDlg::TRecvDlg(MsgMng *_msgMng, MsgBuf *_msg, THosts *_hosts, Cfg *_cfg, LogMng *_logmng) : TListDlg(RECEIVE_DIALOG), editSub(_cfg, this)
{
	msgMng		= _msgMng;
	cfg			= _cfg;
	openFlg		= FALSE;
	logmng		= _logmng;
	hosts		= _hosts;
	hEditFont	= NULL;
	hHeadFont	= NULL;
	fileObj		= NULL;
	shareInfo	= NULL;
	timerID		= 0;
	retryCnt	= 0;
	cryptCapa	= 0;
	logOpt		= 0;
	useClipBuf	= 0;
	useDummyBmp	= 0;
	createCnt++;
	::GetLocalTime(&recvTime);

	if (!hDummyBmp) hDummyBmp = LoadBitmap(TApp::GetInstance(), (LPCSTR)DUMMYPIC_BITMAP);

	memset(&orgFont, 0, sizeof(orgFont));
	msg.Init(_msg);

	MakeListString(cfg, &msg.hostSub, hosts, head);

	status = INIT;
	if (msg.command & IPMSG_ENCRYPTOPT) {
		if ((status = (DecryptMsg() ? INIT : ERR))) {
			msgMng->Send(&msg.hostSub,
				IPMSG_SENDMSG|IPMSG_NOLOGOPT|IPMSG_AUTORETOPT, GetLoadStr(IDS_DECRYPTERR));
		}
	}

	if (msg.command & IPMSG_FILEATTACHOPT) {
		if ((shareInfo = DecodeShareMsg(msg.exBuf, (cfg->ClipMode & CLIP_ENABLE)))) {
			fileObj = new RecvFileObj;
			memset(fileObj, 0, sizeof(RecvFileObj));
		}
	}

	if (shareInfo && (cfg->ClipMode & CLIP_ENABLE)) {
		int		clip_num = 0, noclip_num = 0;
		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				const char *ext = strrchr(shareInfo->fileInfo[i]->Fname(), '.');
				if (ext && strcmpi(ext, ".png") == 0 && clip_num++ < cfg->ClipMax) {
					shareInfo->fileInfo[i]->SetSelected(TRUE);
					char	buf[MAX_PATH];
					MakeClipFileName(msgMng->MakePacketNo(), FALSE, buf);
					shareInfo->fileInfo[i]->SetFname(buf);
				}
				else {
					FreeDecodeShareMsgFile(shareInfo, i);
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
	}

	if (!VerifyUserNameExtension(cfg, &msg)) {
		logOpt |= LOG_UNAUTH;
	}

	if (status != ERR) {
		if ((msg.command & IPMSG_PASSWORDOPT) == 0 || !cfg->PasswdLogCheck)
			logmng->WriteRecvMsg(&msg, logOpt, hosts, shareInfo);

		hAccel = ::LoadAccelerators(TApp::GetInstance(), (LPCSTR)IPMSG_ACCEL);
	}
}

TRecvDlg::~TRecvDlg()
{
	if (shareInfo) 
	{
		if (shareInfo->fileCnt > 0)
			msgMng->Send(&msg.hostSub, IPMSG_RELEASEFILES, msg.packetNo);
		// あとで受信終了通知のコードを入れる
		FreeDecodeShareMsg(shareInfo);
	}
	if (fileObj)
	{
		if (fileObj->conInfo)
			delete fileObj->conInfo;
		delete fileObj;
	}

	ClipBuf *clipBuf;
	while ((clipBuf = (ClipBuf *)clipList.TopObj())) {
		clipList.DelObj(clipBuf);
		delete clipBuf;
	}

	if (hHeadFont)
		::DeleteObject(hHeadFont);
	if (hEditFont)
		::DeleteObject(hEditFont);

	createCnt--;
}

/*
	メッセージの復号化
*/
BOOL TRecvDlg::DecryptMsg()
{
	HCRYPTKEY	hKey=0, hExKey=0;
	char		*capa_hex, *skey_hex, *msg_hex, *hash_hex, *p;
	BYTE		skey[MAX_BUF], sig_data[MAX_BUF], data[MAX_BUF], iv[256/8];
	int			len, msgLen, encMsgLen;
	HCRYPTPROV	target_csp;
	BOOL		(*str2bin_revendian)(const char *, BYTE *, int, int *) = NULL;
	BOOL		(*str2bin)(const char *, BYTE *, int, int *)           = NULL;

	if ((capa_hex = separate_token(msg.msgBuf, ':', &p)) == NULL)
		return	FALSE;
	cryptCapa = strtoul(capa_hex, 0, 16);

	if ((cryptCapa & IPMSG_ENCODE_BASE64) && !pCryptStringToBinary) return FALSE; // for Win2000

	int	kt = (cryptCapa & IPMSG_RSA_2048) ? KEY_2048 :
			 (cryptCapa & IPMSG_RSA_1024) ? KEY_1024 :
			 (cryptCapa & IPMSG_RSA_512)  ? KEY_512  : -1;
	if (kt == -1) return FALSE;

	target_csp	= cfg->priv[kt].hCsp;
	hExKey		= cfg->priv[kt].hKey;

	if (cryptCapa & IPMSG_ENCODE_BASE64) { // base64
		str2bin_revendian = b64str2bin_revendian;
		str2bin = b64str2bin;
	}
	else {			// hex encoding
		str2bin_revendian = hexstr2bin_revendian;
		str2bin = hexstr2bin;
	}

	if ((skey_hex = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;

	if ((msg_hex = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;

	if (cryptCapa & IPMSG_SIGN_SHA1) {
		if ((hash_hex = separate_token(NULL, ':', &p)) == NULL)
			return	FALSE;
	}

	// IV の初期化
	memset(iv, 0, sizeof(iv));
	if (cryptCapa & IPMSG_PACKETNO_IV) strncpyz((char *)iv, msg.packetNoStr, sizeof(iv));

	if (cryptCapa & IPMSG_AES_256) {	// AES
		str2bin_revendian(skey_hex, skey, sizeof(skey), &len);
		// 公開鍵取得
		if (!pCryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		AES	aes(skey, len);
		str2bin(msg_hex, (BYTE *)msg.msgBuf, sizeof(msg.msgBuf), &encMsgLen);
		msgLen = aes.Decrypt((u8 *)msg.msgBuf, (u8 *)msg.msgBuf, encMsgLen, iv);
	}
	else if (cryptCapa & IPMSG_BLOWFISH_128) {	// blowfish
		str2bin_revendian(skey_hex, skey, sizeof(skey), &len);
		// 公開鍵取得
		if (!pCryptDecrypt(hExKey, 0, TRUE, 0, (BYTE *)skey, (DWORD *)&len))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;

		CBlowFish	bl(skey, len);
		str2bin(msg_hex, (BYTE *)msg.msgBuf, sizeof(msg.msgBuf), &encMsgLen);
		msgLen = bl.Decrypt((BYTE *)msg.msgBuf, (BYTE *)msg.msgBuf, encMsgLen, BF_CBC|BF_PKCS5, iv);
	}
	else {	// RC2
		// Skey Blob を作る
		skey[0] = SIMPLEBLOB;
		skey[1] = CUR_BLOB_VERSION;
		*(WORD *)(skey + 2) = 0;
		*(ALG_ID *)(skey + 4) = CALG_RC2;
		*(ALG_ID *)(skey + 8) = CALG_RSA_KEYX;
		str2bin_revendian(skey_hex, skey + SKEY_HEADER_SIZE, sizeof(skey) - SKEY_HEADER_SIZE, &len);

		// セッションキーの import
		if (!pCryptImportKey(target_csp, skey, len + SKEY_HEADER_SIZE, hExKey, 0, &hKey))
			return	wsprintf(msg.msgBuf, "CryptImportKey Err(%X)", GetLastError()), FALSE;

		// メッセージの Decrypt
		str2bin(msg_hex, (BYTE *)msg.msgBuf, sizeof(msg.msgBuf), &encMsgLen);
		msgLen = encMsgLen;
		if (!pCryptDecrypt(hKey, 0, TRUE, 0, (BYTE *)msg.msgBuf, (DWORD *)&msgLen))
			return	wsprintf(msg.msgBuf, "CryptDecrypt Err(%X)", GetLastError()), FALSE;
		pCryptDestroyKey(hKey);
	}

	// 電子署名の検証
	if (cryptCapa & IPMSG_SIGN_SHA1) {
		Host		*host = hosts->GetHostByAddr(&msg.hostSub);

		if (!host) host = cfg->priorityHosts.GetHostByName(&msg.hostSub);
		if (host && IsSameHost(&msg.hostSub, &host->hostSub) && host->pubKey.KeyLen() == 256) {
			HCRYPTHASH	hHash = NULL;
			HCRYPTKEY	hExKey = 0;
			int			capa = host->pubKey.Capa();
			int			kt = (capa & IPMSG_RSA_2048) ? KEY_2048 :
							 (capa & IPMSG_RSA_1024) ? KEY_1024 : KEY_512;

			logOpt |= LOG_SIGN_ERR;
			target_csp = cfg->priv[kt].hCsp;
			host->pubKey.KeyBlob(data, sizeof(data), &len);	// KeyBlob 作成/import
			if (pCryptImportKey(target_csp, data, len, 0, 0, &hExKey)) {
				if (pCryptCreateHash(target_csp, CALG_SHA, 0, 0, &hHash)) {
					if (pCryptHashData(hHash, (BYTE *)msg.msgBuf, msgLen, 0)) {
						int		sigLen = 0;
						str2bin_revendian(hash_hex, sig_data, sizeof(sig_data), &sigLen);
						if (pCryptVerifySignature(hHash, sig_data, sigLen, hExKey, 0, 0)) {
							logOpt &= ~LOG_SIGN_ERR;
							logOpt |= LOG_SIGN_OK;
						}
					}
					pCryptDestroyHash(hHash);
				}
				pCryptDestroyKey(hExKey);
			}
		}
		else logOpt |= LOG_SIGN_NOKEY;
	}
	logOpt |= (cryptCapa & IPMSG_RSA_2048) ? LOG_ENC2 :
			  (cryptCapa & IPMSG_RSA_1024) ? LOG_ENC1 : LOG_ENC0;


	// 暗号化添付メッセージ
	if ((msg.command & IPMSG_ENCEXTMSGOPT)) {
		if ((len = strlen(msg.msgBuf) + 1) < msgLen) {
			strncpyz(msg.exBuf, msg.msgBuf + len, MAX_UDPBUF);
		}
	}

	// UNIX 形式の改行を変換
	MsgMng::UnixNewLineToLocal(msg.msgBuf, msg.msgBuf, sizeof(msg.msgBuf));
	if ((msg.command & IPMSG_UTF8OPT) == 0) {
		strncpyz(msg.msgBuf, AtoU8(msg.msgBuf), sizeof(msg.msgBuf));
	}

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

	editSub.SendMessage(EM_AUTOURLDETECT, 1, 0);
	editSub.SendMessage(EM_SETBKGNDCOLOR, FALSE, ::GetSysColor(COLOR_3DFACE));
	editSub.SendMessage(EM_SETTARGETDEVICE, 0, 0);		// 折り返し

	SetDlgIcon(hWnd);
	SetDlgItemTextU8(RECV_HEAD, head);

	if (msg.command & IPMSG_BROADCASTOPT)
		SetDlgItemTextU8(RECV_TITLE, GetLoadStrU8(IDS_BROADCAST));
	else if (msg.command & IPMSG_MULTICASTOPT)
		SetDlgItemTextU8(RECV_TITLE, GetLoadStrU8(IDS_MULTICAST));
	else
		SetDlgItemTextU8(RECV_TITLE, GetLoadStrU8(IDS_UNICAST));

	if (msg.command & IPMSG_AUTORETOPT)
		SetDlgItemTextU8(RECV_TITLE, GetLoadStrU8(IDS_UNIABSENCE));

	char	buf[MAX_LISTBUF];
	GetWindowTextU8(buf, sizeof(buf));

	if (msg.command & IPMSG_ENCRYPTOPT) {
		strcat(buf, (logOpt & LOG_SIGN_OK) ? " +++" :
					(logOpt & LOG_ENC2)    ? " ++"  :
					(logOpt & LOG_ENC1)    ? " +" : " -");
		if (logOpt & LOG_SIGN_ERR) strcat(buf, GetLoadStrU8(IDS_SIGN_ERROR_TITLE));
	}
	else if (logOpt & LOG_UNAUTH) {
		strcpy(buf, GetLoadStrU8(IDS_UNAUTHORIZED_TITLE));
	}

	SetWindowTextU8(buf);

	char	head2[MAX_NAMEBUF];
	wsprintf(head2, "at %s", Ctime(&recvTime));
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
			SetFileButton(this, FILE_BUTTON, shareInfo);
		}
	}

	if (msg.command & IPMSG_FILEATTACHOPT)
	{
		int	id = (useClipBuf == 2) ? IDS_FILEWITHCLIP :
				 (useClipBuf == 1) ? IDS_WITHCLIP     : IDS_FILEATTACH;
		GetDlgItemTextU8(OPEN_BUTTON, buf, sizeof(buf));
		strcat(buf, " ");
		strcat(buf, GetLoadStrU8(id));
		SetDlgItemTextU8(OPEN_BUTTON, buf);
	}
	if (msg.command & IPMSG_PASSWORDOPT)
	{
		GetDlgItemTextU8(OPEN_BUTTON, buf, sizeof(buf));
		strcat(buf, GetLoadStrU8(IDS_KEYOPEN));
		SetDlgItemTextU8(OPEN_BUTTON, buf);
	}

	if (cfg->QuoteCheck)
		CheckDlgButton(QUOTE_CHECK, cfg->QuoteCheck);

	if (cfg->AbnormalButton)
		SetDlgItemTextU8(IDOK, GetLoadStrU8(IDS_INTERCEPT));

	SetFont();
	SetSize();

	HMENU	hMenu = ::GetSystemMenu(hWnd, FALSE);
	AppendMenuU8(hMenu, MF_SEPARATOR, NULL, NULL);
	SetMainMenu(hMenu);

	if (!IsNewShell())
	{
		ULONG	style = GetWindowLong(GWL_STYLE);
		style &= 0xffffff0f;
		style |= 0x00000080;
		SetWindowLong(GWL_STYLE, style);
	}
//	SetForegroundWindow();

	if (msg.command & IPMSG_NOADDLISTOPT)
		::EnableWindow(GetDlgItem(IDOK), FALSE);
	EvSize(SIZE_RESTORED, 0, 0);

	if (shareInfo && (cfg->ClipMode & CLIP_ENABLE)) {
		for (int i=0; i < shareInfo->fileCnt; i++) {
			if (GET_MODE(shareInfo->fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
				editSub.InsertBitmapByHandle(hDummyBmp, shareInfo->fileInfo[i]->Pos());
				useDummyBmp++;
			}
		}
		SaveFile();	// for clipboard
	}

	return	TRUE;
}

BOOL TRecvDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:
		*msg.msgBuf = 0;
		if (openFlg && IsDlgButtonChecked(QUOTE_CHECK)) {
			editSub.GetTextUTF8(msg.msgBuf, sizeof(msg.msgBuf));
		}
		::PostMessage(GetMainWnd(), WM_SENDDLG_OPEN, (WPARAM)hWnd, (LPARAM)&msg);
		return	TRUE;

	case IDCANCEL:
		if (fileObj && fileObj->conInfo)
			return	EvCommand(0, FILE_BUTTON, 0), TRUE;
		if (clipList.TopObj()) {
			ClipBuf *clipBuf = (ClipBuf *)clipList.TopObj();
			if (clipBuf->finished) {
				if (MessageBoxU8(GetLoadStrU8(IDS_CLIPDESTROY), IP_MSG, MB_OKCANCEL) != IDOK)
					return TRUE;
			}
		}
//		else if (fileObj && MessageBoxU8("OK", IP_MSG, MB_OKCANCEL) != IDOK) return TRUE;

		if (timerID == 0)
			::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, (LPARAM)this);
		else
			TWin::Show(SW_HIDE);
		return	TRUE;

	case IMAGE_BUTTON:
		InsertImages();
		return	TRUE;

	case MISC_ACCEL:
	case HIDE_ACCEL:
		::PostMessage(GetMainWnd(), WM_COMMAND, wID, 0);
		return	TRUE;

	case ALLSELECT_ACCEL:
		editSub.SendMessage(EM_SETSEL, 0, (LPARAM)-1);
		return	TRUE;

	case OPEN_BUTTON:
		if (openFlg)
			return	TRUE;

		if (msg.command & IPMSG_PASSWORDOPT)
		{
			TPasswordDlg	dlg(cfg, this);

			if (!dlg.Exec())
				return	TRUE;

			if (cfg->PasswdLogCheck)
				logmng->WriteRecvMsg(&msg, logOpt, hosts, shareInfo);
		}
		openFlg = TRUE;

		editSub.ShowWindow(SW_SHOW);
		::ShowWindow(GetDlgItem(QUOTE_CHECK), SW_SHOW);
		::ShowWindow(GetDlgItem(OPEN_BUTTON), SW_HIDE);
		::EnableWindow(GetDlgItem(OPEN_BUTTON), FALSE);

		if (shareInfo)
			SetFileButton(this, FILE_BUTTON, shareInfo), EvSize(SIZE_RESTORED, 0, 0);

		if ((cfg->ClipMode & CLIP_CONFIRM) == 0) {
			InsertImages();
		}

		if (msg.command & IPMSG_SECRETOPT)
		{
			char	buf[MAX_LISTBUF];
			int		cmd = IPMSG_READMSG | (msg.command & IPMSG_READCHECKOPT);

			wsprintf(buf, "%ld", msg.packetNo);
			packetNo = msgMng->MakeMsg(readMsgBuf, cmd, buf);
			msgMng->UdpSend(msg.hostSub.addr, msg.hostSub.portNo, readMsgBuf);

			if (msg.command & IPMSG_READCHECKOPT)
				timerID = ::SetTimer(hWnd, IPMSG_RECV_TIMER, cfg->RetryMSec, NULL);
		}
		return	TRUE;

	case FILE_BUTTON:
		if (fileObj && fileObj->conInfo)
		{
			if (fileObj->hThread)
				::SuspendThread(fileObj->hThread);
			int ret = MessageBoxU8(GetLoadStrU8(IDS_TRANSSTOP), IP_MSG, MB_OKCANCEL);
			if (fileObj->hThread)
				::ResumeThread(fileObj->hThread);
			if (ret == IDOK)
				if (fileObj->conInfo) EndRecvFile(TRUE);
		}
		else if (fileObj) {
			TSaveCommonDlg	dlg(shareInfo, cfg, this);
			if (dlg.Exec())
			{
				memset(fileObj, 0, sizeof(RecvFileObj));
				strncpyz(fileObj->saveDir, cfg->lastSaveDir, MAX_PATH_U8);
				SaveFile();
			}
		}
		break;

	case MENU_SAVEPOS:
		if ((cfg->RecvSavePos = !cfg->RecvSavePos) != 0)
		{
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
		if (::ChooseFont(&cf))
		{
			*targetFont = tmpFont;
			SetFont();
			::InvalidateRgn(hWnd, NULL, TRUE);
		}
		cfg->WriteRegistry(CFG_FONT);
		return	TRUE;

	case MENU_DEFAULTFONT:
		cfg->RecvHeadFont = orgFont;
		cfg->RecvEditFont = orgFont;
		SetFont();
		::InvalidateRgn(hWnd, NULL, TRUE);
		cfg->WriteRegistry(CFG_FONT);
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
	}

	return	FALSE;
}

BOOL TRecvDlg::SaveFile(void)
{
	int		target;
	for (target=0; target < shareInfo->fileCnt; target++)
		if (shareInfo->fileInfo[target]->IsSelected())
			break;
	if (target == shareInfo->fileCnt)
		return	FALSE;

	memset(fileObj, 0, (char *)&fileObj->totalTrans - (char *)fileObj);
	fileObj->conInfo = new ConnectInfo;
	fileObj->fileInfo = shareInfo->fileInfo[target];
	fileObj->isDir = GET_MODE(fileObj->fileInfo->Attr()) == IPMSG_FILE_DIR ? TRUE : FALSE;
	fileObj->isClip = GET_MODE(fileObj->fileInfo->Attr()) == IPMSG_FILE_CLIPBOARD ? TRUE : FALSE;
	fileObj->status = fileObj->isDir ? FS_DIRFILESTART : FS_TRANSFILE;
	fileObj->hFile = INVALID_HANDLE_VALUE;
	if (fileObj->isDir)
		strncpyz(fileObj->path, fileObj->saveDir, MAX_PATH_U8);

	if (ConnectRecvFile())
		SetDlgItemTextU8(FILE_BUTTON, GetLoadStrU8(IDS_PREPARETRANS));
	else {
		delete fileObj->conInfo;
		fileObj->conInfo = NULL;
	}
	fileObj->startTick = ::GetTickCount();
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
		return	EvCommand(0, uCmdType, 0);
	}

	return	FALSE;
}

/*
	User定義 Event CallBack
*/
BOOL TRecvDlg::EventUser(UINT uMsg, WPARAM wParam, LPARAM lParam)
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

/*
	WM_TIMER event call back
	送信確認/再送用
*/
BOOL TRecvDlg::EvTimer(WPARAM _timerID, TIMERPROC proc)
{
	if (retryCnt++ < cfg->RetryMax * 2)
	{
		msgMng->UdpSend(msg.hostSub.addr, msg.hostSub.portNo, readMsgBuf);
		return	TRUE;
	}

	::KillTimer(hWnd, IPMSG_RECV_TIMER);
	if (timerID == 0)	// 再入よけ
		return	FALSE;
	timerID = 0;

	if (!::IsWindowVisible(hWnd))
		::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, (LPARAM)this);

	return	TRUE;
}

BOOL GetNoWrapString(HWND hWnd, int cx, char *str, char *buf, int maxlen)
{
	Wstr	wstr(str);
	int		len = wcslen(wstr);
	int		fit_len = 0;
	SIZE    size;
	HDC		hDc;

	if ((hDc = ::GetDC(hWnd))) {
		if (::GetTextExtentExPointW(hDc, wstr, len, cx, &fit_len, NULL, &size)) {
			if (fit_len < len) {
				for (int i=fit_len; i >= 0; i--) {
					if (wstr[i] == '/') {
						wstr.Buf()[i] = 0;
						break;
					}
				}
			}
		}
		::ReleaseDC(hWnd, hDc);
	}

	WtoU8(wstr, buf, maxlen);
	return	TRUE;
}

BOOL TRecvDlg::EvSize(UINT fwSizeType, WORD nWidth, WORD nHeight)
{
	if (fwSizeType != SIZE_RESTORED && fwSizeType != SIZE_MAXIMIZED)
		return	FALSE;

	GetWindowRect(&rect);
	int	xdiff = (rect.right - rect.left) - (orgRect.right - orgRect.left);
	int ydiff = (rect.bottom - rect.top) - (orgRect.bottom - orgRect.top);

	char	buf[MAX_BUF];
	GetNoWrapString(GetDlgItem(RECV_HEAD), headRect.right + xdiff - 40, head, buf, sizeof(buf));
	SetDlgItemTextU8(RECV_HEAD, buf);

	HDWP	hdwp = ::BeginDeferWindowPos(max_recvitem);	// MAX item number
	WINPOS	*wpos;
	BOOL	isFileBtn = IsWindowEnabled(GetDlgItem(FILE_BUTTON));
	UINT	dwFlg = (IsNewShell() ? SWP_SHOWWINDOW : SWP_NOREDRAW) | SWP_NOZORDER;
	UINT	dwHideFlg = SWP_HIDEWINDOW | SWP_NOZORDER;
	if (hdwp == NULL)
		return	FALSE;

// サイズが小さくなる場合の調整値は、Try and Error(^^;
	wpos = &item[title_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(RECV_TITLE), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[head_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(RECV_HEAD), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[head2_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(RECV_HEAD2), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[file_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(FILE_BUTTON), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy, (isFileBtn && openFlg) ? dwFlg : dwHideFlg)) == NULL)
		return	FALSE;

	wpos = &item[open_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(OPEN_BUTTON), NULL, wpos->x, wpos->y, wpos->cx + xdiff, wpos->cy + ydiff, openFlg ? dwHideFlg : dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[edit_item];
	if ((hdwp = ::DeferWindowPos(hdwp, editSub.hWnd, NULL, wpos->x, (isFileBtn ? wpos->y : item[file_item].y), wpos->cx + xdiff, wpos->cy + ydiff + (isFileBtn ? 0 : wpos->y - item[file_item].y), openFlg ? dwFlg : dwHideFlg)) == NULL)
		return	FALSE;

	wpos = &item[image_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IMAGE_BUTTON), NULL, wpos->x, wpos->y + ydiff, wpos->cx, wpos->cy, openFlg && useClipBuf ? dwFlg : dwHideFlg)) == NULL)
		return	FALSE;

	wpos = &item[ok_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IDOK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 3 / 4), wpos->y + ydiff, wpos->cx + (xdiff >= 0 ? 0 : xdiff / 4), wpos->cy, dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[cancel_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(IDCANCEL), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff * 3 / 8), wpos->y + ydiff, wpos->cx + (xdiff >= 0 ? 0 : xdiff * 1 / 4), wpos->cy, dwFlg)) == NULL)
		return	FALSE;

	wpos = &item[quote_item];
	if ((hdwp = ::DeferWindowPos(hdwp, GetDlgItem(QUOTE_CHECK), NULL, wpos->x + (xdiff >= 0 ? xdiff / 2 : xdiff), wpos->y + ydiff, wpos->cx, wpos->cy, openFlg ? dwFlg : dwHideFlg)) == NULL)
		return	FALSE;

	EndDeferWindowPos(hdwp);

	if (!IsNewShell())
		::InvalidateRgn(hWnd, NULL, TRUE);

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
			case WM_LBUTTONDOWN:
//				Debug("EN_LINK (%d %d)\n", el->chrg.cpMin, el->chrg.cpMax);
				editSub.SendMessageW(EM_EXSETSEL, 0, (LPARAM)&el->chrg);
//				editSub.EventUser(WM_EDIT_DBLCLK, 0, 0);
				break;

			case WM_RBUTTONUP:
				editSub.PostMessage(WM_CONTEXTMENU, 0, GetMessagePos());
				break;
			}
		}
		return	TRUE;
	}

	return	FALSE;
}


BOOL TRecvDlg::IsSamePacket(MsgBuf *test_msg)
{
	if (test_msg->packetNo == msg.packetNo && test_msg->hostSub.addr == msg.hostSub.addr && test_msg->hostSub.portNo == msg.hostSub.portNo)
		return	TRUE;
	else
		return	FALSE;
}

void TRecvDlg::SetFont(void)
{
	HFONT	hDlgFont;
	LOGFONT	*editFont;

	if ((hDlgFont = (HFONT)SendMessage(WM_GETFONT, 0, 0L)) == NULL)
		return;
	if (::GetObject(hDlgFont, sizeof(LOGFONT), (LPSTR)&orgFont) == NULL)
		return;

	if (*cfg->RecvHeadFont.lfFaceName == 0)	//初期データセット
		cfg->RecvHeadFont = orgFont;
	if (*cfg->RecvEditFont.lfFaceName == 0)	//初期データセット
		cfg->RecvEditFont = orgFont;

	if (*cfg->RecvHeadFont.lfFaceName && (hDlgFont = ::CreateFontIndirect(&cfg->RecvHeadFont)))
	{
		SendDlgItemMessage(RECV_HEAD, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(RECV_HEAD2, WM_SETFONT, (UINT)hDlgFont, 0L);
		if (hHeadFont)
			::DeleteObject(hHeadFont);
		hHeadFont = hDlgFont;
	}

	editFont = &cfg->RecvEditFont;

	if (editFont->lfFaceName && (hDlgFont = ::CreateFontIndirect(editFont)))
	{
		editSub.SetFont(&cfg->RecvEditFont);
		editSub.ExSetText(msg.msgBuf);
		editSub.SetWindowTextU8(msg.msgBuf);
//		editSub.SendMessage(WM_SETFONT, (UINT)hDlgFont, 0L);
//		if (hEditFont)
//			::DeleteObject(hEditFont);
		hEditFont = hDlgFont;
	}
}

void TRecvDlg::SetSize(void)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);

	::GetWindowPlacement(GetDlgItem(RECV_TITLE), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[title_item]);

	::GetWindowPlacement(GetDlgItem(RECV_HEAD), &wp);
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

	::GetWindowPlacement(GetDlgItem(IDOK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[ok_item]);

	::GetWindowPlacement(GetDlgItem(IDCANCEL), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[cancel_item]);

	::GetWindowPlacement(GetDlgItem(QUOTE_CHECK), &wp);
	RectToWinPos(&wp.rcNormalPosition, &item[quote_item]);

	GetWindowRect(&rect);
	orgRect = rect;

	::GetClientRect(GetDlgItem(RECV_HEAD), &headRect);

	RECT	scRect;
	GetCurrentScreenSize(&scRect);

	int	cx = scRect.right - scRect.left, cy = scRect.bottom - scRect.top;
	int	xsize = rect.right - rect.left + cfg->RecvXdiff, ysize = rect.bottom - rect.top + cfg->RecvYdiff;
	int	x = cfg->RecvXpos, y = cfg->RecvYpos;

	if (cfg->RecvSavePos == 0)
	{
		x = (cx - xsize)/2 + (rand() % (cx/4)) - cx/8;
		y = (cy - ysize)/2 + (rand() % (cy/4)) - cy/8;
	}
	if (x + xsize > cx)
		x = cx - xsize;
	if (y + ysize > cy)
		y = cy - ysize;

	MoveWindow(max(x, scRect.left), max(y, scRect.top), xsize, ysize, TRUE);
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
			ModifyMenuU8(hMenu, MENU_SAVEPOS, MF_BYCOMMAND|(cfg->RecvSavePos ? MF_CHECKED :  0), MENU_SAVEPOS, GetLoadStrU8(IDS_SAVEPOS));
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
	switch (uMsg)
	{
	case WM_RBUTTONUP:
		if (!IsNewShell())
			PopupContextMenu(pos);
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
	AppendMenuU8(hMenu, MF_POPUP, (UINT)::LoadMenu(TApp::GetInstance(), (LPCSTR)RECVFONT_MENU), GetLoadStrU8(IDS_FONTSET));
	AppendMenuU8(hMenu, MF_POPUP, (UINT)::LoadMenu(TApp::GetInstance(), (LPCSTR)SIZE_MENU), GetLoadStrU8(IDS_SIZESET));
	AppendMenuU8(hMenu, MF_STRING, MENU_SAVEPOS, GetLoadStrU8(IDS_SAVEPOS));
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
			if (!::IsWindowVisible(hWnd))
				::PostMessage(GetMainWnd(), WM_RECVDLG_EXIT, 0, (LPARAM)this);
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
		if (mode != SW_HIDE && (cfg->ClipMode & CLIP_CONFIRM) == 0 && openFlg) {
			InsertImages(); // display and remove
		}
		TWin::Show(mode);
	}
}

BOOL TRecvDlg::InsertImages(void)
{
	BOOL	ret = TRUE;

	for (ClipBuf *clipBuf=(ClipBuf *)clipList.TopObj(); clipBuf; ) {
		ClipBuf *next = (ClipBuf *)clipList.NextObj(clipBuf);
		if (clipBuf->finished) {
			int	pos = clipBuf->pos;
			if (useDummyBmp) {
				useDummyBmp--;
				editSub.SendMessageW(EM_SETSEL, pos, pos + 1);
				pos = -1;
			}
			if (!editSub.InsertPng(&clipBuf->vbuf, pos)) ret = FALSE;
			clipList.DelObj(clipBuf);
			delete clipBuf;
		}
		clipBuf = next;
	}
	editSub.SendMessageW(EM_SCROLL, SB_PAGEUP, 0);
	editSub.SendMessageW(EM_SETSEL, 0, 0);
	editSub.SendMessageW(EM_SCROLLCARET, 0, 0);

	::EnableWindow(GetDlgItem(IMAGE_BUTTON), clipList.TopObj() ? TRUE : FALSE);

	if (!ret) MessageBox("Can't decode images.");

	return	ret;
}


BOOL TRecvDlg::TcpEvent(SOCKET sd, LPARAM lParam)
{
	if (fileObj == NULL || fileObj->conInfo == NULL)
		return	FALSE;

	switch (LOWORD(lParam)) {
	case FD_CONNECT:	// connect done
		StartRecvFile();
		break;

	case FD_CLOSE:
		EndRecvFile();
		break;
	}
	return	TRUE;
}

BOOL TRecvDlg::ConnectRecvFile(void)
{
	memset(fileObj->conInfo, 0, sizeof(ConnectInfo));
	fileObj->conInfo->addr = msg.hostSub.addr;
	fileObj->conInfo->port = msg.hostSub.portNo;

	if (!msgMng->Connect(hWnd, fileObj->conInfo))
		return	FALSE;

	if (fileObj->conInfo->complete)
		StartRecvFile();

	return	TRUE;
}

#define	OFFSET 0

BOOL TRecvDlg::StartRecvFile(void)
{
	char	buf[MAX_PATH_U8], tcpbuf[MAX_BUF];
	int		command	= fileObj->isDir ? IPMSG_GETDIRFILES : IPMSG_GETFILEDATA;
	int		opt		= (msg.command & (IPMSG_UTF8OPT|IPMSG_CAPUTF8OPT));

	wsprintf(buf, fileObj->isDir ? "%x:%x:" : "%x:%x:%x:", msg.packetNo, fileObj->fileInfo->Id(), OFFSET);
	fileObj->conInfo->complete = TRUE;
	msgMng->MakeMsg(tcpbuf, command | opt, buf);
	msgMng->ConnectDone(hWnd, fileObj->conInfo);

//fileObj->offset = fileObj->woffset = OFFSET;

	if (::send(fileObj->conInfo->sd, tcpbuf, strlen(tcpbuf), 0) < (int)strlen(tcpbuf))
		return	EndRecvFile(), FALSE;

	fileObj->conInfo->startTick = fileObj->conInfo->lastTick = ::GetTickCount();

	if (!fileObj->isDir)
		fileObj->curFileInfo = *fileObj->fileInfo;

#define MAX_CLIPBOARD (10 * 1024 * 1024) // 10MB
	if (fileObj->isClip) {
		_int64	size = fileObj->curFileInfo.Size();
		ClipBuf	*clipBuf = NULL;

		if (size <= MAX_CLIPBOARD && size > 0) {
			clipBuf = new ClipBuf((int)size, fileObj->curFileInfo.Pos());
			if (clipBuf && clipBuf->vbuf.Buf()) {
				clipList.AddObj(clipBuf);
			}
		}
		if (!clipBuf || !clipBuf->vbuf.Buf()) {
			if (clipBuf) delete clipBuf;
			return	EndRecvFile(), FALSE;
		}
		fileObj->recvBuf = (char *)clipBuf->vbuf.Buf();
	}
	else {
		fileObj->recvBuf = new char [cfg->IoBufMax];
	}

	// 0byte file だけは、特例
	if (!fileObj->isDir && fileObj->curFileInfo.Size() == 0)
	{
		if (OpenRecvFile())
		{
			CloseRecvFile(TRUE);
			fileObj->status = FS_COMPLETE;
		}
		else fileObj->status = FS_ERROR;

		PostMessage(WM_TCPEVENT, fileObj->conInfo->sd, FD_CLOSE);
		return	TRUE;
	}

	DWORD	id;	// 使わず（95系で error になるのを防ぐだけ）
	fileObj->hThread = (HANDLE)~0;	// 微妙な領域を避ける
	// thread 内では MT 対応が必要な crt は使わず
	if ((fileObj->hThread = ::CreateThread(NULL, 0, RecvFileThread, this, 0, &id)) == NULL)
	{
		EndRecvFile();
		return	FALSE;
	}

	return	TRUE;
}

DWORD WINAPI TRecvDlg::RecvFileThread(void *_recvDlg)
{
	TRecvDlg	*recvDlg = (TRecvDlg *)_recvDlg;
	RecvFileObj	*fileObj = recvDlg->fileObj;
	fd_set		rfd;
	timeval		tv;
	int			sock_ret;
	BOOL		(TRecvDlg::*RecvFileFunc)(void) =
				fileObj->isDir ? &TRecvDlg::RecvDirFile : &TRecvDlg::RecvFile;

	FD_ZERO(&rfd);
	FD_SET(fileObj->conInfo->sd, &rfd);

	for (int waitCnt=0; waitCnt < 120 && fileObj->hThread; waitCnt++)
	{
		tv.tv_sec = 1, tv.tv_usec = 0;
		if ((sock_ret = ::select(fileObj->conInfo->sd + 1, &rfd, NULL, NULL, &tv)) > 0)
		{
			waitCnt = 0;
			if (!(recvDlg->*RecvFileFunc)())
				break;
			if (fileObj->status == FS_COMPLETE)
				break;
		}
		else if (sock_ret == 0) {
			FD_ZERO(&rfd);
			FD_SET(fileObj->conInfo->sd, &rfd);
			fileObj->conInfo->lastTick = ::GetTickCount();
			recvDlg->PostMessage(WM_RECVDLG_FILEBUTTON, 0, 0);
		}
		else if (sock_ret == SOCKET_ERROR) {
			break;
		}
	}
	recvDlg->CloseRecvFile(fileObj->status == FS_COMPLETE ? TRUE : FALSE);
	if (fileObj->status != FS_COMPLETE)
		fileObj->status = FS_ERROR;

	recvDlg->PostMessage(WM_TCPEVENT, fileObj->conInfo->sd, FD_CLOSE);
	::ExitThread(0);
	return	0;
}

BOOL TRecvDlg::CloseRecvFile(BOOL setAttr)
{
	if (fileObj->isClip) {
		;
	}
	else if (fileObj->hFile != INVALID_HANDLE_VALUE)
	{
		if (setAttr)
		{
			FILETIME	ft;
			UnixTime2FileTime(fileObj->curFileInfo.Mtime(), &ft);
#if 1	// 暫定処置（protocol format 変更の可能性）
			if (fileObj->isDir || (fileObj->curFileInfo.Mtime() & 0xffffff00))
#endif
				::SetFileTime(fileObj->hFile, NULL, &ft, &ft);
		}
		fileObj->totalTrans += fileObj->offset;
		fileObj->totalFiles++;
		fileObj->offset = fileObj->woffset = 0;

		::CloseHandle(fileObj->hFile);
		fileObj->hFile = INVALID_HANDLE_VALUE;
	}
	return	TRUE;
}

BOOL TRecvDlg::DecodeDirEntry(char *buf, FileInfo *info, char *u8fname)
{
	char	*tok, *ptr, *p;

	ConvertShareMsgEscape(buf);	// "::" -> ';'

	if ((tok = separate_token(buf, ':', &p)) == NULL)	// header size
		return	FALSE;

	if ((tok = separate_token(NULL, ':', &p)) == NULL)	// fname
		return	FALSE;

	if (msg.command & IPMSG_UTF8OPT) {
		strncpyz(u8fname, tok, MAX_PATH_U8);
	}
	else {
		WCHAR	wbuf[MAX_PATH];
		AtoW(tok, wbuf, MAX_PATH);
		WtoU8(wbuf, u8fname, MAX_PATH_U8);
	}

	info->SetFname(u8fname);
	while ((ptr = strchr(tok, '?')))	// UNICODE までの暫定
		*ptr = '_';

	if (strlen(tok) >= MAX_PATH_U8)
		return	FALSE;

	if ((tok = separate_token(NULL, ':', &p)) == NULL)	// size
		return	FALSE;
	info->SetSize(hex2ll(tok));

	if ((tok = separate_token(NULL, ':', &p)) == NULL)	// attr
		return	FALSE;
	info->SetAttr(strtoul(tok, 0, 16));

	while ((tok = separate_token(NULL, ':', &p)))	// exattr
	{
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
#define BIG_ALLOC	50
#define PEEK_SIZE	8

	if (fileObj->status == FS_DIRFILESTART || fileObj->status == FS_TRANSINFO)
	{
		int		size;
		if (fileObj->infoLen == 0)
		{
			if ((size = ::recv(fileObj->conInfo->sd, fileObj->info + (int)fileObj->offset, PEEK_SIZE - (int)fileObj->offset, 0)) <= 0)
				return	FALSE;
			if ((fileObj->offset += size) < PEEK_SIZE)
				return	TRUE;
			fileObj->info[fileObj->offset] = 0;
			if ((fileObj->infoLen = strtoul(fileObj->info, 0, 16)) >= sizeof(fileObj->info) -1 || fileObj->infoLen <= 0)
				return	FALSE;	// too big or small
		}
		if (fileObj->offset < fileObj->infoLen)
		{
			if ((size = ::recv(fileObj->conInfo->sd, fileObj->info + (int)fileObj->offset, fileObj->infoLen - (int)fileObj->offset, 0)) <= 0)
				return	FALSE;
			fileObj->offset += size;
		}
		if (fileObj->offset == fileObj->infoLen)
		{
			fileObj->info[fileObj->infoLen] = 0;
			if (!DecodeDirEntry(fileObj->info, &fileObj->curFileInfo, fileObj->u8fname))
				return	FALSE;	// Illegal entry
			fileObj->offset = fileObj->infoLen = 0;	// 初期化

			if (GET_MODE(fileObj->curFileInfo.Attr()) == IPMSG_FILE_DIR)
			{
				char	buf[MAX_BUF];
				const char *fname = fileObj->dirCnt == 0 ? fileObj->fileInfo->Fname() : fileObj->curFileInfo.Fname();

				if (MakePath(buf, fileObj->path, fname) >= MAX_PATH_U8)
					return	MessageBoxU8(buf, GetLoadStrU8(IDS_PATHTOOLONG)), FALSE;
				if (!IsSafePath(buf, fname))
					return	FALSE;

				if (!CreateDirectoryU8(buf, NULL))
					return	FALSE;
				strncpyz(fileObj->path, buf, MAX_PATH_U8);
				fileObj->dirCnt++;
			}
			else if (GET_MODE(fileObj->curFileInfo.Attr()) == IPMSG_FILE_RETPARENT)
			{
				if (fileObj->curFileInfo.Mtime())	// directory の time stamp をあわせる(NT系のみ)
				{
					FILETIME	ft;
					HANDLE		hFile;
					UnixTime2FileTime(fileObj->curFileInfo.Mtime(), &ft);
					if ((hFile = CreateFileU8(fileObj->path, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0)) != INVALID_HANDLE_VALUE)
					{
						::SetFileTime(hFile, NULL, NULL, &ft);
						::CloseHandle(hFile);
					}
				}
				if (fileObj->curFileInfo.Attr() & IPMSG_FILE_RONLYOPT)
				SetFileAttributesU8(fileObj->path, FILE_ATTRIBUTE_READONLY);
				if (--fileObj->dirCnt <= 0)
				{
					fileObj->status = FS_COMPLETE;
					return	TRUE;
				}
				if (!PathToDir(fileObj->path, fileObj->path)) return	FALSE;
			}
			else {
				if (fileObj->dirCnt == 0)
					return	FALSE;
				
				if (fileObj->curFileInfo.Size() == 0)	// 0byte file
				{
					if (OpenRecvFile())		// 0byteの場合は作成失敗を無視
						CloseRecvFile(TRUE);
				}
				fileObj->status = fileObj->curFileInfo.Size() ? FS_TRANSFILE : FS_TRANSINFO;
			}
			return	TRUE;
		}
	}

	if (fileObj->status == FS_TRANSFILE)
	{
		if (!RecvFile())
		{
			CloseRecvFile();
			return	FALSE;
		}
		if (fileObj->status == FS_ENDFILE)
		{
			CloseRecvFile(TRUE);
			fileObj->status = FS_TRANSINFO;
		}
	}

	return	TRUE;
}

BOOL TRecvDlg::OpenRecvFile(void)
{
	char	path[MAX_BUF];

	if (MakePath(path, fileObj->isDir ? fileObj->path : fileObj->saveDir,
				fileObj->curFileInfo.Fname()) >= MAX_PATH_U8)
		return	MessageBoxU8(path, GetLoadStrU8(IDS_PATHTOOLONG)), FALSE;
	if (!IsSafePath(path, fileObj->curFileInfo.Fname()))
		return	MessageBoxU8(path, GetLoadStrU8(IDS_NOTSAFEPATH)), FALSE;

	if ((fileObj->hFile = CreateFileU8(path, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
			0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) == INVALID_HANDLE_VALUE)
		return	fileObj->isDir ? FALSE : (MessageBoxU8(GetLoadStrU8(IDS_CREATEFAIL), path), FALSE);

	if (fileObj->curFileInfo.Attr() & IPMSG_FILE_RONLYOPT)
		SetFileAttributesU8(path, FILE_ATTRIBUTE_READONLY);

//::SetFilePointer(fileObj->hFile, OFFSET, 0, FILE_BEGIN);
//::SetEndOfFile(fileObj->hFile);

	return	TRUE;
}

BOOL TRecvDlg::RecvFile(void)
{
	int		wresid = (int)(fileObj->offset - fileObj->woffset);
	_int64	remain = fileObj->curFileInfo.Size() - fileObj->offset;
	int		size = 0;

	if (!fileObj->isClip) {
		if (remain > cfg->IoBufMax - wresid)
			remain = cfg->IoBufMax - wresid;
	}

	if ((size = ::recv(fileObj->conInfo->sd, fileObj->recvBuf + wresid, (int)remain, 0)) <= 0)
		return	FALSE;

	if (!fileObj->isClip && fileObj->hFile == INVALID_HANDLE_VALUE) {
		if (!OpenRecvFile())
			return	FALSE;
	}

	wresid += size;
	if (!fileObj->isClip &&
		(fileObj->offset + size >= fileObj->curFileInfo.Size() || cfg->IoBufMax == wresid)) {
		DWORD	wsize;
		if (!::WriteFile(fileObj->hFile, fileObj->recvBuf, wresid, &wsize, 0)
			|| wresid != (int)wsize)
			return	MessageBoxU8(GetLoadStrU8(IDS_WRITEFAIL)), FALSE;
		fileObj->woffset += wresid;
	}
	fileObj->offset += size;

	DWORD	nowTick = ::GetTickCount();

	if (nowTick - fileObj->conInfo->lastTick >= 1000) {
		fileObj->conInfo->lastTick = nowTick;
		PostMessage(WM_RECVDLG_FILEBUTTON, 0, 0);
	}

	if (fileObj->offset >= fileObj->curFileInfo.Size()) {
		fileObj->status = fileObj->isDir ? FS_ENDFILE : FS_COMPLETE;
	}

	return	TRUE;
}

int MakeTransRateStr(char *buf, DWORD ticks, _int64 cur_size, _int64 total_size)
{
	int len = 0;
	buf[len++] = ' ';
	len += MakeSizeString(buf + len, cur_size) -2;	// "MB"部分を抜く
	buf[len++] = '/';
	len += MakeSizeString(buf + len, total_size);
	buf[len++] = ' ';
	len += MakeSizeString(buf + len, cur_size * 1000 / (ticks ? ticks : 10));
	return len + wsprintf(buf + len, "/s (%d%%)", (int)(cur_size * 100 / (total_size ? total_size : 1)));
}

int MakeDirTransRateStr(char *buf, DWORD ticks, _int64 cur_size, int files)
{
	int len = 0;
	buf[len++] = ' ';
	len += wsprintf(buf + len, "Total ");
	len += MakeSizeString(buf + len, cur_size);
	len += wsprintf(buf + len, "/%dfiles (", files);
	len += MakeSizeString(buf + len, cur_size * 1000 / (ticks ? ticks : 1));
	return	len + wsprintf(buf + len, "/s)" );
}

class RecvTransEndDlg : public TDlg {
	RecvFileObj	*fileObj;

public:
	RecvTransEndDlg(RecvFileObj *_fileObj, TWin *_win) : TDlg(_fileObj->status == FS_COMPLETE ? TRANSEND_DIALOG : SUSPEND_DIALOG, _win) { fileObj = _fileObj; }
	virtual BOOL	EvCreate(LPARAM lParam);
	virtual BOOL	EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl);
};

BOOL RecvTransEndDlg::EvCreate(LPARAM lParam)
{
	char	buf[MAX_BUF];
	DWORD	difftick = fileObj->lastTick - fileObj->startTick;
	int		len = 0;

	len += wsprintf(buf + len, "Total: ");
	len += MakeSizeString(buf + len, fileObj->totalTrans);
	len += wsprintf(buf + len, " (");
	len += MakeSizeString(buf + len, fileObj->totalTrans * 1000 / (difftick ? difftick : 1));
	len += wsprintf(buf + len, "/s)\r\n%d", difftick/1000);
	if (difftick/1000 < 20)
		len += wsprintf(buf + len, ".%02d", (difftick%1000) / 10);
	len += wsprintf(buf + len, " sec   ");
	if (fileObj->totalFiles > 1 || fileObj->isDir)
		len += wsprintf(buf + len, "%d files", fileObj->totalFiles);
	else
		len += wsprintf(buf + len, "%s", fileObj->fileInfo->Fname());

	if (fileObj->status == FS_COMPLETE)
	{
		if (fileObj->totalFiles > 1 || fileObj->isDir)
			::EnableWindow(GetDlgItem(EXEC_BUTTON), FALSE);
	}

	SetDlgItemTextU8(FILE_STATIC, buf);

	GetWindowRect(&rect);
	MoveWindow(rect.left + 30, rect.top + 30, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	return	TRUE;
}

BOOL RecvTransEndDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	if (fileObj->status == FS_COMPLETE && wID == IDCANCEL)
		wID = IDOK;

	switch (wID)
	{
	case FOLDER_BUTTON: case EXEC_BUTTON: case IDCANCEL: case IDOK: case IDRETRY:
		EndDialog(wID);
		return	TRUE;
	}

	return	FALSE;
}

BOOL TRecvDlg::EndRecvFile(BOOL manual_suspend)
{
	if (fileObj->hThread)
	{
		HANDLE	hThread = fileObj->hThread;
		fileObj->hThread = 0;	// 中断の合図
		WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
	}

	fileObj->lastTick = fileObj->conInfo->lastTick = ::GetTickCount();
	SetTransferButtonText();

	int			target = ShareMng::GetFileInfoNo(shareInfo, fileObj->fileInfo);
	FileInfo	*fileInfo = fileObj->fileInfo;
	BOOL		isSingleTrans = fileObj->startTick == fileObj->conInfo->startTick;
	ClipBuf		*clipBuf = NULL;
	BOOL		isInsertImage = FALSE;

	::closesocket(fileObj->conInfo->sd);

	if (fileObj->isClip) {
		for (clipBuf=(ClipBuf*)clipList.TopObj(); clipBuf;
				clipBuf=(ClipBuf*)clipList.NextObj(clipBuf)) {
			if (fileObj->recvBuf == (char *)clipBuf->vbuf.Buf()) break;
		}
	}
	else {
		delete [] fileObj->recvBuf;
	}
	fileObj->recvBuf = NULL;
	delete fileObj->conInfo;
	fileObj->conInfo = NULL;

	if (fileObj->status == FS_COMPLETE)
	{
		if (fileObj->isClip && clipBuf) {
			clipBuf->finished = TRUE;
			if (cfg->LogCheck && (cfg->ClipMode & CLIP_SAVE)) {
				SaveImageFile(cfg, fileObj->fileInfo->Fname(), &clipBuf->vbuf);
			}
		}
		BOOL	is_clip_finish = TRUE;
		for (int cnt=0; cnt < shareInfo->fileCnt; cnt++) {
			if (shareInfo->fileInfo[cnt] != fileInfo) {
				if (shareInfo->fileInfo[cnt]->IsSelected()) {
					FreeDecodeShareMsgFile(shareInfo, target);
					return	SaveFile();
				}
				if (GET_MODE(shareInfo->fileInfo[cnt]->Attr()) == IPMSG_FILE_CLIPBOARD) {
					is_clip_finish = FALSE;
				}
			}
		}
		if (fileObj->isClip && is_clip_finish) {
			if ((cfg->ClipMode & CLIP_CONFIRM) == 0 && status == SHOW && openFlg) {
				isInsertImage = TRUE;
			}
		}
	}

	int ret;

	if (manual_suspend)			ret = IDCANCEL;
	else if (fileObj->isClip)	ret = IDOK;
	else						ret = RecvTransEndDlg(fileObj, this).Exec();

	if (ret == EXEC_BUTTON || ret == FOLDER_BUTTON && fileObj->isDir && isSingleTrans)
	{
		char	buf[MAX_BUF];
		MakePath(buf, fileObj->saveDir, fileInfo->Fname());
		ShellExecuteU8(NULL, NULL, buf, 0, 0, SW_SHOW);
	}
	else if (ret == FOLDER_BUTTON)
		ShellExecuteU8(NULL, NULL, fileObj->saveDir, 0, 0, SW_SHOW);

	if (ret == IDOK || ret == FOLDER_BUTTON || ret == EXEC_BUTTON)
		FreeDecodeShareMsgFile(shareInfo, target);

	SetFileButton(this, FILE_BUTTON, shareInfo);
	EvSize(SIZE_RESTORED, 0, 0);

	if (isInsertImage)	InsertImages(); // display and remove

	if (ret == IDRETRY)
		PostMessage(WM_COMMAND, FILE_BUTTON, 0);

	return	TRUE;
}

void TRecvDlg::SetTransferButtonText(void)
{
	char	buf[MAX_LISTBUF];

	if (fileObj->conInfo == NULL)
		return;
	if (fileObj->isDir)
		MakeDirTransRateStr(buf, fileObj->conInfo->lastTick - fileObj->startTick, fileObj->totalTrans + fileObj->offset, fileObj->totalFiles);
	else
		MakeTransRateStr(buf, fileObj->conInfo->lastTick - fileObj->conInfo->startTick, fileObj->status < FS_COMPLETE ? fileObj->offset : fileObj->curFileInfo.Size(), fileObj->curFileInfo.Size());
	SetDlgItemTextU8(FILE_BUTTON, buf);
}

