static char *share_id = 
	"@(#)Copyright (C) H.Shirouzu 2002-2017   share.cpp	Ver4.50";
/* ========================================================================
	Project  Name			: IP Messenger for Win32
	Module Name				: File Share
	Create					: 2002-04-14(Sun)
	Update					: 2017-06-12(Mon)
	Copyright				: H.Shirouzu
	Reference				: 
	======================================================================== */

#include "resource.h"
#include "ipmsg.h"
#include <stddef.h>

using namespace std;

#define BIG_ALLOC 100

// for ShareDict for Cfg
#define SID_PKTNO	"pkt"
#define SID_TRANS	"tr"
#define SID_LASTPKT	"lp"
#define SID_ATTACH	"at"
#define SID_FILE	"f"
#define SID_FNAME	"fn"
#define SID_ID		"id"
#define SID_ATTR	"atr"
#define SID_SIZE	"sz"
#define SID_MTIME	"mt"
#define SID_HOST	"h"
#define SID_UID		"uid"
#define SID_HNAME	"hn"
#define SID_NICK	"nk"
#define SID_ADDR	"adr"
#define SID_STAT	"st"

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
	if (Search(packetNo)) {
		return	NULL;
	}

	ShareInfo *info = new ShareInfo(packetNo);
	AddObj(info);

	return	info;
}

void ShareMng::LoadShare()
{
	time_t	t = time(NULL);

	for (int i=0; i < 1000; i++) {
		IPDict	dict;
		if (!cfg->LoadShare(i, &dict)) {
			break;
		}
		int64	val;
		if (dict.get_int(SID_PKTNO, &val)) {
			auto info = CreateShare((int)val);
			if (!info->UnPack(cfg, dict) || (t - info->attachTime) > MONTH_SEC) {
				DestroyShare(info);
			}
		}
	}
}

void ShareMng::SaveShare(ShareInfo *info)
{
	if (info->fileCnt <= info->clipCnt) {	// clip only
		return;
	}

	IPDict	dict;

	if (info->Pack(&dict)) {
		cfg->SaveShare(info->packetNo, dict);
	}
}

BOOL ShareMng::AddShareCore(ShareInfo *shareInfo, FileInfo *fileInfo, BOOL isClip)
{
	if (!fileInfo) return FALSE;

	if ((shareInfo->fileCnt % BIG_ALLOC) == 0) {
		shareInfo->fileInfo = (FileInfo **)realloc(shareInfo->fileInfo,
			(shareInfo->fileCnt + BIG_ALLOC) * sizeof(FileInfo *));
	}
	shareInfo->fileInfo[shareInfo->fileCnt] = fileInfo;
	shareInfo->fileCnt++;
	if (isClip) {
		shareInfo->clipCnt++;
	}

	return	TRUE;
}

BOOL ShareMng::AddFileShare(ShareInfo *shareInfo, char *fname)
{
	for (int i=0; i < shareInfo->fileCnt; i++) {
		if (strcmp(fname, shareInfo->fileInfo[i]->Fname()) == 0)
			return	FALSE;
	}
	return	AddShareCore(shareInfo, SetFileInfo(fname), FALSE);
}

BOOL ShareMng::AddMemShare(ShareInfo *shareInfo, char *dummy_name, BYTE *data, int size, int pos)
{
	FileInfo	*info = new FileInfo;
	if (!info) return FALSE;

	info->SetAttr(IPMSG_FILE_CLIPBOARD);
	info->SetFname(dummy_name);
	info->SetMemData(data, size);
	info->SetMtime(time(NULL));
	info->SetPos(pos);

	return	AddShareCore(shareInfo, info, TRUE);
}

BOOL ShareMng::DelFileShare(ShareInfo *info, int fileNo)
{
	if (fileNo >= info->fileCnt) {
		return	FALSE;
	}
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
		info->SetSize((int64)fdat.nFileSizeHigh << 32 | fdat.nFileSizeLow);
		GetParentDirU8(fname, cfg->lastOpenDir);
	}
	info->SetMtime(FileTime2UnixTime(&fdat.ftLastWriteTime));
	info->SetCrtime(FileTime2UnixTime(&fdat.ftCreationTime));
	info->SetAtime(FileTime2UnixTime(&fdat.ftLastAccessTime));

	return	info;
}

BOOL ShareMng::AddHostShare(ShareInfo *info, SendEntry *entry, int entryNum)
{
	IPDict	dict;

	info->host = new Host *[info->hostCnt = entryNum];

	info->lastPkt = new UINT [info->hostCnt];
	memset(info->lastPkt, 0, sizeof(UINT) * info->hostCnt);

	info->transStat = new char [info->hostCnt * info->fileCnt];
	memset(info->transStat, TRANS_INIT, info->hostCnt * info->fileCnt);

	for (int i=0; i < entryNum; i++)
	{
		auto h = cfg->fileHosts.GetHostByNameAddr(&entry[i].Host()->hostSub);
		if (h == NULL)
		{
			h = new Host;
			h->hostSub = entry[i].Host()->hostSub;
			h->hostStatus = entry[i].Host()->hostStatus;
			h->updateTime = entry[i].Host()->updateTime;
			h->priority = entry[i].Host()->priority;
			strncpyz(h->nickName, entry[i].Host()->nickName, MAX_NAMEBUF);
			cfg->fileHosts.AddHost(h);
		}
		else h->RefCnt(1);
		info->host[i] = h;
	}

	info->attachTime = time(NULL);

	SaveShare(info);

	statDlg->Refresh();

	return	TRUE;
}

int ShareMng::GetFileInfoNo(ShareInfo *info, FileInfo *fileInfo)
{
	for (int target=0; info->fileCnt; target++) {
		if (info->fileInfo[target] == fileInfo)
			return	target;
	}
	return	-1;
}

BOOL ShareMng::EndHostShare(int packetNo, HostSub *hostSub, FileInfo *fileInfo, BOOL done)
{
	ShareInfo	*info = Search(packetNo);

	if (!info) {
		if (!(info = SearchDelayList(packetNo))) {
			return	FALSE;
		}
	}

	int		i = 0;
	for (i=0; i < info->hostCnt; i++) {
		if (IsSameHost(&info->host[i]->hostSub, hostSub)) {
			break;
		}
	}
	if (i == info->hostCnt) {
		return	FALSE;
	}

	if (fileInfo) {
		info->transStat[info->fileCnt * i + GetFileInfoNo(info, fileInfo)] =
												done ? TRANS_DONE : TRANS_INIT;
		if (!done) {
			statDlg->Refresh();
			return	TRUE;
		}
		for (int j=0; j < info->fileCnt; j++) {
			if (info->transStat[info->fileCnt * i + j] != TRANS_DONE) {
				statDlg->Refresh();
				info->needSave = TRUE;
				return	TRUE;
			}
		}
	}
	if (info->host[i]->RefCnt() == 1) {
		cfg->fileHosts.DelHost(info->host[i]);
		info->host[i]->SafeRelease();
	}
	else {
		info->host[i]->RefCnt(-1);
	}
	memmove(info->host + i, info->host + i + 1, (--info->hostCnt - i) * sizeof(Host *));
	memmove(info->lastPkt + i, info->lastPkt + i + 1, (info->hostCnt - i) * sizeof(UINT));
	memmove(info->transStat + info->fileCnt * i,
		info->transStat + info->fileCnt * (i + 1), (info->hostCnt - i) * info->fileCnt);

	if (info->hostCnt == 0) {
		DestroyShare(info);
	}
	else {
		info->needSave = FALSE;
	}
	statDlg->Refresh();
	return	TRUE;
}

void ShareMng::DestroyShare(ShareInfo *info)
{
	DelObj(info);

	cfg->DeleteShare(info->packetNo);

	BOOL	is_delay = FALSE;
	for (int i=info->fileCnt * info->hostCnt -1; i >= 0; i--) {
		if (info->transStat[i] == ShareMng::TRANS_BUSY) {
			is_delay = TRUE;
			::PostMessage(GetMainWnd(), WM_STOPTRANS, 0, (LPARAM)info->packetNo);
			Debug(" DestroyShare is delayed(%d)\n", info->packetNo);
		}
	}
	if (is_delay) {
		delayList.AddObj(info);
		return;
	}

	while (info->hostCnt-- > 0)
	{
		if (info->host[info->hostCnt]->RefCnt() == 1)
		{
			cfg->fileHosts.DelHost(info->host[info->hostCnt]);
			info->host[info->hostCnt]->SafeRelease();
		}
		else {
			info->host[info->hostCnt]->RefCnt(-1);
		}
	}

	Debug(" DestroyShare done(%d)\n", info->packetNo);
	info->UnInit();

	statDlg->Refresh();
}

ShareInfo *ShareMng::Search(int packetNo)
{
	for (auto info=TopObj(); info; info=NextObj(info)) {
		if (info->packetNo == packetNo)
			return	info;
	}
	return	NULL;
}

ShareInfo *ShareMng::SearchDelayList(int packetNo)
{
	for (auto info=delayList.TopObj(); info; info=delayList.NextObj(info)) {
		if (info->packetNo == packetNo)
			return	info;
	}
	return	NULL;
}

BOOL ShareMng::GetShareCntInfo(ShareCntInfo *cntInfo, ShareInfo *shareInfo)
{

	memset(cntInfo, 0, sizeof(ShareCntInfo));

	for (auto info = shareInfo ? shareInfo : TopObj(); info; info=NextObj(info))
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


BOOL ShareMng::GetAcceptableFileInfo(ConnectInfo *info, MsgBuf *_msg, AcceptFileInfo *fileInfo)
{
	MsgBuf	&msg = *_msg;
	UINT	&logOpt = fileInfo->logOpt;

	char		*tok, *p;
	int			targetID;
	ShareInfo	*shareInfo;

	if ((tok = sep_tok(msg.msgBuf, ':', &p)) == NULL)
		return	FALSE;
	fileInfo->packetNo   = strtol(tok, 0, 16);
	fileInfo->ivPacketNo = msg.packetNo;
	fileInfo->command    = msg.command;
	fileInfo->fileCapa   = 0;

	if ((tok = sep_tok(NULL, ':', &p)) == NULL)
		return	FALSE;
	targetID = strtol(tok, 0, 16);

	if (GET_MODE(fileInfo->command) == IPMSG_GETFILEDATA)
	{
		if ((tok = sep_tok(NULL, ':', &p)) == NULL)
			return	FALSE;
		fileInfo->offset = hex2ll(tok);
	}
	else if (GET_MODE(fileInfo->command) == IPMSG_GETDIRFILES) {
		fileInfo->offset = 0;
	}
	else return	FALSE;

	if ((shareInfo = Search(fileInfo->packetNo)) == NULL)
		return	FALSE;

	int	hidx = 0;

	for (hidx=0; hidx < shareInfo->hostCnt; hidx++)
	{
		if (IsSameHost(&shareInfo->host[hidx]->hostSub, &msg.hostSub))
		{
			if ((logOpt & (LOG_SIGN_OK|LOG_SIGN2_OK))
			 || shareInfo->host[hidx]->hostSub.addr == info->addr) {
				fileInfo->host = shareInfo->host[hidx];

				if ((logOpt & (LOG_SIGN_OK|LOG_SIGN2_OK))
				 && (GET_OPT(fileInfo->command) & IPMSG_ENCFILEOPT)) {
					if ((tok = sep_tok(NULL, ':', &p)) == NULL) return FALSE;
					fileInfo->fileCapa = strtoul(tok, 0, 16);
					if (fileInfo->fileCapa == (IPMSG_AES_256|IPMSG_PACKETNO_IV)) {
						if ((tok = sep_tok(NULL, ':', &p)) == NULL) return FALSE;
						int len = (int)hexstr2bin(tok, fileInfo->aesKey, 32);
						if (len != 32) return FALSE;

						if ((tok = sep_tok(NULL, ':', &p))) {
							if (UINT pkt_no = strtoul(tok, 0, 16)) {	// for reply attack
								if (shareInfo->lastPkt[hidx] >= pkt_no) return FALSE;
								shareInfo->lastPkt[hidx] = pkt_no;
							}
						}
					}
					else if (fileInfo->fileCapa == IPMSG_NOENC_FILEBODY) {
						if ((tok = sep_tok(NULL, ':', &p)) == NULL) return FALSE;

						UINT	pkt_no = strtoul(tok, 0, 16);	// for reply attack
						if (shareInfo->lastPkt[hidx] >= pkt_no) return FALSE;
						shareInfo->lastPkt[hidx] = pkt_no;
					}
					else {
						return	FALSE;
					}
				}
				break;
			}
		}
	}
	if (hidx == shareInfo->hostCnt)
		return	FALSE;

	for (int fidx=0; fidx < shareInfo->fileCnt; fidx++)
	{
		if (shareInfo->fileInfo[fidx]->Id() == targetID)
		{
			fileInfo->fileInfo = shareInfo->fileInfo[fidx];
			if (shareInfo->transStat[shareInfo->fileCnt * hidx + fidx] != TRANS_INIT)
				return	FALSE;	// download 済み（or 最中）

			// dir に対しては IPMSG_GETDIRFILES 以外は認めない
			if (GET_MODE(fileInfo->command) != IPMSG_GETDIRFILES &&
				GET_MODE(fileInfo->fileInfo->Attr()) == IPMSG_FILE_DIR)
				return	FALSE;	

			fileInfo->attachTime = shareInfo->attachTime;
			shareInfo->transStat[shareInfo->fileCnt * hidx + fidx] = TRANS_BUSY;
			statDlg->Refresh();
			return	TRUE;
		}
	}
	return	FALSE;
}

void ShareMng::Cleanup()
{
	time_t		cur_time = 0;
	ShareInfo	*info, *next;

	for (info=TopObj(); info; info=next) {
		next = NextObj(info);
		if (info->attachTime == 0) {
			continue;
		}
		if (!cur_time) {
			cur_time = time(NULL);
		}

		int	clip_host = 0;
		int	i=0, j=0;
		for (i = 0; i < info->fileCnt; i++) {
			for (j=0; j < info->hostCnt; j++) {
				if (info->transStat[info->fileCnt * j + i] == TRANS_BUSY) {
					break;
				}
				if (info->transStat[info->fileCnt * j + i] == TRANS_INIT) {
					if (GET_MODE(info->fileInfo[i]->Attr()) != IPMSG_FILE_CLIPBOARD) {
						break;
					}
					if (info->host[j]->hostStatus & IPMSG_CLIPBOARDOPT) {
						clip_host++;
					}
				}
			}
			if (j != info->hostCnt) {
				break;
			}
		}
		if (info->needSave) {
			SaveShare(info);
		}
		if (i == info->fileCnt && j == info->hostCnt) {
			if (clip_host > 0 && info->attachTime + 1200 > cur_time) {
				continue;
			}
			DestroyShare(info);
		}
	}
}

/* ShareDlg */
TShareDlg::TShareDlg(ShareMng *_shareMng, ShareInfo *_shareInfo, Cfg *_cfg, TSendDlg *_parent) : TDlg(FILE_DIALOG, _parent), shareListView(this)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
	shareMng = _shareMng;
	shareInfo = _shareInfo;
	parentDlg = _parent;
	cfg = _cfg;
}

TShareDlg::~TShareDlg()
{
	shareListView.DeleteAllItems();
}

BOOL TShareDlg::EvCreate(LPARAM lParam)
{
 	shareListView.AttachWnd(GetDlgItem(FILE_LIST));

	char	*title[] = { LoadStrU8(IDS_FILENAME), LoadStrU8(IDS_SIZE), LoadStrU8(IDS_LOCATION), NULL };
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
	shareListView.SetFocus();
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
			if (ShareFileAddDlg(parentDlg, this, shareMng, shareInfo, cfg)) {
				for (i; i < shareInfo->fileCnt; i++) {
					AddList(i);
				}
			}
		}
		break;

	case FOLDER_BUTTON:
		if (BrowseDirDlg(this, LoadStrU8(IDS_FOLDERATTACH), cfg->lastOpenDir,
			cfg->lastOpenDir, sizeof(cfg->lastOpenDir)))
		{
			if (cfg->NoFileTrans == 2 && IsNetVolume(cfg->lastOpenDir)) {
				parentDlg->AppendDropFilesAsText(cfg->lastOpenDir);
			}
			else if (shareMng->AddFileShare(shareInfo, cfg->lastOpenDir))
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

BOOL ShareFileAddDlg(TSendDlg *sendDlg, TDlg *parent, ShareMng *shareMng, ShareInfo *shareInfo,
	Cfg *cfg)
{
	int		bufsize = MAX_MULTI_PATH;
	U8str	buf(bufsize);
	U8str	path(bufsize);

	OpenFileDlg	ofdlg(parent, OpenFileDlg::MULTI_OPEN, 0,
		cfg->NoFileTrans == 2 ? OFN_NONETWORKBUTTON : 0);

	if (!ofdlg.Exec(buf.Buf(), bufsize, LoadStrU8(IDS_ADDFILE),
			LoadStrAsFilterU8(IDS_OPENFILEALLFLTR), cfg->lastOpenDir)) {
		return	FALSE;
	}

	cfg->WriteRegistry(CFG_GENERAL);

	int		dirlen = (int)strlen(cfg->lastOpenDir);
	if (buf[dirlen]) {
		if (cfg->NoFileTrans == 2 && IsNetVolume(buf.Buf())) {
			sendDlg->AppendDropFilesAsText(buf.Buf());
			return	FALSE;
		}
		return	shareMng->AddFileShare(shareInfo, buf.Buf());
	}

	for (const char *fname = buf.s() + dirlen + 1; *fname; fname += strlen(fname) + 1)
	{
		if (MakePathU8(path.Buf(), buf.s(), fname) >= MAX_PATH_U8)
			continue;

		if (cfg->NoFileTrans == 2 && IsNetVolume(path.Buf())) {
			sendDlg->AppendDropFilesAsText(path.Buf());
		}
		else {
			shareMng->AddFileShare(shareInfo, path.Buf());
		}
	}
	return	TRUE;
}

/* TShareStatDlg */
TShareStatDlg::TShareStatDlg(ShareMng *_shareMng, Cfg *_cfg, TWin *_parent) : TDlg(SHARE_DIALOG, _parent), shareListView(this)
{
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
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
	shareListView.SetFocus();

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

	int		i=0, len=0;
	char	buf[MAX_BUF_EX];

	for (auto info=shareMng->TopObj(); info; info=shareMng->NextObj(info))
	{
		if (info->hostCnt == 0)
			continue;

		sprintf(buf, "%d", i);
		shareListView.InsertItem(i, buf, (LPARAM)info);

		len = 0;
		*buf = 0;
		for (int j=0; j < info->fileCnt && len < sizeof(buf); j++) {
			ForcePathToFname(info->fileInfo[j]->Fname(), buf + len);
			strcat(buf + len, " ");
			len += (int)strlen(buf + len);
			if (len + MAX_PATH_U8 >= sizeof(buf)) break;
		}
		shareListView.SetSubItem(i, 1, buf);

		ShareCntInfo	cntInfo;
		shareMng->GetShareCntInfo(&cntInfo, info);
		MakeSizeString(buf, cntInfo.totalSize, MSS_SPACE);
		if (cntInfo.dirCnt) sprintf(buf + strlen(buf), "/%dDIR", cntInfo.dirCnt);
		shareListView.SetSubItem(i, 2, buf);

		shareMng->GetShareCntInfo(&cntInfo, info);
		sprintf(buf, "%d / %d/ %d", cntInfo.fileCnt, cntInfo.doneCnt, cntInfo.transferCnt);
		shareListView.SetSubItem(i, 3, buf);

		len = 0;
		*buf = 0;
		for (int j=0; j < info->hostCnt && len + 100 < sizeof(buf); j++) {
			Host *host = info->host[j];
			len += snprintfz(buf + len, sizeof(buf)-len-1, "%s(%s) ",
							*host->nickName ? host->nickName : host->hostSub.u.userName,
							host->hostSub.u.hostName);
		}
		shareListView.SetSubItem(i, 4, buf);
		i++;
	}
	return	TRUE;
}

BOOL TShareStatDlg::DelList(int idx)
{
	auto info = (ShareInfo *)shareListView.GetItemParam(idx);
	if (info == NULL)
		return	FALSE;

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
	hAccel = ::LoadAccelerators(TApp::hInst(), (LPCSTR)IPMSG_ACCEL);
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

	char	fname[MAX_BUF];
	char	last_dir[MAX_BUF];
	char	buf[MAX_BUF];

	// 最終保存ディレクトリが無くなっている場合、少しさかのぼる
	for (int i=0; i < 5; i++) {
		if (*cfg->lastSaveDir && GetFileAttributesU8(cfg->lastSaveDir) == 0xffffffff)
			if (!GetParentDirU8(cfg->lastSaveDir, cfg->lastSaveDir))
				break;
	}

	strcpy(last_dir, *cfg->lastSaveDir ? cfg->lastSaveDir : ".");

	while (1) {
		FileInfo	*fileInfo = shareInfo->fileInfo[offset];

		MakePathU8(fname, last_dir, fileInfo->Fname());

	// ファイルダイアログ
		TApp::GetApp()->AddWin(this);
		OpenFileDlg	dlg(parentWin, OpenFileDlg::NODEREF_SAVE, (LPOFNHOOKPROC)TApp::WinProc);
		BOOL ret = dlg.Exec(fname, sizeof(fname), LoadStrU8(IDS_SAVEFILE),
							LoadStrAsFilterU8(IDS_OPENFILEALLFLTR), last_dir);
		TApp::GetApp()->DelWin(this);
		hWnd = NULL;

		if (!ret)
			return	FALSE;

	// shortcut の場合は、リンク先に飛ぶ
		if (!isLinkFile) {
			char	*ext = strrchr(fname, '.');
			if (ext && stricmp(ext, ".lnk") == 0) {
				if (ReadLinkU8(fname, last_dir)) {
					if ((GetFileAttributesU8(last_dir) & FILE_ATTRIBUTE_DIRECTORY) == 0)
						GetParentDirU8(last_dir, last_dir);
				}
				continue;
			}
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
			MakePathU8(buf, last_dir, offset == i ? fname : shareInfo->fileInfo[i]->Fname());
			if (GetFileAttributesU8(buf) != 0xffffffff)
			{
				ret = parentWin->MessageBoxU8(LoadStrU8(IDS_OVERWRITE),
					LoadStrU8(IDS_ATTENTION), MB_OKCANCEL|MB_ICONEXCLAMATION);
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

	CreateWindowU8(STATIC_CLASS, "", WS_CHILD|WS_VISIBLE|SS_LEFT, cx, 0, rect.right, ok_ysize,
		hWnd, (HMENU)RESULT_STATIC, TApp::hInst(), NULL);

	DWORD	flg = (shareInfo->fileCnt == 1 ? WS_DISABLED : 0)|WS_CHILD|WS_VISIBLE;
	CreateWindowU8(BUTTON_CLASS, LoadStrU8(IDS_PREVBUTTON), flg | BS_PUSHBUTTON,
		cx, cy, ok_xsize, ok_ysize, hWnd, (HMENU)PRIOR_BUTTON, TApp::hInst(), NULL);
	CreateWindowU8(BUTTON_CLASS, LoadStrU8(IDS_NEXTBUTTON), flg | BS_PUSHBUTTON,
		cx+=ok_xsize+20, cy, ok_xsize, ok_ysize, hWnd, (HMENU)NEXT_BUTTON, TApp::hInst(), NULL);
	CreateWindowU8(BUTTON_CLASS, "", WS_CHILD|WS_VISIBLE|BS_CHECKBOX|BS_AUTOCHECKBOX,
		cx+=ok_xsize+20, cy, lump_size, ok_ysize, hWnd, (HMENU)LUMP_CHECK, TApp::hInst(), NULL);

	if (hostStatus & IPMSG_CAPFILEENCOPT) {
		CreateWindowU8(BUTTON_CLASS, LoadStrU8(IDS_ENCTRANS),
			WS_CHILD|WS_VISIBLE|BS_CHECKBOX|BS_AUTOCHECKBOX, cx+=lump_size+20, cy, ok_xsize,
			ok_ysize, hWnd, (HMENU)ENCTRANS_CHECK, TApp::hInst(), NULL);
	}

	HFONT	hDlgFont = (HFONT)::SendDlgItemMessage(pWnd, IDOK, WM_GETFONT, 0, 0L);
	if (hDlgFont)
	{
		SendDlgItemMessage(RESULT_STATIC, WM_SETFONT, (WPARAM)hDlgFont, 0L);
		SendDlgItemMessage(PRIOR_BUTTON, WM_SETFONT, (WPARAM)hDlgFont, 0L);
		SendDlgItemMessage(NEXT_BUTTON, WM_SETFONT, (WPARAM)hDlgFont, 0L);
		SendDlgItemMessage(LUMP_CHECK, WM_SETFONT, (WPARAM)hDlgFont, 0L);
		if (hostStatus & IPMSG_CAPFILEENCOPT) {
			SendDlgItemMessage(ENCTRANS_CHECK, WM_SETFONT, (WPARAM)hDlgFont, 0L);
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
		if (offset > 0) {
			offset--;
		}
		SetInfo();
		return	TRUE;

	case NEXT_BUTTON:
		if (offset < shareInfo->fileCnt -1) {
			offset++;
		}
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
	char	buf[MAX_BUF];
	char	sizestr[MAX_LISTBUF];

	Wstr	fname_w(shareInfo->fileInfo[offset]->Fname());

	::SetDlgItemTextW(::GetParent(hWnd), 0x47c, fname_w.Buf());

	if (GET_MODE(shareInfo->fileInfo[offset]->Attr()) == IPMSG_FILE_DIR)
		strcpy(sizestr, LoadStrU8(IDS_DIRSAVE));
	else
		MakeSizeString(sizestr, shareInfo->fileInfo[offset]->Size());

	snprintfz(buf, sizeof(buf), LoadStrU8(IDS_FILEINFO), offset + 1, shareInfo->fileCnt,
		shareInfo->fileInfo[offset]->Fname(), sizestr);
	SetDlgItemTextU8(RESULT_STATIC, buf);

	int64	total_size = 0;
	for (int i=0; i < shareInfo->fileCnt; i++) {
		total_size += shareInfo->fileInfo[i]->Size();
	}
	MakeSizeString(sizestr, total_size);
	snprintfz(buf, sizeof(buf), LoadStrU8(IDS_LUMPBUTTON), sizestr, shareInfo->fileCnt);
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

void ConvertShareMsgQuestion(char *s)
{
	while (char *p = strchr(s, '?')) {
		*p = '_';
		s = p+1;
	}
}

/*
	ファイル共有（添付）情報をデコード
	注意：破壊読出し。使用が終わり次第 delete を呼び出すこと。
*/
ShareInfo::ShareInfo(char *msg, DWORD flags)
{
	FileInfo	*info = NULL;
	char		*p;
	char		*file = sep_tok(msg, FILELIST_SEPARATOR, &p);

	Init();

	for (int i=0; file; i++, file=sep_tok(NULL, FILELIST_SEPARATOR, &p))
	{
		ConvertShareMsgEscape(file);	// "::" -> ';'

		char	*p2;
		char	*tok = sep_tok(file, ':', &p2);

		if (tok == NULL)
			break;
		info = new FileInfo(atoi(tok));

		if ((tok = sep_tok(NULL, ':', &p2)) == NULL || strlen(tok) > MAX_FILENAME_U8)
			break;
		ConvertShareMsgQuestion(tok);
		if (!IsValidFileName(tok))
			break;
		info->SetFname(tok);

		if ((tok = sep_tok(NULL, ':', &p2)) == NULL)
			break;
		info->SetSize(hex2ll(tok));

		if ((tok = sep_tok(NULL, ':', &p2)) == NULL)
			break;
		info->SetMtime(strtoul(tok, 0, 16));

		if ((tok = sep_tok(NULL, ':', &p2)))
		{
			info->SetAttr(strtoul(tok, 0, 16));
			u_int	attr_type = GET_MODE(info->Attr());
			if (((flags & SI_FILE) == 0 && attr_type != IPMSG_FILE_CLIPBOARD) ||
				(attr_type != IPMSG_FILE_DIR &&
				 attr_type != IPMSG_FILE_REGULAR &&
				 ((flags & SI_CLIP) == 0 || attr_type != IPMSG_FILE_CLIPBOARD)))
			{
				delete info;
				info = NULL;
				continue;
			}
			if (attr_type == IPMSG_FILE_CLIPBOARD) {
				if ((tok = sep_tok(NULL, ':', &p2))) {
					if (strtoul(tok, 0, 16) == IPMSG_FILE_CLIPBOARDPOS) {
						char	*p3;
						if (sep_tok(tok, '=', &p3) &&
							(tok = sep_tok(NULL, '=', &p3))) {
							info->SetPos(strtoul(tok, 0, 16));
						}
					}
				}
			}
		}
		else info->SetAttr(IPMSG_FILE_REGULAR);

		if ((fileCnt % BIG_ALLOC) == 0) {
			fileInfo = (FileInfo **)realloc(fileInfo, (fileCnt + BIG_ALLOC) * sizeof(FileInfo *));
		}
		fileInfo[fileCnt++] = info;
		info = NULL;
	}
	if (info) {	// デコード中に抜けた
		delete info;
	}
}

ShareInfo::ShareInfo(const IPDict& dict, DWORD flags)
{
	FileInfo	*info = NULL;

	Init();

	IPDictList	dlist;
	dict.get_dict_list(IPMSG_FILE_KEY, &dlist);
	for (auto &h: dlist) {
		U8str	u;
		int64	v = 0;

		if (!h->get_int(IPMSG_FID_KEY, &v)) break;
		info = new FileInfo((int)v);

		if (!h->get_str(IPMSG_FNAME_KEY, &u) || u.Len() > MAX_FILENAME_U8) break;
		ConvertShareMsgEscape(u.Buf());
		ConvertShareMsgQuestion(u.Buf());
		info->SetFname(u.s());

		if (!h->get_int(IPMSG_FSIZE_KEY, &v)) break;
		info->SetSize(v);

		if (!h->get_int(IPMSG_MTIME_KEY, &v)) break;
		info->SetMtime(v);

		if (h->get_int(IPMSG_FATTR_KEY, &v)) {
			info->SetAttr((int)v);
			u_int	attr_type = GET_MODE(info->Attr());

			if (((flags & SI_FILE) == 0 && attr_type != IPMSG_FILE_CLIPBOARD) ||
				(attr_type != IPMSG_FILE_DIR &&
				 attr_type != IPMSG_FILE_REGULAR &&
				 ((flags & SI_CLIP) == 0 || attr_type != IPMSG_FILE_CLIPBOARD)))
			{
				delete info;
				info = NULL;
				continue;
			}
			if (attr_type == IPMSG_FILE_CLIPBOARD) {
				if (h->get_int(IPMSG_CLIPPOS_KEY, &v)) {
					info->SetPos((int)v);
				}
			}
		}
		else info->SetAttr(IPMSG_FILE_REGULAR);

		if ((fileCnt % BIG_ALLOC) == 0) {
			fileInfo = (FileInfo **)realloc(fileInfo, (fileCnt + BIG_ALLOC) * sizeof(FileInfo *));
		}
		fileInfo[fileCnt++] = info;
		info = NULL;
	}

	if (info) {	// デコード中に抜けた
		delete info;
	}
}

/*
	ファイル共有（添付）情報をエンコード
*/
BOOL ShareInfo::EncodeMsg(char *buf, int bufsize, IPDictList *dlist, int *share_len)
{
	int			len=0;
	int			dlen = 0;
	char		fname[MAX_PATH_U8];
	int			id_base = 0;

	Debug("encfile start\n");

	dlist->clear();

	TGenRandom(&id_base, sizeof(id_base));
	id_base &= 0x3fffffff; // 負数になるのを防ぐ

	*buf = 0;
	for (int i=0; i < fileCnt; i++)
	{
		auto	fdict = make_shared<IPDict>();
		char	addition[100] = "";

		ForcePathToFname(fileInfo[i]->Fname(), fname);
		fileInfo[i]->SetId(id_base + i);

		fdict->put_int(IPMSG_FID_KEY, id_base + i);
		fdict->put_str(IPMSG_FNAME_KEY, fname);
		fdict->put_int(IPMSG_FSIZE_KEY, fileInfo[i]->Size());
		fdict->put_int(IPMSG_MTIME_KEY, fileInfo[i]->Mtime());
		fdict->put_int(IPMSG_FATTR_KEY, fileInfo[i]->Attr());

		if (GET_MODE(fileInfo[i]->Attr()) == IPMSG_FILE_CLIPBOARD) {
			sprintf(addition, ":%x=%x", IPMSG_FILE_CLIPBOARDPOS, fileInfo[i]->Pos());
			fdict->put_int(IPMSG_CLIPPOS_KEY, fileInfo[i]->Pos());
		}
		dlist->push_back(fdict);
		dlen += (int)fdict->pack_size() - 5; // IP2:Z

		len += snprintfz(buf + len, bufsize - len, "%d:%s:%llx:%llx:%x%s:%c",
					fileInfo[i]->Id(), fname, fileInfo[i]->Size(),
					fileInfo[i]->Mtime(), fileInfo[i]->Attr(),
					addition, FILELIST_SEPARATOR);

		if (max(dlen, len) >= bufsize - MAX_BUF)
			break;
	}
	*share_len = max(dlen, len);

	return	TRUE;
}

BOOL ShareInfo::Pack(IPDict *dict)
{
	dict->clear();
	dict->put_int(SID_PKTNO, packetNo);
	dict->put_bytes(SID_TRANS, (BYTE *)transStat, hostCnt * fileCnt);
	dict->put_bytes(SID_LASTPKT, (BYTE *)lastPkt, hostCnt * sizeof(UINT));
	dict->put_int(SID_ATTACH, attachTime);

	IPDictList	dlist;

	for (int i=0; i < fileCnt; i++) {
		auto	fi = fileInfo[i];
		auto	fd = make_shared<IPDict>();

		fd->put_str(SID_FNAME, fi->Fname());
		fd->put_int(SID_ID, fi->Id());
		fd->put_int(SID_ATTR, fi->Attr());
		fd->put_int(SID_SIZE, fi->Size());
		fd->put_int(SID_MTIME, fi->Mtime());
		dlist.push_back(fd);
	}
	if (dlist.size() == 0) {
		dict->clear();
		return	FALSE;
	}
	dict->put_dict_list(SID_FILE, dlist);
	dlist.clear();

	for (int i=0; i < hostCnt; i++) {
		auto	hi = host[i];
		auto	hd = make_shared<IPDict>();

		hd->put_str(SID_UID, hi->hostSub.u.userName);
		hd->put_str(SID_HNAME, hi->hostSub.u.hostName);
		hd->put_str(SID_NICK, hi->nickName);
		hd->put_str(SID_ADDR, hi->hostSub.addr.S());
		hd->put_int(SID_STAT, hi->hostStatus);

		dlist.push_back(hd);
	}
	if (dlist.size() == 0) {
		dict->clear();
		return	FALSE;
	}
	dict->put_dict_list(SID_HOST, dlist);

	return	TRUE;
}

BOOL ShareInfo::UnPack(Cfg *cfg, const IPDict &dict)
{
	UnInit();

	int64	val;
	DynBuf	dbuf;

	if (!dict.get_int(SID_PKTNO, &val)) return FALSE;
	packetNo = (int)val;

	if (!dict.get_bytes(SID_TRANS, &dbuf)) return FALSE;
	if (transStat) {
		delete [] transStat;
	}
	transStat = new char [dbuf.UsedSize()];
	memcpy(transStat, dbuf.Buf(), dbuf.UsedSize());
	for (int i=0; i < dbuf.UsedSize(); i++) {
		if (transStat[i] == ShareMng::TRANS_BUSY) {
			transStat[i] = ShareMng::TRANS_INIT;
		}
	}

	if (!dict.get_int(SID_ATTACH, &val)) return FALSE;
	attachTime = val;

	IPDictList	dlist;
	if (!dict.get_dict_list(SID_FILE, &dlist) || dlist.size() == 0) return FALSE;

	fileInfo = (FileInfo **)calloc(sizeof(FileInfo *), dlist.size());
	for (auto &d: dlist) {
		U8str	s;
		if (!d->get_int(SID_ID, &val)) return FALSE;

		auto fi = new FileInfo((int)val);
		fileInfo[fileCnt++] = fi;
		if (!d->get_str(SID_FNAME, &s)) return FALSE;
		fi->SetFname(s.s());

		if (!d->get_int(SID_ATTR, &val)) return FALSE;
		fi->SetAttr((UINT)val);
		if (GET_MODE(fi->Attr()) == IPMSG_FILE_CLIPBOARD) {
			clipCnt++;
		}
		if (!d->get_int(SID_SIZE, &val)) return FALSE;
		fi->SetSize(val);
		if (!d->get_int(SID_MTIME, &val)) return FALSE;
		fi->SetMtime(val);
	}

	if (!dict.get_dict_list(SID_HOST, &dlist) || dlist.size() == 0) return FALSE;
	host = new Host *[dlist.size()];
	memset(host, 0, dlist.size() * sizeof(Host *));

	if (!dict.get_bytes(SID_LASTPKT, &dbuf) || dbuf.UsedSize() != dlist.size() * sizeof(UINT)) {
		return FALSE;
	}
	if (lastPkt) {
		delete [] lastPkt;
	}
	lastPkt = new UINT [dlist.size()];
	memcpy(lastPkt, dbuf.Buf(), dbuf.UsedSize());

	for (auto &d: dlist) {
		HostSub	hs;
		U8str	s;

		if (!d->get_str(SID_UID, &s)) return FALSE;
		strncpyz(hs.u.userName, s.s(), (int)sizeof(hs.u.userName));

		if (!d->get_str(SID_HNAME, &s)) return FALSE;
		strncpyz(hs.u.hostName, s.s(), (int)sizeof(hs.u.hostName));

		if (!d->get_str(SID_ADDR, &s)) return FALSE;
		if (!hs.addr.Set(s.s())) return FALSE;

		auto h = cfg->fileHosts.GetHostByNameAddr(&hs);

		if (!h) {
			if (!d->get_str(SID_NICK, &s)) return FALSE;
			if (!d->get_int(SID_STAT, &val)) return FALSE;

			h = new Host;
			h->hostSub = hs;
			h->updateTime = time(NULL);
			h->hostStatus = (int)val;
			h->priority = 0;
			strncpyz(h->nickName, s.s(), (int)sizeof(h->nickName));
			cfg->fileHosts.AddHost(h);
		}
		host[hostCnt++] = h;
		h->RefCnt(1);
	}
	return	TRUE;
}


