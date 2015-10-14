static char *share_id = 
	"@(#)Copyright (C) H.Shirouzu 2002-2015   share.cpp	Ver3.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: File Share
	Create					: 2002-04-14(Sun)
	Update					: 2015-05-03(Sun)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include <stddef.h>

#define BIG_ALLOC 100

/*
	公開ファイル管理
*/
ShareMng::ShareMng(Cfg *_cfg, MsgMng *_msgMng)
{
	cfg     = _cfg;
	msgMng  = _msgMng;
	statDlg = NULL;
}

ShareInfo *ShareMng::CreateShare(int packetNo)
{
	if (Search(packetNo))
		return	FALSE;

	ShareInfo *info = new ShareInfo(packetNo);
	AddObj(info);

	return	info;
}

BOOL ShareMng::AddShareCore(ShareInfo *shareInfo, FileInfo	*fileInfo)
{
	if (!fileInfo) return FALSE;

	if ((shareInfo->fileCnt % BIG_ALLOC) == 0)
		shareInfo->fileInfo = (FileInfo **)realloc(shareInfo->fileInfo, (shareInfo->fileCnt + BIG_ALLOC) * sizeof(FileInfo *));
	shareInfo->fileInfo[shareInfo->fileCnt] = fileInfo;
	shareInfo->fileCnt++;

	return	TRUE;
}

BOOL ShareMng::AddFileShare(ShareInfo *shareInfo, char *fname)
{
	for (int i=0; i < shareInfo->fileCnt; i++) {
		if (strcmp(fname, shareInfo->fileInfo[i]->Fname()) == 0)
			return	FALSE;
	}
	return	AddShareCore(shareInfo, SetFileInfo(fname));
}

BOOL ShareMng::AddMemShare(ShareInfo *shareInfo, char *dummy_name, BYTE *data, int size, int pos)
{
	FileInfo	*info = new FileInfo;
	if (!info) return FALSE;

	info->SetAttr(IPMSG_FILE_CLIPBOARD);
	info->SetFname(dummy_name);
	info->SetMemData(data, size);
	info->SetMtime(Time());
	info->SetPos(pos);

	return	AddShareCore(shareInfo, info);
}

BOOL ShareMng::DelFileShare(ShareInfo *info, int fileNo)
{
	if (fileNo >= info->fileCnt)
		return	FALSE;
	memmove(info->fileInfo + fileNo, info->fileInfo + fileNo +1, (--info->fileCnt - fileNo) * sizeof(FileInfo *));

	statDlg->Refresh();

	return	TRUE;
}

FileInfo *ShareMng::SetFileInfo(char *fname)
{
	WIN32_FIND_DATA_U8	fdat;

	if (!GetFileInfomationU8(fname, &fdat))	return	FALSE;

	FileInfo	*info = new FileInfo;

	UINT	attr = (fdat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? IPMSG_FILE_DIR : IPMSG_FILE_REGULAR;
	attr |= (fdat.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? IPMSG_FILE_RONLYOPT : 0;
	attr |= (fdat.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? IPMSG_FILE_SYSTEMOPT : 0;
	info->SetAttr(attr);
	info->SetFname(fname);
	if (GET_MODE(info->Attr()) == IPMSG_FILE_DIR)
	{
		info->SetSize(0);
		strncpyz(cfg->lastOpenDir, fname, MAX_PATH_U8);
	}
	else {
		info->SetSize((_int64)fdat.nFileSizeHigh << 32 | fdat.nFileSizeLow);
		GetParentDirU8(fname, cfg->lastOpenDir);
	}
	info->SetMtime(FileTime2UnixTime(&fdat.ftLastWriteTime));
	info->SetCrtime(FileTime2UnixTime(&fdat.ftCreationTime));
	info->SetAtime(FileTime2UnixTime(&fdat.ftLastAccessTime));

	return	info;
}

BOOL ShareMng::AddHostShare(ShareInfo *info, SendEntry *entry, int entryNum)
{
	info->host = new Host *[info->hostCnt = entryNum];
	info->transStat = new char [info->hostCnt * info->fileCnt];
	memset(info->transStat, TRANS_INIT, info->hostCnt * info->fileCnt);

	for (int i=0; i < entryNum; i++)
	{
		info->host[i] = cfg->fileHosts.GetHostByNameAddr(&entry[i].Host()->hostSub);
		if (info->host[i] == NULL)
		{
			info->host[i] = new Host;
			info->host[i]->hostSub = entry[i].Host()->hostSub;
			info->host[i]->hostStatus = entry[i].Host()->hostStatus;
			info->host[i]->updateTime = entry[i].Host()->updateTime;
			info->host[i]->priority = entry[i].Host()->priority;
			strncpyz(info->host[i]->nickName, entry[i].Host()->nickName, MAX_NAMEBUF);
			cfg->fileHosts.AddHost(info->host[i]);
		}
		else info->host[i]->RefCnt(1);
	}

	SYSTEMTIME	st;
	::GetSystemTime(&st);
	::SystemTimeToFileTime(&st, &info->attachTime);

	statDlg->Refresh();

	return	TRUE;
}

int ShareMng::GetFileInfoNo(ShareInfo *info, FileInfo *fileInfo)
{
	for (int target=0; info->fileCnt; target++)
		if (info->fileInfo[target] == fileInfo)
			return	target;
	return	-1;
}

BOOL ShareMng::EndHostShare(int packetNo, HostSub *hostSub, FileInfo *fileInfo, BOOL done)
{
	ShareInfo *info = Search(packetNo);

	if (info == NULL)
		return	FALSE;

	for (int i=0; i < info->hostCnt; i++)
	{
		if (IsSameHostEx(&info->host[i]->hostSub, hostSub))
		{
			if (fileInfo)
			{
				info->transStat[info->fileCnt * i + GetFileInfoNo(info, fileInfo)] =
														done ? TRANS_DONE : TRANS_INIT;
				if (!done)
					return	statDlg->Refresh(), TRUE;
				for (int j=0; j < info->fileCnt; j++)
					if (info->transStat[info->fileCnt * i + j] != TRANS_DONE)
						return	statDlg->Refresh(), TRUE;
			}
			if (info->host[i]->RefCnt(-1) == 0)
			{
				cfg->fileHosts.DelHost(info->host[i]);
				delete info->host[i];
			}
			memmove(info->host + i, info->host + i + 1, (--info->hostCnt - i) * sizeof(Host *));
			memmove(info->transStat + info->fileCnt * i, info->transStat + info->fileCnt * (i + 1), (info->hostCnt - i) * info->fileCnt);
			if (info->hostCnt == 0)
				DestroyShare(info);
			return	statDlg->Refresh(), TRUE;
		}
	}
	return	FALSE;
}

void ShareMng::DestroyShare(ShareInfo *info)
{
	info->next->prev = info->prev;
	info->prev->next = info->next;

	while (info->hostCnt-- > 0)
	{
		if (info->host[info->hostCnt]->RefCnt(-1) == 0)
		{
			cfg->fileHosts.DelHost(info->host[info->hostCnt]);
			delete info->host[info->hostCnt];
		}
	}
	delete [] info->host;
	delete [] info->transStat;

	while (info->fileCnt-- > 0)
		delete info->fileInfo[info->fileCnt];
	free(info->fileInfo);
	statDlg->Refresh();
}

ShareInfo *ShareMng::Search(int packetNo)
{
	for (ShareInfo *info=TopObj(); info; info=NextObj(info))
		if (info->packetNo == packetNo)
			return	info;
	return	NULL;
}

BOOL ShareMng::GetShareCntInfo(ShareCntInfo *cntInfo, ShareInfo *shareInfo)
{

	memset(cntInfo, 0, sizeof(ShareCntInfo));

	for (ShareInfo *info = shareInfo ? shareInfo : TopObj(); info; info=NextObj(info))
	{
		if (info->hostCnt)
		{
			cntInfo->packetCnt++;
			cntInfo->hostCnt += info->hostCnt;
			int i;
			for (i=info->fileCnt * info->hostCnt -1; i >= 0; i--)
			{
				cntInfo->fileCnt++;
				switch (info->transStat[i]) {
				case TRANS_INIT: break;
				case TRANS_BUSY: cntInfo->transferCnt++;	break;
				case TRANS_DONE: cntInfo->doneCnt++;		break;
				}
			}
			for (i=0; i < info->fileCnt; i++)
			{
				if (GET_MODE(info->fileInfo[i]->Attr()) == IPMSG_FILE_DIR)
					cntInfo->dirCnt++;
				cntInfo->totalSize += info->fileInfo[i]->Size();
			}
		}
		if (shareInfo)
			return	TRUE;
	}
	return	TRUE;
}


BOOL ShareMng::GetAcceptableFileInfo(ConnectInfo *info, char *buf, int size,
	AcceptFileInfo *fileInfo)
{
	MsgBuf	msg;
	UINT	&logOpt   = fileInfo->logOpt;
	UINT	cryptCapa = 0;

	logOpt = 0;

	if (!msgMng->ResolveMsg(buf, size, &msg)) return FALSE;

	if (msg.command & IPMSG_ENCRYPTOPT) {
		if (!msgMng->DecryptMsg(&msg, &cryptCapa, &logOpt)) return FALSE;
	}

	char		*tok, *p;
	int			targetID;
	ShareInfo	*shareInfo;

	if ((tok = separate_token(msg.msgBuf, ':', &p)) == NULL)
		return	FALSE;
	fileInfo->packetNo   = strtol(tok, 0, 16);
	fileInfo->ivPacketNo = msg.packetNo;
	fileInfo->command    = msg.command;

	if ((tok = separate_token(NULL, ':', &p)) == NULL)
		return	FALSE;
	targetID = strtol(tok, 0, 16);

	if (GET_MODE(fileInfo->command) == IPMSG_GETFILEDATA)
	{
		if ((tok = separate_token(NULL, ':', &p)) == NULL)
			return	FALSE;
		fileInfo->offset = hex2ll(tok);
	}
	else if (GET_MODE(fileInfo->command) == IPMSG_GETDIRFILES)
		fileInfo->offset = 0;
	else return	FALSE;

	if ((shareInfo = Search(fileInfo->packetNo)) == NULL)
		return	FALSE;

	int	host_cnt, file_cnt;

	for (host_cnt=0; host_cnt < shareInfo->hostCnt; host_cnt++)
	{
		if (IsSameHost(&shareInfo->host[host_cnt]->hostSub, &msg.hostSub))
		{
			if ((logOpt & LOG_SIGN_OK) || shareInfo->host[host_cnt]->hostSub.addr == info->addr) {
				fileInfo->host = shareInfo->host[host_cnt];

				if ((logOpt & LOG_SIGN_OK) && (GET_OPT(fileInfo->command) & IPMSG_ENCFILEOPT)) {
					int		len = 0;
					int		cap = 0;

					if ((tok = separate_token(NULL, ':', &p)) == NULL) return FALSE;
					cap = strtol(tok, 0, 16);
					if (cap != (IPMSG_AES_256|IPMSG_PACKETNO_IV)) return FALSE;

					if ((tok = separate_token(NULL, ':', &p)) == NULL) return FALSE;
					hexstr2bin(tok, fileInfo->aesKey, 32, &len);
					if (len != 32) return FALSE;
				}
				break;
			}
		}
	}
	if (host_cnt == shareInfo->hostCnt)
		return	FALSE;

	for (file_cnt=0; file_cnt < shareInfo->fileCnt; file_cnt++)
	{
		if (shareInfo->fileInfo[file_cnt]->Id() == targetID)
		{
			fileInfo->fileInfo = shareInfo->fileInfo[file_cnt];
			if (shareInfo->transStat[shareInfo->fileCnt * host_cnt + file_cnt] != TRANS_INIT)
				return	FALSE;	// download 済み（or 最中）

			// dir に対しては IPMSG_GETDIRFILES 以外は認めない
			if (GET_MODE(fileInfo->command) != IPMSG_GETDIRFILES &&
				GET_MODE(fileInfo->fileInfo->Attr()) == IPMSG_FILE_DIR)
				return	FALSE;	

			fileInfo->attachTime = shareInfo->attachTime;
			shareInfo->transStat[shareInfo->fileCnt * host_cnt + file_cnt] = TRANS_BUSY;
			statDlg->Refresh();
			return	TRUE;
		}
	}
	return	FALSE;
}

void ShareMng::Cleanup()
{
	int			i, j;
	Time_t		cur_time = 0;
	ShareInfo	*info, *next;

	for (info=TopObj(); info; info=next) {
		next = NextObj(info);
		if (!*(_int64 *)&info->attachTime) continue;
		if (!cur_time) cur_time = Time();

		int	clip_host = 0;
		for (i=0; i < info->fileCnt; i++) {
			for (j=0; j < info->hostCnt; j++) {
				if (info->transStat[info->fileCnt * j + i] == TRANS_BUSY) break;
				if (info->transStat[info->fileCnt * j + i] == TRANS_INIT) {
					if (GET_MODE(info->fileInfo[i]->Attr()) != IPMSG_FILE_CLIPBOARD) break;
					if (info->host[j]->hostStatus & IPMSG_CLIPBOARDOPT) {
						clip_host++;
					}
				}
			}
			if (j != info->hostCnt) break;
		}
		if (i == info->fileCnt && j == info->hostCnt) {
			if (clip_host > 0 && FileTime2UnixTime(&info->attachTime) + 1200 > cur_time) continue;
			DestroyShare(info);
		}
	}
}

/* ShareDlg */
TShareDlg::TShareDlg(ShareMng *_shareMng, ShareInfo *_shareInfo, Cfg *_cfg, TWin *_parent) : TDlg(FILE_DIALOG, _parent), shareListView(this)
{
	shareMng = _shareMng;
	shareInfo = _shareInfo;
	cfg = _cfg;
}

TShareDlg::~TShareDlg()
{
	shareListView.DeleteAllItems();
}

BOOL TShareDlg::EvCreate(LPARAM lParam)
{
 	shareListView.AttachWnd(GetDlgItem(FILE_LIST));

	char	*title[] = { GetLoadStrU8(IDS_FILENAME), GetLoadStrU8(IDS_SIZE), GetLoadStrU8(IDS_LOCATION), NULL };
	int		size[]   = { 120, 70, 180 };
	int		fmt[]    = { LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_LEFT };
	int		i;

	for (i=0; title[i]; i++) {
		shareListView.InsertColumn(i, title[i], size[i], fmt[i]);
	}

	for (i=0; i < shareInfo->fileCnt; i++) {
		AddList(i);
	}

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

	Show();
	::SetFocus(shareListView.hWnd);
	return	TRUE;
}

BOOL TShareDlg::AddList(int idx)
{
	char	buf[MAX_BUF_EX];

	ForcePathToFname(shareInfo->fileInfo[idx]->Fname(), buf);
	shareListView.InsertItem(idx, buf);

	if (GET_MODE(shareInfo->fileInfo[idx]->Attr()) == IPMSG_FILE_DIR)
		strcpy(buf, "(DIR)");
	else
		MakeSizeString(buf, shareInfo->fileInfo[idx]->Size(), MSS_SPACE);
	shareListView.SetSubItem(idx, 1, buf);

	GetParentDirU8(shareInfo->fileInfo[idx]->Fname(), buf);
	shareListView.SetSubItem(idx, 2, buf);

	return	TRUE;
}

BOOL TShareDlg::DelList(int idx)
{
	shareListView.DeleteItem(idx);
	shareMng->DelFileShare(shareInfo, idx);
	return	TRUE;
}

BOOL TShareDlg::EvDropFiles(HDROP hDrop)
{
	int	lastFileCnt = shareInfo->fileCnt;
	parent->EvDropFiles(hDrop);
	while (lastFileCnt < shareInfo->fileCnt)
		AddList(lastFileCnt++);
	return	TRUE;
}

BOOL TShareDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:		EndDialog(wID); break;
	case IDCANCEL:	EndDialog(wID); break;

	case FILE_BUTTON:
		{
			int	i = shareInfo->fileCnt;
			if (FileAddDlg(this, shareMng, shareInfo, cfg))
				for (i; i < shareInfo->fileCnt; i++)
					AddList(i);
		}
		break;

	case FOLDER_BUTTON:
		if (BrowseDirDlg(this, GetLoadStrU8(IDS_FOLDERATTACH), cfg->lastOpenDir, cfg->lastOpenDir))
		{
			if (shareMng->AddFileShare(shareInfo, cfg->lastOpenDir))
			{
				AddList(shareInfo->fileCnt -1);
				cfg->WriteRegistry(CFG_GENERAL);
			}
		}
		break;

	case DEL_BUTTON:
		{
			for (int i=shareInfo->fileCnt-1; i >= 0; i--)
			{
				if (!shareListView.IsSelected(i)) continue;

				DelList(i);
			}
		}
		break;

	default: break;
	}
	return	TRUE;
}

BOOL TShareDlg::FileAddDlg(TDlg *dlg, ShareMng *shareMng, ShareInfo *shareInfo, Cfg *cfg)
{
	int		bufsize = MAX_MULTI_PATH;
	U8str	buf(bufsize);
	U8str	path(bufsize);

	OpenFileDlg	ofdlg(dlg, OpenFileDlg::MULTI_OPEN);

	if (!ofdlg.Exec(buf.Buf(), bufsize, GetLoadStrU8(IDS_ADDFILE),
			GetLoadStrAsFilterU8(IDS_OPENFILEALLFLTR), cfg->lastOpenDir)) {
		return	FALSE;
	}

	cfg->WriteRegistry(CFG_GENERAL);

	int		dirlen = (int)strlen(cfg->lastOpenDir);
	if (buf[dirlen]) {
		return	shareMng->AddFileShare(shareInfo, buf.Buf());
	}

	for (const char *fname = buf.s() + dirlen + 1; *fname; fname += strlen(fname) + 1)
	{
		if (MakePath(path.Buf(), buf.s(), fname) >= MAX_PATH_U8)
			continue;
		shareMng->AddFileShare(shareInfo, path.Buf());
	}
	return	TRUE;
}

/* TShareStatDlg */
TShareStatDlg::TShareStatDlg(ShareMng *_shareMng, Cfg *_cfg, TWin *_parent) : TDlg(SHARE_DIALOG, _parent), shareListView(this)
{
	shareMng = _shareMng;
	cfg = _cfg;
	shareMng->RegisterShareStatDlg(this);
}

TShareStatDlg::~TShareStatDlg()
{
	shareListView.DeleteAllItems();
}

BOOL TShareStatDlg::EvCreate(LPARAM lParam)
{
	char	*title[] = { "No", "Files", "Size", "All/done/trans", "Users", NULL };
	int		size[]   = { 30, 110, 70, 100, 100 };
	int		fmt[]    = { LVCFMT_RIGHT, LVCFMT_LEFT, LVCFMT_RIGHT, LVCFMT_RIGHT, LVCFMT_LEFT };

 	shareListView.AttachWnd(GetDlgItem(SHARE_LIST));

	for (int i=0; title[i]; i++) {
		shareListView.InsertColumn(i, title[i], size[i], fmt[i]);
	}

	SetAllList();

	CheckDlgButton(MODIFY_CHECK, (cfg->fileTransOpt & FT_STRICTDATE) ? 1 : 0);

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
	Show();
	::SetFocus(shareListView.hWnd);

	return	TRUE;
}

BOOL TShareStatDlg::Refresh(void)
{
	shareMng->Cleanup();

	if (hWnd == NULL)
		return	FALSE;
	SetAllList();
	return	TRUE;
}

BOOL TShareStatDlg::SetAllList(void)
{
	shareListView.DeleteAllItems();

	int		i=0, j=0, len=0;
	char	buf[MAX_BUF_EX];

	for (ShareInfo *info=shareMng->TopObj(); info; info=shareMng->NextObj(info))
	{
		if (info->hostCnt == 0)
			continue;

		sprintf(buf, "%d", i);
		shareListView.InsertItem(i, buf, (LPARAM)info);

		len = 0;
		*buf = 0;
		for (j=0; j < info->fileCnt && len < sizeof(buf); j++) {
			ForcePathToFname(info->fileInfo[j]->Fname(), buf + len);
			strcat(buf + len, " ");
			len += (int)strlen(buf + len);
			if (len + MAX_PATH_U8 >= sizeof(buf)) break;
		}
		shareListView.SetSubItem(i, 1, buf);

		ShareCntInfo	cntInfo;
		shareMng->GetShareCntInfo(&cntInfo, info);
		MakeSizeString(buf, cntInfo.totalSize, MSS_SPACE);
		if (cntInfo.dirCnt) wsprintf(buf + strlen(buf), "/%dDIR", cntInfo.dirCnt);
		shareListView.SetSubItem(i, 2, buf);

		shareMng->GetShareCntInfo(&cntInfo, info);
		wsprintf(buf, "%d / %d/ %d", cntInfo.fileCnt, cntInfo.doneCnt, cntInfo.transferCnt);
		shareListView.SetSubItem(i, 3, buf);

		len = 0;
		*buf = 0;
		for (j=0; j < info->hostCnt && len + 100 < sizeof(buf); j++) {
			Host *host = info->host[j];
			len += _snprintf(buf + len, sizeof(buf)-len-1, "%s(%s) ",
							*host->nickName ? host->nickName : host->hostSub.userName,
							host->hostSub.hostName);
		}
		shareListView.SetSubItem(i, 4, buf);
		i++;
	}
	return	TRUE;
}

BOOL TShareStatDlg::DelList(int idx)
{
	ShareInfo	*info = (ShareInfo *)shareListView.GetItemParam(idx);
	if (info == NULL)
		return	FALSE;

	for (int i=info->fileCnt * info->hostCnt -1; i >= 0; i--) {
		if (info->transStat[i] == ShareMng::TRANS_BUSY)
			return	FALSE;
	}

	shareMng->DestroyShare(info);
	shareListView.DeleteItem(idx);

	return	TRUE;
}

BOOL TShareStatDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hWndCtl)
{
	switch (wID)
	{
	case IDOK:		EndDialog(wID); break;
	case IDCANCEL:	EndDialog(wID); break;
	case DEL_BUTTON:
		{
			for (int i=shareListView.GetItemCount()-1; i >= 0; i--)
			{
				if (!shareListView.IsSelected(i))	continue;
				DelList(i);
			}
			SetAllList();
		}
		break;
	case MODIFY_CHECK:
		if (SendDlgItemMessage(MODIFY_CHECK, BM_GETCHECK, 0, 0)) {
			cfg->fileTransOpt &= ~FT_STRICTDATE;
		} else {
			cfg->fileTransOpt |= FT_STRICTDATE;
		}
		cfg->WriteRegistry(CFG_GENERAL);
		break;
	default: break;
	}
	return	TRUE;
}

#define PRIOR_BUTTON	2000
#define NEXT_BUTTON		2001
#define LUMP_CHECK		2002
#define RESULT_STATIC	2003
#define ENCTRANS_CHECK	2004

TSaveCommonDlg::TSaveCommonDlg(ShareInfo *_shareInfo, Cfg *_cfg, ULONG _hostStatus,
	TWin *_parentWin) : TDlg((UINT)0, _parentWin)
{
	parentWin	= _parentWin;
	shareInfo	= _shareInfo;
	cfg			= _cfg;
	offset		= 0;
	hostStatus	= _hostStatus;

	for (int i=0; i < shareInfo->fileCnt; i++) {
		shareInfo->fileInfo[i]->SetSelected(FALSE);
	}
}

int TSaveCommonDlg::Exec(void)
{
	modalFlg = TRUE;

	char	fname[MAX_BUF], last_dir[MAX_BUF], buf[MAX_BUF], *ext;

	// 最終保存ディレクトリが無くなっている場合、少しさかのぼる
	for (int i=0; i < 5; i++) {
		if (*cfg->lastSaveDir && GetFileAttributesU8(cfg->lastSaveDir) == 0xffffffff)
			if (!GetParentDirU8(cfg->lastSaveDir, cfg->lastSaveDir))
				break;
	}

	strcpy(last_dir, *cfg->lastSaveDir ? cfg->lastSaveDir : ".");

	while (1) {
		FileInfo	*fileInfo = shareInfo->fileInfo[offset];

		MakePath(fname, last_dir, fileInfo->Fname());

	// ファイルダイアログ
		TApp::GetApp()->AddWin(this);
		OpenFileDlg	dlg(parentWin, OpenFileDlg::NODEREF_SAVE, (LPOFNHOOKPROC)TApp::WinProc);
		BOOL ret = dlg.Exec(fname, sizeof(fname), GetLoadStrU8(IDS_SAVEFILE),
							GetLoadStrAsFilterU8(IDS_OPENFILEALLFLTR), last_dir);
		TApp::GetApp()->DelWin(this);
		hWnd = NULL;

		if (!ret)
			return	FALSE;

	// shortcut の場合は、リンク先に飛ぶ
		if (!isLinkFile && (ext = strrchr(fname, '.')) && stricmp(ext, ".lnk") == 0) {
			char	arg[MAX_BUF];
			if (ReadLinkU8(fname, last_dir, arg)) {
				if ((GetFileAttributesU8(last_dir) & FILE_ATTRIBUTE_DIRECTORY) == 0)
					GetParentDirU8(last_dir, last_dir);
			}
			continue;
		}

		fileInfo = shareInfo->fileInfo[offset];

		GetParentDirU8(fname, last_dir);
		ForcePathToFname(fname, fname);
		fileInfo->SetSelected(TRUE);

	// 上書き確認
		for (int i=0; i < shareInfo->fileCnt; i++)
		{
			if (!shareInfo->fileInfo[i]->IsSelected())
				continue;
			MakePath(buf, last_dir, offset == i ? fname : shareInfo->fileInfo[i]->Fname());
			if (GetFileAttributesU8(buf) != 0xffffffff)
			{
				ret = parentWin->MessageBoxU8(GetLoadStrU8(IDS_OVERWRITE),
					GetLoadStrU8(IDS_ATTENTION), MB_OKCANCEL|MB_ICONEXCLAMATION);
				if (ret != IDOK) {
					for (int j=0; j < shareInfo->fileCnt; j++)
						shareInfo->fileInfo[j]->SetSelected(FALSE);
				}
				break;
			}
		}
		if (ret) {
			fileInfo->SetFname(fname);
			strcpy(cfg->lastSaveDir, last_dir);
			cfg->WriteRegistry(CFG_GENERAL);
			return	TRUE;
		}
	}
	// not reach
}

BOOL TSaveCommonDlg::EvCreate(LPARAM lParam)
{
	RECT	ok_rect = { 0, 0, 50, 20 }, cl_rect;
	HWND	pWnd = ::GetParent(hWnd);

	::ShowWindow(::GetDlgItem(pWnd, 0x441), SW_HIDE);
	::ShowWindow(::GetDlgItem(pWnd, 0x470), SW_HIDE);

//	if (shareInfo->fileCnt == 1)
//		return	TRUE;

	if (!::GetWindowRect(::GetDlgItem(pWnd, IDOK), &ok_rect))
		return	TRUE;

	int	ok_xsize = ok_rect.right - ok_rect.left;
	int	ok_ysize = ok_rect.bottom - ok_rect.top;

// ボタン高さの2倍分広げる
	SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE)|WS_CLIPSIBLINGS);
	::GetClientRect(pWnd, &cl_rect);
	GetWindowRect(&rect);
	MoveWindow(0, 0, cl_rect.right, (rect.bottom - rect.top) + ok_ysize * 5 / 2, FALSE);
	GetWindowRect(&rect);

	int		cx = 20, cy = ok_ysize;
	int		lump_size = ok_xsize + 30;

	CreateWindowU8(STATIC_CLASS, "", WS_CHILD|WS_VISIBLE|SS_LEFT, cx, 0, rect.right, ok_ysize, hWnd, (HMENU)RESULT_STATIC, TApp::GetInstance(), NULL);

	DWORD	flg = (shareInfo->fileCnt == 1 ? WS_DISABLED : 0)|WS_CHILD|WS_VISIBLE;
	CreateWindowU8(BUTTON_CLASS, GetLoadStrU8(IDS_PREVBUTTON), flg | BS_PUSHBUTTON,
		cx, cy, ok_xsize, ok_ysize, hWnd, (HMENU)PRIOR_BUTTON, TApp::GetInstance(), NULL);
	CreateWindowU8(BUTTON_CLASS, GetLoadStrU8(IDS_NEXTBUTTON), flg | BS_PUSHBUTTON,
		cx+=ok_xsize+20, cy, ok_xsize, ok_ysize, hWnd, (HMENU)NEXT_BUTTON, TApp::GetInstance(), NULL);
	CreateWindowU8(BUTTON_CLASS, "", WS_CHILD|WS_VISIBLE|BS_CHECKBOX|BS_AUTOCHECKBOX,
		cx+=ok_xsize+20, cy, lump_size, ok_ysize, hWnd, (HMENU)LUMP_CHECK, TApp::GetInstance(), NULL);

	if (hostStatus & IPMSG_CAPFILEENCOPT) {
		CreateWindowU8(BUTTON_CLASS, GetLoadStrU8(IDS_ENCTRANS),
			WS_CHILD|WS_VISIBLE|BS_CHECKBOX|BS_AUTOCHECKBOX, cx+=lump_size+20, cy, ok_xsize,
			ok_ysize, hWnd, (HMENU)ENCTRANS_CHECK, TApp::GetInstance(), NULL);
	}

	HFONT	hDlgFont = (HFONT)::SendDlgItemMessage(pWnd, IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont)
	{
		SendDlgItemMessage(RESULT_STATIC, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(PRIOR_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(NEXT_BUTTON, WM_SETFONT, (UINT)hDlgFont, 0L);
		SendDlgItemMessage(LUMP_CHECK, WM_SETFONT, (UINT)hDlgFont, 0L);
		if (hostStatus & IPMSG_CAPFILEENCOPT) {
			SendDlgItemMessage(ENCTRANS_CHECK, WM_SETFONT, (UINT)hDlgFont, 0L);
		}
	}
	SetInfo();

	CheckDlgButton(LUMP_CHECK, cfg->LumpCheck);
	if (cfg->LumpCheck) LumpCheck();

	if (hostStatus & IPMSG_CAPFILEENCOPT) {
		CheckDlgButton(ENCTRANS_CHECK, cfg->EncTransCheck);
	}

	return	TRUE;
}

BOOL TSaveCommonDlg::EvCommand(WORD wNotifyCode, WORD wID, LPARAM hwndCtl)
{
	switch (wID) {
	case PRIOR_BUTTON:
		if (offset > 0)
			offset--;
		SetInfo();
		return	TRUE;

	case NEXT_BUTTON:
		if (offset < shareInfo->fileCnt -1)
			offset++;
		SetInfo();
		return	TRUE;

	case LUMP_CHECK:
		LumpCheck();
		return	TRUE;

	case ENCTRANS_CHECK:
		EncTransCheck();
		return	TRUE;
	}
	return	FALSE;
}

BOOL TSaveCommonDlg::LumpCheck()
{
	cfg->LumpCheck = (int)SendDlgItemMessage(LUMP_CHECK, BM_GETCHECK, 0, 0);

	for (int i=0; i < shareInfo->fileCnt; i++) {
		shareInfo->fileInfo[i]->SetSelected(cfg->LumpCheck);
	}
	return	TRUE;
}

BOOL TSaveCommonDlg::EncTransCheck()
{
	if (hostStatus & IPMSG_CAPFILEENCOPT) {
		cfg->EncTransCheck = (int)SendDlgItemMessage(ENCTRANS_CHECK, BM_GETCHECK, 0, 0);
	}
	return	TRUE;
}

BOOL TSaveCommonDlg::EventApp(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return	FALSE;
}

BOOL TSaveCommonDlg::SetInfo(void)
{
	char	buf[MAX_BUF], sizestr[MAX_LISTBUF];
	Wstr	fname_w(shareInfo->fileInfo[offset]->Fname());

	::SetDlgItemTextW(::GetParent(hWnd), 0x480, fname_w.Buf());

	if (GET_MODE(shareInfo->fileInfo[offset]->Attr()) == IPMSG_FILE_DIR)
		strcpy(sizestr, GetLoadStrU8(IDS_DIRSAVE));
	else
		MakeSizeString(sizestr, shareInfo->fileInfo[offset]->Size());

	wsprintf(buf, GetLoadStrU8(IDS_FILEINFO), offset + 1, shareInfo->fileCnt,
		shareInfo->fileInfo[offset]->Fname(), sizestr);
	SetDlgItemTextU8(RESULT_STATIC, buf);

	_int64	total_size = 0;
	for (int i=0; i < shareInfo->fileCnt; i++)
		total_size += shareInfo->fileInfo[i]->Size();
	MakeSizeString(sizestr, total_size);
	wsprintf(buf, GetLoadStrU8(IDS_LUMPBUTTON), sizestr, shareInfo->fileCnt);
	SetDlgItemTextU8(LUMP_CHECK, buf);

	const char	*ext = strrchr(shareInfo->fileInfo[offset]->Fname(), '.');
	isLinkFile = (ext && stricmp(ext, ".lnk") == 0) ? TRUE : FALSE;

	return	TRUE;
}


/*
	ファイル名に ':' を含む場合、"::" とエスケープされているが、
	Windows では使えないので、';' に置き換える
*/
void ConvertShareMsgEscape(char *str)
{
	char	*ptr;
	while ((ptr = strstr(str, "::")))
	{
		*ptr++ = ';';
		memmove(ptr, ptr + 1, strlen(ptr));
	}
}

/*
	ファイル共有（添付）情報をデコード
	注意：破壊読出し。使用が終わり次第 delete を呼び出すこと。
*/
ShareInfo::ShareInfo(char *msg, BOOL enable_clip)
{
	FileInfo	*info = NULL;
	char		*tok, *p, *p2, *p3;
	char		*file = separate_token(msg, FILELIST_SEPARATOR, &p);

	Init();

	for (int i=0; file; i++, file=separate_token(NULL, FILELIST_SEPARATOR, &p))
	{
		ConvertShareMsgEscape(file);	// "::" -> ';'
		if ((tok = separate_token(file, ':', &p2)) == NULL)
			break;
		info = new FileInfo(atoi(tok));

		if ((tok = separate_token(NULL, ':', &p2)) == NULL || strlen(tok) > MAX_FILENAME_U8)
			break;
		while ((p3 = strchr(tok, '?')))	// UNICODE 対応までの暫定
			*p3 = '_';
		if (!IsValidFileName(tok))
			break;
		info->SetFname(tok);

		if ((tok = separate_token(NULL, ':', &p2)) == NULL)
			break;
		info->SetSize(hex2ll(tok));

		if ((tok = separate_token(NULL, ':', &p2)) == NULL)
			break;
		info->SetMtime(strtoul(tok, 0, 16));

		if ((tok = separate_token(NULL, ':', &p2)))
		{
			info->SetAttr(strtoul(tok, 0, 16));
			u_int	attr_type = GET_MODE(info->Attr());
			if (attr_type != IPMSG_FILE_DIR &&
				attr_type != IPMSG_FILE_REGULAR &&
				(!enable_clip || attr_type != IPMSG_FILE_CLIPBOARD))
			{
				delete info;
				info = NULL;
				continue;
			}
			if (attr_type == IPMSG_FILE_CLIPBOARD) {
				if ((tok = separate_token(NULL, ':', &p2))) {
					if (strtoul(tok, 0, 16) == IPMSG_FILE_CLIPBOARDPOS) {
						if (separate_token(tok, '=', &p3) &&
							(tok = separate_token(NULL, '=', &p3))) {
							info->SetPos(strtoul(tok, 0, 16));
						}
					}
				}
			}
		}
		else info->SetAttr(IPMSG_FILE_REGULAR);

		if ((fileCnt % BIG_ALLOC) == 0)
			fileInfo = (FileInfo **)realloc(fileInfo, (fileCnt + BIG_ALLOC) * sizeof(FileInfo *));

		fileInfo[fileCnt++] = info;
		info = NULL;
	}
	if (info)	// デコード中に抜けた
		delete info;
}

/*
	ファイル共有（添付）情報をエンコード
*/
BOOL ShareInfo::EncodeMsg(char *buf, int bufsize, BOOL incMem)
{
	int		offset=0;
	char	fname[MAX_PATH_U8];
	int		id_base = 0;

	TGenRandom(&id_base, sizeof(id_base));
	id_base &= 0x3fffffff; // 負数になるのを防ぐ

	*buf = 0;
	for (int i=0; i < fileCnt; i++)
	{
		char	addition[100] = "";
		if (GET_MODE(fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
			if (!incMem) continue;
			sprintf(addition, ":%x=%x", IPMSG_FILE_CLIPBOARDPOS, fileInfo[i]->Pos());
		}
		ForcePathToFname(fileInfo[i]->Fname(), fname);
		fileInfo[i]->SetId(id_base + i);

		offset += sprintf(buf + offset, "%d:%s:%I64x:%x:%x%s:%c",
					fileInfo[i]->Id(), fname, fileInfo[i]->Size(),
					fileInfo[i]->Mtime(), fileInfo[i]->Attr(),
					addition, FILELIST_SEPARATOR);

		if (offset + MAX_BUF > bufsize)
			break;
	}
	return	TRUE;
}

